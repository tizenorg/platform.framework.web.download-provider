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
#include <download-provider-log.h>
#include <download-provider-pthread.h>
#include <download-provider-ipc.h>
#include <download-provider-utils.h>

#include <bundle.h> // for notification bundle
#ifdef T30
#include <bundle_internal.h>
#endif
#include <app_control.h>
#include <app_control_internal.h>

#ifdef SUPPORT_CHECK_IPC
#include <sys/ioctl.h>
#endif

#define DP_CHECK_CONNECTION do {\
	int dp_errorcode = __check_connections();\
	if (dp_errorcode != DP_ERROR_NONE) {\
		CLIENT_MUTEX_UNLOCK(&g_function_mutex);\
		return __dp_interface_convert_errorcode(dp_errorcode);\
	}\
} while(0)

#define DP_PRE_CHECK_ID do {\
	if (id <= 0) {\
		TRACE_ERROR("[CHECK ID] (%d)", id);\
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;\
	}\
} while(0)

// define type
typedef struct {
	int channel; // query & response
	int notify;  // event from provider
} dp_interface_ipc;

typedef struct {
	int id;
	dp_interface_state_changed_cb state;
	void *state_data;
	dp_interface_progress_cb progress;
	void *progress_data;
} dp_interface_slot;

// declare the variables
dp_interface_ipc *g_dp_client = NULL;
dp_interface_slot g_interface_slots[MAX_DOWNLOAD_HANDLE];
static pthread_mutex_t g_function_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_clear_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t g_dp_event_thread_id = 0;

//////////// defines functions /////////////////


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
	case DP_ERROR_NETWORK_ERROR:
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

static int __create_socket()
{
	int sockfd = -1;
	struct sockaddr_un clientaddr;

	if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		TRACE_STRERROR("[CRITICAL] socket system error");
		return -1;
	}

	bzero(&clientaddr, sizeof clientaddr);
	clientaddr.sun_family = AF_UNIX;
	memset(clientaddr.sun_path, 0x00, sizeof(clientaddr.sun_path));
	strncpy(clientaddr.sun_path, IPC_SOCKET, strlen(IPC_SOCKET));
	clientaddr.sun_path[strlen(IPC_SOCKET)] = '\0';
	if (connect(sockfd,
		(struct sockaddr*)&clientaddr, sizeof(clientaddr)) < 0) {
		close(sockfd);
		if (errno == EACCES || errno == EPERM) {
			TRACE_STRERROR("check permission");
			return -DP_ERROR_PERMISSION_DENIED;
		}
		return -1;
	}
	TRACE_DEBUG("sockfd [%d]", sockfd);
	return sockfd;
}

static void __clear_interface()
{
	TRACE_DEBUG("");
	CLIENT_MUTEX_LOCK(&g_clear_mutex);
	if (g_dp_client != NULL) {
		if (g_dp_client->channel >= 0)
			close(g_dp_client->channel);
		g_dp_client->channel= -1;
		if (g_dp_client->notify >= 0)
			close(g_dp_client->notify);
		g_dp_client->notify = -1;
		free(g_dp_client);
		g_dp_client = NULL;
	}
	CLIENT_MUTEX_UNLOCK(&g_clear_mutex);
}

static int __bp_disconnect(const char *funcname)
{
	TRACE_DEBUG("%s", funcname);
	if (g_dp_event_thread_id > 0 &&
			pthread_kill(g_dp_event_thread_id, 0) != ESRCH) {
		if (pthread_cancel(g_dp_event_thread_id) != 0) {
			TRACE_STRERROR("pthread:%d", (int)g_dp_event_thread_id);
		}
		g_dp_event_thread_id = 0;
	}
	__clear_interface();
	return DP_ERROR_NONE;
}

// listen ASYNC state event, no timeout
static void *__dp_event_manager(void *arg)
{
	if (g_dp_client == NULL) {
		TRACE_STRERROR("[CRITICAL] INTERFACE null");
		return 0;
	}

	size_t path_size = sizeof(NOTIFY_DIR) + 11;
	char notify_fifo[path_size];
	snprintf((char *)&notify_fifo, path_size,"%s/%d", NOTIFY_DIR, getpid());
	TRACE_DEBUG("IPC ESTABILISH %s", notify_fifo);
	g_dp_client->notify = open(notify_fifo, O_RDONLY, 0600);
	if (g_dp_client->notify < 0) {
		TRACE_STRERROR("[CRITICAL] failed to ESTABILISH IPC %s", notify_fifo);
		g_dp_event_thread_id = 0;
		CLIENT_MUTEX_LOCK(&g_function_mutex);
		__clear_interface();
		CLIENT_MUTEX_UNLOCK(&g_function_mutex);
		return 0;
	}

	// deferred wait to cancal until next function called.
	// ex) function : select, read in this thread
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

	while(g_dp_client != NULL && g_dp_client->notify >= 0) {

		if (g_dp_event_thread_id <= 0 ||
				pthread_self() != g_dp_event_thread_id) {
			TRACE_ERROR("competitive threads self:%0x global:%0x",
				pthread_self(), g_dp_event_thread_id);
			// another thread may work. just terminate
			break;
		}

		// blocking fifo.
		dp_ipc_event_fmt eventinfo;
		memset(&eventinfo, 0x00, sizeof(dp_ipc_event_fmt));
		if (dp_ipc_read(g_dp_client->notify, &eventinfo,
				sizeof(dp_ipc_event_fmt), __FUNCTION__) <= 0 ||
				(eventinfo.id <= 0 &&
					eventinfo.errorcode == DP_ERROR_CLIENT_DOWN)) {
			TRACE_INFO("expelled by provider");
			g_dp_event_thread_id = 0;
			CLIENT_MUTEX_LOCK(&g_function_mutex);
			__clear_interface();
			CLIENT_MUTEX_UNLOCK(&g_function_mutex);
			return 0;
		}

		int index = -1;
		if ((index = __get_my_slot_index(eventinfo.id)) < 0) {
			TRACE_ERROR("[CRITICAL] not found slot id:%d", eventinfo.id);
			continue;
		}

		// begin protect callback sections & thread safe
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

		if (eventinfo.state == DP_STATE_DOWNLOADING &&
				eventinfo.received_size > 0) {
			if (eventinfo.id == g_interface_slots[index].id &&
				g_interface_slots[index].progress != NULL) {
				// progress event
				g_interface_slots[index].progress(eventinfo.id,
					eventinfo.received_size,
					g_interface_slots[index].progress_data);
			}
		} else {
			if (eventinfo.id == g_interface_slots[index].id &&
				g_interface_slots[index].state != NULL) {
				// state event
				g_interface_slots[index].state(eventinfo.id,
					__dp_interface_convert_state(eventinfo.state),
					g_interface_slots[index].state_data);
			}
		}
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	} // while

	g_dp_event_thread_id = 0; // set 0 to not call pthread_cancel
	TRACE_DEBUG("thread end by itself");
	return 0;
}






