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

#ifdef DP_DBUS_ACTIVATION
#include <dbus/dbus.h>
#endif

#ifdef SUPPORT_CHECK_IPC
#include <sys/ioctl.h>
#endif

#define DP_CHECK_CONNECTION do {\
	TRACE_INFO("");\
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
		pthread_mutex_unlock(&g_function_mutex);\
		if (errorcode == DP_ERROR_IO_ERROR)\
			__disconnect_from_provider();\
		return DOWNLOAD_ADAPTOR_ERROR_IO_ERROR;\
	}\
} while(0)

#define DP_PRE_CHECK_ID do {\
	if (id <= 0) {\
		TRACE_ERROR("[CHECK ID] (%d)", id);\
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;\
	}\
} while(0)

#define MAX_DOWNLOAD_HANDLE 5

#ifdef SUPPORT_LOG_MESSAGE
#include <dlog.h>
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "DOWNLOAD_PROVIDER_INTERFACE"
#define TRACE_ERROR(format, ARG...)  \
{ \
LOGE(format, ##ARG); \
}
#define TRACE_STRERROR(format, ARG...)  \
{ \
LOGE(format" [%s]", ##ARG, strerror(errno)); \
}
#define TRACE_INFO(format, ARG...)  \
{ \
LOGI(format, ##ARG); \
}
#else
#define TRACE_ERROR(format, ARG...) ;
#define TRACE_STRERROR(format, ARG...) ;
#define TRACE_INFO(format, ARG...) ;
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
static pthread_t g_interface_event_thread_id = 0;

#ifdef DP_DBUS_ACTIVATION
/* DBUS Activation */
static int __dp_call_dp_interface_service(void)
{
	DBusConnection *connection = NULL;
	DBusError dbus_error;

	dbus_error_init(&dbus_error);

	connection = dbus_bus_get(DBUS_BUS_SYSTEM, &dbus_error);
	if (connection == NULL) {
		TRACE_ERROR("[ERROR] dbus_bus_get");
		if (dbus_error_is_set(&dbus_error)) {
			TRACE_ERROR("[DBUS] dbus_bus_get: %s", dbus_error.message);
			dbus_error_free(&dbus_error);
		}
		return -1;
	}

	dbus_uint32_t result = 0;
	if (dbus_bus_start_service_by_name
			(connection,
			DP_DBUS_SERVICE_DBUS, 0, &result, &dbus_error) == FALSE) {
		if (dbus_error_is_set(&dbus_error)) {
			TRACE_ERROR("[DBUS] dbus_bus_start_service_by_name: %s",
				dbus_error.message);
			dbus_error_free(&dbus_error);
		}
		dbus_connection_unref(connection);
		return -1;
	}
	if (result == DBUS_START_REPLY_ALREADY_RUNNING) {
		TRACE_INFO("DBUS_START_REPLY_ALREADY_RUNNING [%d]", result);
	} else if (result == DBUS_START_REPLY_SUCCESS) {
		TRACE_INFO("DBUS_START_REPLY_SUCCESS [%d]", result);
	}
	dbus_connection_unref(connection);
	return 0;
}
#endif

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
	default:
		break;
	}
	return DP_NETWORK_TYPE_ALL;
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
	default:
		break;
	}
	return DOWNLOAD_ADAPTOR_NETWORK_ALL;
}

