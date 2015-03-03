/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <pthread.h>
#include <signal.h>

#include <dlog.h>
#include <download-provider-interface.h>
#include <download-provider.h>
#include <tzplatform_config.h>

#define DEFAULT_INSTALL_PATH tzplatform_getenv(TZ_USER_DOWNLOADS)

#ifdef SUPPORT_CHECK_IPC
#include <sys/ioctl.h>
#endif

#define DP_CHECK_CONNECTION do {\
	if (__check_connections() != DP_ERROR_NONE) {\
		pthread_mutex_unlock(&g_function_mutex);\
		return DOWNLOAD_ADAPTOR_ERROR_IO_ERROR;\
	}\
} while(0)

#define DP_CHECK_PROVIDER_STATUS do {\
	dp_error_type errorcode = DP_ERROR_NONE;\
	errorcode = __ipc_check_ready_status(g_interface_info->cmd_socket);\
	if (errorcode != DP_ERROR_NONE) {\
		pthread_mutex_unlock(&g_interface_info->mutex);\
		if (errorcode == DP_ERROR_IO_ERROR)\
			__disconnect_from_provider();\
		pthread_mutex_unlock(&g_function_mutex);\
		return DOWNLOAD_ADAPTOR_ERROR_IO_ERROR;\
	}\
} while(0)

#define DP_PRE_CHECK_ID do {\
	if (id <= 0) {\
		TRACE_ERROR("[CHECK ID] (%d)", id);\
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;\
	}\
} while(0)

#define MAX_DOWNLOAD_HANDLE 32

#ifdef SUPPORT_LOG_MESSAGE
#include <dlog.h>
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "DOWNLOAD_PROVIDER_INTERFACE"
#define TRACE_DEBUG(format, ARG...) LOGD(format, ##ARG)
#define TRACE_ERROR(format, ARG...) LOGE(format, ##ARG)
#define TRACE_STRERROR(format, ARG...) LOGE(format" [%s]", ##ARG, strerror(errno))
#define TRACE_INFO(format, ARG...) LOGI(format, ##ARG)

#ifdef SECURE_LOGD
#define TRACE_SECURE_DEBUG(format, ARG...) SECURE_LOGD(format, ##ARG)
#else
#define TRACE_SECURE_DEBUG(...) do { } while(0)
#endif
#ifdef SECURE_LOGI
#define TRACE_SECURE_INFO(format, ARG...) SECURE_LOGI(format, ##ARG)
#else
#define TRACE_SECURE_INFO(...) do { } while(0)
#endif
#ifdef SECURE_LOGE
#define TRACE_SECURE_ERROR(format, ARG...) SECURE_LOGE(format, ##ARG)
#else
#define TRACE_SECURE_ERROR(...) do { } while(0)
#endif

#else
#define TRACE_DEBUG(...) do { } while(0)
#define TRACE_ERROR(...) do { } while(0)
#define TRACE_STRERROR(...) do { } while(0)
#define TRACE_INFO(...) do { } while(0)
#define TRACE_SECURE_DEBUG(...) do { } while(0)
#define TRACE_SECURE_INFO(...) do { } while(0)
#define TRACE_SECURE_ERROR(...) do { } while(0)
#endif

// define type
typedef struct {
	// send command * get return value.
	int cmd_socket;
	// getting event from download-provider
	int event_socket;
	pthread_mutex_t mutex; // lock before using, unlock after using
} dp_interface_info;

typedef struct {
	dp_interface_state_changed_cb state;
	void *state_data;
	dp_interface_progress_cb progress;
	void *progress_data;
} dp_interface_callback;

typedef struct {
	int id;
	dp_interface_callback callback;
} dp_interface_slot;

// declare the variables
dp_interface_info *g_interface_info = NULL;
dp_interface_slot g_interface_slots[MAX_DOWNLOAD_HANDLE];
static pthread_mutex_t g_function_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_clear_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t g_interface_event_thread_id = 0;

//////////// defines functions /////////////////



static int __dp_interface_convert_network_adaptor(int type)
{
	switch (type) {
	case DOWNLOAD_ADAPTOR_NETWORK_WIFI:
		return DP_NETWORK_TYPE_WIFI;
	case DOWNLOAD_ADAPTOR_NETWORK_DATA_NETWORK:
		return DP_NETWORK_TYPE_DATA_NETWORK;
	case DOWNLOAD_ADAPTOR_NETWORK_WIFI_DIRECT:
		return DP_NETWORK_TYPE_WIFI_DIRECT;
	case DOWNLOAD_ADAPTOR_NETWORK_ALL:
		return DP_NETWORK_TYPE_ALL;
	default:
		break;
	}
	return type;
}

static int __dp_interface_convert_network_provider(int type)
{
	switch (type) {
	case DP_NETWORK_TYPE_WIFI:
		return DOWNLOAD_ADAPTOR_NETWORK_WIFI;
	case DP_NETWORK_TYPE_DATA_NETWORK:
		return DOWNLOAD_ADAPTOR_NETWORK_DATA_NETWORK;
	case DP_NETWORK_TYPE_WIFI_DIRECT:
		return DOWNLOAD_ADAPTOR_NETWORK_WIFI_DIRECT;
	case DOWNLOAD_ADAPTOR_NETWORK_ALL:
		return DP_NETWORK_TYPE_ALL;
	default:
		break;
	}
	return type;
}

static int __dp_interface_convert_state(int state)
{
	switch (state) {
	case DP_STATE_READY:
		TRACE_DEBUG("READY");
		return DOWNLOAD_ADPATOR_STATE_READY;
	case DP_STATE_CONNECTING:
		TRACE_DEBUG("CONNECTING/QUEUED");
		return DOWNLOAD_ADPATOR_STATE_QUEUED;
	case DP_STATE_QUEUED:
		TRACE_DEBUG("QUEUED");
		return DOWNLOAD_ADPATOR_STATE_QUEUED;
	case DP_STATE_DOWNLOADING:
		TRACE_DEBUG("DOWNLOADING");
		return DOWNLOAD_ADPATOR_STATE_DOWNLOADING;
	case DP_STATE_PAUSE_REQUESTED:
		TRACE_DEBUG("PAUSE_REQUESTED/DOWNLOADING");
		return DOWNLOAD_ADPATOR_STATE_DOWNLOADING;
	case DP_STATE_PAUSED:
		TRACE_DEBUG("PAUSED");
		return DOWNLOAD_ADPATOR_STATE_PAUSED;
	case DP_STATE_COMPLETED:
		TRACE_DEBUG("COMPLETED");
		return DOWNLOAD_ADPATOR_STATE_COMPLETED;
	case DP_STATE_CANCELED:
		TRACE_DEBUG("CANCELED");
		return DOWNLOAD_ADPATOR_STATE_CANCELED;
	case DP_STATE_FAILED:
		TRACE_DEBUG("FAILED");
		return DOWNLOAD_ADPATOR_STATE_FAILED;
	default:
		break;
	}
	return DOWNLOAD_ADPATOR_STATE_NONE;
}