////////////////// new download-provider ///////////////////////////////
////////////////// created on 7 November, 2013 /////////////////////////

#define DP_CHECK_IPC_SOCK (g_dp_client == NULL ? -1 : g_dp_client->channel)

static void __dp_ipc_clear_garbage(int sock, const size_t length)
{
	if (length > 0) {
		char garbage[length];
		if (read(sock, &garbage, length) == 0) {
			TRACE_ERROR("sock:%d closed peer", sock);
		}
	}
}

static int __dp_ipc_response(int sock, int download_id, short section,
	unsigned property, size_t *size)
{
	dp_ipc_fmt *ipc_info = dp_ipc_get_fmt(sock);
	if (ipc_info == NULL || ipc_info->section != section ||
			ipc_info->property != property ||
			(download_id >= 0 && ipc_info->id != download_id)) {
		TRACE_STRERROR("socket read ipcinfo");
		free(ipc_info);
		return DP_ERROR_IO_ERROR;
	}
	int errorcode = ipc_info->errorcode;
	if (size != NULL)
		*size = ipc_info->size;
	free(ipc_info);
	return errorcode;
}

static int __connect_to_provider()
{
	int errorcode = DP_ERROR_NONE;

	CLIENT_MUTEX_LOCK(&g_clear_mutex);

	if (g_dp_client == NULL) {
		g_dp_client =
			(dp_interface_ipc *)calloc(1, sizeof(dp_interface_ipc));

		if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
			TRACE_ERROR("failed to ignore SIGPIPE signal");
		}
	}

	if (g_dp_client != NULL) {

		int connect_retry = 3;
		g_dp_client->channel = -1;
		while(g_dp_client->channel < 0 && connect_retry-- > 0) {
			int ret = __create_socket();
			if (ret == -1) {
				TRACE_STRERROR("failed to connect to provider(remains:%d)", connect_retry);
				struct timespec ts;
				ts.tv_sec = 0;
				ts.tv_nsec = 20000000;
				nanosleep(&ts, NULL);
			} else if (ret >= 0) {
				struct timeval tv_timeo = { 1, 500000 }; // 1.5 second
				g_dp_client->channel = ret;
				if (setsockopt(g_dp_client->channel, SOL_SOCKET, SO_RCVTIMEO, &tv_timeo, sizeof(tv_timeo)) < 0) {
					TRACE_STRERROR("[CRITICAL] setsockopt SO_RCVTIMEO");
				}
			} else {
				errorcode = -ret;
				TRACE_STRERROR("check error:%d", errorcode);
				goto EXIT_CONNECT;
			}
		}
		if (g_dp_client->channel < 0) {
			TRACE_STRERROR("[CRITICAL] connect system error");
			errorcode = DP_ERROR_IO_ERROR;
			goto EXIT_CONNECT;
		}

		if (dp_ipc_query(g_dp_client->channel, -1, DP_SEC_INIT,
				DP_PROP_NONE, DP_ERROR_NONE, 0) < 0) {
			errorcode = DP_ERROR_IO_ERROR;
			goto EXIT_CONNECT;
		}
#ifndef SO_PEERCRED
		dp_credential cred;
		cred.pid = getpid();
		cred.uid = getuid();
		cred.gid = getgid();
		// send PID. Not support SO_PEERCRED
		if (dp_ipc_write(g_dp_client->channel,
				&cred, sizeof(dp_credential)) < 0) {
			errorcode = DP_ERROR_IO_ERROR;
			goto EXIT_CONNECT;
		}
#endif

		errorcode = __dp_ipc_response(g_dp_client->channel,
				-1, DP_SEC_INIT, DP_PROP_NONE, NULL);
		// Commented by justine.bang
		// Above ipc_query call try to wake up download-provider.
		// But, we can sometimes meet EINTR, EAGAIN or EINPROGRESS signal if systemd is slow.
		// So, If meet EINTR ,EAGAIN or EINPROGRESS in this line, it's better to wait response from download-provider one more time.
		if (errorcode == DP_ERROR_IO_ERROR && (errno == EINTR || errno == EAGAIN || errno == EINPROGRESS)) {
			errorcode = __dp_ipc_response(g_dp_client->channel,
					-1, DP_SEC_INIT, DP_PROP_NONE, NULL);
		}

		if (errorcode == DP_ERROR_NONE && g_dp_event_thread_id <= 0) {
			if (pthread_create(&g_dp_event_thread_id, NULL,
					__dp_event_manager, g_dp_client) != 0) {
				TRACE_STRERROR("failed to create event-manager");
				errorcode = DP_ERROR_IO_ERROR;
			} else {
				pthread_detach(g_dp_event_thread_id);
			}
		}

	}

EXIT_CONNECT:
	CLIENT_MUTEX_UNLOCK(&g_clear_mutex);
	if (errorcode != DP_ERROR_NONE)
		__bp_disconnect(__FUNCTION__);

	return errorcode;
}

static dp_error_type __check_connections()
{
	int ret = 0;

	if (g_dp_client == NULL)
		if ((ret = __connect_to_provider()) != DP_ERROR_NONE)
			return ret;

	if (g_dp_client == NULL || g_dp_client->channel < 0) {
		TRACE_ERROR("[CHECK IPC]");
		return DP_ERROR_IO_ERROR;
	}
	return DP_ERROR_NONE;
}