static int __dp_interface_convert_state(int state)
{
	switch (state) {
	case DP_STATE_READY:
		TRACE_INFO("READY");
		return DOWNLOAD_ADPATOR_STATE_READY;
	case DP_STATE_CONNECTING:
		TRACE_INFO("CONNECTING/QUEUED");
		return DOWNLOAD_ADPATOR_STATE_QUEUED;
	case DP_STATE_QUEUED:
		TRACE_INFO("QUEUED");
		return DOWNLOAD_ADPATOR_STATE_QUEUED;
	case DP_STATE_DOWNLOADING:
		TRACE_INFO("DOWNLOADING");
		return DOWNLOAD_ADPATOR_STATE_DOWNLOADING;
	case DP_STATE_PAUSE_REQUESTED:
		TRACE_INFO("PAUSE_REQUESTED/DOWNLOADING");
		return DOWNLOAD_ADPATOR_STATE_DOWNLOADING;
	case DP_STATE_PAUSED:
		TRACE_INFO("PAUSED");
		return DOWNLOAD_ADPATOR_STATE_PAUSED;
	case DP_STATE_COMPLETED:
		TRACE_INFO("COMPLETED");
		return DOWNLOAD_ADPATOR_STATE_COMPLETED;
	case DP_STATE_CANCELED:
		TRACE_INFO("CANCELED");
		return DOWNLOAD_ADPATOR_STATE_CANCELED;
	case DP_STATE_FAILED:
		TRACE_INFO("FAILED");
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
		TRACE_INFO("ERROR_NONE");
		return DOWNLOAD_ADAPTOR_ERROR_NONE;
	case DP_ERROR_INVALID_PARAMETER:
		TRACE_INFO("ERROR_INVALID_PARAMETER");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	case DP_ERROR_OUT_OF_MEMORY:
		TRACE_INFO("ERROR_OUT_OF_MEMORY");
		return DOWNLOAD_ADAPTOR_ERROR_OUT_OF_MEMORY;
	case DP_ERROR_IO_ERROR:
		TRACE_INFO("ERROR_IO_ERROR");
		return DOWNLOAD_ADAPTOR_ERROR_IO_ERROR;
	case DP_ERROR_NETWORK_UNREACHABLE:
		TRACE_INFO("ERROR_NETWORK_UNREACHABLE");
		return DOWNLOAD_ADAPTOR_ERROR_NETWORK_UNREACHABLE;
	case DP_ERROR_NO_SPACE:
		TRACE_INFO("ERROR_NO_SPACE");
		return DOWNLOAD_ADAPTOR_ERROR_NO_SPACE;
	case DP_ERROR_FIELD_NOT_FOUND:
		TRACE_INFO("ERROR_FIELD_NOT_FOUND");
		return DOWNLOAD_ADAPTOR_ERROR_FIELD_NOT_FOUND;
	case DP_ERROR_INVALID_STATE:
		TRACE_INFO("ERROR_INVALID_STATE");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_STATE;
	case DP_ERROR_CONNECTION_FAILED:
		TRACE_INFO("ERROR_CONNECTION_TIMED_OUT/CONNECTION_FAILED");
		return DOWNLOAD_ADAPTOR_ERROR_CONNECTION_TIMED_OUT;
	case DP_ERROR_INVALID_URL:
		TRACE_INFO("ERROR_INVALID_URL");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_URL;
	case DP_ERROR_INVALID_DESTINATION:
		TRACE_INFO("ERROR_INVALID_DESTINATION");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_DESTINATION;
	case DP_ERROR_QUEUE_FULL:
		TRACE_INFO("ERROR_QUEUE_FULL");
		return DOWNLOAD_ADAPTOR_ERROR_QUEUE_FULL;
	case DP_ERROR_ALREADY_COMPLETED:
		TRACE_INFO("ERROR_ALREADY_COMPLETED");
		return DOWNLOAD_ADAPTOR_ERROR_ALREADY_COMPLETED;
	case DP_ERROR_FILE_ALREADY_EXISTS:
		TRACE_INFO("ERROR_FILE_ALREADY_EXISTS");
		return DOWNLOAD_ADAPTOR_ERROR_FILE_ALREADY_EXISTS;
	case DP_ERROR_TOO_MANY_DOWNLOADS:
		TRACE_INFO("ERROR_TOO_MANY_DOWNLOADS");
		return DOWNLOAD_ADAPTOR_ERROR_TOO_MANY_DOWNLOADS;
	case DP_ERROR_NO_DATA:
		TRACE_INFO("ERROR_NO_DATA");
		return DOWNLOAD_ADAPTOR_ERROR_NO_DATA;
	case DP_ERROR_UNHANDLED_HTTP_CODE:
		TRACE_INFO("ERROR_UNHANDLED_HTTP_CODE");
		return DOWNLOAD_ADAPTOR_ERROR_UNHANDLED_HTTP_CODE;
	case DP_ERROR_CANNOT_RESUME:
		TRACE_INFO("ERROR_CANNOT_RESUME");
		return DOWNLOAD_ADAPTOR_ERROR_CANNOT_RESUME;
	case DP_ERROR_ID_NOT_FOUND:
		TRACE_INFO("ERROR_ID_NOT_FOUND");
		return DOWNLOAD_ADAPTOR_ERROR_ID_NOT_FOUND;
	case DP_ERROR_UNKNOWN:
		TRACE_INFO("ERROR_INVALID_STATE/UNKNOWN");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_STATE;
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
		errorcode = DP_ERROR_IO_EINTR;
	}
	return errorcode;
}