static int __dp_interface_convert_errorcode(int errorcode)
{
	switch (errorcode) {
	case DP_ERROR_NONE:
		return DOWNLOAD_ADAPTOR_ERROR_NONE;
	case DP_ERROR_INVALID_PARAMETER:
		TRACE_DEBUG("ERROR_INVALID_PARAMETER");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	case DP_ERROR_OUT_OF_MEMORY:
		TRACE_DEBUG("ERROR_OUT_OF_MEMORY");
		return DOWNLOAD_ADAPTOR_ERROR_OUT_OF_MEMORY;
	case DP_ERROR_IO_EAGAIN:
		TRACE_DEBUG("ERROR_IO_ERROR(EAGAIN)");
		return DOWNLOAD_ADAPTOR_ERROR_IO_ERROR;
	case DP_ERROR_IO_EINTR:
		TRACE_DEBUG("ERROR_IO_ERROR(EINTR)");
		return DOWNLOAD_ADAPTOR_ERROR_IO_ERROR;
	case DP_ERROR_IO_ERROR:
		TRACE_DEBUG("ERROR_IO_ERROR");
		return DOWNLOAD_ADAPTOR_ERROR_IO_ERROR;
	case DP_ERROR_NETWORK_UNREACHABLE:
		TRACE_DEBUG("ERROR_NETWORK_UNREACHABLE");
		return DOWNLOAD_ADAPTOR_ERROR_NETWORK_UNREACHABLE;
	case DP_ERROR_NO_SPACE:
		TRACE_DEBUG("ERROR_NO_SPACE");
		return DOWNLOAD_ADAPTOR_ERROR_NO_SPACE;
	case DP_ERROR_FIELD_NOT_FOUND:
		TRACE_DEBUG("ERROR_FIELD_NOT_FOUND");
		return DOWNLOAD_ADAPTOR_ERROR_FIELD_NOT_FOUND;
	case DP_ERROR_INVALID_STATE:
		TRACE_DEBUG("ERROR_INVALID_STATE");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_STATE;
	case DP_ERROR_CONNECTION_FAILED:
		TRACE_DEBUG("ERROR_CONNECTION_TIMED_OUT/CONNECTION_FAILED");
		return DOWNLOAD_ADAPTOR_ERROR_CONNECTION_TIMED_OUT;
	case DP_ERROR_INVALID_URL:
		TRACE_DEBUG("ERROR_INVALID_URL");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_URL;
	case DP_ERROR_INVALID_DESTINATION:
		TRACE_DEBUG("ERROR_INVALID_DESTINATION");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_DESTINATION;
	case DP_ERROR_PERMISSION_DENIED:
		TRACE_DEBUG("ERROR_PERMISSION_DENIED");
		return DOWNLOAD_ADAPTOR_ERROR_PERMISSION_DENIED;
	case DP_ERROR_QUEUE_FULL:
		TRACE_DEBUG("ERROR_QUEUE_FULL");
		return DOWNLOAD_ADAPTOR_ERROR_QUEUE_FULL;
	case DP_ERROR_ALREADY_COMPLETED:
		TRACE_DEBUG("ERROR_ALREADY_COMPLETED");
		return DOWNLOAD_ADAPTOR_ERROR_ALREADY_COMPLETED;
	case DP_ERROR_FILE_ALREADY_EXISTS:
		TRACE_DEBUG("ERROR_FILE_ALREADY_EXISTS");
		return DOWNLOAD_ADAPTOR_ERROR_FILE_ALREADY_EXISTS;
	case DP_ERROR_TOO_MANY_DOWNLOADS:
		TRACE_DEBUG("ERROR_TOO_MANY_DOWNLOADS");
		return DOWNLOAD_ADAPTOR_ERROR_TOO_MANY_DOWNLOADS;
	case DP_ERROR_NO_DATA:
		TRACE_DEBUG("ERROR_NO_DATA");
		return DOWNLOAD_ADAPTOR_ERROR_NO_DATA;
	case DP_ERROR_UNHANDLED_HTTP_CODE:
		TRACE_DEBUG("ERROR_UNHANDLED_HTTP_CODE");
		return DOWNLOAD_ADAPTOR_ERROR_UNHANDLED_HTTP_CODE;
	case DP_ERROR_CANNOT_RESUME:
		TRACE_DEBUG("ERROR_CANNOT_RESUME");
		return DOWNLOAD_ADAPTOR_ERROR_CANNOT_RESUME;
	case DP_ERROR_ID_NOT_FOUND:
		TRACE_DEBUG("ERROR_ID_NOT_FOUND");
		return DOWNLOAD_ADAPTOR_ERROR_ID_NOT_FOUND;
	case DP_ERROR_UNKNOWN:
		TRACE_DEBUG("ERROR_INVALID_STATE/UNKNOWN");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_STATE;
	case DP_ERROR_INVALID_NETWORK_TYPE:
		TRACE_DEBUG("ERROR_INVALID_NETWORK_TYPE");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_NETWORK_TYPE;
	default:
		break;
	}
	return DOWNLOAD_ADAPTOR_ERROR_NONE;
}

static int __get_my_slot_index(int id)
{
	int i = 0;
	// search same info in array.
	for (; i < MAX_DOWNLOAD_HANDLE; i++)
		if (g_interface_slots[i].id == id)
			return i;
	return -1;
}

static int __get_empty_slot_index()
{
	int i = 0;
	for (; i < MAX_DOWNLOAD_HANDLE; i++)
		if (g_interface_slots[i].id <= 0)
			return i;
	return -1;
}

static dp_error_type __get_standard_errorcode(dp_error_type basecode)
{
	dp_error_type errorcode = basecode;
	if (errno == EPIPE) {
		TRACE_ERROR("[EPIPE] Broken Pipe [%d]", errno);
		errorcode = DP_ERROR_IO_ERROR;
	} else if (errno == EAGAIN) {
		TRACE_ERROR
			("[EAGAIN] Resource temporarily unavailable [%d]",
			errno);
		errorcode = DP_ERROR_IO_EAGAIN;
	} else if (errno == EINTR) {
		TRACE_ERROR("[EINTR] Interrupted System Call [%d]", errno);
		errorcode = DP_ERROR_IO_ERROR;
	}
	return errorcode;
}

static int __ipc_read_custom_type(int fd, void *value, size_t type_size)
{
	if (fd < 0) {
		TRACE_ERROR("[CHECK SOCKET]");
		return -1;
	}
	if (value == NULL) {
		TRACE_ERROR("[CHECK value]");
		return -1;
	}
	if (type_size <= 0) {
		TRACE_ERROR("[CHECK size]");
		return -1;
	}

	ssize_t recv_bytes = read(fd, value, type_size);
	if (recv_bytes < 0) {
		TRACE_STRERROR("[CRITICAL] read");
		return -1;
	}
	return 0;
}

static int __ipc_read_int(int fd)
{
	int value = -1;

	if (fd < 0) {
		TRACE_ERROR("[CHECK SOCKET]");
		return -1;
	}
	ssize_t recv_bytes = read(fd, &value, sizeof(int));
	if (recv_bytes < 0) {
		TRACE_STRERROR("[CRITICAL] read");
		return -1;
	}
	return value;
}

static int __ipc_read_download_id(int fd)
{
	int value = -1;
	int read_len = 0;
	int try_count = 5;

	if (fd < 0) {
		TRACE_ERROR("[CHECK SOCKET]");
		return -1;
	}
	do {
		read_len = read(fd, &value, sizeof(int));
		if (read_len < 0) {
			TRACE_STRERROR("[CRITICAL] read");
			return -1;
		}
		try_count--;
	} while (read_len == 0 && value == 0 && try_count > 0);
	return value;
}

// keep the order/ unsigned , str
static char *__ipc_read_string(int fd)
{
	unsigned length = 0;
	size_t recv_size = 0;
	unsigned remain_size = 0;
	size_t buffer_size = 0;
	char *str = NULL;

	if (fd < 0) {
		TRACE_ERROR("[ERROR] CHECK FD[%d]", fd);
		return NULL;
	}

	// read flexible URL from client.
	ssize_t recv_bytes = read(fd, &length, sizeof(unsigned));
	if (recv_bytes < 0) {
		TRACE_STRERROR("[ERROR] read FD[%d] length[%d]", fd, length);
		return NULL;
	}
	if (length < 1 || length > DP_MAX_URL_LEN) {
		TRACE_ERROR("[STRING LEGNTH] [%d]", length);
		return NULL;
	}
	str = (char *)calloc((length + 1), sizeof(char));
	if (str == NULL) {
		TRACE_STRERROR("[ERROR] calloc length:%d FD[%d]", length, fd);
		return NULL;
	}
	remain_size = length;
	do {
		buffer_size = 0;
		if (remain_size > DP_DEFAULT_BUFFER_SIZE)
			buffer_size = DP_DEFAULT_BUFFER_SIZE;
		else
			buffer_size = remain_size;
		recv_size = (size_t)read(fd, str + (int)(length - remain_size),
				buffer_size * sizeof(char));
		if (recv_size > DP_DEFAULT_BUFFER_SIZE) {
			recv_size = -1;
			break;
		}
		if (recv_size > 0)
			remain_size = remain_size - (unsigned)recv_size;
	} while (recv_size > 0 && remain_size > 0);

	if (recv_size == 0) {
		TRACE_STRERROR("[ERROR] closed peer:%d", fd);
		free(str);
		return NULL;
	}
	str[length] = '\0';
	return str;
}

int __ipc_read_bundle(int fd, bundle_raw **b)
{
	unsigned length = 0;
	size_t recv_size = 0;
	unsigned remain_size = 0;
	size_t buffer_size = 0;
	bundle_raw *b_raw = NULL;

	if (fd < 0) {
		TRACE_ERROR("[ERROR] CHECK FD[%d]", fd);
		return 0;
	}

	// read bundle data from client.
	ssize_t recv_bytes = read(fd, &length, sizeof(unsigned));
	if (recv_bytes < 0) {
		TRACE_STRERROR("[ERROR] read FD[%d] length[%d]", fd, length);
		return 0;
	}
	if (length < 1 || length > DP_MAX_URL_LEN) {
		TRACE_ERROR("[STRING LEGNTH] [%d]", length);
		return 0;
	}
	b_raw = (bundle_raw *)calloc(length, 1);
	if (b_raw == NULL) {
		TRACE_STRERROR("[ERROR] calloc length:%d FD[%d]", length, fd);
		return 0;
	}
	remain_size = length;
	do {
		buffer_size = 0;
		if (remain_size > DP_DEFAULT_BUFFER_SIZE)
			buffer_size = DP_DEFAULT_BUFFER_SIZE;
		else
			buffer_size = remain_size;
		recv_size = (size_t)read(fd, b_raw + (int)(length - remain_size),
				buffer_size);
		if (recv_size > DP_DEFAULT_BUFFER_SIZE) {
			recv_size = -1;
			break;
		}
		if (recv_size > 0)
			remain_size = remain_size - (unsigned)recv_size;
	} while (recv_size > 0 && remain_size > 0);

	if (recv_size == 0) {
		TRACE_STRERROR("[ERROR] closed peer:%d", fd);
		bundle_free_encoded_rawdata(&b_raw);
		return 0;
	}
	*b = b_raw;
	return (int)length;
}