static int __dp_ipc_set_binary(const int id, const unsigned property,
	const bundle_raw *string, const size_t length, const char *funcname)
{
	int errorcode = DP_ERROR_NONE;
	if (string == NULL || length <= 0) {
		TRACE_ERROR("%s check binary (%d)", funcname, length);
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	DP_PRE_CHECK_ID;
	CLIENT_MUTEX_LOCK(&g_function_mutex);
	DP_CHECK_CONNECTION;

	int sock = DP_CHECK_IPC_SOCK;
	if (dp_ipc_query(sock, id, DP_SEC_SET, property, DP_ERROR_NONE, length * sizeof(unsigned char)) < 0) {
		errorcode = DP_ERROR_IO_ERROR;
		TRACE_ERROR("%s check ipc sock:%d", funcname, sock);
	} else {
		if (dp_ipc_write(sock, (void*)string, length * sizeof(unsigned char)) < 0) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("%s check ipc sock:%d", funcname, sock);
		} else {
			errorcode = __dp_ipc_response(sock, id, DP_SEC_SET, property, NULL);
		}
	}
	if (errorcode == DP_ERROR_IO_ERROR)
		__bp_disconnect(funcname);
	CLIENT_MUTEX_UNLOCK(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

static int __dp_ipc_set_string(const int id, const short section, const unsigned property,
	const char *string, const char *funcname)
{
	int errorcode = DP_ERROR_NONE;
	size_t length = 0;
	if (string == NULL || (length = strlen(string)) <= 0 ||
			length > DP_MAX_STR_LEN) {
		TRACE_ERROR("%s check string (%d:%s)", funcname, length, string);
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	DP_PRE_CHECK_ID;
	CLIENT_MUTEX_LOCK(&g_function_mutex);
	DP_CHECK_CONNECTION;

	int sock = DP_CHECK_IPC_SOCK;
	if (dp_ipc_query(sock, id, section, property, DP_ERROR_NONE, length * sizeof(char)) < 0) {
		errorcode = DP_ERROR_IO_ERROR;
		TRACE_ERROR("%s check ipc sock:%d", funcname, sock);
	} else {
		if (dp_ipc_write(sock, (void*)string, length * sizeof(char)) < 0) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("%s check ipc sock:%d", funcname, sock);
		} else {
			errorcode = __dp_ipc_response(sock, id, section, property, NULL);
		}
	}
	if (errorcode == DP_ERROR_IO_ERROR)
		__bp_disconnect(funcname);
	CLIENT_MUTEX_UNLOCK(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

static int __dp_ipc_get_string(const int id, const unsigned property,
	char **string, const char *funcname)
{
	int errorcode = DP_ERROR_NONE;

	if (string == NULL) {
		TRACE_ERROR("%s check buffer", funcname);
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}
	DP_PRE_CHECK_ID;
	CLIENT_MUTEX_LOCK(&g_function_mutex);
	DP_CHECK_CONNECTION;

	int sock = DP_CHECK_IPC_SOCK;
	if (dp_ipc_query(sock, id, DP_SEC_GET, property, DP_ERROR_NONE, 0) < 0) {
		errorcode = DP_ERROR_IO_ERROR;
		TRACE_ERROR("%s check ipc sock:%d", funcname, sock);
	} else {
		size_t string_length = 0;
		*string = NULL;
		errorcode = __dp_ipc_response(sock, id, DP_SEC_GET, property, &string_length);
		if (errorcode == DP_ERROR_NONE) {
			if (string_length > 0) {
				char *recv_str = (char *)calloc((string_length + (size_t)1), sizeof(char));
				if (recv_str == NULL) {
					TRACE_STRERROR("check memory length:%d", string_length);
					errorcode = DP_ERROR_OUT_OF_MEMORY;
					__dp_ipc_clear_garbage(sock, string_length);
				} else {
					if (dp_ipc_read(sock, recv_str, string_length, funcname) <= 0) {
						errorcode = DP_ERROR_IO_ERROR;
						free(recv_str);
					} else {
						recv_str[string_length] = '\0';
						*string = recv_str;
					}
				}
			} else {
				errorcode = DP_ERROR_IO_ERROR;
			}
		}
	}

	if (errorcode == DP_ERROR_IO_ERROR)
		__bp_disconnect(funcname);
	CLIENT_MUTEX_UNLOCK(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

// send command and int & return errorcode
static int __dp_ipc_set_int(const int id, const unsigned section, const unsigned property,
	const int value, const char *funcname)
{
	int errorcode = DP_ERROR_NONE;

	DP_PRE_CHECK_ID;
	CLIENT_MUTEX_LOCK(&g_function_mutex);
	DP_CHECK_CONNECTION;

	int sock = DP_CHECK_IPC_SOCK;
	if (dp_ipc_query(sock, id, section, property, DP_ERROR_NONE, sizeof(int)) < 0) {
		errorcode = DP_ERROR_IO_ERROR;
		TRACE_ERROR("%s check ipc sock:%d", funcname, sock);
	} else {
		if (dp_ipc_write(sock, (void *)&value, sizeof(int)) < 0) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("%s check ipc sock:%d", funcname, sock);
		} else {
			errorcode = __dp_ipc_response(sock, id, section, property, NULL);
		}
	}
	if (errorcode == DP_ERROR_IO_ERROR)
		__bp_disconnect(funcname);
	CLIENT_MUTEX_UNLOCK(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

// send command & return errorcode and int
static int __dp_ipc_get_int(const int id, const unsigned property,
	int *value, const char *funcname)
{
	int errorcode = DP_ERROR_NONE;

	if (value == NULL) {
		TRACE_ERROR("%s check buffer", funcname);
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	DP_PRE_CHECK_ID;
	CLIENT_MUTEX_LOCK(&g_function_mutex);
	DP_CHECK_CONNECTION;

	int sock = DP_CHECK_IPC_SOCK;
	if (dp_ipc_query(sock, id, DP_SEC_GET, property, DP_ERROR_NONE, 0) < 0) {
		errorcode = DP_ERROR_IO_ERROR;
		TRACE_ERROR("%s check ipc sock:%d", funcname, sock);
	} else {
		size_t extra_size = 0;
		errorcode = __dp_ipc_response(sock, id, DP_SEC_GET, property, &extra_size);
		if (errorcode == DP_ERROR_NONE) {
			if (extra_size == sizeof(int)) {
				if (dp_ipc_read(sock, value, extra_size, funcname) < 0)
					errorcode = DP_ERROR_IO_ERROR;
			} else {
				errorcode = DP_ERROR_IO_ERROR;
			}
		}
	}

	if (errorcode == DP_ERROR_IO_ERROR)
		__bp_disconnect(funcname);
	CLIENT_MUTEX_UNLOCK(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

// send command & return errorcode.
int __dp_ipc_echo(const int id, const short section,
	const unsigned property, const char *funcname)
{
	int errorcode = DP_ERROR_NONE;

	DP_PRE_CHECK_ID;
	CLIENT_MUTEX_LOCK(&g_function_mutex);
	DP_CHECK_CONNECTION;

	int sock = DP_CHECK_IPC_SOCK;
	if (dp_ipc_query(sock, id, section, property, DP_ERROR_NONE, 0) < 0) {
		errorcode = DP_ERROR_IO_ERROR;
		TRACE_ERROR("%s check ipc sock:%d", funcname, sock);
	} else {
		errorcode = __dp_ipc_response(sock, id, section, property, NULL);
	}

	if (errorcode == DP_ERROR_IO_ERROR)
		__bp_disconnect(funcname);
	CLIENT_MUTEX_UNLOCK(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

/////////////////////// APIs /////////////////////////////////

int dp_interface_create(int *id)
{
	int errorcode = DP_ERROR_NONE;
	int index = -1;

	if (id == NULL) {
		TRACE_ERROR("[CHECK id variable]");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	CLIENT_MUTEX_LOCK(&g_function_mutex);

	if ((index = __get_empty_slot_index()) < 0) {
		TRACE_ERROR
			("[ERROR] TOO_MANY_DOWNLOADS[%d]", MAX_DOWNLOAD_HANDLE);
		CLIENT_MUTEX_UNLOCK(&g_function_mutex);
		return DOWNLOAD_ADAPTOR_ERROR_TOO_MANY_DOWNLOADS;
	}

	DP_CHECK_CONNECTION;

	dp_ipc_fmt *ipc_info = NULL;
	int sock = DP_CHECK_IPC_SOCK;
	if (dp_ipc_query(sock, -1, DP_SEC_CONTROL, DP_PROP_CREATE, DP_ERROR_NONE, 0) < 0) {
		errorcode = DP_ERROR_IO_ERROR;
	} else {
		ipc_info = dp_ipc_get_fmt(sock);
		if (ipc_info == NULL) {
			TRACE_ERROR("[ERROR] IPC INFO is NULL");
			errorcode = DP_ERROR_IO_ERROR;
		} else if (ipc_info->section != DP_SEC_CONTROL ||
				ipc_info->property != DP_PROP_CREATE ||
				ipc_info->size != 0) {
			TRACE_ERROR("sock:%d id:%d section:%d property:%d size:%d",
				sock, ipc_info->id, ipc_info->section, ipc_info->property,
				ipc_info->size);
			errorcode = DP_ERROR_IO_ERROR;
		} else {
			TRACE_DEBUG("download_id:%d", ipc_info->id);
			if (errorcode == DP_ERROR_NONE && ipc_info->id > 0) {
				*id = ipc_info->id;
				g_interface_slots[index].id = ipc_info->id;
				g_interface_slots[index].state = NULL;
				g_interface_slots[index].state_data = NULL;
				g_interface_slots[index].progress = NULL;
				g_interface_slots[index].progress_data = NULL;
			}
		}
	}
	free(ipc_info);
	if (errorcode == DP_ERROR_IO_ERROR)
		__bp_disconnect(__FUNCTION__);
	CLIENT_MUTEX_UNLOCK(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

int dp_interface_destroy(const int id)
{
	int index = -1;
	DP_PRE_CHECK_ID;
	CLIENT_MUTEX_LOCK(&g_function_mutex);
	DP_CHECK_CONNECTION;

	if ((index = __get_my_slot_index(id)) >= 0) {
		g_interface_slots[index].id = 0;
		g_interface_slots[index].state = NULL;
		g_interface_slots[index].state_data = NULL;
		g_interface_slots[index].progress = NULL;
		g_interface_slots[index].progress_data = NULL;
	}
	CLIENT_MUTEX_UNLOCK(&g_function_mutex);
	return __dp_ipc_echo(id, DP_SEC_CONTROL, DP_PROP_DESTROY, __FUNCTION__);
}

int dp_interface_start(const int id)
{
	return __dp_ipc_echo(id, DP_SEC_CONTROL, DP_PROP_START, __FUNCTION__);
}

int dp_interface_pause(const int id)
{
	return __dp_ipc_echo(id, DP_SEC_CONTROL, DP_PROP_PAUSE, __FUNCTION__);
}

int dp_interface_cancel(const int id)
{
	return __dp_ipc_echo(id, DP_SEC_CONTROL, DP_PROP_CANCEL, __FUNCTION__);
}

int dp_interface_set_url(const int id, const char *url)
{
	if (url == NULL)
		return __dp_ipc_echo(id, DP_SEC_UNSET, DP_PROP_URL, __FUNCTION__);
	return __dp_ipc_set_string(id, DP_SEC_SET, DP_PROP_URL, url, __FUNCTION__);
}

int dp_interface_get_url(const int id, char **url)
{
	return __dp_ipc_get_string(id, DP_PROP_URL, url, __FUNCTION__);
}

int dp_interface_set_destination(const int id, const char *path)
{
	if (path == NULL)
		return __dp_ipc_echo(id, DP_SEC_UNSET, DP_PROP_DESTINATION, __FUNCTION__);
	return __dp_ipc_set_string(id, DP_SEC_SET, DP_PROP_DESTINATION, path, __FUNCTION__);
}

int dp_interface_get_destination(const int id, char **path)
{
	return __dp_ipc_get_string(id, DP_PROP_DESTINATION, path, __FUNCTION__);
}

int dp_interface_set_file_name(const int id, const char *file_name)
{
	if (file_name == NULL)
		return __dp_ipc_echo(id, DP_SEC_UNSET, DP_PROP_FILENAME, __FUNCTION__);
	return __dp_ipc_set_string(id, DP_SEC_SET, DP_PROP_FILENAME, file_name, __FUNCTION__);
}

int dp_interface_get_file_name(const int id, char **file_name)
{
	return __dp_ipc_get_string(id, DP_PROP_FILENAME, file_name, __FUNCTION__);
}

int dp_interface_get_downloaded_file_path(const int id, char **path)
{
	return __dp_ipc_get_string(id, DP_PROP_SAVED_PATH, path, __FUNCTION__);
}

int dp_interface_get_temp_path(const int id, char **temp_path)
{
	return __dp_ipc_get_string(id, DP_PROP_TEMP_SAVED_PATH, temp_path, __FUNCTION__);
}

int dp_interface_get_content_name(const int id, char **content_name)
{
	return __dp_ipc_get_string(id, DP_PROP_CONTENT_NAME, content_name, __FUNCTION__);
}

int dp_interface_get_etag(const int id, char **etag)
{
	return __dp_ipc_get_string(id, DP_PROP_ETAG, etag, __FUNCTION__);
}

int dp_interface_set_temp_file_path(const int id, const char *path)
{
	return __dp_ipc_set_string(id, DP_SEC_SET, DP_PROP_TEMP_SAVED_PATH, path, __FUNCTION__);
}

int dp_interface_set_network_type(const int id, int net_type)
{
	return __dp_ipc_set_int(id, DP_SEC_SET, DP_PROP_NETWORK_TYPE,
		net_type, __FUNCTION__);
}

int dp_interface_get_network_type(const int id, int *net_type)
{
	if (net_type == NULL) {
		TRACE_ERROR("check buffer");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}
	return __dp_ipc_get_int(id, DP_PROP_NETWORK_TYPE, net_type, __FUNCTION__);
}

int dp_interface_get_network_bonding(const int id, int *enable)
{
	if (enable == NULL) {
		TRACE_ERROR("check buffer");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}
	return __dp_ipc_get_int(id, DP_PROP_NETWORK_BONDING, enable, __FUNCTION__);
}

int dp_interface_set_network_bonding(const int id, int enable)
{
	return __dp_ipc_set_int(id, DP_SEC_SET, DP_PROP_NETWORK_BONDING,
		enable, __FUNCTION__);
}

int dp_interface_set_state_changed_cb(const int id,
	dp_interface_state_changed_cb callback, void *user_data)
{
	int errorcode = __dp_ipc_echo(id, DP_SEC_SET, DP_PROP_STATE_CALLBACK, __FUNCTION__);
	if (errorcode == DOWNLOAD_ADAPTOR_ERROR_NONE) {
		CLIENT_MUTEX_LOCK(&g_function_mutex);
		int index = __get_my_slot_index(id);
		if (index < 0) {
			index = __get_empty_slot_index();
			if (index >= 0) {
				g_interface_slots[index].id = id;
			} else {
				TRACE_ERROR("too many download limit:%d",
					MAX_DOWNLOAD_HANDLE);
				errorcode = DOWNLOAD_ADAPTOR_ERROR_TOO_MANY_DOWNLOADS;
			}
		}
		if (index >= 0) {
			g_interface_slots[index].state = callback;
			g_interface_slots[index].state_data = user_data;
		}
		CLIENT_MUTEX_UNLOCK(&g_function_mutex);
	}
	return errorcode;
}

int dp_interface_unset_state_changed_cb(const int id)
{
	CLIENT_MUTEX_LOCK(&g_function_mutex);
	int index = __get_my_slot_index(id);
	if (index >= 0) {
		g_interface_slots[index].state = NULL;
		g_interface_slots[index].state_data = NULL;
	}
	CLIENT_MUTEX_UNLOCK(&g_function_mutex);
	if (index < 0)
		return DOWNLOAD_ADAPTOR_ERROR_ID_NOT_FOUND;
	return __dp_ipc_echo(id, DP_SEC_UNSET, DP_PROP_STATE_CALLBACK, __FUNCTION__);
}

int dp_interface_set_progress_cb(const int id,
	dp_interface_progress_cb callback, void *user_data)
{
	int errorcode = __dp_ipc_echo(id, DP_SEC_SET, DP_PROP_PROGRESS_CALLBACK, __FUNCTION__);
	if (errorcode == DOWNLOAD_ADAPTOR_ERROR_NONE) {
		CLIENT_MUTEX_LOCK(&g_function_mutex);
		int index = __get_my_slot_index(id);
		if (index < 0) {
			index = __get_empty_slot_index();
			if (index >= 0) {
				g_interface_slots[index].id = id;
			} else {
				TRACE_ERROR("too many download limit:%d",
					MAX_DOWNLOAD_HANDLE);
				errorcode = DOWNLOAD_ADAPTOR_ERROR_TOO_MANY_DOWNLOADS;
			}
		}
		if (index >= 0) {
			g_interface_slots[index].progress = callback;
			g_interface_slots[index].progress_data = user_data;
		}
		CLIENT_MUTEX_UNLOCK(&g_function_mutex);
	}
	return errorcode;
}

int dp_interface_unset_progress_cb(const int id)
{
	CLIENT_MUTEX_LOCK(&g_function_mutex);
	int index = __get_my_slot_index(id);
	if (index >= 0) {
		g_interface_slots[index].progress = NULL;
		g_interface_slots[index].progress_data = NULL;
	}
	CLIENT_MUTEX_UNLOCK(&g_function_mutex);
	if (index < 0)
		return DOWNLOAD_ADAPTOR_ERROR_ID_NOT_FOUND;
	return __dp_ipc_echo(id, DP_SEC_UNSET, DP_PROP_PROGRESS_CALLBACK, __FUNCTION__);
}

int dp_interface_get_state(const int id, int *state)
{
	if (state == NULL) {
		TRACE_ERROR("check buffer");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}
	int dp_state = DP_STATE_NONE;
	int errorcode =  __dp_ipc_get_int(id, DP_PROP_STATE, &dp_state, __FUNCTION__);
	if (errorcode == DOWNLOAD_ADAPTOR_ERROR_NONE)
		*state = __dp_interface_convert_state(dp_state);
	return errorcode;
}

int dp_interface_get_content_size(const int id,
	unsigned long long *content_size)
{
	int errorcode = DP_ERROR_NONE;

	if (content_size == NULL) {
		TRACE_ERROR("check buffer");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	DP_PRE_CHECK_ID;
	CLIENT_MUTEX_LOCK(&g_function_mutex);
	DP_CHECK_CONNECTION;

	int sock = DP_CHECK_IPC_SOCK;
	if (dp_ipc_query(sock, id, DP_SEC_GET, DP_PROP_TOTAL_FILE_SIZE, DP_ERROR_NONE, 0) < 0) {
		errorcode = DP_ERROR_IO_ERROR;
		TRACE_ERROR("%s check ipc sock:%d", __FUNCTION__, sock);
	} else {
		size_t extra_size = 0;
		errorcode = __dp_ipc_response(sock, id, DP_SEC_GET, DP_PROP_TOTAL_FILE_SIZE, &extra_size);
		if (errorcode == DP_ERROR_NONE) {
			if (extra_size == sizeof(unsigned long long)) {
				if (dp_ipc_read(sock, content_size, extra_size, __FUNCTION__) < 0)
					errorcode = DP_ERROR_IO_ERROR;
			} else {
				errorcode = DP_ERROR_IO_ERROR;
			}
		}
	}

	if (errorcode == DP_ERROR_IO_ERROR)
		__bp_disconnect(__FUNCTION__);
	CLIENT_MUTEX_UNLOCK(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

int dp_interface_get_mime_type(const int id, char **mime_type)
{
	if (mime_type == NULL) {
		TRACE_ERROR("check buffer");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}
	return __dp_ipc_get_string(id, DP_PROP_MIME_TYPE, mime_type,
		__FUNCTION__);
}

int dp_interface_set_auto_download(const int id, int enable)
{
	short section = DP_SEC_SET;
	if (enable <= 0)
		section = DP_SEC_UNSET;
	return __dp_ipc_echo(id, section, DP_PROP_AUTO_DOWNLOAD, __FUNCTION__);
}

int dp_interface_get_auto_download(const int id, int *enable)
{
	if (enable == NULL) {
		TRACE_ERROR("check buffer");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}
	return __dp_ipc_get_int(id, DP_PROP_AUTO_DOWNLOAD, enable, __FUNCTION__);
}

int dp_interface_get_error(const int id, int *error)
{
	if (error == NULL) {
		TRACE_ERROR("check buffer");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}
	int errorcode = DP_ERROR_NONE;
	int ret = __dp_ipc_get_int(id, DP_PROP_ERROR, &errorcode, __FUNCTION__);
	if (ret == DOWNLOAD_ADAPTOR_ERROR_NONE)
		*error = __dp_interface_convert_errorcode(errorcode);
	return ret;
}

int dp_interface_get_http_status(const int id, int *http_status)
{
	if (http_status == NULL) {
		TRACE_ERROR("check buffer");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}
	return __dp_ipc_get_int(id, DP_PROP_HTTP_STATUS, http_status,
		__FUNCTION__);
}

int dp_interface_set_notification_title(const int id, const char *title)
{
	if (title == NULL)
		return __dp_ipc_echo(id, DP_SEC_UNSET, DP_PROP_NOTIFICATION_SUBJECT, __FUNCTION__);
	return __dp_ipc_set_string(id, DP_SEC_SET, DP_PROP_NOTIFICATION_SUBJECT, title, __FUNCTION__);
}

int dp_interface_get_notification_title(const int id, char **title)
{
	if (title == NULL) {
		TRACE_ERROR("check buffer");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}
	return __dp_ipc_get_string(id, DP_PROP_NOTIFICATION_SUBJECT, title, __FUNCTION__);
}

int dp_interface_set_notification_description(const int id, const char *description)
{
	if (description == NULL)
		return __dp_ipc_echo(id, DP_SEC_UNSET, DP_PROP_NOTIFICATION_DESCRIPTION, __FUNCTION__);
	return __dp_ipc_set_string(id, DP_SEC_SET, DP_PROP_NOTIFICATION_DESCRIPTION, description, __FUNCTION__);
}

int dp_interface_get_notification_description(const int id, char **description)
{
	if (description == NULL) {
		TRACE_ERROR("check buffer");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}
	return __dp_ipc_get_string(id, DP_PROP_NOTIFICATION_DESCRIPTION, description, __FUNCTION__);
}

int dp_interface_set_notification_type(const int id, int type)
{
	if (type == DP_NOTIFICATION_TYPE_NONE)
		return __dp_ipc_echo(id, DP_SEC_UNSET, DP_PROP_NOTIFICATION_TYPE, __FUNCTION__);
	return __dp_ipc_set_int(id, DP_SEC_SET, DP_PROP_NOTIFICATION_TYPE, type, __FUNCTION__);
}

int dp_interface_get_notification_type(const int id, int *type)
{
	if (type == NULL) {
		TRACE_ERROR("check buffer");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}
	return __dp_ipc_get_int(id, DP_PROP_NOTIFICATION_TYPE, type, __FUNCTION__);
}

int dp_interface_set_notification_bundle(const int id, const int type, void *bundle_param)
{
	int errorcode = DOWNLOAD_ADAPTOR_ERROR_NONE;
	if (type != DP_NOTIFICATION_BUNDLE_TYPE_ONGOING &&
			type !=  DP_NOTIFICATION_BUNDLE_TYPE_COMPLETE &&
			type !=  DP_NOTIFICATION_BUNDLE_TYPE_FAILED) {
		TRACE_ERROR("check type:%d id:%d", type, id);
		errorcode = DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
		return errorcode;
	}
	if (bundle_param == NULL) { // unset
		return __dp_ipc_set_int(id, DP_SEC_UNSET, DP_PROP_NOTIFICATION_RAW, type, __FUNCTION__);
	} else { // set
		int length = 0;
		bundle_raw *raw_buffer = NULL;
		int result = bundle_encode_raw(bundle_param, &raw_buffer, &length);
		if (result == 0 && length > 0) {
			errorcode = __dp_ipc_set_int(id, DP_SEC_SET, DP_PROP_NOTIFICATION_RAW, type, __FUNCTION__);
			if (errorcode == DOWNLOAD_ADAPTOR_ERROR_NONE) {
				errorcode = __dp_ipc_set_binary(id, DP_PROP_NOTIFICATION_RAW, raw_buffer, (size_t)length, __FUNCTION__);
			}
		} else {
			TRACE_ERROR("failed to encode raws error:%d type:%d id:%d", result, type, id);
			errorcode = DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
		}
		if (raw_buffer != NULL)
			bundle_free_encoded_rawdata(&raw_buffer);
	}
	return errorcode;
}

int dp_interface_get_notification_bundle(const int id, const int type, void **bundle_param)
{
	if (bundle_param == NULL) {
		TRACE_ERROR("check buffer");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}
	int errorcode = DOWNLOAD_ADAPTOR_ERROR_NONE;
	if (type != DP_NOTIFICATION_BUNDLE_TYPE_ONGOING &&
			type !=  DP_NOTIFICATION_BUNDLE_TYPE_COMPLETE &&
			type !=  DP_NOTIFICATION_BUNDLE_TYPE_FAILED) {
		TRACE_ERROR("check type:%d id:%d", type, id);
		errorcode = DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
		return errorcode;
	}

	// send type, get errorcode with extra_size, get bundle binary

	errorcode = DP_ERROR_NONE;

	DP_PRE_CHECK_ID;
	CLIENT_MUTEX_LOCK(&g_function_mutex);
	DP_CHECK_CONNECTION;

	const unsigned section = DP_SEC_GET;
	const unsigned property = DP_PROP_NOTIFICATION_RAW;
	size_t extra_size = 0;

	int sock = DP_CHECK_IPC_SOCK;
	if (dp_ipc_query(sock, id, section, property, DP_ERROR_NONE, sizeof(int)) < 0) {
		errorcode = DP_ERROR_IO_ERROR;
		TRACE_ERROR("check ipc sock:%d", sock);
	} else {
		if (dp_ipc_write(sock, (void *)&type, sizeof(int)) < 0) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", sock);
		} else {
			errorcode = __dp_ipc_response(sock, id, section, property, &extra_size);
		}
	}
	if (errorcode == DP_ERROR_NONE && extra_size > 0) {
		unsigned char *recv_raws = (unsigned char *)calloc(extra_size, sizeof(unsigned char));
		if (recv_raws == NULL) {
			TRACE_STRERROR("sock:%d check memory length:%d", sock, extra_size);
			errorcode = DP_ERROR_OUT_OF_MEMORY;
			__dp_ipc_clear_garbage(sock, extra_size);
		} else {
			if (dp_ipc_read(sock, recv_raws, extra_size, __FUNCTION__) <= 0) {
				TRACE_ERROR("sock:%d check ipc length:%d", sock, extra_size);
				errorcode = DP_ERROR_IO_ERROR;
				free(recv_raws);
			} else {
				TRACE_DEBUG("sock:%d length:%d raws", sock, extra_size);
				*bundle_param = bundle_decode_raw(recv_raws, extra_size);
				free(recv_raws);
			}
		}
	}
	if (errorcode == DP_ERROR_IO_ERROR)
		__bp_disconnect(__FUNCTION__);
	CLIENT_MUTEX_UNLOCK(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

int dp_interface_set_notification_service_handle(const int id, const int type, void *handle)
{
	int errorcode = DOWNLOAD_ADAPTOR_ERROR_NONE;
	if (type != DP_NOTIFICATION_SERVICE_TYPE_ONGOING &&
			type !=  DP_NOTIFICATION_SERVICE_TYPE_COMPLETE &&
			type !=  DP_NOTIFICATION_SERVICE_TYPE_FAILED) {
		TRACE_ERROR("check type:%d id:%d", type, id);
		errorcode = DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
		return errorcode;
	}
	if (handle == NULL) { // unset
		return __dp_ipc_set_int(id, DP_SEC_UNSET, DP_PROP_NOTIFICATION_RAW, type, __FUNCTION__);
	} else { // set
		int length = 0;
		bundle_raw *raw_buffer = NULL;
		bundle *bundle_data = NULL;
		int result = app_control_export_as_bundle((app_control_h) handle, &bundle_data);
		if (result == APP_CONTROL_ERROR_NONE) {
			result = bundle_encode_raw(bundle_data, &raw_buffer, &length);
			if (result == 0 && length > 0) {
				errorcode = __dp_ipc_set_int(id, DP_SEC_SET, DP_PROP_NOTIFICATION_RAW, type, __FUNCTION__);
				if (errorcode == DOWNLOAD_ADAPTOR_ERROR_NONE) {
					errorcode = __dp_ipc_set_binary(id, DP_PROP_NOTIFICATION_RAW, raw_buffer, (size_t)length, __FUNCTION__);
				}
			} else {
				TRACE_ERROR("failed to encode raws error:%d type:%d id:%d", result, type, id);
				errorcode = DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
			}
		} else {
			TRACE_ERROR("failed to encode service handle error:%d type:%d id:%d", result, type, id);
			errorcode = DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
		}
		if (raw_buffer != NULL)
			bundle_free_encoded_rawdata(&raw_buffer);
		if (bundle_data != NULL)
			bundle_free(bundle_data);
	}
	return errorcode;
}

int dp_interface_get_notification_service_handle(const int id, const int type, void **handle)
{
	bundle *bundle_data = NULL;
	if (handle == NULL) {
		TRACE_ERROR("check buffer");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}
	int errorcode = DOWNLOAD_ADAPTOR_ERROR_NONE;
	if (type != DP_NOTIFICATION_SERVICE_TYPE_ONGOING &&
			type !=  DP_NOTIFICATION_SERVICE_TYPE_COMPLETE &&
			type !=  DP_NOTIFICATION_SERVICE_TYPE_FAILED) {
		TRACE_ERROR("check type:%d id:%d", type, id);
		errorcode = DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
		return errorcode;
	}

	// send type, get errorcode with extra_size, get bundle binary

	errorcode = DP_ERROR_NONE;

	DP_PRE_CHECK_ID;
	CLIENT_MUTEX_LOCK(&g_function_mutex);
	DP_CHECK_CONNECTION;

	const unsigned section = DP_SEC_GET;
	const unsigned property = DP_PROP_NOTIFICATION_RAW;
	size_t extra_size = 0;

	int sock = DP_CHECK_IPC_SOCK;
	if (dp_ipc_query(sock, id, section, property, DP_ERROR_NONE, sizeof(int)) < 0) {
		errorcode = DP_ERROR_IO_ERROR;
		TRACE_ERROR("check ipc sock:%d", sock);
	} else {
		if (dp_ipc_write(sock, (void *)&type, sizeof(int)) < 0) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", sock);
		} else {
			errorcode = __dp_ipc_response(sock, id, section, property, &extra_size);
		}
	}
	if (errorcode == DP_ERROR_NONE && extra_size > 0) {
		unsigned char *recv_raws = (unsigned char *)calloc(extra_size, sizeof(unsigned char));
		if (recv_raws == NULL) {
			TRACE_STRERROR("sock:%d check memory length:%d", sock, extra_size);
			errorcode = DP_ERROR_OUT_OF_MEMORY;
			__dp_ipc_clear_garbage(sock, extra_size);
		} else {
			if (dp_ipc_read(sock, recv_raws, extra_size, __FUNCTION__) <= 0) {
				TRACE_ERROR("sock:%d check ipc length:%d", sock, extra_size);
				errorcode = DP_ERROR_IO_ERROR;
				free(recv_raws);
			} else {
				TRACE_DEBUG("sock:%d length:%d raws", sock, extra_size);

				bundle_data = bundle_decode_raw(recv_raws, extra_size);
				if (bundle_data) {
					int result = 0;
					result = app_control_create((app_control_h *)handle);
					if (result ==	APP_CONTROL_ERROR_NONE) {
						result = app_control_import_from_bundle((app_control_h)*handle, bundle_data);
						if (result !=	APP_CONTROL_ERROR_NONE) {
							TRACE_ERROR("failed to import service handle error:%d type:%d id:%d", result, type, id);
							errorcode = DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
						}
					} else {
						TRACE_ERROR("failed to create service handle error:%d type:%d id:%d", result, type, id);
						errorcode = DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
					}
					bundle_free(bundle_data);
				}
				free(recv_raws);
			}
		}
	}
	if (errorcode == DP_ERROR_IO_ERROR)
		__bp_disconnect(__FUNCTION__);
	CLIENT_MUTEX_UNLOCK(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

int dp_interface_add_http_header_field(const int id, const char *field,
	const char *value)
{
	// cmd + field string + value string
	// wait response
	int errorcode = DP_ERROR_NONE;
	size_t field_length = 0;
	size_t value_length = 0;
	if (field == NULL || (field_length = strlen(field)) <= 0 ||
			field_length > DP_MAX_STR_LEN) {
		TRACE_ERROR("check field (%d:%s)", field_length, field);
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}
	if (value == NULL || (value_length = strlen(value)) <= 0 ||
			value_length > DP_MAX_STR_LEN) {
		TRACE_ERROR("check value (%d:%s)", value_length, value);
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	DP_PRE_CHECK_ID;
	CLIENT_MUTEX_LOCK(&g_function_mutex);
	DP_CHECK_CONNECTION;

	int sock = DP_CHECK_IPC_SOCK;
	short section = DP_SEC_SET;
	unsigned property = DP_PROP_HTTP_HEADER;
	if (dp_ipc_query(sock, id, section, property, DP_ERROR_NONE, field_length * sizeof(char)) < 0) {
		errorcode = DP_ERROR_IO_ERROR;
		TRACE_ERROR("check ipc sock:%d", sock);
	} else {
		if (dp_ipc_write(sock, (void*)field, field_length * sizeof(char)) < 0) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", sock);
		} else {
			errorcode = __dp_ipc_response(sock, id, section, property, NULL);
			if (errorcode == DP_ERROR_NONE) {
				if (dp_ipc_query(sock, id, section, property, DP_ERROR_NONE, value_length * sizeof(char)) < 0) {
					errorcode = DP_ERROR_IO_ERROR;
					TRACE_ERROR("check ipc sock:%d", sock);
				} else {
					if (dp_ipc_write(sock, (void*)value, value_length * sizeof(char)) < 0) {
						errorcode = DP_ERROR_IO_ERROR;
						TRACE_ERROR("check ipc sock:%d", sock);
					} else {
						errorcode = __dp_ipc_response(sock, id, section, property, NULL);
					}
				}
			}
		}
	}
	if (errorcode == DP_ERROR_IO_ERROR)
		__bp_disconnect(__FUNCTION__);
	CLIENT_MUTEX_UNLOCK(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

int dp_interface_get_http_header_field(const int id, const char *field,
	char **value)
{
	// cmd + field string
	// wait response + value string
	int errorcode = DP_ERROR_NONE;
	size_t length = 0;
	if (field == NULL || (length = strlen(field)) <= 0 ||
			length > DP_MAX_STR_LEN) {
		TRACE_ERROR("check field (%d:%s)", length, field);
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}
	if (value == NULL) {
		TRACE_ERROR("check pointer for value");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	DP_PRE_CHECK_ID;
	CLIENT_MUTEX_LOCK(&g_function_mutex);
	DP_CHECK_CONNECTION;

	int sock = DP_CHECK_IPC_SOCK;
	short section = DP_SEC_GET;
	unsigned property = DP_PROP_HTTP_HEADER;
	if (dp_ipc_query(sock, id, section, property, DP_ERROR_NONE, length * sizeof(char)) < 0) {
		errorcode = DP_ERROR_IO_ERROR;
		TRACE_ERROR("check ipc sock:%d", sock);
	} else {
		if (dp_ipc_write(sock, (void*)field, length * sizeof(char)) < 0) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", sock);
		} else {
			size_t string_length = 0;
			*value = NULL;
			errorcode = __dp_ipc_response(sock, id, section, property, &string_length);
			if (errorcode == DP_ERROR_NONE) {
				if (string_length > 0) {
					char *recv_str = (char *)calloc((string_length + (size_t)1), sizeof(char));
					if (recv_str == NULL) {
						TRACE_STRERROR("check memory length:%d", string_length);
						errorcode = DP_ERROR_OUT_OF_MEMORY;
						__dp_ipc_clear_garbage(sock, string_length);
					} else {
						if (dp_ipc_read(sock, recv_str, string_length, __FUNCTION__) <= 0) {
							errorcode = DP_ERROR_IO_ERROR;
							free(recv_str);
						} else {
							recv_str[string_length] = '\0';
							*value = recv_str;
						}
					}
				} else {
					errorcode = DP_ERROR_IO_ERROR;
				}
			}
		}
	}
	if (errorcode == DP_ERROR_IO_ERROR)
		__bp_disconnect(__FUNCTION__);
	CLIENT_MUTEX_UNLOCK(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

int dp_interface_get_http_header_field_list(const int id, char ***fields,
	int *length)
{
	// cmd
	// wait response
	// wait size
	// wait strings
	int errorcode = DP_ERROR_NONE;
	if (fields == NULL) {
		TRACE_ERROR("check pointer for fields");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}
	if (length == NULL) {
		TRACE_ERROR("check pointer for length");
		return DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER;
	}

	DP_PRE_CHECK_ID;
	CLIENT_MUTEX_LOCK(&g_function_mutex);
	DP_CHECK_CONNECTION;

	int sock = DP_CHECK_IPC_SOCK;
	short section = DP_SEC_GET;
	unsigned property = DP_PROP_HTTP_HEADERS;
	if (dp_ipc_query(sock, id, section, property, DP_ERROR_NONE, 0) < 0) {
		errorcode = DP_ERROR_IO_ERROR;
		TRACE_ERROR("check ipc sock:%d", sock);
	} else {

		int array_size = 0;
		size_t extra_size = 0;
		errorcode = __dp_ipc_response(sock, id, section, property, &extra_size);
		if (errorcode == DP_ERROR_NONE) {
			if (extra_size == sizeof(int)) {
				if (dp_ipc_read(sock, &array_size, extra_size, __FUNCTION__) < 0) {
					errorcode = DP_ERROR_IO_ERROR;
				} else {
					if (array_size <= 0) {
						errorcode = DP_ERROR_NO_DATA;
						*length = 0;
					} else {
						int i = 0;
						char **recv_strings = NULL;
						recv_strings = (char **)calloc(array_size, sizeof(char *));
						if (recv_strings == NULL) {
							TRACE_STRERROR("check memory size:%d", array_size);
							errorcode = DP_ERROR_OUT_OF_MEMORY;
							*length = 0;
						} else {
							// get strings.
							for (; i < array_size; i++) {
								size_t string_length = 0;
								errorcode = __dp_ipc_response(sock, id, section, property, &string_length);
								recv_strings[i] = NULL;
								if (errorcode == DP_ERROR_NONE && string_length > 0) {
									char *recv_str = (char *)calloc((string_length + (size_t)1), sizeof(char));
									if (recv_str == NULL) {
										TRACE_STRERROR("check memory length:%d", string_length * sizeof(char));
										errorcode = DP_ERROR_OUT_OF_MEMORY;
										break;
									} else {
										if (dp_ipc_read(sock, recv_str, string_length, __FUNCTION__) <= 0) {
											errorcode = DP_ERROR_IO_ERROR;
											free(recv_str);
										} else {
											recv_str[string_length] = '\0';
											recv_strings[i] = recv_str;
										}
									}
								}
							}
							*fields = recv_strings;
						}
						if (errorcode != DP_ERROR_NONE) { // if error, free all allocated memories
							int j = 0;
							for (; j < i; j++) {
								free(recv_strings[j]);
							}
							free(recv_strings);
							*length = 0;
							*fields = NULL;
							if (errorcode != DP_ERROR_IO_ERROR)
								__bp_disconnect(__FUNCTION__); // clear IPC, can not expect the size of futher packets
						}
						*length = i;
					}
				}
			} else {
				errorcode = DP_ERROR_IO_ERROR;
			}
		}


	}
	if (errorcode == DP_ERROR_IO_ERROR)
		__bp_disconnect(__FUNCTION__);
	CLIENT_MUTEX_UNLOCK(&g_function_mutex);
	return __dp_interface_convert_errorcode(errorcode);
}

int dp_interface_remove_http_header_field(const int id,
	const char *field)
{
	return __dp_ipc_set_string(id, DP_SEC_UNSET, DP_PROP_HTTP_HEADER, field, __FUNCTION__);
}
