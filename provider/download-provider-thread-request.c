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
#include <sys/types.h>

#include <signal.h>

#include <app_manager.h>
#include <sys/smack.h>
#include <bundle.h>

#include "download-provider.h"
#include "download-provider-log.h"
#include "download-provider-config.h"
#include "download-provider-slots.h"
#include "download-provider-socket.h"
#include "download-provider-pthread.h"
#include "download-provider-db.h"
#include "download-provider-queue.h"
#include "download-provider-request.h"
#include "download-provider-network.h"
#include "download-provider-da-interface.h"
#include "download-provider-notification.h"

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
		case DP_CMD_SET_NOTIFICATION_BUNDLE:
			return "SET_NOTIFICATION_BUNDLE";
		case DP_CMD_SET_NOTIFICATION_TITLE:
			return "SET_NOTIFICATION_TITLE";
		case DP_CMD_SET_NOTIFICATION_DESCRIPTION:
			return "SET_NOTIFICATION_DESCRIPTION";
		case DP_CMD_SET_NOTIFICATION_TYPE:
			return "SET_NOTIFICATION_TYPE";
		case DP_CMD_GET_NOTIFICATION_BUNDLE:
			return "GET_NOTIFICATION_BUNDLE";
		case DP_CMD_GET_NOTIFICATION_TITLE:
			return "GET_NOTIFICATION_TITLE";
		case DP_CMD_GET_NOTIFICATION_DESCRIPTION:
			return "GET_NOTIFICATION_DESCRIPTION";
		case DP_CMD_GET_NOTIFICATION_TYPE:
			return "GET_NOTIFICATION_TYPE";
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

	if (s1 == NULL || s2 == NULL) {
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

	if (slots == NULL)
		return -1;

	for (i = 0; i < DP_MAX_REQUEST; i++) {
		if (slots[i].request == NULL)
			return i;
	}
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
		if (slots[i].request != NULL) {
			if (slots[i].request->id == id)
				return i;
		}
	}
	return -1;
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

	for (i = 0; i < DP_MAX_REQUEST; i++) {

		CLIENT_MUTEX_LOCK(&privates->requests[i].mutex);

		if (privates->requests[i].request == NULL) {
			CLIENT_MUTEX_UNLOCK(&privates->requests[i].mutex);
			continue;
		}

		request = privates->requests[i].request;

		if (request->group == NULL || request->id <= 0 ||
				request->group != group) {
			CLIENT_MUTEX_UNLOCK(&privates->requests[i].mutex);
			continue;
		}

		// if stopped, paused or not started yet, clear request
		if (__is_started(request->state) < 0) {
			TRACE_DEBUG("[FREE][%d] state[%s]", request->id,
				dp_print_state(request->state));
			CLIENT_MUTEX_UNLOCK(&privates->requests[i].mutex);
			dp_request_slot_free(&privates->requests[i]);
			continue;
		}

		// disconnect the request from group.
		TRACE_SECURE_DEBUG("[DISCONNECT][%d] state:%s pkg:%s sock:%d",
			request->id, dp_print_state(request->state),
			request->group->pkgname, request->group->cmd_socket);
		request->group = NULL;
		request->state_cb = 0;
		request->progress_cb = 0;

		CLIENT_MUTEX_UNLOCK(&privates->requests[i].mutex);

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
		TRACE_SECURE_DEBUG("[REMOVE][%d] TEMP FILE [%s]", id, path);
		if (unlink(path) != 0)
			TRACE_STRERROR("[ERROR][%d] remove file", id);
	}
	free(path);
}

static int __dp_check_valid_directory(dp_request *request, char *dir)
{
	int ret = -1;
	int ret_val = 0;
	char *dir_label = NULL;
	struct stat dir_state;

	ret_val = stat(dir, &dir_state);
	if (ret_val == 0) {
		dp_credential cred;
		if (request->group == NULL) {
			cred.uid = dp_db_cond_get_int(DP_DB_TABLE_GROUPS,
						DP_DB_GROUPS_COL_UID,
						DP_DB_GROUPS_COL_PKG,
						DP_DB_COL_TYPE_TEXT, request->packagename);
			cred.gid = dp_db_cond_get_int(DP_DB_TABLE_GROUPS,
						DP_DB_GROUPS_COL_GID,
						DP_DB_GROUPS_COL_PKG,
						DP_DB_COL_TYPE_TEXT, request->packagename);
		} else {
			cred = request->group->credential;
		}
		if (dir_state.st_uid == cred.uid
				&& (dir_state.st_mode & (S_IRUSR | S_IWUSR))
				== (S_IRUSR | S_IWUSR)) {
			ret = 0;
		} else if (dir_state.st_gid == cred.gid
				&& (dir_state.st_mode & (S_IRGRP | S_IWGRP))
				== (S_IRGRP | S_IWGRP)) {
			ret = 0;
		} else if ((dir_state.st_mode & (S_IROTH | S_IWOTH))
				== (S_IROTH | S_IWOTH)) {
			ret = 0;
		}
	}

	if (ret != 0)
		return ret;

	ret_val = smack_getlabel(dir, &dir_label, SMACK_LABEL_ACCESS);
	if (ret_val != 0) {
		TRACE_SECURE_ERROR("[ERROR][%d][SMACK ERROR]", request->id);
		free(dir_label);
		return -1;
	}

	char *smack_label = NULL;
	if (request->group == NULL) {
		// get smack_label from sql
		smack_label = dp_db_cond_get_text(DP_DB_TABLE_GROUPS,
				DP_DB_GROUPS_COL_SMACK_LABEL, DP_DB_GROUPS_COL_PKG,
				DP_DB_COL_TYPE_TEXT, request->packagename);
		if (smack_label != NULL) {
			ret_val = smack_have_access(smack_label, dir_label, "rw");
			free(smack_label);
		}
	} else {
		if (request->group->smack_label != NULL) {
			ret_val = smack_have_access(request->group->smack_label, dir_label, "rw");
		}
	}
	if (ret_val == 0) {
		TRACE_SECURE_ERROR("[ERROR][%d][SMACK NO RULE]", request->id);
		ret = -1;
	} else if (ret_val < 0){
		TRACE_SECURE_ERROR("[ERROR][%d][SMACK ERROR]", request->id);
		ret = -1;
	}
	free(dir_label);
	return ret;
}