static dp_error_type __ipc_return(int fd)
{
	dp_error_type errorcode = DP_ERROR_NONE;

	if (fd < 0) {
		TRACE_ERROR("[CHECK SOCKET]");
		return DP_ERROR_IO_ERROR;
	}
	ssize_t recv_bytes = read(fd, &errorcode, sizeof(dp_error_type));
	if (recv_bytes < 0) {
		TRACE_STRERROR("[CRITICAL] read");
		return __get_standard_errorcode(DP_ERROR_IO_ERROR);
	}
	if (errorcode != DP_ERROR_NONE)
		TRACE_ERROR("return : %d", errorcode);
	return errorcode;
}

static dp_event_info* __ipc_event(int fd)
{
	dp_event_info *event = NULL;

	if (fd < 0) {
		TRACE_ERROR("[CHECK SOCKET]");
		return NULL;
	}

	event = (dp_event_info *) calloc(1, sizeof(dp_event_info));
	if (event == NULL) {
		TRACE_ERROR("[CHECK ALLOCATION]");
		return NULL;
	}
	ssize_t recv_bytes = read(fd, event, sizeof(dp_event_info));
	if (recv_bytes < 0) {
		TRACE_STRERROR("[CRITICAL] read");
		free(event);
		return NULL;
	}
	return event;
}

static int __ipc_send_int(int fd, int value)
{
	if (fd < 0) {
		TRACE_ERROR("[CHECK FD] [%d]", fd);
		return -1;
	}

	if (fd >= 0 && write(fd, &value, sizeof(int)) < 0) {
		TRACE_STRERROR("[CRITICAL] send");
		return -1;
	}
	return 0;
}

// keep the order/ unsigned , str
static dp_error_type __ipc_send_string(int fd, const char *str)
{
	unsigned length = 0;

	if (fd < 0) {
		TRACE_ERROR("[CHECK FD]");
		return DP_ERROR_IO_ERROR;
	}
	if (str == NULL || (length = strlen(str)) <= 0) {
		TRACE_ERROR("[CHECK STRING]");
		return DP_ERROR_INVALID_PARAMETER;
	}

	if (fd >= 0 && write(fd, &length, sizeof(unsigned)) < 0) {
		TRACE_STRERROR("[CRITICAL] send");
		return DP_ERROR_IO_ERROR;
	}
	if (fd >= 0 && write(fd, str, length * sizeof(char)) < 0) {
		TRACE_STRERROR("[CRITICAL] send");
		return DP_ERROR_IO_ERROR;
	}
	return DP_ERROR_NONE;
}

static dp_error_type __ipc_send_raw_bundle(int fd, int type, const bundle_raw *b, int len)
{
	if (fd < 0) {
		TRACE_ERROR("[CHECK FD]");
		return DP_ERROR_IO_ERROR;
	}
	if (b == NULL || len <= 0) {
		TRACE_ERROR("[CHECK STRING]");
		return DP_ERROR_INVALID_PARAMETER;
	}
	if (fd >= 0 && write(fd, &type, sizeof(unsigned)) < 0) {
		TRACE_STRERROR("[CRITICAL] send");
		return DP_ERROR_IO_ERROR;
	}
	if (fd >= 0 && write(fd, &len, sizeof(unsigned)) < 0) {
		TRACE_STRERROR("[CRITICAL] send");
		return DP_ERROR_IO_ERROR;
	}
	if (fd >= 0 && write(fd, b, len) < 0) {
		TRACE_STRERROR("[CRITICAL] send");
		return DP_ERROR_IO_ERROR;
	}
	return DP_ERROR_NONE;
}

static dp_error_type __ipc_send_command
	(int fd, int id, dp_command_type cmd)
{
	if (fd < 0) {
		TRACE_ERROR("[CHECK SOCKET]");
		return DP_ERROR_IO_ERROR;
	}

	dp_command command;
	command.id = id;
	command.cmd = cmd;
	if (fd >= 0 && write(fd, &command, sizeof(dp_command)) < 0) {
		TRACE_STRERROR("[CRITICAL] send");
		return DP_ERROR_IO_ERROR;
	}
	return DP_ERROR_NONE;
}

static dp_error_type __ipc_send_command_return
	(int id, dp_command_type cmd)
{
	if (cmd <= DP_CMD_NONE) {
		TRACE_ERROR("[CHECK COMMAND] (%d)", cmd);
		return DP_ERROR_INVALID_PARAMETER;
	}
	// send commnad with ID
	if (__ipc_send_command(g_interface_info->cmd_socket, id, cmd) !=
			DP_ERROR_NONE)
		return DP_ERROR_IO_ERROR;
	// return from provider.
	return __ipc_return(g_interface_info->cmd_socket);
}

static int __create_socket()
{
	int sockfd = -1;
	struct timeval tv_timeo = { 2, 500000 }; //2.5 second
	struct sockaddr_un clientaddr;

	if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		TRACE_STRERROR("[CRITICAL] socket system error");
		return -1;
	}

	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv_timeo,
			sizeof( tv_timeo ) ) < 0) {
		TRACE_STRERROR("[CRITICAL] setsockopt SO_RCVTIMEO");
		close(sockfd);
		return -1;
	}

	bzero(&clientaddr, sizeof clientaddr);
	clientaddr.sun_family = AF_UNIX;
	memset(clientaddr.sun_path, 0x00, sizeof(clientaddr.sun_path));
	strncpy(clientaddr.sun_path, DP_IPC, strlen(DP_IPC));
	clientaddr.sun_path[strlen(DP_IPC)] = '\0';
	if (connect(sockfd,
		(struct sockaddr*)&clientaddr, sizeof(clientaddr)) < 0) {
		close(sockfd);
		return -1;
	}
	TRACE_DEBUG("sockfd [%d]", sockfd);
	return sockfd;
}

static void __clear_interface()
{
	TRACE_DEBUG("");
	pthread_mutex_lock(&g_clear_mutex);
	if (g_interface_info != NULL) {
		shutdown(g_interface_info->cmd_socket, 0);
		close(g_interface_info->cmd_socket);
		g_interface_info->cmd_socket= -1;
		shutdown(g_interface_info->event_socket, 0);
		close(g_interface_info->event_socket);
		g_interface_info->event_socket = -1;
		pthread_mutex_destroy(&g_interface_info->mutex);
		free(g_interface_info);
		g_interface_info = NULL;
	}
	pthread_mutex_unlock(&g_clear_mutex);
}

static int __disconnect_from_provider()
{
	TRACE_DEBUG("");
	if (g_interface_event_thread_id > 0 &&
			pthread_kill(g_interface_event_thread_id, 0) != ESRCH) {
		if (pthread_cancel(g_interface_event_thread_id) != 0) {
			TRACE_STRERROR("pthread:%d", (int)g_interface_event_thread_id);
		}
		g_interface_event_thread_id = 0;
	}
	__clear_interface();
	return DP_ERROR_NONE;
}

#ifdef SUPPORT_CHECK_IPC
// clear read buffer. call in head of API before calling IPC_SEND
static void __clear_read_buffer(int fd)
{
	int i;
	int unread_count;
	char tmp_char;

	// FIONREAD : Returns the number of bytes immediately readable
	if (ioctl(fd, FIONREAD, &unread_count) >= 0) {
		if (unread_count > 0) {
			TRACE_DEBUG("[CLEAN] garbage packet[%ld]", unread_count);
			for ( i = 0; i < unread_count; i++) {
				if (read(fd, &tmp_char, sizeof(char)) < 0) {
					TRACE_STRERROR("[CHECK] read");
					break;
				}
			}
		}
	}
}
#endif

// ask to provider before sending a command.
// if provider wait in some commnad, can not response immediately
// capi will wait in read block.
// after asking, call clear_read_buffer.
static dp_error_type __ipc_check_ready_status(int fd)
{
	dp_error_type errorcode = DP_ERROR_NONE;

#ifdef SUPPORT_CHECK_IPC
	// echo from provider
	errorcode = __ipc_send_command_return(-1, DP_CMD_ECHO);
	if (errorcode == DP_ERROR_NONE)
		__clear_read_buffer(fd);
#endif
	return errorcode;
}

