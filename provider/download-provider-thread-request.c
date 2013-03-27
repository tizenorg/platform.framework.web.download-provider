/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd All Rights Reserved
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
#include <unistd.h>

#include <time.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

#include <signal.h>

#include <app_manager.h>

#include "download-provider.h"
#include "download-provider-log.h"
#include "download-provider-config.h"
#include "download-provider-slots.h"
#include "download-provider-socket.h"
#include "download-provider-pthread.h"
#include "download-provider-db.h"
#include "download-provider-queue.h"
#include "download-provider-request.h"
#include "download-provider-da-interface.h"

void dp_terminate(int signo);

static char *__print_command(dp_command_type cmd)
{
	switch(cmd)
	{
		case DP_CMD_CREATE :
			return "CREATE";
		case DP_CMD_START :
			return "START";
		case DP_CMD_PAUSE :
			return "PAUSE";
		case DP_CMD_CANCEL :
			return "CANCEL";
		case DP_CMD_DESTROY :
			return "DESTROY";
		case DP_CMD_FREE :
			return "FREE";
		case DP_CMD_ECHO :
			return "ECHO";
		case DP_CMD_SET_URL :
			return "SET_URL";
		case DP_CMD_SET_DESTINATION :
			return "SET_DESTINATION";
		case DP_CMD_SET_FILENAME :
			return "SET_FILENAME";
		case DP_CMD_SET_NOTIFICATION :
			return "SET_NOTIFICATION";
		case DP_CMD_SET_STATE_CALLBACK :
			return "SET_STATE_CALLBACK";
		case DP_CMD_SET_PROGRESS_CALLBACK :
			return "SET_PROGRESS_CALLBACK";
		case DP_CMD_SET_AUTO_DOWNLOAD :
			return "SET_AUTO_DOWNLOAD";
		case DP_CMD_SET_NETWORK_TYPE :
			return "SET_NETWORK_TYPE";
		case DP_CMD_SET_HTTP_HEADER :
			return "SET_HTTP_HEADER";
		case DP_CMD_DEL_HTTP_HEADER :
			return "DEL_HTTP_HEADER";
		case DP_CMD_GET_HTTP_HEADER :
			return "GET_HTTP_HEADER";
		case DP_CMD_GET_URL :
			return "GET_URL";
		case DP_CMD_GET_DESTINATION :
			return "GET_DESTINATION";
		case DP_CMD_GET_FILENAME :
			return "GET_FILENAME";
		case DP_CMD_GET_NOTIFICATION :
			return "GET_NOTIFICATION";
		case DP_CMD_GET_STATE_CALLBACK :
			return "GET_STATE_CALLBACK";
		case DP_CMD_GET_PROGRESS_CALLBACK :
			return "GET_PROGRESS_CALLBACK";
		case DP_CMD_GET_HTTP_HEADERS :
			return "GET_HTTP_HEADERS";
		case DP_CMD_GET_HTTP_HEADER_LIST:
			return "GET_HTTP_HEADER_LIST";
		case DP_CMD_ADD_EXTRA_PARAM :
			return "ADD_EXTRA_PARAM";
		case DP_CMD_GET_EXTRA_PARAM :
			return "GET_EXTRA_PARAM";
		case DP_CMD_REMOVE_EXTRA_PARAM :
			return "REMOVE_EXTRA_PARAM";
		case DP_CMD_GET_AUTO_DOWNLOAD :
			return "GET_AUTO_DOWNLOAD";
		case DP_CMD_GET_NETWORK_TYPE :
			return "GET_NETWORK_TYPE";
		case DP_CMD_GET_SAVED_PATH :
			return "GET_SAVED_PATH";
		case DP_CMD_GET_TEMP_SAVED_PATH :
			return "GET_TEMP_SAVED_PATH";
		case DP_CMD_GET_MIME_TYPE :
			return "GET_MIME_TYPE";
		case DP_CMD_GET_RECEIVED_SIZE :
			return "GET_RECEIVED_SIZE";
		case DP_CMD_GET_TOTAL_FILE_SIZE :
			return "GET_TOTAL_FILE_SIZE";
		case DP_CMD_GET_CONTENT_NAME :
			return "GET_CONTENT_NAME";
		case DP_CMD_GET_HTTP_STATUS :
			return "GET_HTTP_STATUS";
		case DP_CMD_GET_ETAG :
			return "DP_CMD_GET_ETAG";
		case DP_CMD_GET_STATE :
			return "GET_STATE";
		case DP_CMD_GET_ERROR :
			return "ERROR";
		case DP_CMD_SET_COMMAND_SOCKET :
			return "SET_COMMAND_SOCKET";
		case DP_CMD_SET_EVENT_SOCKET :
			return "SET_EVENT_SOCKET";
		default :
			break;
	}
	return "UNKNOWN COMMAND";
}