static int __dp_call_cancel_agent(dp_request *request)
{
	int ret = -1;
	if (request != NULL) {
		if (request->agent_id >= 0) {
			TRACE_INFO("[%d]cancel_agent(%d) state:%s", request->id,
				request->agent_id, dp_print_state(request->state));
			if (dp_cancel_agent_download(request->agent_id) == 0)
				ret = 0;
		} else {
			TRACE_DEBUG("[CHECK] agent-id");
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
		TRACE_DEBUG("[RECV] length %d", length);
		if (length <= 0) {
			ret = DP_ERROR_INVALID_PARAMETER;
		} else {
			key = dp_ipc_read_string(fd);
			if (key == NULL) {
				TRACE_ERROR("[ERROR][%d][IO_ERROR] key", id);
				ret = DP_ERROR_IO_ERROR;
			} else {
				if (length > 1) {
					TRACE_SECURE_DEBUG("[RECV] key : %s", key);
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
					if (dp_db_is_full_error() == 0) {
						TRACE_ERROR("[SQLITE_FULL][%d]", id);
						ret = DP_ERROR_NO_SPACE;
					} else {
						ret = DP_ERROR_OUT_OF_MEMORY;
					}
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
		TRACE_DEBUG("[RECV] length %d", length);
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
			TRACE_ERROR("[fail][%d][sql]", id);
			TRACE_SECURE_ERROR("[fail]key:%s", key);
			ret = DP_ERROR_OUT_OF_MEMORY;
		}
	}
	TRACE_DEBUG("[ERROR][%d][%s]", id, dp_print_errorcode(ret));
	TRACE_SECURE_DEBUG("key:%s", key);
	free(key);
	return ret;
}

static int __dp_get_http_header_fields(int fd, int id, char ***values,
		unsigned *count)
{
	dp_error_type ret = DP_ERROR_NONE;
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
	char *smack_label = NULL;
	int ret = 0;

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
		TRACE_SECURE_DEBUG("package : %s", pkgname);
	} else
		TRACE_ERROR("[CRITICAL] app_manager_get_package");

	//// TEST CODE ... to allow sample client ( no package name ).
	if (pkgname == NULL) {
		pkgname = dp_strdup("unknown_app");
		TRACE_DEBUG("default package naming : %s", pkgname);
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
				TRACE_SECURE_INFO("UPDATE Group: slot:%d pid:%d sock:%d [%s]",
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

	int conds_count = 4;
	db_conds_list_fmt conds_p[conds_count];
	memset(&conds_p, 0x00, conds_count * sizeof(db_conds_list_fmt));
	conds_p[0].column = DP_DB_GROUPS_COL_UID;
	conds_p[0].type = DP_DB_COL_TYPE_INT;
	conds_p[0].value = &credential.uid;
	conds_p[1].column = DP_DB_GROUPS_COL_GID;
	conds_p[1].type = DP_DB_COL_TYPE_INT;
	conds_p[1].value = &credential.gid;
	conds_p[2].column = DP_DB_GROUPS_COL_PKG;
	conds_p[2].type = DP_DB_COL_TYPE_TEXT;
	conds_p[2].value = pkgname;

	if (dp_is_smackfs_mounted() == 1) {
		ret = smack_new_label_from_socket(clientfd, &smack_label);
		if (ret < 0) {
			TRACE_ERROR("[CRITICAL] cannot get smack label");
			free(pkgname);
			free(smack_label);
			return -1;
		}
		TRACE_SECURE_INFO("credential label:[%s]", smack_label);
		groups[i].group->smack_label = smack_label;

		conds_p[3].column = DP_DB_GROUPS_COL_SMACK_LABEL;
		conds_p[3].type = DP_DB_COL_TYPE_TEXT;
		conds_p[3].value = smack_label;
	} else {
		conds_count = 3; // ignore smack label
		groups[i].group->smack_label = NULL;
	}

	if (dp_db_insert_columns(DP_DB_TABLE_GROUPS, conds_count, conds_p) < 0) {
		free(pkgname);
		free(smack_label);
		if (dp_db_is_full_error() == 0)
			TRACE_ERROR("[SQLITE_FULL]");
		return -1;
	}

	TRACE_SECURE_INFO("New Group: slot:%d pid:%d sock:%d [%s]", i,
		credential.pid, clientfd, pkgname);
	free(pkgname);
	return 0;
}


static int __dp_set_group_event_sock(int clientfd,
	dp_group_slots *groups, dp_credential credential)
{
	int i = 0;

	TRACE_DEBUG("Check event pid:%d sock:%d", credential.pid, clientfd);
	// search same pid in groups
	for (i = 0; i < DP_MAX_GROUP; i++) {
		if (groups[i].group != NULL &&
				groups[i].group->credential.pid == credential.pid) {
			if (groups[i].group->event_socket > 0 &&
					groups[i].group->event_socket != clientfd)
				dp_socket_free(groups[i].group->event_socket);
			groups[i].group->event_socket = clientfd;
			TRACE_SECURE_INFO
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

static dp_error_type __dp_do_get_command(int sock, dp_command* cmd, dp_request *request)
{
	unsigned is_checked = 1;
	dp_error_type errorcode = DP_ERROR_NONE;

	char *read_str = NULL;
	// No read(), write a string
	switch(cmd->cmd) {
	case DP_CMD_GET_URL:
		read_str = dp_request_get_url(cmd->id, &errorcode);
		break;
	case DP_CMD_GET_DESTINATION:
		read_str = dp_request_get_destination(cmd->id, request, &errorcode);
		break;
	case DP_CMD_GET_FILENAME:
		read_str = dp_request_get_filename(cmd->id, request, &errorcode);
		break;
	case DP_CMD_GET_SAVED_PATH:
		read_str = dp_request_get_savedpath(cmd->id, request, &errorcode);
		break;
	case DP_CMD_GET_TEMP_SAVED_PATH:
		read_str = dp_request_get_tmpsavedpath(cmd->id, request, &errorcode);
		break;
	case DP_CMD_GET_MIME_TYPE:
		read_str = dp_request_get_mimetype(cmd->id, request, &errorcode);
		break;
	case DP_CMD_GET_CONTENT_NAME:
		read_str = dp_request_get_contentname(cmd->id, request, &errorcode);
		break;
	case DP_CMD_GET_ETAG:
		read_str = dp_request_get_etag(cmd->id, request, &errorcode);
		break;
	case DP_CMD_GET_NOTIFICATION_TITLE:
		read_str = dp_request_get_title(cmd->id, request, &errorcode);
		break;
	case DP_CMD_GET_NOTIFICATION_DESCRIPTION:
		read_str = dp_request_get_description(cmd->id, request, &errorcode);
		break;
	default:
		is_checked = 0;
		break;
	}
	if (is_checked == 1) {
		if (read_str == NULL || strlen(read_str) < 1)
			errorcode = DP_ERROR_NO_DATA;
		dp_ipc_send_errorcode(sock, errorcode);
		if (errorcode == DP_ERROR_NONE) {
			dp_ipc_send_string(sock, read_str);
		} else {
			TRACE_ERROR("[ERROR][%d][%s][%s]", cmd->id,
				__print_command(cmd->cmd), dp_print_errorcode(errorcode));
		}
		free(read_str);
		return errorcode;
	}

	// No read(), write a integer variable
	int read_int = 0;
	errorcode = DP_ERROR_NONE;
	is_checked = 1;
	switch(cmd->cmd) {
	case DP_CMD_GET_NOTIFICATION:
		read_int = dp_db_get_int_column(cmd->id, DP_DB_TABLE_REQUEST_INFO,
						DP_DB_COL_NOTIFICATION_ENABLE);
		break;
	case DP_CMD_GET_AUTO_DOWNLOAD:
		read_int = dp_db_get_int_column(cmd->id, DP_DB_TABLE_REQUEST_INFO,
						DP_DB_COL_AUTO_DOWNLOAD);
		break;
	case DP_CMD_GET_NETWORK_TYPE:
		read_int = dp_db_get_int_column(cmd->id, DP_DB_TABLE_REQUEST_INFO,
						DP_DB_COL_NETWORK_TYPE);
		break;
	case DP_CMD_GET_HTTP_STATUS:
		read_int = dp_db_get_int_column(cmd->id, DP_DB_TABLE_DOWNLOAD_INFO,
						DP_DB_COL_HTTP_STATUS);
		break;
	case DP_CMD_GET_STATE:
		if (request == NULL) {
			read_int =  dp_db_get_state(cmd->id);
		} else {
			read_int = request->state;
		}
		break;
	case DP_CMD_GET_NOTIFICATION_TYPE:
		TRACE_DEBUG("DP_CMD_GET_NOTIFICATION_TYPE");
		read_int = dp_request_get_noti_type(cmd->id, request, &errorcode);
		break;
	case DP_CMD_GET_ERROR:
		if (request == NULL) {
			read_int = dp_db_get_int_column(cmd->id,
					DP_DB_TABLE_LOG, DP_DB_COL_ERRORCODE);
		} else {
			read_int = request->error;
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
			dp_ipc_send_custom_type(sock, &read_int, sizeof(int));
		} else {
			TRACE_ERROR("[ERROR][%d][%s][%s]", cmd->id,
				__print_command(cmd->cmd), dp_print_errorcode(errorcode));
		}
		return errorcode;
	}

	// No read(), write a long long variable
	unsigned long long recv_long = 0;
	errorcode = DP_ERROR_NONE;
	is_checked = 1;
	switch(cmd->cmd) {
	case DP_CMD_GET_RECEIVED_SIZE:
		if (request == NULL)
			errorcode = DP_ERROR_NO_DATA;
		else
			recv_long = request->received_size;
		break;
	case DP_CMD_GET_TOTAL_FILE_SIZE:
		if (request != NULL) {
			recv_long = request->file_size;
		} else {
			long long file_size =
				dp_db_get_int64_column(cmd->id,
					DP_DB_TABLE_DOWNLOAD_INFO, DP_DB_COL_CONTENT_SIZE);
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
			TRACE_ERROR("[ERROR][%d][%s][%s]", cmd->id,
				__print_command(cmd->cmd), dp_print_errorcode(errorcode));
		}
		return errorcode;
	}

	// No read(), write a bundle variable
	bundle_raw *b_raw = NULL;
	int length = -1;
	char *column = NULL;
	errorcode = DP_ERROR_NONE;
	is_checked = 1;
	switch(cmd->cmd) {
	case DP_CMD_GET_NOTIFICATION_BUNDLE:
		TRACE_DEBUG("DP_CMD_GET_NOTIFICATION_BUNDLE");
		dp_ipc_send_errorcode(sock, DP_ERROR_NONE);
		if ((dp_ipc_read_custom_type(sock, &read_int, sizeof(int)) < 0)) {
			TRACE_ERROR("DP_CMD_GET_NOTIFICATION_BUNDLE read fail");
			errorcode = DP_ERROR_IO_ERROR;
			break;
		}
		switch(read_int) {
		case DP_NOTIFICATION_BUNDLE_TYPE_ONGOING:
			column = DP_DB_COL_RAW_BUNDLE_ONGOING;
			break;
		case DP_NOTIFICATION_BUNDLE_TYPE_COMPLETE:
			column = DP_DB_COL_RAW_BUNDLE_COMPLETE;
			break;
		case DP_NOTIFICATION_BUNDLE_TYPE_FAILED:
			column = DP_DB_COL_RAW_BUNDLE_FAIL;
			break;
		default:
			TRACE_ERROR("[CHECK TYPE][%d]", read_int);
			errorcode = DP_ERROR_INVALID_PARAMETER;
			break;
		}
		b_raw = dp_request_get_bundle(cmd->id, request,
						&errorcode, column, &length);
		break;
	default:
		is_checked = 0;
		break;
	}
	if (is_checked == 1) {
		dp_ipc_send_errorcode(sock, errorcode);
		if (errorcode == DP_ERROR_NONE) {
			dp_ipc_send_bundle(sock, b_raw, length);
		} else {
			TRACE_ERROR("[ERROR][%d][%s][%s]", cmd->id,
				__print_command(cmd->cmd), dp_print_errorcode(errorcode));
		}
		bundle_free_encoded_rawdata(&b_raw);
		return errorcode;
	}

	dp_ipc_send_errorcode(sock, DP_ERROR_NONE);
	// complex read() and write().
	char *read_str2 = NULL;
	errorcode = DP_ERROR_NONE;
	read_str = NULL;
	is_checked = 1;
	switch(cmd->cmd) {
	case DP_CMD_GET_HTTP_HEADER:
		if ((read_str = dp_ipc_read_string(sock)) == NULL) {
			errorcode = DP_ERROR_IO_ERROR;
			break;
		}
		read_str2 = dp_db_cond_get_text_column(cmd->id,
			DP_DB_TABLE_HTTP_HEADERS, DP_DB_COL_HEADER_DATA,
			DP_DB_COL_HEADER_FIELD, DP_DB_COL_TYPE_TEXT, read_str);
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
				cmd->id, &values, &rows_count);
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
	case DP_CMD_GET_EXTRA_PARAM:
	{
		char **values = NULL;
		unsigned rows_count = 0;
		errorcode = __dp_get_extra_param_values(sock,
				cmd->id, &values, &rows_count);
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
	default:
		is_checked = 0;
		break;
	}
	if (is_checked == 1) {
		if (errorcode != DP_ERROR_NONE) {
			TRACE_ERROR("[ERROR][%d][%s][%s]", cmd->id,
				__print_command(cmd->cmd), dp_print_errorcode(errorcode));
		}
		free(read_str);
		free(read_str2);
		return errorcode;
	}
	return DP_ERROR_UNKNOWN;
}

static dp_error_type __dp_do_set_command(int sock, dp_command *cmd, dp_request *request)
{
	unsigned is_checked = 1;
	int read_int = 0;
	dp_error_type errorcode = DP_ERROR_NONE;
	char *read_str = NULL;
	bundle_raw *b_raw = NULL;
	unsigned bundle_length = 0;
	int noti_bundle_type = 0;

	dp_ipc_send_errorcode(sock, DP_ERROR_NONE);
	// read a interger or a string, return errorcode.
	errorcode = DP_ERROR_NONE;
	switch(cmd->cmd) {
	case DP_CMD_SET_STATE_CALLBACK:
		if (dp_ipc_read_custom_type(sock, &read_int, sizeof(int)) < 0) {
			errorcode = DP_ERROR_IO_ERROR;
			break;
		}
		errorcode = dp_request_set_state_event(cmd->id, request, read_int);
		break;
	case DP_CMD_SET_PROGRESS_CALLBACK:
		if (dp_ipc_read_custom_type(sock, &read_int, sizeof(int)) < 0) {
			errorcode = DP_ERROR_IO_ERROR;
			break;
		}
		errorcode = dp_request_set_progress_event(cmd->id, request, read_int);
		break;
	case DP_CMD_SET_NETWORK_TYPE:
		if (dp_ipc_read_custom_type(sock, &read_int, sizeof(int)) < 0) {
			errorcode = DP_ERROR_IO_ERROR;
			break;
		}
		if (read_int == DP_NETWORK_TYPE_ALL ||
				read_int == DP_NETWORK_TYPE_WIFI ||
				read_int == DP_NETWORK_TYPE_DATA_NETWORK ||
				read_int == DP_NETWORK_TYPE_ETHERNET ||
				read_int == DP_NETWORK_TYPE_WIFI_DIRECT)
			errorcode = dp_request_set_network_type(cmd->id, request, read_int);
		else
			errorcode = DP_ERROR_INVALID_PARAMETER;
		break;
	case DP_CMD_SET_AUTO_DOWNLOAD:
		if (dp_ipc_read_custom_type(sock, &read_int, sizeof(int)) < 0) {
			errorcode = DP_ERROR_IO_ERROR;
			break;
		}
		errorcode = dp_request_set_auto_download(cmd->id, request, read_int);
		break;
	case DP_CMD_SET_NOTIFICATION:
		if (dp_ipc_read_custom_type(sock, &read_int, sizeof(int)) < 0) {
			errorcode = DP_ERROR_IO_ERROR;
			break;
		}
		errorcode = dp_request_set_notification(cmd->id, request, read_int);
		break;
	case DP_CMD_SET_URL:
		if ((read_str = dp_ipc_read_string(sock)) == NULL) {
			errorcode = DP_ERROR_IO_ERROR;
			break;
		}
		errorcode = dp_request_set_url(cmd->id, request, read_str);
		break;
	case DP_CMD_SET_DESTINATION:
		if ((read_str = dp_ipc_read_string(sock)) == NULL) {
			errorcode = DP_ERROR_IO_ERROR;
			break;
		}
		if (dp_is_smackfs_mounted() == 1 &&
				__dp_check_valid_directory(request, read_str) != 0){
			errorcode = DP_ERROR_PERMISSION_DENIED;
			break;
		}
		errorcode = dp_request_set_destination(cmd->id, request, read_str);
		break;
	case DP_CMD_SET_FILENAME:
		if ((read_str = dp_ipc_read_string(sock)) == NULL) {
			errorcode = DP_ERROR_IO_ERROR;
			break;
		}
		errorcode = dp_request_set_filename(cmd->id, request, read_str);
		break;
	case DP_CMD_SET_NOTIFICATION_BUNDLE:
		bundle_length = dp_ipc_read_bundle(sock, &noti_bundle_type, &b_raw);
		if (bundle_length == 0) {
			errorcode = DP_ERROR_IO_ERROR;
			break;
		} else if (bundle_length == -1) {
			errorcode = DP_ERROR_INVALID_PARAMETER;
			break;
		}
		errorcode = dp_request_set_bundle(cmd->id, request,
				noti_bundle_type, b_raw, bundle_length);
		break;
	case DP_CMD_SET_NOTIFICATION_TITLE:
		if ((read_str = dp_ipc_read_string(sock)) == NULL) {
			errorcode = DP_ERROR_IO_ERROR;
			break;
		}
		errorcode = dp_request_set_title(cmd->id, request, read_str);
		break;
	case DP_CMD_SET_NOTIFICATION_DESCRIPTION:
		if ((read_str = dp_ipc_read_string(sock)) == NULL) {
			errorcode = DP_ERROR_IO_ERROR;
			break;
		}
		errorcode = dp_request_set_description(cmd->id, request, read_str);
		break;
	case DP_CMD_SET_NOTIFICATION_TYPE:
		if ((dp_ipc_read_custom_type(sock, &read_int, sizeof(int)) < 0)) {
			errorcode = DP_ERROR_IO_ERROR;
			break;
		}
		if (read_int == DP_NOTIFICATION_TYPE_NONE ||
				read_int == DP_NOTIFICATION_TYPE_COMPLETE_ONLY ||
				read_int == DP_NOTIFICATION_TYPE_ALL)
			errorcode = dp_request_set_noti_type(cmd->id, request, read_int);
		else
			errorcode = DP_ERROR_INVALID_PARAMETER;
		break;
	default:
		is_checked = 0;
		break;
	}
	if (is_checked == 1) {
		free(read_str);
		bundle_free_encoded_rawdata(&b_raw);
		if (errorcode != DP_ERROR_NONE) {
			TRACE_ERROR("[ERROR][%d][%s][%s]", cmd->id,
				__print_command(cmd->cmd), dp_print_errorcode(errorcode));
		}
		if (errorcode == DP_ERROR_IO_ERROR)
			return errorcode;
		dp_ipc_send_errorcode(sock, errorcode);
		return errorcode;
	}

	// complex read() and write().
	char *read_str2 = NULL;
	errorcode = DP_ERROR_NONE;
	read_str = NULL;
	is_checked = 1;
	switch(cmd->cmd) {
	case DP_CMD_SET_HTTP_HEADER:
	{
		if ((read_str = dp_ipc_read_string(sock)) == NULL) {
			errorcode = DP_ERROR_IO_ERROR;
			break;
		}
		if ((read_str2 = dp_ipc_read_string(sock)) == NULL) {
			errorcode = DP_ERROR_IO_ERROR;
			break;
		}
		int conds_count = 3; // id + field + data
		db_conds_list_fmt conds_p[conds_count];
		memset(&conds_p, 0x00,
		conds_count * sizeof(db_conds_list_fmt));
		conds_p[0].column = DP_DB_COL_ID;
		conds_p[0].type = DP_DB_COL_TYPE_INT;
		conds_p[0].value = &cmd->id;
		conds_p[1].column = DP_DB_COL_HEADER_FIELD;
		conds_p[1].type = DP_DB_COL_TYPE_TEXT;
		conds_p[1].value = read_str;
		conds_p[2].column = DP_DB_COL_HEADER_DATA;
		conds_p[2].type = DP_DB_COL_TYPE_TEXT;
		conds_p[2].value = read_str2;
		if (dp_db_get_conds_rows_count(DP_DB_TABLE_HTTP_HEADERS,
				DP_DB_COL_ID, "AND", 2, conds_p) <= 0) { // insert
			if (dp_db_insert_columns(DP_DB_TABLE_HTTP_HEADERS,
					conds_count, conds_p) < 0) {
				if (dp_db_is_full_error() == 0) {
					TRACE_ERROR("[SQLITE_FULL][%d]", cmd->id);
					errorcode = DP_ERROR_NO_SPACE;
				} else {
					errorcode = DP_ERROR_OUT_OF_MEMORY;
				}
			}
		} else { // update data by field
			if (dp_db_cond_set_column(cmd->id, DP_DB_TABLE_HTTP_HEADERS,
					DP_DB_COL_HEADER_DATA, DP_DB_COL_TYPE_TEXT, read_str2,
					DP_DB_COL_HEADER_FIELD, DP_DB_COL_TYPE_TEXT, read_str) < 0) {
				if (dp_db_is_full_error() == 0) {
					TRACE_ERROR("[SQLITE_FULL][%d]", cmd->id);
					errorcode = DP_ERROR_NO_SPACE;
				} else {
					errorcode = DP_ERROR_OUT_OF_MEMORY;
				}
			}
		}
		dp_ipc_send_errorcode(sock, errorcode);
		break;
	}
	case DP_CMD_DEL_HTTP_HEADER:
		if ((read_str = dp_ipc_read_string(sock)) == NULL) {
			errorcode = DP_ERROR_IO_ERROR;
			break;
		}
		if (dp_db_get_cond_rows_count(cmd->id, DP_DB_TABLE_HTTP_HEADERS,
				DP_DB_COL_HEADER_FIELD, DP_DB_COL_TYPE_TEXT,
				read_str) <= 0) {
			errorcode = DP_ERROR_NO_DATA;
		} else {
			if (dp_db_cond_remove(cmd->id, DP_DB_TABLE_HTTP_HEADERS,
					DP_DB_COL_HEADER_FIELD, DP_DB_COL_TYPE_TEXT, read_str) < 0)
				errorcode = DP_ERROR_OUT_OF_MEMORY;
		}
		dp_ipc_send_errorcode(sock, errorcode);
		break;
	case DP_CMD_ADD_EXTRA_PARAM:
		errorcode = __dp_add_extra_param(sock, cmd->id);
		if (errorcode != DP_ERROR_IO_ERROR)
			dp_ipc_send_errorcode(sock, errorcode);
		break;
	case DP_CMD_REMOVE_EXTRA_PARAM:
		errorcode = __dp_remove_extra_param(sock, cmd->id);
		if (errorcode != DP_ERROR_IO_ERROR)
			dp_ipc_send_errorcode(sock, errorcode);
		break;
	default:
		is_checked = 0;
		break;
	}
	if (is_checked == 1) {
		if (errorcode != DP_ERROR_NONE) {
			TRACE_ERROR("[ERROR][%d][%s][%s]", cmd->id,
				__print_command(cmd->cmd), dp_print_errorcode(errorcode));
		}
		free(read_str);
		free(read_str2);
		return errorcode;
	}
	return DP_ERROR_UNKNOWN;
}

static dp_error_type __dp_do_action_command(int sock, dp_command* cmd, dp_request *request)
{
	unsigned is_checked = 1;
	int read_int = 0;
	dp_error_type errorcode = DP_ERROR_NONE;

	read_int = 0;
	errorcode = DP_ERROR_NONE;
	is_checked = 1;
	switch(cmd->cmd) {
	case DP_CMD_DESTROY:
		if (request != NULL) {// just update the state
			if (__is_started(request->state) == 0) {
				read_int = DP_STATE_CANCELED;
				if (dp_db_set_column(cmd->id, DP_DB_TABLE_LOG, DP_DB_COL_STATE,
					DP_DB_COL_TYPE_INT, &read_int) < 0) {
					errorcode = DP_ERROR_OUT_OF_MEMORY;
				} else {
					if (__dp_call_cancel_agent(request) < 0)
						TRACE_ERROR("[fail][%d]cancel_agent", cmd->id);
					request->state = DP_STATE_CANCELED;
					if (request->auto_notification == 1 &&
									request->packagename != NULL) {
						request->noti_priv_id = dp_set_downloadedinfo_notification
								(request->noti_priv_id, request->id,
								request->packagename, request->state);
					} else {
						int noti_type = dp_db_get_int_column(request->id,
								DP_DB_TABLE_NOTIFICATION, DP_DB_COL_NOTI_TYPE);
						if (noti_type > DP_NOTIFICATION_TYPE_NONE &&
								request->packagename != NULL)
							request->noti_priv_id = dp_set_downloadedinfo_notification
									(request->noti_priv_id, request->id,
									request->packagename, request->state);
					}
				}
			}
		}
		break;
	case DP_CMD_PAUSE:
	{
		// to check fastly, divide the case by request value
		if (request == NULL) {
			dp_state_type state = dp_db_get_state(cmd->id);
			// already paused or stopped
			if (state > DP_STATE_DOWNLOADING) {
				errorcode = DP_ERROR_INVALID_STATE;
			} else {
				// change state to paused.
				state = DP_STATE_PAUSED;
				if (dp_db_set_column(cmd->id, DP_DB_TABLE_LOG,
						DP_DB_COL_STATE, DP_DB_COL_TYPE_INT, &state) < 0)
					errorcode = DP_ERROR_OUT_OF_MEMORY;
			}
			break;
		}

		if (request->state > DP_STATE_DOWNLOADING) {
			errorcode = DP_ERROR_INVALID_STATE;
			break;
		}

		// before downloading including QUEUED
		if (__is_downloading(request->state) < 0) {
			dp_state_type state = DP_STATE_PAUSED;
			if (dp_db_set_column(cmd->id, DP_DB_TABLE_LOG, DP_DB_COL_STATE,
					DP_DB_COL_TYPE_INT, &state) < 0) {
				errorcode = DP_ERROR_OUT_OF_MEMORY;
			} else {
				request->state = state;
			}
			break;
		}
		// only downloading.
		dp_state_type state = DP_STATE_PAUSE_REQUESTED;
		if (dp_db_set_column(cmd->id, DP_DB_TABLE_LOG,
				DP_DB_COL_STATE, DP_DB_COL_TYPE_INT, &state) < 0) {
			errorcode = DP_ERROR_OUT_OF_MEMORY;
		} else {
			TRACE_INFO("[%s][%d]pause_agent(%d) state:%s",
				__print_command(cmd->cmd), cmd->id,
				request->agent_id,
				dp_print_state(request->state));
			if (dp_pause_agent_download
					(request->agent_id) < 0) {
				TRACE_ERROR("[fail][%d]pause_agent(%d)",
					cmd->id, request->agent_id);
			}
			request->state = state;
			request->error = DP_ERROR_NONE;
			request->pause_time = (int)time(NULL);
			dp_db_update_date(cmd->id, DP_DB_TABLE_LOG,
				DP_DB_COL_ACCESS_TIME);
		}
		break;
	}
	case DP_CMD_CANCEL:
	{
		// to check fastly, divide the case by request value
		if (request == NULL) {
			dp_state_type state = dp_db_get_state(cmd->id);
			// already paused or stopped
			if (__is_stopped(state) == 0) {
				errorcode = DP_ERROR_INVALID_STATE;
			} else {
				// change state to canceled.
				state = DP_STATE_CANCELED;
				if (dp_db_set_column(cmd->id, DP_DB_TABLE_LOG,
						DP_DB_COL_STATE, DP_DB_COL_TYPE_INT, &state) < 0)
					errorcode = DP_ERROR_OUT_OF_MEMORY;
			}
			break;
		}

		if (__is_stopped(request->state) == 0) {
			TRACE_ERROR("[%s][%d]__is_stopped agentid:%d state:%s",
				__print_command(cmd->cmd), cmd->id,
				request->agent_id, dp_print_state(request->state));
			errorcode = DP_ERROR_INVALID_STATE;
		} else {
			// change state to canceled.
			dp_state_type state = DP_STATE_CANCELED;
			if (dp_db_set_column(cmd->id, DP_DB_TABLE_LOG, DP_DB_COL_STATE,
					DP_DB_COL_TYPE_INT, &state) < 0)
				errorcode = DP_ERROR_OUT_OF_MEMORY;
		}
		if (errorcode == DP_ERROR_NONE) {
			if (__dp_call_cancel_agent(request) < 0)
				TRACE_ERROR("[fail][%d]cancel_agent", cmd->id);
			request->state = DP_STATE_CANCELED;
		}
		dp_db_update_date(cmd->id, DP_DB_TABLE_LOG, DP_DB_COL_ACCESS_TIME);
		break;
	}
	default:
		is_checked = 0;
		break;
	}
	if (is_checked == 1) {
		if (errorcode != DP_ERROR_NONE) {
			TRACE_ERROR("[ERROR][%d][%s][%s]", cmd->id,
				__print_command(cmd->cmd), dp_print_errorcode(errorcode));
		}
		dp_ipc_send_errorcode(sock, errorcode);
		return errorcode;
	}
	return DP_ERROR_UNKNOWN;
}

static dp_error_type __do_dp_start_command(int sock, int id, dp_privates *privates,
	dp_client_group *group, dp_request *request)
{
	dp_error_type errorcode = DP_ERROR_NONE;

	// need URL at least
	char *url = dp_request_get_url(id, &errorcode);
	free(url);
	if (errorcode == DP_ERROR_NO_DATA) {
		errorcode = DP_ERROR_INVALID_URL;
		dp_ipc_send_errorcode(sock, errorcode);
		return errorcode;
	}

	if (request == NULL) { // Support Re-download
		// Support Re-download
		int index = __get_empty_request_index(privates->requests);
		if (index < 0) { // Busy, No Space in slot
			errorcode = DP_ERROR_QUEUE_FULL;
			TRACE_SECURE_ERROR("[ERROR][%d][RESTORE][%s]", id,
				dp_print_errorcode(errorcode));
		} else {
			CLIENT_MUTEX_LOCK(&privates->requests[index].mutex);
			dp_request *tmp_request =
				dp_request_load_from_log (id, &errorcode);
			if (tmp_request != NULL) {
				// restore callback info
				tmp_request->state_cb = dp_db_get_int_column(id,
						DP_DB_TABLE_REQUEST_INFO, DP_DB_COL_STATE_EVENT);
				tmp_request->progress_cb = dp_db_get_int_column(id,
						DP_DB_TABLE_REQUEST_INFO, DP_DB_COL_PROGRESS_EVENT);
				tmp_request->auto_notification =
					dp_db_get_int_column(id, DP_DB_TABLE_REQUEST_INFO,
						DP_DB_COL_NOTIFICATION_ENABLE);
				if (group != NULL) // for send event to client
					tmp_request->group = group;
				privates->requests[index].request = tmp_request;
				request = privates->requests[index].request;
			} else {
				TRACE_SECURE_ERROR("[ERROR][%d][RESTORE][%s]", id,
					dp_print_errorcode(errorcode));
				dp_ipc_send_errorcode(sock, errorcode);
				CLIENT_MUTEX_UNLOCK(&privates->requests[index].mutex);
				return errorcode;
			}
			CLIENT_MUTEX_UNLOCK(&privates->requests[index].mutex);
		}
	}

	// check status
	if (errorcode == DP_ERROR_NONE &&
			(__is_started(request->state) == 0 ||
			request->state == DP_STATE_COMPLETED))
		errorcode = DP_ERROR_INVALID_STATE;

	if (errorcode == DP_ERROR_NONE) {

		dp_state_type state = DP_STATE_QUEUED;
		if (dp_db_set_column(id, DP_DB_TABLE_LOG,
				DP_DB_COL_STATE, DP_DB_COL_TYPE_INT, &state) < 0) {
			errorcode = DP_ERROR_OUT_OF_MEMORY;
		} else {
			if (group != NULL)
				group->queued_count++;
			request->start_time = (int)time(NULL);
			request->pause_time = 0;
			request->stop_time = 0;
			request->state = state;
			request->error = DP_ERROR_NONE;
			dp_db_update_date(id, DP_DB_TABLE_LOG, DP_DB_COL_ACCESS_TIME);
		}

	}

	dp_ipc_send_errorcode(sock, errorcode);
	if (errorcode != DP_ERROR_NONE) {
		TRACE_SECURE_ERROR("[ERROR][%d][START][%s]", id,
			dp_print_errorcode(errorcode));
	}
	return errorcode;
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

	TRACE_DEBUG("Ready to listen [%d][%s]", listenfd, DP_IPC);

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

		if (privates == NULL || privates->listen_fd < 0) {
			TRACE_DEBUG("Terminate Thread");
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
				TRACE_DEBUG
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
			if (dp_ipc_read_custom_type(clientfd,
					&credential.uid, sizeof(int)) < 0) {
				TRACE_ERROR("[CRITICAL] not support SO_PEERCRED");
				close(clientfd);
				continue;
			}
			if (dp_ipc_read_custom_type(clientfd,
					&credential.gid, sizeof(int)) < 0) {
				TRACE_ERROR("[CRITICAL] not support SO_PEERCRED");
				close(clientfd);
				continue;
			}
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
			int time_of_job = (int)time(NULL);
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

				if (command.cmd == 0 || command.cmd >= DP_CMD_LAST_SECT) { // Client meet some Error.
					TRACE_SECURE_INFO("[Closed Peer] pkg:%s sock:%d slot:%d",
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
					TRACE_DEBUG("[ECHO] sock:%d", sock);
					if (dp_ipc_send_errorcode(sock,
							DP_ERROR_NONE) < 0) {
						// disconnect this group, bad client
						TRACE_ERROR("[ECHO] IO ERROR CLEAN sock:%d", sock);
						FD_CLR(sock, &listen_fdset);
						__clear_group(privates, group);
						privates->groups[i].group = NULL;
					}
					continue;
				}

				if (command.cmd == DP_CMD_CREATE) {
					TRACE_SECURE_INFO("[CREATE] id:%d sock:%d pid:%d gindex:%d pkg:%s",
						command.id, sock,
						group->credential.pid, i, group->pkgname);
					// search empty slot in privates->requests
					index =
						__get_empty_request_index(privates->requests);
					if (index < 0) {
						TRACE_ERROR("[CHECK] [DP_ERROR_QUEUE_FULL]");
						// Busy, No Space in slot
						dp_ipc_send_errorcode(sock, DP_ERROR_QUEUE_FULL);
					} else {
						errorcode = DP_ERROR_NONE;
						CLIENT_MUTEX_LOCK(&privates->requests[index].mutex);
						errorcode = dp_request_create(command.id, group,
							&privates->requests[index].request);
						dp_ipc_send_errorcode(sock, errorcode);
						if (errorcode == DP_ERROR_NONE) {
							dp_ipc_send_custom_type(sock,
								&privates->requests[index].request->id,
								sizeof(int));
							TRACE_DEBUG("[CREATE] GOOD id:%d slot:%d time:%d",
								privates->requests[index].request->id,
								index, ((int)time(NULL) - time_of_job));
						} else {
							TRACE_ERROR("[ERROR][%s]",
								dp_print_errorcode(errorcode));
						}
						CLIENT_MUTEX_UNLOCK(&privates->requests[index].mutex);
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

				dp_request_slots *request_slot = NULL;
				unsigned is_loaded = 0;
				errorcode = DP_ERROR_NONE;

				if (index >= 0) {
					CLIENT_MUTEX_LOCK(&privates->requests[index].mutex);
					if (privates->requests[index].request != NULL)
						is_loaded = 1;
					else
						CLIENT_MUTEX_UNLOCK(&privates->requests[index].mutex);
				}

				errorcode = DP_ERROR_UNKNOWN; // check matched command

				// divide on memory or from DB
				if (is_loaded == 1 && index >= 0) { // already locked

					TRACE_SECURE_INFO("[%s] id:%d sock:%d pid:%d gindex:%d slot:%d pkg:%s",
						__print_command(command.cmd), command.id, sock,
						group->credential.pid, i, index, group->pkgname);

					request_slot = &privates->requests[index];

					// auth by pkgname
					if (__cmp_string(group->pkgname, request_slot->request->packagename) < 0) {
						TRACE_SECURE_ERROR("[ERROR][%d] Auth [%s]/[%s]",
							command.id, group->pkgname, request_slot->request->packagename);
						TRACE_ERROR("[ERROR][%d][INVALID_PARAMETER]", command.id);
						dp_ipc_send_errorcode(sock, DP_ERROR_INVALID_PARAMETER);
						CLIENT_MUTEX_UNLOCK(&request_slot->mutex);
						continue;
					}

					// if no group, update group.
					if (request_slot->request->group == NULL)
						request_slot->request->group = group;

					if (command.cmd == DP_CMD_START)
						errorcode = __do_dp_start_command(sock, command.id, privates, group, request_slot->request);
					else if (command.cmd > DP_CMD_SET_SECT && command.cmd < DP_CMD_LAST_SECT)
						errorcode = __dp_do_set_command(sock, &command, request_slot->request);
					else if (command.cmd > DP_CMD_GET_SECT && command.cmd < DP_CMD_SET_SECT)
						errorcode = __dp_do_get_command(sock, &command, request_slot->request);
					else if (command.cmd > DP_CMD_ACTION_SECT && command.cmd < DP_CMD_GET_SECT)
						errorcode = __dp_do_action_command(sock, &command, request_slot->request);

					CLIENT_MUTEX_UNLOCK(&request_slot->mutex);

					if (command.cmd == DP_CMD_START && errorcode == DP_ERROR_NONE) {
						//send signal to queue thread
						dp_thread_queue_manager_wake_up();
					}
					if (command.cmd == DP_CMD_FREE) {// enter after unlock
						dp_request_slot_free(request_slot);
						errorcode = DP_ERROR_NONE;
					}

				} else { // not found on the slots

					TRACE_SECURE_INFO("[%s] id:%d sock:%d pid:%d gindex:%d slot:no pkg:%s",
						__print_command(command.cmd), command.id, sock,
						group->credential.pid, i, group->pkgname);

					int check_id = dp_db_get_int_column(command.id,
							DP_DB_TABLE_LOG, DP_DB_COL_ID);
					if (check_id < 0 || check_id != command.id) {
						errorcode = DP_ERROR_ID_NOT_FOUND;
						TRACE_ERROR("[ERROR][%d][ID_NOT_FOUND]", command.id);
						dp_ipc_send_errorcode(sock, DP_ERROR_ID_NOT_FOUND);
						continue;
					}
					// auth by pkgname
					char *auth_pkgname = dp_db_get_text_column(command.id,
							DP_DB_TABLE_LOG, DP_DB_COL_PACKAGENAME);
					if (auth_pkgname == NULL) {
						TRACE_ERROR("[ERROR][%d][ID_NOT_FOUND]", command.id);
						dp_ipc_send_errorcode(sock, DP_ERROR_ID_NOT_FOUND);
						continue;
					}
					// auth by pkgname
					if (__cmp_string(group->pkgname, auth_pkgname) < 0) {
						TRACE_SECURE_ERROR("[ERROR][%d] Auth [%s]/[%s]",
							command.id, group->pkgname, auth_pkgname);
						TRACE_ERROR("[ERROR][%d][INVALID_PARAMETER]", command.id);
						dp_ipc_send_errorcode(sock, DP_ERROR_INVALID_PARAMETER);
						free(auth_pkgname);
						continue;
					}
					free(auth_pkgname);

					if (command.cmd == DP_CMD_START)
						errorcode = __do_dp_start_command(sock, command.id, privates, group, NULL);
					else if (command.cmd == DP_CMD_FREE)
						errorcode = DP_ERROR_NONE;
					else if (command.cmd > DP_CMD_SET_SECT && command.cmd < DP_CMD_LAST_SECT)
						errorcode = __dp_do_set_command(sock, &command, NULL);
					else if (command.cmd > DP_CMD_GET_SECT && command.cmd < DP_CMD_SET_SECT)
						errorcode = __dp_do_get_command(sock, &command, NULL);
					else if (command.cmd > DP_CMD_ACTION_SECT && command.cmd < DP_CMD_GET_SECT)
						errorcode = __dp_do_action_command(sock, &command, NULL);

					if (command.cmd == DP_CMD_START && errorcode == DP_ERROR_NONE) {
						//send signal to queue thread
						dp_thread_queue_manager_wake_up();
					}
				}

				if (errorcode == DP_ERROR_IO_ERROR) {
					TRACE_ERROR("[IO_ERROR][%d] slot:%d pid:%d",
						command.id, i,
						privates->groups[i].group->credential.pid);
					FD_CLR(sock, &listen_fdset);
					__clear_group(privates, group);
					privates->groups[i].group = NULL;
					continue;
				}
				if (errorcode == DP_ERROR_UNKNOWN) { // UnKnown Command
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
			TRACE_DEBUG("[TIMEOUT] prev %ld, now %ld, setted %ld sec",
				prev_timeout, now_timeout, flexible_timeout);
			if (prev_timeout == 0) {
				prev_timeout = now_timeout;
			} else if (now_timeout < prev_timeout ||
				(now_timeout - prev_timeout) > flexible_timeout) {
				TRACE_ERROR("[WARN] check system date prev[%ld]now[%ld]",
					prev_timeout, now_timeout);
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

				TRACE_DEBUG
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
							CLIENT_MUTEX_LOCK(&privates->requests[index].mutex);
							dp_request *request =
								privates->requests[index].request;
							// if downloading..remain it.
							if (request == NULL || __is_downloading(request->state) == 0) {
								CLIENT_MUTEX_UNLOCK(&privates->requests[index].mutex);
								continue;
							}
							CLIENT_MUTEX_UNLOCK(&privates->requests[index].mutex);
							TRACE_DEBUG("[CLEAR][%d] 48 hour state:%s",
								request->id,
								dp_print_state(request->state));
							// unload from slots ( memory )
							dp_request_slot_free(&privates->requests[index]);
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
				CLIENT_MUTEX_LOCK(&privates->requests[i].mutex);
				if (privates->requests[i].request == NULL) {
					CLIENT_MUTEX_UNLOCK(&privates->requests[i].mutex);
					continue;
				}
				dp_request *request = privates->requests[i].request;

				// If downloading is too slow ? how to deal this request?
				// can limit too slot download using starttime.(48 hours)
				if (__is_downloading(request->state) == 0) {
					CLIENT_MUTEX_UNLOCK(&privates->requests[i].mutex);
					continue;
				}

				// paused & agent_id not exist.... unload from memory.
				if (request->state == DP_STATE_PAUSED &&
						dp_is_alive_download(request->agent_id) == 0) {
					TRACE_DEBUG("[FREE][%d] dead agent-id(%d) state:%s",
						request->id, request->agent_id,
						dp_print_state(request->state));
					CLIENT_MUTEX_UNLOCK(&privates->requests[i].mutex);
					dp_request_slot_free(&privates->requests[i]);
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
					TRACE_DEBUG
						("[FREE][%d] start in %d sec state:%s last:%ld",
						download_id, DP_CARE_CLIENT_MAX_INTERVAL,
						dp_print_state(request->state),
						request->start_time);
					if (request->group != NULL &&
							request->state_cb == 1 &&
							request->group->event_socket >= 0)
						dp_ipc_send_event(request->group->event_socket,
							download_id, state, errorcode, 0);
					CLIENT_MUTEX_UNLOCK(&privates->requests[i].mutex);
					dp_request_slot_free(&privates->requests[i]);
					// no problem although updating is failed.
					if (dp_db_request_update_status(download_id, state, errorcode) < 0) {
						TRACE_ERROR("[fail][%d][sql] set state:%s error:%s",
							download_id, dp_print_state(state), dp_print_errorcode(errorcode));
					}
					continue;
				}

				// stopped by ipchanged. decide auto resume
				if (request->stop_time <= 0 &&
						request->state == DP_STATE_FAILED &&
						request->error == DP_ERROR_CONNECTION_FAILED) {
					if (dp_get_network_connection_instant_status() !=
							DP_NETWORK_TYPE_OFF &&
							request->ip_changed == 1) {
						TRACE_DEBUG("[RESUME][%d] will be queued",
							request->id);
						request->state = DP_STATE_QUEUED;
						request->error = DP_ERROR_NONE;
						ready_requests++;
					} else {
						dp_request_state_response(request);
					}
				}

				// client should call DESTROY command in MAX_INTERVAL sec after finished
				if (request->stop_time > 0 &&
						(now_timeout - request->stop_time) >
						DP_CARE_CLIENT_MAX_INTERVAL) {
					// check state again. stop_time means it's stopped
					if (__is_stopped(request->state) == 0) {
						TRACE_DEBUG
							("[FREE][%d] by timeout state:%s stop:%ld",
							request->id,
							dp_print_state(request->state),
							request->stop_time);
						CLIENT_MUTEX_UNLOCK(&privates->requests[i].mutex);
						dp_request_slot_free(&privates->requests[i]);
						continue;
					}
				}

				// check after clear
				if (request->state == DP_STATE_QUEUED)
					ready_requests++;

				CLIENT_MUTEX_UNLOCK(&privates->requests[i].mutex);
			}

			if (ready_requests > 0) {
				//send signal to queue thread will check queue.
				dp_thread_queue_manager_wake_up();
			} else {
				// if no request & timeout is bigger than 60 sec
				// terminate by self.
				if ((now_timeout - prev_timeout) >= flexible_timeout &&
					dp_get_request_count(privates->requests) <= 0) {
					TRACE_DEBUG("No Request. Terminate Daemon");
					break;
				}
			}
			prev_timeout = now_timeout;
		} else {
			prev_timeout = 0;
		}
	}
	TRACE_DEBUG("terminate main thread ...");
	dp_terminate(SIGTERM);
	pthread_exit(NULL);
	return 0;
}