// listen ASYNC state event, no timeout
static void *__dp_interface_event_manager(void *arg)
{
	int maxfd, index;
	fd_set rset, read_fdset;
	dp_event_info *eventinfo = NULL;

	if (g_interface_info == NULL) {
		TRACE_STRERROR("[CRITICAL] INTERFACE null");
		return 0;
	}
	if (g_interface_info->event_socket < 0) {
		TRACE_STRERROR("[CRITICAL] IPC NOT ESTABILISH");
		return 0;
	}

	// deferred wait to cancal until next function called.
	// ex) function : select, read in this thread
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

	maxfd = g_interface_info->event_socket;
	FD_ZERO(&read_fdset);
	FD_SET(g_interface_info->event_socket, &read_fdset);

	int sock = g_interface_info->event_socket;

	while(g_interface_info != NULL
			&& g_interface_info->event_socket >= 0) {
		rset = read_fdset;
		if (select((maxfd + 1), &rset, 0, 0, 0) < 0) {
			TRACE_STRERROR("[CRITICAL] select");
			break;
		}

		if (g_interface_event_thread_id <=0
			|| pthread_self() != g_interface_event_thread_id) {
			TRACE_ERROR
				("[CRITICAL] [CHECK TID] SELF ID [%d] Global ID (%d)",
				pthread_self(), g_interface_event_thread_id);
			// another thread may work. just terminate
			break;
		}

		if (FD_ISSET(sock, &rset) > 0) {
			// read state info from socket
			eventinfo = __ipc_event(sock);
			if (eventinfo == NULL || eventinfo->id <= 0) {
				// failed to read from socket // ignore this status
				free(eventinfo);
				TRACE_STRERROR("[CRITICAL] Can not read Event packet");
				g_interface_event_thread_id = 0;
				__clear_interface();
				return 0;
			}

			if ((index = __get_my_slot_index(eventinfo->id)) < 0) {
				TRACE_ERROR("[CRITICAL] not found slot for [%d]",
					eventinfo->id);
				free(eventinfo);
				continue;
			}

			// begin protect callback sections & thread safe
			pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
			dp_interface_callback *callback =
				&g_interface_slots[index].callback;

			if (eventinfo->state == DP_STATE_DOWNLOADING
				&& eventinfo->received_size > 0) {
				if (eventinfo->id == g_interface_slots[index].id &&
					callback->progress != NULL) {
					// progress event
					callback->progress(eventinfo->id,
						eventinfo->received_size,
						callback->progress_data);
				}
			} else {
				if (eventinfo->id == g_interface_slots[index].id &&
					callback->state != NULL) {
					// state event
					callback->state(eventinfo->id,
						__dp_interface_convert_state(eventinfo->state),
						callback->state_data);
				}
			}
			free(eventinfo);
			pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		}
	} // while

	FD_CLR(sock, &read_fdset);
	g_interface_event_thread_id = 0; // set 0 to not call pthread_cancel
	TRACE_DEBUG("thread end by itself");
	return 0;
}

static int __connect_to_provider()
{
	pthread_mutex_lock(&g_clear_mutex);
	if (g_interface_info == NULL) {

		g_interface_info =
			(dp_interface_info *) calloc(1, sizeof(dp_interface_info));
	}

	if (g_interface_info != NULL) {

		int connect_retry = 3;
		g_interface_info->cmd_socket = -1;
		while(g_interface_info->cmd_socket < 0 && connect_retry-- > 0) {
			g_interface_info->cmd_socket = __create_socket();
			if (g_interface_info->cmd_socket < 0)
				usleep(50000);
		}
		if (g_interface_info->cmd_socket < 0) {
			TRACE_STRERROR("[CRITICAL] connect system error");
			free(g_interface_info);
			g_interface_info = NULL;
			pthread_mutex_unlock(&g_clear_mutex);
			return DP_ERROR_IO_ERROR;
		}
		// send a command
		if (__ipc_send_int(g_interface_info->cmd_socket,
			DP_CMD_SET_COMMAND_SOCKET) < 0) {
			close(g_interface_info->cmd_socket);
			free(g_interface_info);
			g_interface_info = NULL;
			pthread_mutex_unlock(&g_clear_mutex);
			return DP_ERROR_IO_ERROR;
		}
#ifndef SO_PEERCRED
		// send PID. Not support SO_PEERCRED
		if (__ipc_send_int(g_interface_info->cmd_socket, getpid()) < 0) {
			close(g_interface_info->cmd_socket);
			free(g_interface_info);
			g_interface_info = NULL;
			pthread_mutex_unlock(&g_clear_mutex);
			return DP_ERROR_IO_ERROR;
		}
		if (__ipc_send_int(g_interface_info->cmd_socket, getuid()) < 0) {
			close(g_interface_info->cmd_socket);
			free(g_interface_info);
			g_interface_info = NULL;
			pthread_mutex_unlock(&g_clear_mutex);
			return DP_ERROR_IO_ERROR;
		}
		if (__ipc_send_int(g_interface_info->cmd_socket, getgid()) < 0) {
			close(g_interface_info->cmd_socket);
			free(g_interface_info);
			g_interface_info = NULL;
			pthread_mutex_unlock(&g_clear_mutex);
			return DP_ERROR_IO_ERROR;
		}
#endif
		g_interface_info->event_socket = __create_socket();
		if (g_interface_info->event_socket < 0) {
			TRACE_STRERROR("[CRITICAL] connect system error");
			close(g_interface_info->cmd_socket);
			free(g_interface_info);
			g_interface_info = NULL;
			pthread_mutex_unlock(&g_clear_mutex);
			return DP_ERROR_IO_ERROR;
		}
		// send a command
		if (__ipc_send_int(g_interface_info->event_socket,
			DP_CMD_SET_EVENT_SOCKET) < 0) {
			close(g_interface_info->cmd_socket);
			close(g_interface_info->event_socket);
			free(g_interface_info);
			g_interface_info = NULL;
			pthread_mutex_unlock(&g_clear_mutex);
			return DP_ERROR_IO_ERROR;
		}
#ifndef SO_PEERCRED
		// send PID. Not support SO_PEERCRED
		if (__ipc_send_int
				(g_interface_info->event_socket, getpid()) < 0) {
			close(g_interface_info->cmd_socket);
			close(g_interface_info->event_socket);
			free(g_interface_info);
			g_interface_info = NULL;
			pthread_mutex_unlock(&g_clear_mutex);
			return DP_ERROR_IO_ERROR;
		}
#endif

		int ret = pthread_mutex_init(&g_interface_info->mutex, NULL);
		if (ret != 0) {
			TRACE_STRERROR("ERR:pthread_mutex_init FAIL with %d.", ret);
			__clear_interface();
			pthread_mutex_unlock(&g_clear_mutex);
			return DP_ERROR_IO_ERROR;
		}

	}
	pthread_mutex_unlock(&g_clear_mutex);
	if (g_interface_event_thread_id <= 0) {
		// create thread here ( getting event_socket )
		pthread_attr_t thread_attr;
		if (pthread_attr_init(&thread_attr) != 0) {
			TRACE_STRERROR("[CRITICAL] pthread_attr_init");
			__disconnect_from_provider();
			return DP_ERROR_IO_ERROR;
		}
		if (pthread_attr_setdetachstate(&thread_attr,
			PTHREAD_CREATE_DETACHED) != 0) {
			TRACE_STRERROR
				("[CRITICAL] pthread_attr_setdetachstate");
			__disconnect_from_provider();
			return DP_ERROR_IO_ERROR;
		}
		if (pthread_create(&g_interface_event_thread_id,
				&thread_attr, __dp_interface_event_manager,
				g_interface_info) != 0) {
			TRACE_STRERROR("[CRITICAL] pthread_create");
			__clear_interface();
			g_interface_event_thread_id = 0;
			return DP_ERROR_IO_ERROR;
		}
	}
	return DP_ERROR_NONE;
}

static dp_error_type __check_connections()
{
	int ret = 0;

	if (g_interface_info == NULL)
		if ((ret = __connect_to_provider()) != DP_ERROR_NONE)
			return ret;

	if (g_interface_info == NULL || g_interface_info->cmd_socket < 0) {
		TRACE_ERROR("[CHECK IPC]");
		return DP_ERROR_IO_ERROR;
	}
	return DP_ERROR_NONE;
}