/* compare two string */
static int __cmp_string(char *s1, char *s2)
{
	size_t s1_len = 0;
	size_t s2_len = 0;

	if (!s1 || !s2) {
		TRACE_ERROR("[CHECK PARAM]");
		return -1;
	}

	s1_len = strlen(s1);
	if (s1_len <= 0) {
		TRACE_ERROR("[CHECK PARAM] len[%d]", s1_len);
		return -1;
	}

	s2_len = strlen(s2);
	if (s2_len <= 0) {
		TRACE_ERROR("[CHECK PARAM] len[%d]", s2_len);
		return -1;
	}

	if (s1_len != s2_len) {
		TRACE_ERROR("[DIFF] len[%d:%d]", s1_len, s2_len);
		return -1;
	}

	if (strncmp(s1, s2, s1_len) != 0) {
		TRACE_ERROR("[DIFF] cmp");
		return -1;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
/// @brief	return index of empty slot
static int __get_empty_request_index(dp_request_slots *slots)
{
	int i = 0;

	if (!slots)
		return -1;

	for (i = 0; i < DP_MAX_REQUEST; i++)
		if (!slots[i].request)
			return i;
	return -1;
}

//////////////////////////////////////////////////////////////////////////
/// @brief	return index of slot having same ID
/// @param	ID want to search
static int __get_same_request_index(dp_request_slots *slots, int id)
{
	int i = 0;

	if (!slots || id < 0)
		return -1;

	for (i = 0; i < DP_MAX_REQUEST; i++) {
		if (slots[i].request) {
			if (slots[i].request->id == id) {
				return i;
			}
		}
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////
/// @brief	return string via IPC
static void __send_return_string(int fd, dp_error_type errcode, char* str)
{
	if (fd < 0 || !str)
		return ;
	dp_ipc_send_errorcode(fd, errcode);
	dp_ipc_send_string(fd, str);
}

//////////////////////////////////////////////////////////////////////////
/// @brief	return custom value via IPC
static void __send_return_custom_type(int fd, dp_error_type errcode, void *value, size_t type_size)
{
	dp_ipc_send_errorcode(fd, errcode);
	dp_ipc_send_custom_type(fd, value, type_size);
}

static int __is_downloading(dp_state_type state)
{
	if (state == DP_STATE_CONNECTING || state == DP_STATE_DOWNLOADING)
		return 0;
	return -1;
}

static int __is_started(dp_state_type state)
{
	if (state == DP_STATE_QUEUED || __is_downloading(state) == 0)
		return 0;
	return -1;
}

static int __is_stopped(dp_state_type state)
{
	if (state == DP_STATE_COMPLETED || state == DP_STATE_FAILED ||
			state == DP_STATE_CANCELED)
		return 0;
	return -1;
}

// cancel the downloading if no auto-download, then clear group
static void __clear_group(dp_privates *privates, dp_client_group *group)
{
	dp_request *request = NULL;
	int i = 0;
	int state = DP_STATE_FAILED;
	int errorcode = DP_ERROR_CLIENT_DOWN;

	TRACE_INFO("");

	for (i = 0; i < DP_MAX_REQUEST; i++) {
		if (privates->requests[i].request == NULL)
			continue;
		if (privates->requests[i].request->group == NULL)
			continue;
		if (privates->requests[i].request->id <= 0)
			continue;
		if (privates->requests[i].request->group != group)
			continue;

		request = privates->requests[i].request;

		CLIENT_MUTEX_LOCK(&request->mutex);

		// if stopped, paused or not started yet, clear request
		if (__is_started(request->state) < 0) {
			TRACE_INFO("[FREE][%d] state[%s]", request->id,
				dp_print_state(request->state));
			CLIENT_MUTEX_UNLOCK(&request->mutex);
			dp_request_free(request);
			privates->requests[i].request = NULL;
			continue;
		}

		// disconnect the request from group.
		TRACE_INFO("[DISCONNECT][%d] state:%s pkg:%s sock:%d",
			request->id, dp_print_state(request->state),
			request->group->pkgname, request->group->cmd_socket);
		request->group = NULL;
		request->state_cb = 0;
		request->progress_cb = 0;

		// care started requests (queued, connecting, downloading)
		int auto_download = dp_db_get_int_column(request->id,
				DP_DB_TABLE_REQUEST_INFO, DP_DB_COL_AUTO_DOWNLOAD);
		if (auto_download <= 0) {
			// cancel the requests which not setted auto-downloading
			TRACE_INFO("[CLEAR][%d] no-auto state:%s",
				request->id, dp_print_state(request->state));

			if (dp_db_set_column(request->id, DP_DB_TABLE_LOG,
					DP_DB_COL_STATE, DP_DB_COL_TYPE_INT, &state) == 0) {
				if (dp_db_set_column(request->id, DP_DB_TABLE_LOG,
						DP_DB_COL_ERRORCODE, DP_DB_COL_TYPE_INT,
						&errorcode) < 0) {
					TRACE_ERROR("[fail][%d][sql] set error:%s",
						request->id, dp_print_errorcode(errorcode));
				}
				// regardless errorcode, if success to update the state.
				if (request->agent_id > 0) {
					TRACE_INFO("[%d]cancel_agent(%d) state:%s error:%s",
						request->id, request->agent_id,
						dp_print_state(state),
						dp_print_errorcode(errorcode));
					if (dp_cancel_agent_download(request->agent_id) < 0)
						TRACE_INFO("[fail][%d]cancel_agent", request->id);
				}
				CLIENT_MUTEX_UNLOCK(&request->mutex);
				dp_request_free(request);
				privates->requests[i].request = NULL;
				continue;
			} else {
				TRACE_ERROR("[fail][%d][sql] set state:%s", request->id,
					dp_print_state(state));
			}
		}
		CLIENT_MUTEX_UNLOCK(&request->mutex);

	}
	// clear this group
	dp_client_group_free(group);
}

static void __dp_remove_tmpfile(int id, dp_request *request)
{
	dp_error_type errorcode = DP_ERROR_NONE;
	char *path =
		dp_request_get_tmpsavedpath(id, request, &errorcode);
	if (errorcode == DP_ERROR_NONE &&
			(path != NULL && strlen(path) > 0) &&
			dp_is_file_exist(path) == 0) {
		TRACE_INFO("[REMOVE][%d] TEMP FILE [%s]", id, path);
		if (unlink(path) != 0)
			TRACE_STRERROR("[ERROR][%d] remove file", id);
	}
	free(path);
}

static int __dp_call_cancel_agent(dp_request *request)
{
	int ret = -1;
	if (request != NULL) {
		if (request->agent_id > 0) {
			TRACE_INFO("[%d]cancel_agent(%d) state:%s", request->id,
				request->agent_id, dp_print_state(request->state));
			if (dp_cancel_agent_download(request->agent_id) == 0)
				ret = 0;
		} else {
			TRACE_INFO("[CHECK] agent-id");
		}
	}
	return ret;
}

static int __dp_add_extra_param(int fd, int id)
{
	dp_error_type ret = DP_ERROR_NONE;
	int length = 0;
	int i = 0;
	unsigned values_length = 0;
	char *key = NULL;
	char **values = NULL;

	if (fd < 0) {
		TRACE_ERROR("[CHECK] socket");
		return DP_ERROR_IO_ERROR;
	}
	if (id < 0) {
		TRACE_ERROR("[CHECK] ID");
		return DP_ERROR_INVALID_PARAMETER;
	}

	if (dp_ipc_read_custom_type(fd, &length, sizeof(int)) < 0) {
		TRACE_ERROR("[ERROR][%d] [DP_ERROR_IO_ERROR]", id);
		ret = DP_ERROR_IO_ERROR;
	} else {
		TRACE_INFO("[RECV] length %d", length);
		if (length <= 0) {
			ret = DP_ERROR_INVALID_PARAMETER;
		} else {
			key = dp_ipc_read_string(fd);
			if (key == NULL) {
				TRACE_ERROR("[ERROR][%d][IO_ERROR] key", id);
				ret = DP_ERROR_IO_ERROR;
			} else {
				if (length > 1) {
					TRACE_INFO("[RECV] key : %s", key);
					// get values
					values = (char **)calloc((length - 1), sizeof(char *));
					if (values == NULL) {
						ret = DP_ERROR_OUT_OF_MEMORY;
					} else {
						for (i = 0; i < length - 1; i++) {
							values[i] = dp_ipc_read_string(fd);
							if (values[i] == NULL) {
								ret = DP_ERROR_IO_ERROR;
								break;
							}
							values_length++;
						}
					}
				} else {
					TRACE_ERROR("[ERROR][%d] length [%d]", id, length);
					ret = DP_ERROR_INVALID_PARAMETER;
				}
			}
		}
	}
	if (ret == DP_ERROR_NONE) {
		// store to DB
		for (i = 0; i < length - 1; i++) {
			int conds_count = 3;
			db_conds_list_fmt conds_p[conds_count]; // id + key + value
			memset(&conds_p, 0x00, conds_count * sizeof(db_conds_list_fmt));
			conds_p[0].column = DP_DB_COL_ID;
			conds_p[0].type = DP_DB_COL_TYPE_INT;
			conds_p[0].value = &id;
			conds_p[1].column = DP_DB_COL_EXTRA_KEY;
			conds_p[1].type = DP_DB_COL_TYPE_TEXT;
			conds_p[1].value = key;
			conds_p[2].column = DP_DB_COL_EXTRA_VALUE;
			conds_p[2].type = DP_DB_COL_TYPE_TEXT;
			conds_p[2].value = values[i];
			int check_key =
				dp_db_get_conds_rows_count(DP_DB_TABLE_NOTIFICATION,
					DP_DB_COL_ID, "AND", conds_count, conds_p);
			if (check_key <= 0) { // create newly
				// insert
				if (dp_db_insert_columns(DP_DB_TABLE_NOTIFICATION,
						conds_count, conds_p) < 0) {
					ret = DP_ERROR_OUT_OF_MEMORY;
					break;
				}
			} // else skip. already exist
		}
	}
	free(key);
	for (i = 0; i < values_length; i++) {
		free(values[i]);
	}
	free(values);
	return ret;
}

static int __dp_get_extra_param_values(int fd, int id, char ***values,
		unsigned *count)
{
	dp_error_type ret = DP_ERROR_NONE;
	int length = 0;
	char *key = NULL;
	char **rows_array = NULL;

	if (fd < 0) {
		TRACE_ERROR("[CHECK] socket");
		return DP_ERROR_IO_ERROR;
	}
	if (id < 0) {
		TRACE_ERROR("[CHECK] ID");
		return DP_ERROR_INVALID_PARAMETER;
	}

	if (dp_ipc_read_custom_type(fd, &length, sizeof(int)) < 0) {
		TRACE_ERROR("[ERROR][%d] [DP_ERROR_IO_ERROR]", id);
		ret = DP_ERROR_IO_ERROR;
	} else {
		TRACE_INFO("[RECV] length %d", length);
		if (length != 1) { // only a key
			ret = DP_ERROR_INVALID_PARAMETER;
		} else {
			if ((key = dp_ipc_read_string(fd)) == NULL) {
				TRACE_ERROR("[ERROR][%d][IO_ERROR] key", id);
				ret = DP_ERROR_IO_ERROR;
			}
		}
	}
	if (ret == DP_ERROR_NONE) {
		int conds_count = 2;
		db_conds_list_fmt conds_p[conds_count]; // id + key + value
		memset(&conds_p, 0x00, conds_count * sizeof(db_conds_list_fmt));
		conds_p[0].column = DP_DB_COL_ID;
		conds_p[0].type = DP_DB_COL_TYPE_INT;
		conds_p[0].value = &id;
		conds_p[1].column = DP_DB_COL_EXTRA_KEY;
		conds_p[1].type = DP_DB_COL_TYPE_TEXT;
		conds_p[1].value = key;
		int check_rows = dp_db_get_conds_rows_count
			(DP_DB_TABLE_NOTIFICATION, DP_DB_COL_EXTRA_VALUE, "AND",
			conds_count, conds_p);
		if (check_rows <= 0) {
			// NO_DATA
			ret = DP_ERROR_NO_DATA;
		} else {
			rows_array = (char **)calloc(check_rows, sizeof(char *));
			if (rows_array == NULL) {
				ret = DP_ERROR_OUT_OF_MEMORY;
			} else {
				// getting the array from DB with key condition
				int rows_count =
					dp_db_get_conds_list(DP_DB_TABLE_NOTIFICATION,
						DP_DB_COL_EXTRA_VALUE, DP_DB_COL_TYPE_TEXT,
						(void **)rows_array, check_rows, -1, NULL, NULL,
						"AND", conds_count, conds_p);
				if (rows_count <= 0) {
					// NO_DATA
					ret = DP_ERROR_NO_DATA;
					free(rows_array);
				} else {
					*count = rows_count;
					*values = rows_array;
				}
			}
			free(key);
		}
	}
	return ret;
}

static int __dp_remove_extra_param(int fd, int id)
{
	dp_error_type ret = DP_ERROR_NONE;
	char *key = NULL;

	if (fd < 0) {
		TRACE_ERROR("[CHECK] socket");
		return DP_ERROR_IO_ERROR;
	}
	if (id < 0) {
		TRACE_ERROR("[CHECK] ID");
		return DP_ERROR_INVALID_PARAMETER;
	}

	if ((key = dp_ipc_read_string(fd)) == NULL) {
		TRACE_ERROR("[ERROR][%d] INVALID_PARAMETER", id);
		ret = DP_ERROR_IO_ERROR;
	}
	if (ret == DP_ERROR_NONE) {
		if (dp_db_cond_remove(id, DP_DB_TABLE_NOTIFICATION,
				DP_DB_COL_EXTRA_KEY, DP_DB_COL_TYPE_TEXT, key) < 0) {
			TRACE_ERROR("[fail][%d][sql] set key:%s", id, key);
			ret = DP_ERROR_OUT_OF_MEMORY;
		}
	}
	TRACE_ERROR
		("[ERROR][%d][%s] key:%s", id, dp_print_errorcode(ret), key);
	free(key);
	return ret;
}

static int __dp_get_http_header_fields(int fd, int id, char ***values,
		unsigned *count)
{
	dp_error_type ret = DP_ERROR_NONE;
	int length = 0;
	char **rows_array = NULL;

	if (fd < 0) {
		TRACE_ERROR("[CHECK] socket");
		return DP_ERROR_IO_ERROR;
	}
	if (id < 0) {
		TRACE_ERROR("[CHECK] ID");
		return DP_ERROR_INVALID_PARAMETER;
	}

	db_conds_list_fmt conds_p;
	conds_p.column = DP_DB_COL_ID;
	conds_p.type = DP_DB_COL_TYPE_INT;
	conds_p.value = &id;
	conds_p.is_like = 0;
	int check_rows = dp_db_get_conds_rows_count
		(DP_DB_TABLE_HTTP_HEADERS, DP_DB_COL_HEADER_FIELD, "AND",
		1, &conds_p);
	if (check_rows <= 0) {
		// NO_DATA
		ret = DP_ERROR_NO_DATA;
	} else {
		rows_array = (char **)calloc(check_rows, sizeof(char *));
		if (rows_array == NULL) {
			ret = DP_ERROR_OUT_OF_MEMORY;
		} else {
			int rows_count =
				dp_db_get_conds_list(DP_DB_TABLE_HTTP_HEADERS,
					DP_DB_COL_HEADER_FIELD, DP_DB_COL_TYPE_TEXT,
					(void **)rows_array, check_rows, -1, NULL, NULL,
					"AND", 1, &conds_p);
			if (rows_count <= 0) {
				// NO_DATA
				ret = DP_ERROR_NO_DATA;
				free(rows_array);
			} else {
				*count = rows_count;
				*values = rows_array;
			}
		}
	}
	return ret;
}

static int __dp_set_group_new(int clientfd, dp_group_slots *groups,
	dp_credential credential, fd_set *listen_fdset)
{
	// search in groups.
	// if same group. update it.
	// search same pkg or pid in groups
	int pkg_len = 0;
	int i = 0;
	struct timeval tv_timeo; // 2.5 sec
	char *pkgname = NULL;

	tv_timeo.tv_sec = 2;
	tv_timeo.tv_usec = 500000;
	if (setsockopt(clientfd, SOL_SOCKET, SO_RCVTIMEO, &tv_timeo,
		sizeof(tv_timeo)) < 0) {
		TRACE_STRERROR("[CRITICAL] setsockopt SO_RCVTIMEO");
		return -1;
	}

	// getting the package name via pid
	if (app_manager_get_package(credential.pid, &pkgname) ==
			APP_MANAGER_ERROR_NONE) {
		TRACE_INFO("package : %s", pkgname);
	} else
		TRACE_ERROR("[CRITICAL] app_manager_get_package");

	//// TEST CODE ... to allow sample client ( no package name ).
	if (pkgname == NULL) {
		pkgname = dp_strdup("unknown_app");
		TRACE_INFO("default package naming : %s", pkgname);
	}

	if (pkgname == NULL) {
		TRACE_ERROR("[CRITICAL] app_manager_get_package");
		return -1;
	}
	if ((pkg_len = strlen(pkgname)) <= 0) {
		TRACE_ERROR("[CRITICAL] pkgname:%s", pkgname);
		free(pkgname);
		return -1;
	}

	for (i = 0; i < DP_MAX_GROUP; i++) {
		if (groups[i].group != NULL) {
			// clean garbage slot
			if (groups[i].group->cmd_socket <= 0 ||
					groups[i].group->pkgname == NULL) {
				dp_client_group_free(groups[i].group);
				groups[i].group = NULL;
				continue;
			}
			if (strlen(groups[i].group->pkgname) == pkg_len &&
					strncmp(groups[i].group->pkgname, pkgname,
						pkg_len) == 0 ) {
				// Found Same Group
				TRACE_INFO("UPDATE Group: slot:%d pid:%d sock:%d [%s]",
					i, credential.pid, clientfd, pkgname);
				if (groups[i].group->cmd_socket > 0 &&
						groups[i].group->cmd_socket != clientfd) {
					FD_CLR(groups[i].group->cmd_socket, listen_fdset);
					dp_socket_free(groups[i].group->cmd_socket);
				}
				groups[i].group->cmd_socket = clientfd;
				free(pkgname);
				return 0;
			}
		}
	}

	// new client
	// search emtpy slot in groups
	for (i = 0; i < DP_MAX_GROUP; i++)
		if (groups[i].group == NULL)
			break;
	if (i >= DP_MAX_GROUP) {
		TRACE_ERROR("[CRITICAL] No space in groups");
		free(pkgname);
		return -1;
	}
	// allocation
	groups[i].group =
		(dp_client_group *)calloc(1, sizeof(dp_client_group));
	if (groups[i].group == NULL) {
		TRACE_ERROR("[CRITICAL] calloc, ignore this client");
		free(pkgname);
		return -1;
	}
	// fill info
	groups[i].group->cmd_socket = clientfd;
	groups[i].group->event_socket = -1;
	groups[i].group->queued_count = 0;
	groups[i].group->pkgname = dp_strdup(pkgname);
	groups[i].group->credential.pid = credential.pid;
	groups[i].group->credential.uid = credential.uid;
	groups[i].group->credential.gid = credential.gid;
	TRACE_INFO("New Group: slot:%d pid:%d sock:%d [%s]", i,
		credential.pid, clientfd, pkgname);
	free(pkgname);
	return 0;
}


static int __dp_set_group_event_sock(int clientfd,
	dp_group_slots *groups, dp_credential credential)
{
	int i = 0;

	TRACE_INFO("Check event pid:%d sock:%d", credential.pid, clientfd);
	// search same pid in groups
	for (i = 0; i < DP_MAX_GROUP; i++) {
		if (groups[i].group != NULL &&
				groups[i].group->credential.pid == credential.pid) {
			if (groups[i].group->event_socket > 0 &&
					groups[i].group->event_socket != clientfd)
				dp_socket_free(groups[i].group->event_socket);
			groups[i].group->event_socket = clientfd;
			TRACE_INFO
				("Found Group : slot:%d pid:%d csock:%d esock:%d [%s]",
					i, credential.pid, groups[i].group->cmd_socket,
					clientfd, groups[i].group->pkgname);
			break;
		}
	}
	if (i >= DP_MAX_GROUP) {
		TRACE_ERROR
			("[CRITICAL] Not found group for PID [%d]", credential.pid);
		return -1;
	}
	return 0;
}

// in url-download, make 3 connection before send CREATE command.
// after accepting, fill info to pacakgelist.
// 3 socket per 1 package ( info/request/progress )
void *dp_thread_requests_manager(void *arg)
{
	fd_set rset, eset, listen_fdset, except_fdset;
	struct timeval timeout; // for timeout of select
	long flexible_timeout = DP_CARE_CLIENT_MAX_INTERVAL;
	int listenfd, clientfd, maxfd;
	socklen_t clientlen;
	struct sockaddr_un clientaddr;
	dp_credential credential;
	unsigned i, is_timeout;
	int prev_timeout = 0;
	dp_error_type errorcode = DP_ERROR_NONE;

	dp_privates *privates = (dp_privates*)arg;
	if (privates == NULL || privates->groups == NULL) {
		TRACE_ERROR("[CRITICAL] Invalid Address");
		dp_terminate(SIGTERM);
		pthread_exit(NULL);
		return 0;
	}

	listenfd = privates->listen_fd;
	maxfd = listenfd;

	TRACE_INFO("Ready to listen [%d][%s]", listenfd, DP_IPC);

	FD_ZERO(&listen_fdset);
	FD_ZERO(&except_fdset);
	FD_SET(listenfd, &listen_fdset);
	FD_SET(listenfd, &except_fdset);

	while (privates != NULL && privates->listen_fd >= 0) {

		// select with timeout
		// initialize timeout structure for calling timeout exactly
		memset(&timeout, 0x00, sizeof(struct timeval));
		timeout.tv_sec = flexible_timeout;
		clientfd = -1;
		credential.pid = -1;
		credential.uid = -1;
		credential.gid = -1;
		is_timeout = 1;

		rset = listen_fdset;
		eset = except_fdset;

		if (select((maxfd + 1), &rset, 0, &eset, &timeout) < 0) {
			TRACE_STRERROR("[CRITICAL] select");
			break;
		}

		if (privates == NULL) {
			TRACE_INFO("Terminate Thread");
			break;
		}

		if (FD_ISSET(listenfd, &eset) > 0) {
			TRACE_ERROR("[CRITICAL] listenfd Exception of socket");
			break;
		} else if (FD_ISSET(listenfd, &rset) > 0) {
			// new client(package) called url_download_create API.
			// update g_dp_request_max_fd & g_dp_info_max_fd
			// add to socket to g_dp_request_rset & g_dp_info_rset

			is_timeout = 0;

			// Anyway accept client.
			clientlen = sizeof(clientaddr);
			clientfd = accept(listenfd,
							(struct sockaddr*)&clientaddr,
							&clientlen);
			if (clientfd < 0) {
				TRACE_ERROR("[CRITICAL] accept provider was crashed ?");
				// provider need the time of refresh.
				break;
			}

			dp_command_type connect_cmd = DP_CMD_NONE;
			if (dp_ipc_read_custom_type(clientfd,
					&connect_cmd, sizeof(dp_command_type)) < 0) {
				TRACE_ERROR("[CRITICAL] CAPI not support CONNECT CMD");
				close(clientfd);
				continue;
			}
			if (connect_cmd <= 0) {
				TRACE_ERROR("[CRITICAL] peer terminate ?");
				close(clientfd);
				continue;
			}

#ifdef SO_PEERCRED
			// getting the info of client
			socklen_t cr_len = sizeof(credential);
			if (getsockopt(clientfd, SOL_SOCKET, SO_PEERCRED,
				&credential, &cr_len) == 0) {
				TRACE_INFO
					("credential : pid=%d, uid=%d, gid=%d",
					credential.pid, credential.uid, credential.gid);
			}
#else // In case of not supported SO_PEERCRED
			int client_pid = 0;
			if (dp_ipc_read_custom_type(clientfd,
					&client_pid, sizeof(int)) < 0) {
				TRACE_ERROR("[CRITICAL] not support SO_PEERCRED");
				close(clientfd);
				continue;
			}
			if (client_pid <= 0) {
				TRACE_ERROR("[CRITICAL] not support SO_PEERCRED");
				close(clientfd);
				continue;
			}
			credential.pid = client_pid;
			credential.uid = 5000;
			credential.gid = 5000;
#endif

			switch(connect_cmd) {
			case DP_CMD_SET_COMMAND_SOCKET:
				if (__dp_set_group_new(clientfd, privates->groups,
						credential, &listen_fdset) == 0) {
					FD_SET(clientfd, &listen_fdset);
					if (clientfd > maxfd)
						maxfd = clientfd;
				} else {
					close(clientfd);
				}
				break;
			case DP_CMD_SET_EVENT_SOCKET:
				if (__dp_set_group_event_sock(clientfd,
						privates->groups, credential) < 0)
					close(clientfd);
				break;
			default:
				TRACE_ERROR("[CRITICAL] Bad access,ignore this client");
				close(clientfd);
				break;
			}
		} // New Connection

		// listen cmd_socket of all group
		for (i = 0; i < DP_MAX_GROUP; i++) {
			dp_client_group *group = privates->groups[i].group;
			if (group == NULL || group->cmd_socket < 0)
				continue;
			const int sock = group->cmd_socket;
			if (FD_ISSET(sock, &rset) > 0) {
				int index = -1;
				dp_command command;

				is_timeout = 0;
				command.cmd = DP_CMD_NONE;
				command.id = -1;

				if (dp_ipc_read_custom_type(sock, &command,
						sizeof(dp_command)) < 0) {
					TRACE_STRERROR("failed to read cmd");
					//Resource temporarily unavailable
					continue;
				}

				// print detail info
				TRACE_INFO("[%s] id:%d sock:%d pkg:%s pid:%d slot:%d",
					__print_command(command.cmd), command.id, sock,
					group->pkgname, group->credential.pid, i);

				if (command.cmd == 0) { // Client meet some Error.
					TRACE_INFO("[Closed Peer] pkg:%s sock:%d slot:%d",
						group->pkgname, sock, i);
					// check all request included to this group
					FD_CLR(sock, &listen_fdset);
					__clear_group(privates, group);
					privates->groups[i].group = NULL;
					continue;
				}

				// Echo .client can check whether provider is busy
				if (command.cmd == DP_CMD_ECHO) {
					// provider can clear read buffer here
					TRACE_INFO("[ECHO] sock:%d", sock);
					if (dp_ipc_send_errorcode(sock,
							DP_ERROR_NONE) < 0) {
						// disconnect this group, bad client
						FD_CLR(sock, &listen_fdset);
						__clear_group(privates, group);
						privates->groups[i].group = NULL;
					}
					continue;
				}

				if (command.cmd == DP_CMD_CREATE) {
					// search empty slot in privates->requests
					index =
						__get_empty_request_index(privates->requests);
					if (index < 0) {
						TRACE_ERROR("[CHECK] [DP_ERROR_QUEUE_FULL]");
						// Busy, No Space in slot
						dp_ipc_send_errorcode(sock, DP_ERROR_QUEUE_FULL);
					} else {
						errorcode = DP_ERROR_NONE;
						errorcode = dp_request_create(command.id, group,
							&privates->requests[index].request);
						dp_ipc_send_errorcode(sock, errorcode);
						if (errorcode == DP_ERROR_NONE) {
							TRACE_INFO("[CREATE] GOOD id:%d slot:%d",
								privates->requests[index].request->id,
								index);
							dp_ipc_send_custom_type(sock,
								&privates->requests[index].request->id,
								sizeof(int));
						} else {
							TRACE_ERROR("[ERROR][%s]",
								dp_print_errorcode(errorcode));
						}
					}
					continue;
				}

				// below commands must need ID
				// check exception before searching slots.
				if (command.id < 0) {
					TRACE_ERROR("[CHECK PROTOCOL] ID should not be -1");
					dp_ipc_send_errorcode(sock,
						DP_ERROR_INVALID_PARAMETER);
					// disconnect this group, bad client
					FD_CLR(sock, &listen_fdset);
					__clear_group(privates, group);
					privates->groups[i].group = NULL;
					continue;
				}

				// search id in requests slot
				index = __get_same_request_index(privates->requests,
						command.id);

				// GET API works even if request is NULL.
				dp_request *request = NULL;
				if (index >= 0)
					request = privates->requests[index].request;

				// check for all command.
				if (request == NULL) {
					int check_id = dp_db_get_int_column(command.id,
							DP_DB_TABLE_LOG, DP_DB_COL_ID);
					if (check_id != command.id) {
						errorcode = DP_ERROR_ID_NOT_FOUND;
						TRACE_ERROR("[ERROR][%d][%s]", command.id,
							dp_print_errorcode(errorcode));
						dp_ipc_send_errorcode(sock, errorcode);
						continue;
					}
				}

				// Authentication by packagename.
				char *auth_pkgname = NULL;
				errorcode = DP_ERROR_NONE;
				if (request != NULL) {
					auth_pkgname = dp_strdup(request->packagename);
					if (auth_pkgname == NULL)
						errorcode = DP_ERROR_OUT_OF_MEMORY;
				} else {
					auth_pkgname = dp_db_get_text_column(command.id,
							DP_DB_TABLE_LOG, DP_DB_COL_PACKAGENAME);
					if (auth_pkgname == NULL)
						errorcode = DP_ERROR_ID_NOT_FOUND;
				}
				if (errorcode == DP_ERROR_NONE) {
					// auth by pkgname
					if (__cmp_string(group->pkgname, auth_pkgname) < 0) {
						TRACE_ERROR("[ERROR][%d] Auth [%s]/[%s]",
							command.id, group->pkgname, auth_pkgname);
						errorcode = DP_ERROR_INVALID_PARAMETER;
					}
				}
				free(auth_pkgname);
				if (errorcode != DP_ERROR_NONE) {
					TRACE_ERROR("[ERROR][%d][%s]", command.id,
							dp_print_errorcode(errorcode));
					dp_ipc_send_errorcode(sock, errorcode);
					continue;
				}

				// if no group, update group.
				if (request != NULL && request->group == NULL)
					request->group = group;

				unsigned is_checked = 1;
				int read_int = 0;
				char *read_str = NULL;

				// read a interger or a string, return errorcode.
				errorcode = DP_ERROR_NONE;
				switch(command.cmd) {
				case DP_CMD_SET_STATE_CALLBACK:
					if (dp_ipc_read_custom_type(sock, &read_int,
							sizeof(int)) < 0) {
						errorcode = DP_ERROR_IO_ERROR;
						break;
					}
					errorcode = dp_request_set_state_event(command.id,
							request, read_int);
					break;
				case DP_CMD_SET_PROGRESS_CALLBACK:
					if (dp_ipc_read_custom_type(sock, &read_int,
							sizeof(int)) < 0) {
						errorcode = DP_ERROR_IO_ERROR;
						break;
					}
					errorcode = dp_request_set_progress_event
							(command.id, request, read_int);
					break;
				case DP_CMD_SET_NETWORK_TYPE:
					if (dp_ipc_read_custom_type(sock, &read_int,
							sizeof(int)) < 0) {
						errorcode = DP_ERROR_IO_ERROR;
						break;
					}
					errorcode = dp_request_set_network_type(command.id,
							request, read_int);
					break;
				case DP_CMD_SET_AUTO_DOWNLOAD:
					if (dp_ipc_read_custom_type(sock, &read_int,
							sizeof(int)) < 0) {
						errorcode = DP_ERROR_IO_ERROR;
						break;
					}
					errorcode = dp_request_set_auto_download(command.id,
							request, read_int);
					break;
				case DP_CMD_SET_NOTIFICATION:
					if (dp_ipc_read_custom_type(sock, &read_int,
							sizeof(int)) < 0) {
						errorcode = DP_ERROR_IO_ERROR;
						break;
					}
					errorcode = dp_request_set_notification(command.id,
							request, read_int);
					break;
				case DP_CMD_SET_URL:
					if ((read_str = dp_ipc_read_string(sock)) == NULL) {
						errorcode = DP_ERROR_IO_ERROR;
						break;
					}
					errorcode = dp_request_set_url(command.id, request,
							read_str);
					break;
				case DP_CMD_SET_DESTINATION:
					if ((read_str = dp_ipc_read_string(sock)) == NULL) {
						errorcode = DP_ERROR_IO_ERROR;
						break;
					}
					errorcode = dp_request_set_destination(command.id,
							request, read_str);
					break;
				case DP_CMD_SET_FILENAME:
					if ((read_str = dp_ipc_read_string(sock)) == NULL) {
						errorcode = DP_ERROR_IO_ERROR;
						break;
					}
					errorcode = dp_request_set_filename(command.id,
							request, read_str);
					break;
				default:
					is_checked = 0;
					break;
				}
				if (is_checked == 1) {
					free(read_str);
					if (errorcode != DP_ERROR_NONE) {
						TRACE_ERROR("[ERROR][%d][%s][%s]", command.id,
							__print_command(command.cmd),
							dp_print_errorcode(errorcode));
					}
					if (errorcode == DP_ERROR_IO_ERROR) {
						FD_CLR(sock, &listen_fdset);
						__clear_group(privates, group);
						privates->groups[i].group = NULL;
						continue;
					}
					dp_ipc_send_errorcode(sock, errorcode);
					continue;
				}

				// No read(), write a string
				read_str = NULL;
				errorcode = DP_ERROR_NONE;
				is_checked = 1;
				switch(command.cmd) {
				case DP_CMD_GET_URL:
					read_str = dp_request_get_url(command.id, request,
							&errorcode);
					break;
				case DP_CMD_GET_DESTINATION:
					read_str = dp_request_get_destination(command.id,
							request, &errorcode);
					break;
				case DP_CMD_GET_FILENAME:
					read_str = dp_request_get_filename(command.id,
							request, &errorcode);
					break;
				case DP_CMD_GET_SAVED_PATH:
					read_str = dp_request_get_savedpath(command.id,
							request, &errorcode);
					break;
				case DP_CMD_GET_TEMP_SAVED_PATH:
					read_str = dp_request_get_tmpsavedpath(command.id,
							request, &errorcode);
					break;
				case DP_CMD_GET_MIME_TYPE:
					read_str = dp_request_get_mimetype(command.id,
							request, &errorcode);
					break;
				case DP_CMD_GET_CONTENT_NAME:
					read_str = dp_request_get_contentname(command.id,
							request, &errorcode);
					break;
				case DP_CMD_GET_ETAG:
					read_str = dp_request_get_etag(command.id, request,
							&errorcode);
					break;
				default:
					is_checked = 0;
					break;
				}
				if (is_checked == 1) {
					dp_ipc_send_errorcode(sock, errorcode);
					if (errorcode == DP_ERROR_NONE) {
						dp_ipc_send_string(sock, read_str);
					} else {
						TRACE_ERROR("[ERROR][%d][%s][%s]", command.id,
							__print_command(command.cmd),
							dp_print_errorcode(errorcode));
					}
					free(read_str);
					continue;
				}

				// No read(), write a integer variable
				read_int = 0;
				errorcode = DP_ERROR_NONE;
				is_checked = 1;
				switch(command.cmd) {
				case DP_CMD_GET_NOTIFICATION:
					read_int = dp_db_get_int_column(command.id,
									DP_DB_TABLE_REQUEST_INFO,
									DP_DB_COL_NOTIFICATION_ENABLE);
					break;
				case DP_CMD_GET_AUTO_DOWNLOAD:
					read_int = dp_db_get_int_column(command.id,
									DP_DB_TABLE_REQUEST_INFO,
									DP_DB_COL_AUTO_DOWNLOAD);
					break;
				case DP_CMD_GET_NETWORK_TYPE:
					read_int = dp_db_get_int_column(command.id,
									DP_DB_TABLE_REQUEST_INFO,
									DP_DB_COL_NETWORK_TYPE);
					break;
				case DP_CMD_GET_HTTP_STATUS:
					read_int = dp_db_get_int_column(command.id,
									DP_DB_TABLE_DOWNLOAD_INFO,
									DP_DB_COL_HTTP_STATUS);
					break;
				case DP_CMD_GET_STATE:
					if (request == NULL) {
						read_int = dp_db_get_int_column(command.id,
								DP_DB_TABLE_LOG, DP_DB_COL_STATE);
					} else {
						CLIENT_MUTEX_LOCK(&request->mutex);
						read_int = request->state;
						CLIENT_MUTEX_UNLOCK(&request->mutex);
					}
					break;
				case DP_CMD_GET_ERROR:
					if (request == NULL) {
						read_int = dp_db_get_int_column(command.id,
								DP_DB_TABLE_LOG, DP_DB_COL_ERRORCODE);
					} else {
						CLIENT_MUTEX_LOCK(&request->mutex);
						read_int = request->error;
						CLIENT_MUTEX_UNLOCK(&request->mutex);
					}
					break;
				default:
					is_checked = 0;
					break;
				}
				if (is_checked == 1) {
					if (read_int < 0)
						errorcode = DP_ERROR_NO_DATA;
					dp_ipc_send_errorcode(sock, errorcode);
					if (errorcode == DP_ERROR_NONE) {
						dp_ipc_send_custom_type(sock, &read_int,
							sizeof(int));
					} else {
						TRACE_ERROR("[ERROR][%d][%s][%s]", command.id,
							__print_command(command.cmd),
							dp_print_errorcode(errorcode));
					}
					continue;
				}

				// No read(), write a long long variable
				unsigned long long recv_long = 0;
				errorcode = DP_ERROR_NONE;
				is_checked = 1;
				switch(command.cmd) {
				case DP_CMD_GET_RECEIVED_SIZE:
					if (request == NULL) {
						errorcode = DP_ERROR_NO_DATA;
					} else {
						CLIENT_MUTEX_LOCK(&request->mutex);
						recv_long = request->received_size;
						CLIENT_MUTEX_UNLOCK(&request->mutex);
					}
					break;
				case DP_CMD_GET_TOTAL_FILE_SIZE:
					if (request != NULL) {
						CLIENT_MUTEX_LOCK(&request->mutex);
						recv_long = request->file_size;
						CLIENT_MUTEX_UNLOCK(&request->mutex);
					} else {
						long long file_size = dp_db_get_int64_column
								(command.id, DP_DB_TABLE_DOWNLOAD_INFO,
								DP_DB_COL_CONTENT_SIZE);
						if (file_size < 0)
							errorcode = DP_ERROR_NO_DATA;
						else // casting
							recv_long = file_size;
					}
					break;
				default:
					is_checked = 0;
					break;
				}
				if (is_checked == 1) {
					dp_ipc_send_errorcode(sock, errorcode);
					if (errorcode == DP_ERROR_NONE) {
						dp_ipc_send_custom_type(sock, &recv_long,
							sizeof(unsigned long long));
					} else {
						TRACE_ERROR("[ERROR][%d][%s][%s]", command.id,
							__print_command(command.cmd),
							dp_print_errorcode(errorcode));
					}
					continue;
				}

				read_int = 0;
				errorcode = DP_ERROR_NONE;
				is_checked = 1;
				switch(command.cmd) {
				case DP_CMD_DESTROY:
					if (request == NULL) {// just update the state
						if (dp_db_set_column(command.id,
								DP_DB_TABLE_LOG, DP_DB_COL_STATE,
								DP_DB_COL_TYPE_INT, &read_int) < 0)
							errorcode = DP_ERROR_OUT_OF_MEMORY;
						break;
					}
					if (request != NULL) { // call cancel in some cases
						CLIENT_MUTEX_LOCK(&request->mutex);
						if (__is_started(request->state) == 0) {
							read_int = DP_STATE_CANCELED;
							if (dp_db_set_column(command.id,
								DP_DB_TABLE_LOG, DP_DB_COL_STATE,
								DP_DB_COL_TYPE_INT, &read_int) < 0) {
								errorcode = DP_ERROR_OUT_OF_MEMORY;
							} else {
								if (__dp_call_cancel_agent(request) < 0)
									TRACE_INFO("[fail][%d]cancel_agent",
										command.id);
								request->state = DP_STATE_CANCELED;
							}
						}
						CLIENT_MUTEX_UNLOCK(&request->mutex);
					}
					break;
				case DP_CMD_FREE:
					// [destory]-[return]-[free]
					// No return errorcode
					if (request != NULL) {
						dp_request_free(request);
						privates->requests[index].request = NULL;
					}
					break;
				case DP_CMD_START:
				{
					if (request != NULL) {
						CLIENT_MUTEX_LOCK(&request->mutex);
						if (__is_started(request->state) == 0 ||
								request->state == DP_STATE_COMPLETED)
							errorcode = DP_ERROR_INVALID_STATE;
						CLIENT_MUTEX_UNLOCK(&request->mutex);
					}
					if (errorcode != DP_ERROR_NONE)
						break;

					// need URL at least
					char *url = dp_request_get_url(command.id, request,
							&errorcode);
					if (url == NULL) {
						errorcode = DP_ERROR_INVALID_URL;
						break;
					}
					free(url);

					if (request == NULL) { // Support Re-download
						index = __get_empty_request_index
								(privates->requests);
						if (index < 0) { // Busy, No Space in slot
							errorcode = DP_ERROR_QUEUE_FULL;
						} else {
							request = dp_request_load_from_log
										(command.id, &errorcode);
							if (request != NULL) {
								// restore callback info
								CLIENT_MUTEX_LOCK(&request->mutex);
								request->state_cb =
									dp_db_get_int_column(command.id,
											DP_DB_TABLE_REQUEST_INFO,
											DP_DB_COL_STATE_EVENT);
								request->progress_cb =
									dp_db_get_int_column(command.id,
											DP_DB_TABLE_REQUEST_INFO,
											DP_DB_COL_PROGRESS_EVENT);
								privates->requests[index].request =
									request;
								CLIENT_MUTEX_UNLOCK(&request->mutex);
							}
						}
					}
					if (errorcode != DP_ERROR_NONE)
						break;

					dp_state_type state = DP_STATE_QUEUED;
					if (dp_db_set_column(command.id, DP_DB_TABLE_LOG,
							DP_DB_COL_STATE, DP_DB_COL_TYPE_INT,
							&state) < 0) {
						errorcode = DP_ERROR_OUT_OF_MEMORY;
					} else {
						group->queued_count++;
						CLIENT_MUTEX_LOCK(&request->mutex);
						request->start_time = (int)time(NULL);
						request->pause_time = 0;
						request->stop_time = 0;
						request->state = state;
						request->error = DP_ERROR_NONE;
						CLIENT_MUTEX_UNLOCK(&request->mutex);
						dp_db_update_date(command.id, DP_DB_TABLE_LOG,
							DP_DB_COL_ACCESS_TIME);
						//send signal to queue thread
						dp_thread_queue_manager_wake_up();
					}
					break;
				}
				case DP_CMD_PAUSE:
				{
					// to check fastly, divide the case by request value
					if (request == NULL) {
						dp_state_type state =
							dp_db_get_int_column(command.id,
								DP_DB_TABLE_LOG, DP_DB_COL_STATE);
						// already paused or stopped
						if (state > DP_STATE_DOWNLOADING) {
							errorcode = DP_ERROR_INVALID_STATE;
						} else {
							// change state to paused.
							state = DP_STATE_PAUSED;
							if (dp_db_set_column(command.id,
									DP_DB_TABLE_LOG, DP_DB_COL_STATE,
									DP_DB_COL_TYPE_INT, &state) < 0)
								errorcode = DP_ERROR_OUT_OF_MEMORY;
						}
						break;
					}

					CLIENT_MUTEX_LOCK(&request->mutex);

					if (request->state > DP_STATE_DOWNLOADING) {
						errorcode = DP_ERROR_INVALID_STATE;
						CLIENT_MUTEX_UNLOCK(&request->mutex);
						break;
					}

					// before downloading including QUEUED
					if (__is_downloading(request->state) < 0) {
						dp_state_type state = DP_STATE_PAUSED;
						if (dp_db_set_column(command.id,
								DP_DB_TABLE_LOG, DP_DB_COL_STATE,
								DP_DB_COL_TYPE_INT, &state) < 0) {
							errorcode = DP_ERROR_OUT_OF_MEMORY;
						} else {
							request->state = state;
						}
						CLIENT_MUTEX_UNLOCK(&request->mutex);
						break;
					}
					// only downloading.
					dp_state_type state = DP_STATE_PAUSE_REQUESTED;
					if (dp_db_set_column(command.id, DP_DB_TABLE_LOG,
							DP_DB_COL_STATE, DP_DB_COL_TYPE_INT,
							&state) < 0) {
						errorcode = DP_ERROR_OUT_OF_MEMORY;
					} else {
						TRACE_INFO("[%s][%d]pause_agent(%d) state:%s",
							__print_command(command.cmd), command.id,
							request->agent_id,
							dp_print_state(request->state));
						if (dp_pause_agent_download
								(request->agent_id) < 0) {
							TRACE_INFO("[fail][%d]pause_agent(%d)",
								command.id, request->agent_id);
						}
						request->state = state;
						request->error = DP_ERROR_NONE;
						request->pause_time = (int)time(NULL);
						dp_db_update_date(command.id, DP_DB_TABLE_LOG,
							DP_DB_COL_ACCESS_TIME);
					}
					CLIENT_MUTEX_UNLOCK(&request->mutex);
					break;
				}
				case DP_CMD_CANCEL:
				{
					// to check fastly, divide the case by request value
					if (request == NULL) {
						dp_state_type state =
							dp_db_get_int_column(command.id,
								DP_DB_TABLE_LOG, DP_DB_COL_STATE);
						// already paused or stopped
						if (__is_stopped(state) == 0) {
							errorcode = DP_ERROR_INVALID_STATE;
						} else {
							// change state to canceled.
							state = DP_STATE_CANCELED;
							if (dp_db_set_column(command.id,
									DP_DB_TABLE_LOG, DP_DB_COL_STATE,
									DP_DB_COL_TYPE_INT, &state) < 0)
								errorcode = DP_ERROR_OUT_OF_MEMORY;
						}
						break;
					}

					CLIENT_MUTEX_LOCK(&request->mutex);

					if (__is_stopped(request->state) == 0) {
						errorcode = DP_ERROR_INVALID_STATE;
					} else {
						// change state to canceled.
						dp_state_type state = DP_STATE_CANCELED;
						if (dp_db_set_column(command.id,
								DP_DB_TABLE_LOG, DP_DB_COL_STATE,
								DP_DB_COL_TYPE_INT, &state) < 0)
							errorcode = DP_ERROR_OUT_OF_MEMORY;
					}
					if (errorcode == DP_ERROR_NONE) {
						TRACE_INFO("[%s][%d]cancel_agent(%d) state:%s",
							__print_command(command.cmd), command.id,
							request->agent_id,
							dp_print_state(request->state));
						if (__dp_call_cancel_agent(request) < 0)
							TRACE_INFO("[fail][%d]cancel_agent",
								command.id);
						request->state = DP_STATE_CANCELED;
					}
					CLIENT_MUTEX_UNLOCK(&request->mutex);
					dp_db_update_date(command.id, DP_DB_TABLE_LOG,
							DP_DB_COL_ACCESS_TIME);
					break;
				}
				default:
					is_checked = 0;
					break;
				}
				if (is_checked == 1) {
					if (errorcode != DP_ERROR_NONE) {
						TRACE_ERROR("[ERROR][%d][%s][%s]", command.id,
							__print_command(command.cmd),
							dp_print_errorcode(errorcode));
					}
					if (command.cmd != DP_CMD_FREE) // free no return
						dp_ipc_send_errorcode(sock, errorcode);
					continue;
				}

				// complex read() and write().
				char *read_str2 = NULL;
				errorcode = DP_ERROR_NONE;
				read_str = NULL;
				is_checked = 1;
				switch(command.cmd) {
				case DP_CMD_SET_HTTP_HEADER:
				{
					if ((read_str = dp_ipc_read_string(sock)) == NULL) {
						errorcode = DP_ERROR_IO_ERROR;
						break;
					}
					read_str2 = dp_ipc_read_string(sock);
					if (read_str2 == NULL) {
						errorcode = DP_ERROR_IO_ERROR;
						break;
					}
					int conds_count = 3; // id + field + data
					db_conds_list_fmt conds_p[conds_count];
					memset(&conds_p, 0x00,
						conds_count * sizeof(db_conds_list_fmt));
					conds_p[0].column = DP_DB_COL_ID;
					conds_p[0].type = DP_DB_COL_TYPE_INT;
					conds_p[0].value = &command.id;
					conds_p[1].column = DP_DB_COL_HEADER_FIELD;
					conds_p[1].type = DP_DB_COL_TYPE_TEXT;
					conds_p[1].value = read_str;
					conds_p[2].column = DP_DB_COL_HEADER_DATA;
					conds_p[2].type = DP_DB_COL_TYPE_TEXT;
					conds_p[2].value = read_str2;
					if (dp_db_get_conds_rows_count
							(DP_DB_TABLE_HTTP_HEADERS, DP_DB_COL_ID,
							"AND", 2, conds_p) <= 0) { // insert
						if (dp_db_insert_columns
								(DP_DB_TABLE_HTTP_HEADERS, conds_count,
								conds_p) < 0)
							errorcode = DP_ERROR_OUT_OF_MEMORY;
					} else { // update data by field
						if (dp_db_cond_set_column(command.id,
								DP_DB_TABLE_HTTP_HEADERS,
								DP_DB_COL_HEADER_DATA,
								DP_DB_COL_TYPE_TEXT, read_str2,
								DP_DB_COL_HEADER_FIELD,
								DP_DB_COL_TYPE_TEXT, read_str) < 0)
							errorcode = DP_ERROR_OUT_OF_MEMORY;
					}
					dp_ipc_send_errorcode(sock, errorcode);
					break;
				}
				case DP_CMD_DEL_HTTP_HEADER:
					if ((read_str = dp_ipc_read_string(sock)) == NULL) {
						errorcode = DP_ERROR_IO_ERROR;
						break;
					}
					if (dp_db_get_cond_rows_count(command.id,
							DP_DB_TABLE_HTTP_HEADERS,
							DP_DB_COL_HEADER_FIELD, DP_DB_COL_TYPE_TEXT,
							read_str) < 0) {
						errorcode = DP_ERROR_NO_DATA;
					} else {
						if (dp_db_cond_remove(command.id,
								DP_DB_TABLE_HTTP_HEADERS,
								DP_DB_COL_HEADER_FIELD,
								DP_DB_COL_TYPE_TEXT, read_str) < 0)
							errorcode = DP_ERROR_OUT_OF_MEMORY;
					}
					dp_ipc_send_errorcode(sock, errorcode);
					break;
				case DP_CMD_GET_HTTP_HEADER:
					if ((read_str = dp_ipc_read_string(sock)) == NULL) {
						errorcode = DP_ERROR_IO_ERROR;
						break;
					}
					read_str2 = dp_db_cond_get_text_column(command.id,
						DP_DB_TABLE_HTTP_HEADERS, DP_DB_COL_HEADER_DATA,
						DP_DB_COL_HEADER_FIELD, DP_DB_COL_TYPE_TEXT,
						read_str);
					if (read_str2 == NULL)
						errorcode = DP_ERROR_NO_DATA;
					dp_ipc_send_errorcode(sock, errorcode);
					if (errorcode == DP_ERROR_NONE)
						dp_ipc_send_string(sock, read_str2);
					break;
				case DP_CMD_GET_HTTP_HEADER_LIST:
				{
					char **values = NULL;
					unsigned rows_count = 0;
					errorcode = __dp_get_http_header_fields(sock,
							command.id, &values, &rows_count);
					if (errorcode == DP_ERROR_NONE) {
						__send_return_custom_type(sock, DP_ERROR_NONE,
							&rows_count, sizeof(int));
						// sending strings
						int i = 0;
						for (i = 0; i < rows_count; i++) {
							if (dp_ipc_send_string(sock, values[i]) < 0)
								break;
						}
						for (i = 0; i < rows_count; i++)
							free(values[i]);
					} else {
						if (errorcode != DP_ERROR_IO_ERROR)
							dp_ipc_send_errorcode(sock, errorcode);
					}
					free(values);
					break;
				}
				case DP_CMD_ADD_EXTRA_PARAM:
					errorcode = __dp_add_extra_param(sock, command.id);
					if (errorcode != DP_ERROR_IO_ERROR)
						dp_ipc_send_errorcode(sock, errorcode);
					break;
				case DP_CMD_GET_EXTRA_PARAM:
				{
					char **values = NULL;
					unsigned rows_count = 0;
					errorcode = __dp_get_extra_param_values(sock,
							command.id, &values, &rows_count);
					if (errorcode == DP_ERROR_NONE) {
						__send_return_custom_type(sock, DP_ERROR_NONE,
							&rows_count, sizeof(int));
						// sending strings
						int i = 0;
						for (i = 0; i < rows_count; i++) {
							if (dp_ipc_send_string(sock, values[i]) < 0)
								break;
						}
						for (i = 0; i < rows_count; i++)
							free(values[i]);
					} else {
						if (errorcode != DP_ERROR_IO_ERROR)
							dp_ipc_send_errorcode(sock, errorcode);
					}
					free(values);
					break;
				}
				case DP_CMD_REMOVE_EXTRA_PARAM:
					errorcode =
						__dp_remove_extra_param(sock, command.id);
					if (errorcode != DP_ERROR_IO_ERROR)
						dp_ipc_send_errorcode(sock, errorcode);
					break;
				default: // UNKNOWN COMMAND
					is_checked = 0;
					break;
				}
				if (is_checked == 1) {
					if (errorcode != DP_ERROR_NONE) {
						TRACE_ERROR("[ERROR][%d][%s][%s]", command.id,
							__print_command(command.cmd),
							dp_print_errorcode(errorcode));
						if (errorcode == DP_ERROR_IO_ERROR) {
							FD_CLR(sock, &listen_fdset);
							__clear_group(privates, group);
							privates->groups[i].group = NULL;
						}
					}
					free(read_str);
					free(read_str2);
				} else { // UnKnown Command
					TRACE_INFO("[UNKNOWN][%d] slot:%d pid:%d",
						command.id, i,
						privates->groups[i].group->credential.pid);
					dp_ipc_send_errorcode(sock, DP_ERROR_IO_ERROR);
					// disconnect this group, bad client
					FD_CLR(sock, &listen_fdset);
					__clear_group(privates, group);
					privates->groups[i].group = NULL;
				}

			} // FD_ISSET
		} // DP_MAX_GROUP

		// timeout
		if (is_timeout == 1) {
			int now_timeout = (int)time(NULL);
			TRACE_INFO("[TIMEOUT] prev %ld, now %ld, setted %ld sec",
				prev_timeout, now_timeout, flexible_timeout);
			if (prev_timeout == 0) {
				prev_timeout = now_timeout;
			} else {
				if ((now_timeout - prev_timeout) <
						DP_CARE_CLIENT_MIN_INTERVAL) {
					// this is error.
					// terminate Process
					TRACE_STRERROR
					("[CRITICAL] Sock exception prev[%ld]now[%ld][%ld]",
					prev_timeout, now_timeout, flexible_timeout);
					break;
				}
			}

			// get 48hour old request from log DB
			// clear old request
			dp_db_limit_rows(DP_LOG_DB_LIMIT_ROWS);
			int old_request_count = dp_db_get_count_by_limit_time();
			if (old_request_count > 0) {
				if (old_request_count > DP_LOG_DB_CLEAR_LIMIT_ONE_TIME)
					old_request_count = DP_LOG_DB_CLEAR_LIMIT_ONE_TIME;

				TRACE_INFO
					("[CLEAR] [%d] old reqeusts", old_request_count);

				dp_request_slots *old_requests =
					dp_request_slots_new(old_request_count);
				if (old_requests != NULL) {
					int list_count = dp_db_get_list_by_limit_time
							(old_requests, old_request_count);
					for (i = 0; i < list_count; i++) {
						// search on slots by ID.
						int index = __get_same_request_index
								(privates->requests,
								old_requests[i].request->id);
						if (index >= 0) {
							dp_request *request =
								privates->requests[index].request;
							// if downloading..remain it.
							CLIENT_MUTEX_LOCK(&request->mutex);
							if (__is_downloading(request->state) == 0) {
								CLIENT_MUTEX_UNLOCK(&request->mutex);
								continue;
							}
							CLIENT_MUTEX_UNLOCK(&request->mutex);
							TRACE_INFO("[CLEAR][%d] 48 hour state:%s",
								request->id,
								dp_print_state(request->state));
							// unload from slots ( memory )
							dp_request_free(request);
							privates->requests[index].request = NULL;
						}
						// remove tmp file regardless 
						__dp_remove_tmpfile
							(old_requests[i].request->id,
							old_requests[i].request);
						// remove from DB
						dp_db_remove_all(old_requests[i].request->id);
					}
					dp_request_slots_free(old_requests, old_request_count);
				}
			}

			// clean slots
			int ready_requests = 0;
			for (i = 0; i < DP_MAX_REQUEST; i++) {
				if (privates->requests[i].request == NULL)
					continue;
				dp_request *request = privates->requests[i].request;

				CLIENT_MUTEX_LOCK(&request->mutex);

				// If downloading is too slow ? how to deal this request?
				// can limit too slot download using starttime.(48 hours)
				if (__is_downloading(request->state) == 0) {
					CLIENT_MUTEX_UNLOCK(&request->mutex);
					continue;
				}

				// paused & agent_id not exist.... unload from memory.
				if (request->state == DP_STATE_PAUSED &&
						dp_is_alive_download(request->agent_id) == 0) {
					CLIENT_MUTEX_UNLOCK(&request->mutex);
					TRACE_INFO("[FREE][%d] dead agent-id(%d) state:%s",
						request->id, request->agent_id,
						dp_print_state(request->state));
					dp_request_free(request);
					privates->requests[i].request = NULL;
					continue;
				}

				// client should call START command in 60 sec
				// unload from memory
				if (request->start_time <= 0 &&
						(now_timeout - request->create_time) >
						DP_CARE_CLIENT_MAX_INTERVAL) {
					int download_id = request->id;
					dp_state_type state = DP_STATE_FAILED;
					errorcode = DP_ERROR_RESPONSE_TIMEOUT;
					TRACE_INFO
						("[FREE][%d] start in %d sec state:%s last:%ld",
						download_id, DP_CARE_CLIENT_MAX_INTERVAL,
						dp_print_state(request->state),
						request->start_time);
					if (request->group != NULL &&
							request->state_cb != NULL &&
							request->group->event_socket >= 0)
						dp_ipc_send_event(request->group->event_socket,
							download_id, state, errorcode, 0);
					CLIENT_MUTEX_UNLOCK(&request->mutex);
					dp_request_free(request);
					privates->requests[i].request = NULL;
					// no problem although updating is failed.
					if (dp_db_set_column(download_id, DP_DB_TABLE_LOG,
							DP_DB_COL_STATE, DP_DB_COL_TYPE_INT,
							&state) < 0)
						TRACE_ERROR("[fail][%d][sql] set state:%s",
							download_id, dp_print_state(state));
					if (dp_db_set_column(download_id, DP_DB_TABLE_LOG,
							DP_DB_COL_ERRORCODE, DP_DB_COL_TYPE_INT,
							&errorcode) < 0)
						TRACE_ERROR("[fail][%d][sql] set error:%s",
							download_id, dp_print_errorcode(errorcode));
					continue;
				}

				// client should call DESTROY command in 60 sec after finished
				if (request->stop_time > 0 &&
						(now_timeout - request->stop_time) >
						DP_CARE_CLIENT_MAX_INTERVAL) {
					// check state again. stop_time means it's stopped
					if (__is_stopped(request->state) == 0) {
						CLIENT_MUTEX_UNLOCK(&request->mutex);
						TRACE_INFO
							("[FREE][%d] by timeout state:%s stop:%ld",
							request->id,
							dp_print_state(request->state),
							request->stop_time);
						dp_request_free(request);
						privates->requests[i].request = NULL;
						continue;
					}
				}

				// check after clear
				if (request->state == DP_STATE_QUEUED)
					ready_requests++;

				CLIENT_MUTEX_UNLOCK(&request->mutex);
			}

			if (ready_requests > 0) {
				//send signal to queue thread will check queue.
				dp_thread_queue_manager_wake_up();
			} else {
#ifdef DP_SUPPORT_DBUS_ACTIVATION
				// if no request & timeout is bigger than 60 sec
				// terminate by self.
				if ((now_timeout - prev_timeout) >= flexible_timeout &&
					dp_get_request_count(privates->requests) <= 0) {
					TRACE_INFO("No Request. Terminate Daemon");
					break;
				}
#endif
			}
			prev_timeout = now_timeout;
		} // timeout
	}
	TRACE_INFO("terminate main thread ...");
	dp_terminate(SIGTERM);
	pthread_exit(NULL);
	return 0;
}