static int __ipc_read_custom_type(int fd, void *value, size_t type_size)
{
	if (fd < 0) {
		TRACE_ERROR("[CHECK SOCKET]");
		return -1;
	}

	if (read(fd, value, type_size) < 0) {
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
	if (read(fd, &value, sizeof(int)) < 0) {
		TRACE_STRERROR("[CRITICAL] read");
		return -1;
	}
	return value;
}

// keep the order/ unsigned , str
static char *__ipc_read_string(int fd)
{
	unsigned length = 0;
	char *str = NULL;

	if (fd < 0) {
		TRACE_ERROR("[CHECK FD]");
		return NULL;
	}

	if (read(fd, &length, sizeof(unsigned)) < 0) {
		TRACE_STRERROR("failed to read length [%d]", length);
		return NULL;
	}
	if (length < 1 || length > DP_MAX_STR_LEN) {
		TRACE_ERROR("[STRING LEGNTH] [%d]", length);
		return NULL;
	}
	str = (char *)calloc((length + 1), sizeof(char));
	if (str == NULL) {
		TRACE_STRERROR("[CRITICAL] allocation");
		return NULL;
	}
	if (read(fd, str, length * sizeof(char)) < 0) {
		TRACE_STRERROR("failed to read string");
		free(str);
		str = NULL;
		return NULL;
	}
	str[length] = '\0';
	return str;
}

static dp_error_type __ipc_return(int fd)
{
	dp_error_type errorcode = DP_ERROR_NONE;

	if (fd < 0) {
		TRACE_ERROR("[CHECK SOCKET]");
		return DP_ERROR_IO_ERROR;
	}

	if (read(fd, &errorcode, sizeof(dp_error_type)) < 0) {
		TRACE_STRERROR("[CRITICAL] read");
		return __get_standard_errorcode(DP_ERROR_IO_ERROR);
	}
	TRACE_INFO("return : %d", errorcode);
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
	if (read(fd, event, sizeof(dp_event_info)) < 0) {
		TRACE_STRERROR("[CRITICAL] read");
		free(event);
		return NULL;
	}

	TRACE_INFO("EVENT INFO (ID : %d state : %d error : %d)",
		event->id, event->state, event->err);
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
	TRACE_INFO("");

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

	TRACE_INFO("");

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
	TRACE_INFO("sockfd [%d]", sockfd);
	return sockfd;
}

static int __disconnect_from_provider()
{
	TRACE_INFO("");

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
	if (g_interface_event_thread_id > 0) {
		TRACE_INFO("STOP event thread");
		pthread_cancel(g_interface_event_thread_id);
		g_interface_event_thread_id = 0;
		TRACE_INFO("OK terminate event thread");
	}
	return DP_ERROR_NONE;
}

#ifdef SUPPORT_CHECK_IPC
// clear read buffer. call in head of API before calling IPC_SEND
static void __clear_read_buffer(int fd)
{
	long i;
	long unread_count;
	char tmp_char;

	// FIONREAD : Returns the number of bytes immediately readable
	if (ioctl(fd, FIONREAD, &unread_count) >= 0) {
		if (unread_count > 0) {
			TRACE_INFO("[CLEAN] garbage packet[%ld]", unread_count);
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

	// deferred wait to cencal until next function called.
	// ex) function : select, read in this thread
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

	TRACE_INFO("FD [%d]", g_interface_info->event_socket);

	maxfd = g_interface_info->event_socket;
	FD_ZERO(&read_fdset);
	FD_SET(g_interface_info->event_socket, &read_fdset);

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
			return 0;
		}

		pthread_mutex_lock(&g_function_mutex);

		if (g_interface_info == NULL
			|| g_interface_info->event_socket < 0) {
			TRACE_ERROR("[CRITICAL] IPC BROKEN Ending Event Thread");
			pthread_mutex_unlock(&g_function_mutex);
			// disconnected by main thread. just terminate
			return 0;
		}

		if (FD_ISSET(g_interface_info->event_socket, &rset) > 0) {
			// read state info from socket
			eventinfo = __ipc_event(g_interface_info->event_socket);
			if (eventinfo == NULL || eventinfo->id <= 0) {
				// failed to read from socket // ignore this status
				free(eventinfo);
				TRACE_STRERROR("[CRITICAL] Can not read Event packet");
				pthread_mutex_unlock(&g_function_mutex);
				if (__get_standard_errorcode(DP_ERROR_IO_ERROR) ==
						DP_ERROR_IO_ERROR) // if not timeout. end thread
					break;
				continue;
			}

			if ((index = __get_my_slot_index(eventinfo->id)) < 0) {
				TRACE_ERROR("[CRITICAL] not found slot for [%d]",
					eventinfo->id);
				free(eventinfo);
				pthread_mutex_unlock(&g_function_mutex);
				continue;
			}

			pthread_mutex_unlock(&g_function_mutex);

			// begin protect callback sections
			pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

			dp_interface_callback *callback =
				&g_interface_slots[index].callback;

			if (eventinfo->state == DP_STATE_DOWNLOADING
				&& eventinfo->received_size > 0) {
				if (callback->progress != NULL) {
					// progress event
					TRACE_INFO("ID %d progress callback %p",
						eventinfo->id, callback->progress );
					callback->progress(eventinfo->id,
						eventinfo->received_size,
						callback->progress_data);
				}
			} else {
				if (callback->state != NULL) {
					// state event
					TRACE_INFO("ID %d state callback %p", eventinfo->id,
						callback->state);
					callback->state(eventinfo->id,
						__dp_interface_convert_state(eventinfo->state),
						callback->state_data);
				}
			}
			free(eventinfo);

			// end protect callback sections
			pthread_setcancelstate (PTHREAD_CANCEL_ENABLE,  NULL);
			continue;
		}
		pthread_mutex_unlock(&g_function_mutex);
	} // while

	FD_ZERO(&read_fdset);

	TRACE_INFO("Terminate Event Thread");
	pthread_mutex_lock(&g_function_mutex);
	TRACE_INFO("Disconnect All Connection");
	g_interface_event_thread_id = 0; // set 0 to not call pthread_cancel
	__disconnect_from_provider();
	pthread_mutex_unlock(&g_function_mutex);
	return 0;
}

static int __connect_to_provider()
{
	TRACE_INFO("");

	if (g_interface_info == NULL) {

#ifdef DP_DBUS_ACTIVATION
		if (__dp_call_dp_interface_service() < 0) {
			TRACE_ERROR("[DBUS IO] __dp_call_dp_interface_service");
			return DP_ERROR_IO_ERROR;
		}
#endif

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
			return DP_ERROR_IO_ERROR;
		}
		// send a command
		if (__ipc_send_int(g_interface_info->cmd_socket,
			DP_CMD_SET_COMMAND_SOCKET) < 0) {
			close(g_interface_info->cmd_socket);
			free(g_interface_info);
			g_interface_info = NULL;
			return DP_ERROR_IO_ERROR;
		}
#ifndef SO_PEERCRED
		// send PID. Not support SO_PEERCRED
		if (__ipc_send_int(g_interface_info->cmd_socket, getpid()) < 0) {
			close(g_interface_info->cmd_socket);
			free(g_interface_info);
			g_interface_info = NULL;
			return DP_ERROR_IO_ERROR;
		}
#endif
		g_interface_info->event_socket = __create_socket();
		if (g_interface_info->event_socket < 0) {
			TRACE_STRERROR("[CRITICAL] connect system error");
			close(g_interface_info->cmd_socket);
			free(g_interface_info);
			g_interface_info = NULL;
			return DP_ERROR_IO_ERROR;
		}
		// send a command
		if (__ipc_send_int(g_interface_info->event_socket,
			DP_CMD_SET_EVENT_SOCKET) < 0) {
			close(g_interface_info->cmd_socket);
			close(g_interface_info->event_socket);
			free(g_interface_info);
			g_interface_info = NULL;
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
			return DP_ERROR_IO_ERROR;
		}
#endif

		int ret = pthread_mutex_init(&g_interface_info->mutex, NULL);
		if (ret != 0) {
			TRACE_STRERROR("ERR:pthread_mutex_init FAIL with %d.", ret);
			__disconnect_from_provider();
			return DP_ERROR_IO_ERROR;
		}

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
				__disconnect_from_provider();
				return DP_ERROR_IO_ERROR;
			}
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
	int fd = g_interface_info->cmd_socket;

	if (fd < 0) {
		TRACE_ERROR("[CHECK SOCKET]");
		return DOWNLOAD_ADAPTOR_ERROR_IO_ERROR;
	}
	if (value == NULL || strlen(value) <= 0) {
		TRACE_ERROR("[CHECK url]");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	DP_PRE_CHECK_ID;

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	// send commnad with ID
	errorcode = __ipc_send_command(fd, id, cmd);
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
	int fd = g_interface_info->cmd_socket;

	if (fd < 0) {
		TRACE_ERROR("[CHECK SOCKET]");
		return DOWNLOAD_ADAPTOR_ERROR_IO_ERROR;
	}
	if (strings == NULL || count == 0) {
		TRACE_ERROR("[CHECK strings]");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	DP_PRE_CHECK_ID;

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	// send commnad with ID
	errorcode = __ipc_send_command(fd, id, cmd);
	if (errorcode == DP_ERROR_NONE) {
		if (__ipc_send_int(fd, (int)count) == 0) {
			int i = 0;
			for (; i < count; i++) {
				// send string
				TRACE_INFO("[SEND] %s", strings[i]);
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
	int fd = g_interface_info->cmd_socket;
	char *recv_str = NULL;

	if (fd < 0) {
		TRACE_ERROR("[CHECK SOCKET]");
		return DOWNLOAD_ADAPTOR_ERROR_IO_ERROR;
	}

	DP_PRE_CHECK_ID;

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	errorcode = __ipc_send_command_return(id, cmd);
	if (errorcode == DP_ERROR_NONE) {
		// getting state with ID from provider.
		recv_str = __ipc_read_string(fd);
		if (recv_str != NULL) {
			*value = recv_str;
			TRACE_INFO("ID : %d recv_str : %s", id, *value);
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

static dp_error_type __dp_interface_get_strings
	(const int id, const dp_command_type cmd, const char **strings,
	const unsigned length, char ***values, unsigned *count)
{
	int errorcode = DP_ERROR_NONE;
	int fd = g_interface_info->cmd_socket;
	int i = 0;
	int recv_str_index = 0;
	char **recv_strings = NULL;

	if (fd < 0) {
		TRACE_ERROR("[CHECK SOCKET]");
		return DOWNLOAD_ADAPTOR_ERROR_IO_ERROR;
	}

	DP_PRE_CHECK_ID;

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	errorcode = __ipc_send_command(fd, id, cmd);
	if (errorcode == DP_ERROR_NONE) {
		if (length > 0 && strings != NULL) {
			if (__ipc_send_int(fd, (int)length) == 0) {
				for (i = 0; i < length; i++) {
					// send string
					TRACE_INFO("[SEND] %s", strings[i]);
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
						TRACE_INFO("[RECV] %s", recv_str);
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
	int fd = g_interface_info->cmd_socket;

	if (fd < 0) {
		TRACE_ERROR("[CHECK SOCKET]");
		return DOWNLOAD_ADAPTOR_ERROR_IO_ERROR;
	}

	DP_PRE_CHECK_ID;

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	errorcode = __ipc_send_command_return(id, cmd);
	if (errorcode == DP_ERROR_NONE) {
		recv_int = __ipc_read_int(fd);
		if (recv_int >= 0) {
			*value = recv_int;
			TRACE_INFO("ID : %d recv_int : %d", id, *value);
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
	int fd = g_interface_info->cmd_socket;

	DP_PRE_CHECK_ID;

	pthread_mutex_lock(&g_function_mutex);

	DP_CHECK_CONNECTION;

	pthread_mutex_lock(&g_interface_info->mutex);

	DP_CHECK_PROVIDER_STATUS;

	// send commnad with ID
	errorcode = __ipc_send_command(fd, id, cmd);
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


/////////////////////// APIs /////////////////////////////////

int dp_interface_create(int *id)
{
	int errorcode = DP_ERROR_NONE;
	int t_id = 0;
	int index = -1;

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
		t_id = __ipc_read_int(g_interface_info->cmd_socket);
		if (t_id >= 0) {
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
		TRACE_INFO("Request to Free the memory for ID : %d", id);
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
	TRACE_INFO("");
	return __dp_interface_set_string(id, DP_CMD_SET_URL, url);
}

int dp_interface_get_url(const int id, char **url)
{
	TRACE_INFO("");
	return __dp_interface_get_string(id, DP_CMD_GET_URL, url);
}

int dp_interface_set_network_type(const int id, int net_type)
{
	TRACE_INFO("");
	return __dp_interface_set_int(id, DP_CMD_SET_NETWORK_TYPE,
		__dp_interface_convert_network_adaptor(net_type));
}

int dp_interface_get_network_type(const int id, int *net_type)
{
	TRACE_INFO("");
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
	TRACE_INFO("");
	return __dp_interface_set_string(id, DP_CMD_SET_DESTINATION, path);
}


int dp_interface_get_destination(const int id, char **path)
{
	TRACE_INFO("");
	return __dp_interface_get_string(id, DP_CMD_GET_DESTINATION, path);
}

int dp_interface_set_file_name(const int id, const char *file_name)
{
	TRACE_INFO("");
	return __dp_interface_set_string(id, DP_CMD_SET_FILENAME, file_name);
}

int dp_interface_get_file_name(const int id, char **file_name)
{
	TRACE_INFO("");
	return __dp_interface_get_string(id, DP_CMD_GET_FILENAME, file_name);
}

int dp_interface_set_ongoing_notification(const int id, int enable)
{
	return dp_interface_set_notification(id, enable);
}

int dp_interface_set_notification(const int id, int enable)
{
	TRACE_INFO("");
	return __dp_interface_set_int(id, DP_CMD_SET_NOTIFICATION, enable);
}

int dp_interface_get_ongoing_notification(const int id, int *enable)
{
	return dp_interface_get_notification(id, enable);
}

int dp_interface_get_notification(const int id, int *enable)
{
	TRACE_INFO("");
	return __dp_interface_get_int(id, DP_CMD_GET_NOTIFICATION, enable);
}

int dp_interface_get_downloaded_file_path(const int id, char **path)
{
	TRACE_INFO("");
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
	TRACE_INFO("ID : %d key : %s value : %s", id, *key, *value);
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
	if (__ipc_send_command
		(g_interface_info->cmd_socket, id, DP_CMD_SET_HTTP_HEADER)
		!= DP_ERROR_NONE) {
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

	if (__ipc_send_command
		(g_interface_info->cmd_socket, id, DP_CMD_GET_HTTP_HEADER)
		!= DP_ERROR_NONE) {
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
			TRACE_INFO("ID : %d field:%s value: %s", id, field, *value);
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
	TRACE_INFO("");
	return __dp_interface_get_strings(id, DP_CMD_GET_HTTP_HEADER_LIST,
		NULL, 0, fields, length);
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
	if (__ipc_send_command
		(g_interface_info->cmd_socket, id, DP_CMD_DEL_HTTP_HEADER)
		!= DP_ERROR_NONE) {
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
	TRACE_INFO("");
	int statecode = DOWNLOAD_ADPATOR_STATE_NONE;
	int ret = __dp_interface_get_int(id, DP_CMD_GET_STATE, &statecode);
	if (ret == DOWNLOAD_ADAPTOR_ERROR_NONE)
		*state = __dp_interface_convert_state(statecode);
	return ret;
}

int dp_interface_get_temp_path(const int id, char **temp_path)
{
	TRACE_INFO("");
	return __dp_interface_get_string
		(id, DP_CMD_GET_TEMP_SAVED_PATH, temp_path);
}

int dp_interface_get_content_name(const int id, char **content_name)
{
	TRACE_INFO("");
	return __dp_interface_get_string
		(id, DP_CMD_GET_CONTENT_NAME, content_name);
}

int dp_interface_get_content_size(const int id,
	unsigned long long *content_size)
{
	int errorcode = DP_ERROR_NONE;

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
		} else {
			TRACE_INFO("ID : %d content_size %lld", id, *content_size);
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
	TRACE_INFO("");
	return __dp_interface_get_string
		(id, DP_CMD_GET_MIME_TYPE, mime_type);
}

int dp_interface_set_auto_download(const int id, int enable)
{
	TRACE_INFO("");
	return __dp_interface_set_int(id, DP_CMD_SET_AUTO_DOWNLOAD, enable);
}

int dp_interface_get_auto_download(const int id, int *enable)
{
	TRACE_INFO("");
	return __dp_interface_get_int(id, DP_CMD_GET_AUTO_DOWNLOAD, enable);
}

int dp_interface_get_error(const int id, int *error)
{
	TRACE_INFO("");
	int errorcode = DP_ERROR_NONE;
	int ret = __dp_interface_get_int(id, DP_CMD_GET_ERROR, &errorcode);
	if (ret == DOWNLOAD_ADAPTOR_ERROR_NONE)
		*error = __dp_interface_convert_errorcode(errorcode);
	return ret;
}

int dp_interface_get_http_status(const int id, int *http_status)
{
	TRACE_INFO("");
	return __dp_interface_get_int
		(id, DP_CMD_GET_HTTP_STATUS, http_status);
}

int dp_interface_add_noti_extra(const int id, const char *key,
	const char **values, const unsigned length)
{
	int i = 0;

	TRACE_INFO("");

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
	TRACE_INFO("");
	return __dp_interface_get_strings(id, DP_CMD_GET_EXTRA_PARAM,
		&key, 1, values, length);
}

int dp_interface_remove_noti_extra_key(const int id, const char *key)
{
	TRACE_INFO("");
	return __dp_interface_set_string
		(id, DP_CMD_REMOVE_EXTRA_PARAM, key);
}