// used frequently
static dp_error_type __dp_interface_set_string
	(const int id, const dp_command_type cmd, const char *value)
{
	dp_error_type errorcode = DP_ERROR_NONE;
	if (value == NULL || strlen(value) <= 0) {
		TRACE_ERROR("[CHECK url]");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	DP_PRE_CHECK_ID;

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	int fd = g_interface_info->cmd_socket;

	// send commnad with ID
	errorcode = __ipc_send_command_return(id, cmd);
	if (errorcode == DP_ERROR_NONE) {
		// send string
		errorcode = __ipc_send_string(fd, value);
		if (errorcode == DP_ERROR_NONE) {
			// return from provider.
			errorcode = __ipc_return(fd);
		}
	}
	pthread_mutex_unlock(&g_interface_info->mutex);
	if (errorcode == DP_ERROR_IO_ERROR)
		__disconnect_from_provider();
	pthread_mutex_unlock(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

static dp_error_type __dp_interface_set_strings
	(const int id, const dp_command_type cmd, const char **strings,
	const unsigned count)
{
	dp_error_type errorcode = DP_ERROR_NONE;
	if (strings == NULL || count == 0) {
		TRACE_ERROR("[CHECK strings]");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	DP_PRE_CHECK_ID;

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	int fd = g_interface_info->cmd_socket;

	// send commnad with ID
	errorcode = __ipc_send_command_return(id, cmd);
	if (errorcode == DP_ERROR_NONE) {
		if (__ipc_send_int(fd, (int)count) == 0) {
			int i = 0;
			for (; i < count; i++) {
				// send string
				errorcode = __ipc_send_string(fd, strings[i]);
				if (errorcode != DP_ERROR_NONE)
					break;
			}
		} else {
			errorcode = DP_ERROR_IO_ERROR;
		}
		if (errorcode == DP_ERROR_NONE) {
			// return from provider.
			errorcode = __ipc_return(fd);
		}
	}
	pthread_mutex_unlock(&g_interface_info->mutex);
	if (errorcode == DP_ERROR_IO_ERROR)
		__disconnect_from_provider();
	pthread_mutex_unlock(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

static dp_error_type __dp_interface_get_string
	(const int id, const dp_command_type cmd, char **value)
{
	int errorcode = DP_ERROR_NONE;
	char *recv_str = NULL;

	if (value == NULL) {
		TRACE_ERROR("[CHECK buffer]");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	DP_PRE_CHECK_ID;

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	int fd = g_interface_info->cmd_socket;

	errorcode = __ipc_send_command_return(id, cmd);
	if (errorcode == DP_ERROR_NONE) {
		// getting state with ID from provider.
		recv_str = __ipc_read_string(fd);
		if (recv_str != NULL)
			*value = recv_str;
		else
			errorcode = __get_standard_errorcode(DP_ERROR_IO_ERROR);
	}
	pthread_mutex_unlock(&g_interface_info->mutex);
	if (errorcode == DP_ERROR_IO_ERROR)
		__disconnect_from_provider();
	pthread_mutex_unlock(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

static dp_error_type __dp_interface_get_strings
	(const int id, const dp_command_type cmd, const char **strings,
	const unsigned length, char ***values, unsigned *count)
{
	int errorcode = DP_ERROR_NONE;
	int i = 0;
	int recv_str_index = 0;
	char **recv_strings = NULL;

	if (values == NULL || count == NULL) {
		TRACE_ERROR("[CHECK buffer]");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	DP_PRE_CHECK_ID;

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	int fd = g_interface_info->cmd_socket;

	errorcode = __ipc_send_command_return(id, cmd);
	if (errorcode == DP_ERROR_NONE) {
		if (length > 0 && strings != NULL) {
			if (__ipc_send_int(fd, (int)length) == 0) {
				for (i = 0; i < length; i++) {
					// send string
					errorcode = __ipc_send_string(fd, strings[i]);
					if (errorcode != DP_ERROR_NONE)
						break;
				}
			} else {
				errorcode = DP_ERROR_IO_ERROR;
			}
		}
		if (errorcode == DP_ERROR_NONE) {
			// return from provider.
			errorcode = __ipc_return(fd);
		}
	}
	if (errorcode == DP_ERROR_NONE) {
		int recv_int = __ipc_read_int(fd);
		if (recv_int < 0) {
			errorcode = DP_ERROR_IO_ERROR;
		} else if (recv_int > 0) {
			recv_strings = (char **)calloc(recv_int, sizeof(char *));
			if (recv_strings == NULL) {
				errorcode = DP_ERROR_OUT_OF_MEMORY;
			} else {
				for (i = 0; i < recv_int; i++) {
					char *recv_str = __ipc_read_string(fd);
					if (recv_str == NULL) {
						errorcode =
							__get_standard_errorcode(DP_ERROR_IO_ERROR);
						break;
					} else {
						recv_strings[recv_str_index++] = recv_str;
					}
				}
			}
		}
		if (errorcode == DP_ERROR_NONE) {
			*count = recv_str_index;
			*values = recv_strings;
		} else {
			*count = 0;
			for (i = 0; i < recv_str_index; i++)
				free(recv_strings[i]);
			free(recv_strings);
		}
	}
	pthread_mutex_unlock(&g_interface_info->mutex);
	if (errorcode == DP_ERROR_IO_ERROR)
		__disconnect_from_provider();
	pthread_mutex_unlock(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

static dp_error_type __dp_interface_get_int
	(const int id, dp_command_type cmd, int *value)
{
	int errorcode = DP_ERROR_NONE;
	int recv_int = -1;

	if (value == NULL) {
		TRACE_ERROR("[CHECK buffer]");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	DP_PRE_CHECK_ID;

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	int fd = g_interface_info->cmd_socket;

	errorcode = __ipc_send_command_return(id, cmd);
	if (errorcode == DP_ERROR_NONE) {
		recv_int = __ipc_read_int(fd);
		if (recv_int >= 0) {
			*value = recv_int;
			TRACE_DEBUG("ID : %d recv_int : %d", id, *value);
		} else {
			errorcode = __get_standard_errorcode(DP_ERROR_IO_ERROR);
		}
	}
	pthread_mutex_unlock(&g_interface_info->mutex);
	if (errorcode == DP_ERROR_IO_ERROR)
		__disconnect_from_provider();
	pthread_mutex_unlock(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

static dp_error_type __dp_interface_set_int
	(const int id, dp_command_type cmd, const int value)
{
	int errorcode = DP_ERROR_NONE;

	DP_PRE_CHECK_ID;

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	int fd = g_interface_info->cmd_socket;

	// send commnad with ID
	errorcode = __ipc_send_command_return(id, cmd);
	if (errorcode == DP_ERROR_NONE) {
		// send string
		if (__ipc_send_int(fd, value) == 0) {
			// return from provider.
			errorcode = __ipc_return(fd);
		} else {
			errorcode = DP_ERROR_IO_ERROR;
		}
	}
	pthread_mutex_unlock(&g_interface_info->mutex);
	if (errorcode == DP_ERROR_IO_ERROR)
		__disconnect_from_provider();
	pthread_mutex_unlock(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

static dp_error_type __dp_interface_set_raw_bundle
	(const int id, const dp_command_type cmd, int type, const bundle_raw *b, int len)
{
	dp_error_type errorcode = DP_ERROR_NONE;
	if (b == NULL) {
		TRACE_ERROR("[CHECK bundle]");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	DP_PRE_CHECK_ID;

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	int fd = g_interface_info->cmd_socket;

	// send commnad with ID
	errorcode = __ipc_send_command_return(id, cmd);
	if (errorcode == DP_ERROR_NONE) {
		// send raw bundle
		errorcode = __ipc_send_raw_bundle(fd, type, b, len);
		if (errorcode == DP_ERROR_NONE) {
			// return from provider.
			errorcode = __ipc_return(fd);
		}
	}
	pthread_mutex_unlock(&g_interface_info->mutex);
	if (errorcode == DP_ERROR_IO_ERROR)
		__disconnect_from_provider();
	pthread_mutex_unlock(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

static dp_error_type __dp_interface_get_raw_bundle
	(const int id, const dp_command_type cmd, int type, bundle_raw **value, int *len)
{
	int errorcode = DP_ERROR_NONE;

	if (value == NULL) {
		TRACE_ERROR("[CHECK buffer]");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	DP_PRE_CHECK_ID;

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	int fd = g_interface_info->cmd_socket;

	errorcode = __ipc_send_command_return(id, cmd);
	if (errorcode == DP_ERROR_NONE) {
		errorcode = __ipc_send_int(fd, type);
		if(errorcode == 0) {
			errorcode = __ipc_return(g_interface_info->cmd_socket);
			if (errorcode == DP_ERROR_NONE) {
				*len = __ipc_read_bundle(fd, value);
				if (*len <= 0)
					errorcode = __get_standard_errorcode(DP_ERROR_IO_ERROR);
			} else {
				TRACE_ERROR("[ERROR] Fail to get result for sending type value]");
				errorcode = DP_ERROR_IO_ERROR;
			}
		} else {
			TRACE_ERROR("[ERROR] Fail to send type]");
			errorcode = DP_ERROR_IO_ERROR;
		}
	} else {
		TRACE_ERROR("[ERROR] Fail to send command]");
		errorcode = DP_ERROR_IO_ERROR;
	}
	pthread_mutex_unlock(&g_interface_info->mutex);
	if (errorcode == DP_ERROR_IO_ERROR)
		__disconnect_from_provider();
	pthread_mutex_unlock(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}


static int _mkdir(const char *dir, mode_t mode) {
        char tmp[256];
        char *p = NULL;
        size_t len;

        snprintf(tmp, sizeof(tmp),"%s",dir);
        len = strlen(tmp);
        if(tmp[len - 1] == '/')
                tmp[len - 1] = 0;
        for(p = tmp + 1; *p; p++)
                if(*p == '/') {
                        *p = 0;
                        mkdir(tmp, mode);
                        *p = '/';
                }
        return mkdir(tmp, mode);
}

static dp_error_type __create_dir(const char *install_dir)
{
    dp_error_type ret = DP_ERROR_NONE;
        /* read/write/search permissions for owner and group,
         * and with read/search permissions for others as fas as
         * umask allows. */
    if (_mkdir(install_dir, S_IRWXU | S_IRWXG | S_IRWXO)) {
        ret = DP_ERROR_PERMISSION_DENIED;
    }
    return ret;
}


/////////////////////// APIs /////////////////////////////////

int dp_interface_create(int *id)
{
	int errorcode = DP_ERROR_NONE;
	int t_id = 0;
	int index = -1;

	if (id == NULL) {
		TRACE_ERROR("[CHECK id variable]");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	pthread_mutex_lock(&g_function_mutex);

	if ((index = __get_empty_slot_index()) < 0) {
		TRACE_ERROR
			("[ERROR] TOO_MANY_DOWNLOADS[%d]", MAX_DOWNLOAD_HANDLE);
		pthread_mutex_unlock(&g_function_mutex);
		return DOWNLOAD_ADAPTOR_ERROR_TOO_MANY_DOWNLOADS;
	}

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	errorcode = __ipc_send_command_return(-1, DP_CMD_CREATE);
	if (errorcode == DP_ERROR_NONE) {
		// getting state with ID from provider.
		t_id = __ipc_read_download_id(g_interface_info->cmd_socket);
		if (t_id > 0) {
			*id = t_id;
			g_interface_slots[index].id = t_id;
			g_interface_slots[index].callback.state = NULL;
			g_interface_slots[index].callback.state_data = NULL;
			g_interface_slots[index].callback.progress = NULL;
			g_interface_slots[index].callback.progress_data = NULL;
			errorcode = DP_ERROR_NONE;
		} else {
			errorcode = __get_standard_errorcode(DP_ERROR_IO_ERROR);
		}
	}
	pthread_mutex_unlock(&g_interface_info->mutex);
	if (errorcode == DP_ERROR_IO_ERROR)
		__disconnect_from_provider();
	pthread_mutex_unlock(&g_function_mutex);
	if (errorcode == DP_ERROR_NONE){
	    // safely set the default path, maybe overwritten by the user later
	    const char* dest_path = DEFAULT_INSTALL_PATH;
        if (dest_path){
            struct stat dir_state;
            int stat_ret = stat(dest_path, &dir_state);
            if (stat_ret != 0) {
                __create_dir(dest_path);
            }
            dp_interface_set_destination(t_id, dest_path);
        }
	}
	return __dp_interface_convert_errorcode(errorcode);
}

int dp_interface_destroy(const int id)
{
	int index = -1;
	int errorcode = DP_ERROR_NONE;

	DP_PRE_CHECK_ID;

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	if ((index = __get_my_slot_index(id)) >= 0) {
		g_interface_slots[index].id = 0;
		g_interface_slots[index].callback.state = NULL;
		g_interface_slots[index].callback.state_data = NULL;
		g_interface_slots[index].callback.progress = NULL;
		g_interface_slots[index].callback.progress_data = NULL;
	}
	errorcode = __ipc_send_command_return(id, DP_CMD_DESTROY);
	if (errorcode == DP_ERROR_NONE) {
		// after getting errorcode, send FREE to provider.
		// send again DP_CMD_FREE with ID.
		errorcode = __ipc_send_command
			(g_interface_info->cmd_socket, id, DP_CMD_FREE);
	}
	pthread_mutex_unlock(&g_interface_info->mutex);
	if (errorcode == DP_ERROR_IO_ERROR)
		__disconnect_from_provider();
	pthread_mutex_unlock(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}


int dp_interface_start(const int id)
{

    int errorcode = DP_ERROR_NONE;
	DP_PRE_CHECK_ID;

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	errorcode = __ipc_send_command_return(id, DP_CMD_START);
	pthread_mutex_unlock(&g_interface_info->mutex);
	if (errorcode == DP_ERROR_IO_ERROR)
		__disconnect_from_provider();
	pthread_mutex_unlock(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

int dp_interface_pause(const int id)
{
	int errorcode = DP_ERROR_NONE;

	DP_PRE_CHECK_ID;

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	errorcode = __ipc_send_command_return(id, DP_CMD_PAUSE);
	pthread_mutex_unlock(&g_interface_info->mutex);
	if (errorcode == DP_ERROR_IO_ERROR)
		__disconnect_from_provider();
	pthread_mutex_unlock(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

int dp_interface_cancel(const int id)
{
	int errorcode = DP_ERROR_NONE;

	DP_PRE_CHECK_ID;

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	errorcode = __ipc_send_command_return(id, DP_CMD_CANCEL);
	pthread_mutex_unlock(&g_interface_info->mutex);
	if (errorcode == DP_ERROR_IO_ERROR)
		__disconnect_from_provider();
	pthread_mutex_unlock(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

int dp_interface_set_url(const int id, const char *url)
{
	return __dp_interface_set_string(id, DP_CMD_SET_URL, url);
}

int dp_interface_get_url(const int id, char **url)
{
	return __dp_interface_get_string(id, DP_CMD_GET_URL, url);
}

int dp_interface_set_network_type(const int id, int net_type)
{
	return __dp_interface_set_int(id, DP_CMD_SET_NETWORK_TYPE,
		__dp_interface_convert_network_adaptor(net_type));
}

int dp_interface_get_network_type(const int id, int *net_type)
{
	if (net_type == NULL) {
		TRACE_ERROR("[CHECK buffer]");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}
	int network_type = DP_NETWORK_TYPE_ALL;
	int ret = __dp_interface_get_int
		(id, DP_CMD_GET_NETWORK_TYPE, &network_type);
	if (ret == DOWNLOAD_ADAPTOR_ERROR_NONE)
		*net_type =
			__dp_interface_convert_network_provider(network_type);
	return ret;
}

int dp_interface_set_destination(const int id, const char *path)
{
    if (path && strlen(path)>0) {
        return __dp_interface_set_string(id, DP_CMD_SET_DESTINATION, path);
    } else {
        return __dp_interface_convert_errorcode(DP_ERROR_NONE);
    }
}


int dp_interface_get_destination(const int id, char **path)
{
	return __dp_interface_get_string(id, DP_CMD_GET_DESTINATION, path);
}

int dp_interface_set_file_name(const int id, const char *file_name)
{
	return __dp_interface_set_string(id, DP_CMD_SET_FILENAME, file_name);
}

int dp_interface_get_file_name(const int id, char **file_name)
{
	return __dp_interface_get_string(id, DP_CMD_GET_FILENAME, file_name);
}

int dp_interface_set_ongoing_notification(const int id, int enable)
{
	return dp_interface_set_notification(id, enable);
}

int dp_interface_set_notification(const int id, int enable)
{
	return __dp_interface_set_int(id, DP_CMD_SET_NOTIFICATION, enable);
}

int dp_interface_get_ongoing_notification(const int id, int *enable)
{
	return dp_interface_get_notification(id, enable);
}

int dp_interface_get_notification(const int id, int *enable)
{
	return __dp_interface_get_int(id, DP_CMD_GET_NOTIFICATION, enable);
}

int dp_interface_get_downloaded_file_path(const int id, char **path)
{
	return __dp_interface_get_string(id, DP_CMD_GET_SAVED_PATH, path);
}

int dp_interface_set_notification_extra_param(const int id, char *key,
	char *value)
{
#if 0
	DP_PRE_CHECK_ID;

	if (key == NULL || value == NULL) {
		TRACE_ERROR("[CHECK param]");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	// send commnad with ID
	if (__ipc_send_command
		(g_interface_info->cmd_socket, id, DP_CMD_SET_EXTRA_PARAM)
		!= DP_ERROR_NONE) {
		pthread_mutex_unlock(&g_interface_info->mutex);
		__disconnect_from_provider();
		pthread_mutex_unlock(&g_function_mutex);
		return DOWNLOAD_ADAPTOR_ERROR_IO_ERROR;
	}
	if (__ipc_send_string(g_interface_info->cmd_socket, key)
		!= DP_ERROR_NONE) {
		pthread_mutex_unlock(&g_interface_info->mutex);
		__disconnect_from_provider();
		pthread_mutex_unlock(&g_function_mutex);
		return DOWNLOAD_ADAPTOR_ERROR_IO_ERROR;
	}

	if (__ipc_send_string(g_interface_info->cmd_socket, value)
		!= DP_ERROR_NONE) {
		pthread_mutex_unlock(&g_interface_info->mutex);
		__disconnect_from_provider();
		pthread_mutex_unlock(&g_function_mutex);
		return DOWNLOAD_ADAPTOR_ERROR_IO_ERROR;
	}

	int errorcode =
		__ipc_return(g_interface_info->cmd_socket);
	pthread_mutex_unlock(&g_interface_info->mutex);
	if (errorcode == DP_ERROR_IO_ERROR) {
		TRACE_ERROR("[CHECK IO] (%d)", id);
		__disconnect_from_provider();
	}
	pthread_mutex_unlock(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
#endif
	return DOWNLOAD_ADAPTOR_ERROR_NONE;
}

int dp_interface_get_notification_extra_param(const int id, char **key,
	char **value)
{
#if 0
	int errorcode = DP_ERROR_NONE;
	char *key_str = NULL;
	char *value_str = NULL;

	DP_PRE_CHECK_ID;

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	errorcode = __ipc_send_command_return(id, DP_CMD_GET_EXTRA_PARAM);
	if (errorcode != DP_ERROR_NONE) {
		pthread_mutex_unlock(&g_interface_info->mutex);
		if (errorcode == DP_ERROR_IO_ERROR)
			__disconnect_from_provider();
		pthread_mutex_unlock(&g_function_mutex);
		return __dp_interface_convert_errorcode(errorcode);
	}
	// getting state with ID from provider.
	key_str = __ipc_read_string(g_interface_info->cmd_socket);
	if (key_str == NULL) {
		pthread_mutex_unlock(&g_interface_info->mutex);
		errorcode = __get_standard_errorcode(DP_ERROR_IO_ERROR);
		if (errorcode == DP_ERROR_IO_ERROR)
			__disconnect_from_provider();
		pthread_mutex_unlock(&g_function_mutex);
		return __dp_interface_convert_errorcode(errorcode);
	}

	value_str = __ipc_read_string(g_interface_info->cmd_socket);
	pthread_mutex_unlock(&g_interface_info->mutex);
	if (value_str == NULL) {
		free(key_str);
		errorcode = __get_standard_errorcode(DP_ERROR_IO_ERROR);
		if (errorcode == DP_ERROR_IO_ERROR)
			__disconnect_from_provider();
		pthread_mutex_unlock(&g_function_mutex);
		return __dp_interface_convert_errorcode(errorcode);
	}

	*key = key_str;
	*value = value_str;
	pthread_mutex_unlock(&g_function_mutex);
#endif
	return DOWNLOAD_ADAPTOR_ERROR_NONE;
}

int dp_interface_add_http_header_field(const int id, const char *field,
	const char *value)
{
	int errorcode = DP_ERROR_NONE;

	DP_PRE_CHECK_ID;

	if (field == NULL || value == NULL) {
		TRACE_ERROR("[CHECK field or value]");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	// send commnad with ID
	if (__ipc_send_command_return(id, DP_CMD_SET_HTTP_HEADER) !=
			DP_ERROR_NONE) {
		pthread_mutex_unlock(&g_interface_info->mutex);
		__disconnect_from_provider();
		pthread_mutex_unlock(&g_function_mutex);
		return DOWNLOAD_ADAPTOR_ERROR_IO_ERROR;
	}

	if (__ipc_send_string(g_interface_info->cmd_socket, field)
		!= DP_ERROR_NONE) {
		pthread_mutex_unlock(&g_interface_info->mutex);
		__disconnect_from_provider();
		pthread_mutex_unlock(&g_function_mutex);
		return DOWNLOAD_ADAPTOR_ERROR_IO_ERROR;
	}

	if (__ipc_send_string(g_interface_info->cmd_socket, value)
		!= DP_ERROR_NONE) {
		pthread_mutex_unlock(&g_interface_info->mutex);
		__disconnect_from_provider();
		pthread_mutex_unlock(&g_function_mutex);
		return DOWNLOAD_ADAPTOR_ERROR_IO_ERROR;
	}
	// return from provider.
	errorcode = __ipc_return(g_interface_info->cmd_socket);
	pthread_mutex_unlock(&g_interface_info->mutex);
	if (errorcode == DP_ERROR_IO_ERROR) {
		TRACE_ERROR("[CHECK IO] (%d)", id);
		__disconnect_from_provider();
	}
	pthread_mutex_unlock(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

int dp_interface_get_http_header_field(const int id, const char *field,
	char **value)
{
	int errorcode = DP_ERROR_NONE;
	char *str = NULL;

	DP_PRE_CHECK_ID;

	if (field == NULL || value == NULL) {
		TRACE_ERROR("[CHECK field or value]");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	if (__ipc_send_command_return(id, DP_CMD_GET_HTTP_HEADER) !=
			DP_ERROR_NONE) {
		pthread_mutex_unlock(&g_interface_info->mutex);
		__disconnect_from_provider();
		pthread_mutex_unlock(&g_function_mutex);
		return DOWNLOAD_ADAPTOR_ERROR_IO_ERROR;
	}

	if (__ipc_send_string(g_interface_info->cmd_socket, field)
		!= DP_ERROR_NONE) {
		pthread_mutex_unlock(&g_interface_info->mutex);
		__disconnect_from_provider();
		pthread_mutex_unlock(&g_function_mutex);
		return DOWNLOAD_ADAPTOR_ERROR_IO_ERROR;
	}

	errorcode = __ipc_return(g_interface_info->cmd_socket);
	if (errorcode == DP_ERROR_NONE) {
		// getting string with ID from provider.
		str = __ipc_read_string(g_interface_info->cmd_socket);
		if (str != NULL) {
			*value = str;
		} else {
			errorcode = __get_standard_errorcode(DP_ERROR_IO_ERROR);
		}
	}
	pthread_mutex_unlock(&g_interface_info->mutex);
	if (errorcode == DP_ERROR_IO_ERROR || str == NULL)
		__disconnect_from_provider();
	pthread_mutex_unlock(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

int dp_interface_get_http_header_field_list(const int id, char ***fields,
	int *length)
{
	return __dp_interface_get_strings(id, DP_CMD_GET_HTTP_HEADER_LIST,
		NULL, 0, fields, (unsigned *)length);
}

int dp_interface_remove_http_header_field(const int id,
	const char *field)
{
	int errorcode = DP_ERROR_NONE;

	DP_PRE_CHECK_ID;

	if (field == NULL) {
		TRACE_ERROR("[CHECK field]");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	// send commnad with ID
	if (__ipc_send_command_return(id, DP_CMD_DEL_HTTP_HEADER) !=
			DP_ERROR_NONE) {
		pthread_mutex_unlock(&g_interface_info->mutex);
		__disconnect_from_provider();
		pthread_mutex_unlock(&g_function_mutex);
		return DOWNLOAD_ADAPTOR_ERROR_IO_ERROR;
	}
	if (__ipc_send_string(g_interface_info->cmd_socket, field)
		!= DP_ERROR_NONE) {
		pthread_mutex_unlock(&g_interface_info->mutex);
		__disconnect_from_provider();
		pthread_mutex_unlock(&g_function_mutex);
		return DOWNLOAD_ADAPTOR_ERROR_IO_ERROR;
	}
	// return from provider.
	errorcode = __ipc_return(g_interface_info->cmd_socket);
	pthread_mutex_unlock(&g_interface_info->mutex);
	if (errorcode == DP_ERROR_IO_ERROR) {
		TRACE_ERROR("[CHECK IO] (%d)", id);
		__disconnect_from_provider();
	}
	pthread_mutex_unlock(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

int dp_interface_set_state_changed_cb(const int id,
	dp_interface_state_changed_cb callback, void *user_data)
{
	int errorcode = DP_ERROR_NONE;
	int index = -1;

	if (callback == NULL) {
		dp_interface_unset_state_changed_cb(id);
		return DOWNLOAD_ADAPTOR_ERROR_NONE;
	}

	errorcode =
		__dp_interface_set_int(id, DP_CMD_SET_STATE_CALLBACK, 1);
	if (errorcode == DOWNLOAD_ADAPTOR_ERROR_NONE) {
		pthread_mutex_lock(&g_function_mutex);
		// search same info in array.
		index = __get_my_slot_index(id);
		if (index < 0) {
			index = __get_empty_slot_index();
			if (index >= 0) {
				g_interface_slots[index].id = id;
			} else {
				TRACE_ERROR("[ERROR] TOO_MANY_DOWNLOADS [%d]",
					MAX_DOWNLOAD_HANDLE);
				errorcode = DOWNLOAD_ADAPTOR_ERROR_TOO_MANY_DOWNLOADS;
			}
		}
		if (index >= 0) {
			g_interface_slots[index].callback.state = callback;
			g_interface_slots[index].callback.state_data = user_data;
		}
		pthread_mutex_unlock(&g_function_mutex);
	}
	return errorcode;
}

int dp_interface_unset_state_changed_cb(const int id)
{
	int errorcode = DP_ERROR_NONE;
	int index = -1;

	errorcode =
		__dp_interface_set_int(id, DP_CMD_SET_STATE_CALLBACK, 0);
	// clear by force although failed to clear in provider
	pthread_mutex_lock(&g_function_mutex);
	if ((index = __get_my_slot_index(id)) >= 0) {
		g_interface_slots[index].callback.state = NULL;
		g_interface_slots[index].callback.state_data = NULL;
	}
	pthread_mutex_unlock(&g_function_mutex);
	return errorcode;
}

int dp_interface_set_progress_cb(const int id,
	dp_interface_progress_cb callback, void *user_data)
{
	int errorcode = DP_ERROR_NONE;
	int index = -1;

	if (callback == NULL) {
		dp_interface_unset_progress_cb(id);
		return DOWNLOAD_ADAPTOR_ERROR_NONE;
	}

	errorcode =
		__dp_interface_set_int(id, DP_CMD_SET_PROGRESS_CALLBACK, 1);
	if (errorcode == DOWNLOAD_ADAPTOR_ERROR_NONE) {
		pthread_mutex_lock(&g_function_mutex);
		// search same info in array.
		index = __get_my_slot_index(id);
		if (index < 0) {
			index = __get_empty_slot_index();
			if (index >= 0) {
				g_interface_slots[index].id = id;
			} else {
				TRACE_ERROR("[ERROR] TOO_MANY_DOWNLOADS [%d]",
					MAX_DOWNLOAD_HANDLE);
				errorcode = DOWNLOAD_ADAPTOR_ERROR_TOO_MANY_DOWNLOADS;
			}
		}
		if (index >= 0) {
			g_interface_slots[index].callback.progress = callback;
			g_interface_slots[index].callback.progress_data = user_data;
		}
		pthread_mutex_unlock(&g_function_mutex);
	}
	return errorcode;
}

int dp_interface_unset_progress_cb(const int id)
{
	int errorcode = DP_ERROR_NONE;
	int index = -1;

	errorcode =
		__dp_interface_set_int(id, DP_CMD_SET_PROGRESS_CALLBACK, 0);
	// clear by force although failed to clear in provider
	pthread_mutex_lock(&g_function_mutex);
	if ((index = __get_my_slot_index(id)) >= 0) {
		g_interface_slots[index].callback.progress = NULL;
		g_interface_slots[index].callback.progress_data = NULL;
	}
	pthread_mutex_unlock(&g_function_mutex);
	return errorcode;
}

int dp_interface_get_state(const int id, int *state)
{
	if (state == NULL) {
		TRACE_ERROR("[CHECK buffer]");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}
	int statecode = DOWNLOAD_ADPATOR_STATE_NONE;
	int ret = __dp_interface_get_int(id, DP_CMD_GET_STATE, &statecode);
	if (ret == DOWNLOAD_ADAPTOR_ERROR_NONE)
		*state = __dp_interface_convert_state(statecode);
	return ret;
}

int dp_interface_get_temp_path(const int id, char **temp_path)
{
	return __dp_interface_get_string
		(id, DP_CMD_GET_TEMP_SAVED_PATH, temp_path);
}

int dp_interface_get_content_name(const int id, char **content_name)
{
	return __dp_interface_get_string
		(id, DP_CMD_GET_CONTENT_NAME, content_name);
}

int dp_interface_get_content_size(const int id,
	unsigned long long *content_size)
{
	int errorcode = DP_ERROR_NONE;

	if (content_size == NULL) {
		TRACE_ERROR("[CHECK buffer content_size]");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	DP_PRE_CHECK_ID;

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	errorcode =
		__ipc_send_command_return(id, DP_CMD_GET_TOTAL_FILE_SIZE);
	if (errorcode == DP_ERROR_NONE) {
		// getting content_size from provider.
		if (__ipc_read_custom_type(g_interface_info->cmd_socket,
				content_size, sizeof(unsigned long long)) < 0) {
			errorcode = __get_standard_errorcode(DP_ERROR_IO_ERROR);
		}
	}
	pthread_mutex_unlock(&g_interface_info->mutex);
	if (errorcode == DP_ERROR_IO_ERROR)
		__disconnect_from_provider();
	pthread_mutex_unlock(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

int dp_interface_get_mime_type(const int id, char **mime_type)
{
	return __dp_interface_get_string
		(id, DP_CMD_GET_MIME_TYPE, mime_type);
}

int dp_interface_set_auto_download(const int id, int enable)
{
	return __dp_interface_set_int(id, DP_CMD_SET_AUTO_DOWNLOAD, enable);
}

int dp_interface_get_auto_download(const int id, int *enable)
{
	return __dp_interface_get_int(id, DP_CMD_GET_AUTO_DOWNLOAD, enable);
}

int dp_interface_get_error(const int id, int *error)
{
	if (error == NULL) {
		TRACE_ERROR("[CHECK buffer error]");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}
	int errorcode = DP_ERROR_NONE;
	int ret = __dp_interface_get_int(id, DP_CMD_GET_ERROR, &errorcode);
	if (ret == DOWNLOAD_ADAPTOR_ERROR_NONE)
		*error = __dp_interface_convert_errorcode(errorcode);
	return ret;
}

int dp_interface_get_http_status(const int id, int *http_status)
{
	return __dp_interface_get_int
		(id, DP_CMD_GET_HTTP_STATUS, http_status);
}

int dp_interface_add_noti_extra(const int id, const char *key,
	const char **values, const unsigned length)
{
	int i = 0;

	if (key == NULL || values == NULL) {
		TRACE_ERROR("[CHECK key/values] (%d)", id);
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}
	if (length <= 0) {
		TRACE_ERROR("[CHECK legnth] (%d)", id);
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	char *strings[length + 1];
	strings[0] = (char *)key;
	for (i = 0; i < length; i++) {
		strings[i + 1] = (char *)values[i];
	}
	return __dp_interface_set_strings(id, DP_CMD_ADD_EXTRA_PARAM,
		(const char **)strings, length + 1);
}

int dp_interface_get_noti_extra_values(const int id, const char *key,
	char ***values, unsigned *length)
{
	return __dp_interface_get_strings(id, DP_CMD_GET_EXTRA_PARAM,
		&key, 1, values, length);
}

int dp_interface_remove_noti_extra_key(const int id, const char *key)
{
	return __dp_interface_set_string
		(id, DP_CMD_REMOVE_EXTRA_PARAM, key);
}

int dp_interface_set_notification_bundle(const int id, int type, bundle *b)
{
	bundle_raw *r = NULL;
	int len = 0;
	int retval = -1;
	retval = bundle_encode_raw(b, &r, &len);
	if (retval == 0)
		retval = __dp_interface_set_raw_bundle(id, DP_CMD_SET_NOTIFICATION_BUNDLE, type, r, len);
	else {
		bundle_free_encoded_rawdata(&r);
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}
	bundle_free_encoded_rawdata(&r);
	return retval;
}

int dp_interface_get_notification_bundle(const int id, int type, bundle **b)
{
	bundle_raw *r = NULL;
	int len = 0;
	download_adaptor_error_e error = DOWNLOAD_ADAPTOR_ERROR_NONE;

	if (b == NULL) {
		TRACE_ERROR("[CHECK bundle] (%d)", id);
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	error = __dp_interface_get_raw_bundle(id, DP_CMD_GET_NOTIFICATION_BUNDLE, type, &r, &len);
	if (error == DOWNLOAD_ADAPTOR_ERROR_NONE) {
		*b = bundle_decode_raw(r, len);
	}
	bundle_free_encoded_rawdata(&r);
	return error;
}

int dp_interface_set_notification_title(const int id, const char *title)
{
	return __dp_interface_set_string(id, DP_CMD_SET_NOTIFICATION_TITLE, title);
}

int dp_interface_get_notification_title(const int id, char **title)
{
	return __dp_interface_get_string(id, DP_CMD_GET_NOTIFICATION_TITLE, title);
}

int dp_interface_set_notification_description(const int id, const char *description)
{
	return __dp_interface_set_string(id, DP_CMD_SET_NOTIFICATION_DESCRIPTION, description);
}

int dp_interface_get_notification_description(const int id, char **description)
{
	return __dp_interface_get_string(id, DP_CMD_GET_NOTIFICATION_DESCRIPTION, description);
}

int dp_interface_set_notification_type(const int id, int type)
{
	return __dp_interface_set_int(id, DP_CMD_SET_NOTIFICATION_TYPE, type);
}

int dp_interface_get_notification_type(const int id, int *type)
{
	return __dp_interface_get_int(id, DP_CMD_GET_NOTIFICATION_TYPE, type);
}
