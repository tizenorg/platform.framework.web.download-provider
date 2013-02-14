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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include "download-provider.h"
#include "download-provider-log.h"
#include "download-provider-pthread.h"
#include "download-provider-socket.h"
#include "download-provider-db.h"
#include "download-provider-queue.h"
#include "download-provider-notification.h"
#include "download-provider-request.h"

#include "download-agent-defs.h"
#include "download-agent-interface.h"

int dp_is_file_exist(const char *file_path)
{
	struct stat file_state;
	int stat_ret;

	if (file_path == NULL) {
		TRACE_ERROR("[NULL-CHECK] file path is NULL");
		return -1;
	}

	stat_ret = stat(file_path, &file_state);

	if (stat_ret == 0)
		if (file_state.st_mode & S_IFREG)
			return 0;

	return -1;
}

static int __change_error(int err)
{
	int ret = DP_ERROR_NONE;
	switch (err) {
	case DA_RESULT_OK:
		ret = DP_ERROR_NONE;
		break;
	case DA_ERR_INVALID_ARGUMENT:
		ret = DP_ERROR_INVALID_PARAMETER;
		break;
	case DA_ERR_FAIL_TO_MEMALLOC:
		ret = DP_ERROR_OUT_OF_MEMORY;
		break;
	case DA_ERR_UNREACHABLE_SERVER:
		ret = DP_ERROR_NETWORK_UNREACHABLE;
		break;
	case DA_ERR_HTTP_TIMEOUT:
		ret = DP_ERROR_CONNECTION_TIMED_OUT;
		break;
	case DA_ERR_DISK_FULL:
		ret = DP_ERROR_NO_SPACE;
		break;
	case DA_ERR_INVALID_STATE:
		ret = DP_ERROR_INVALID_STATE;
		break;
	case DA_ERR_NETWORK_FAIL:
		ret = DP_ERROR_CONNECTION_FAILED;
		break;
	case DA_ERR_INVALID_URL:
		ret = DP_ERROR_INVALID_URL;
		break;
	case DA_ERR_INVALID_INSTALL_PATH:
		ret = DP_ERROR_INVALID_DESTINATION;
		break;
	case DA_ERR_ALREADY_MAX_DOWNLOAD:
		ret = DP_ERROR_TOO_MANY_DOWNLOADS;
		break;
	case DA_ERR_FAIL_TO_CREATE_THREAD:
	case DA_ERR_FAIL_TO_OBTAIN_MUTEX:
	case DA_ERR_FAIL_TO_ACCESS_FILE:
	case DA_ERR_FAIL_TO_GET_CONF_VALUE:
	case DA_ERR_FAIL_TO_ACCESS_STORAGE:
		ret = DP_ERROR_IO_ERROR;
		break;
	}
	return ret;
}

static void __download_info_cb(user_download_info_t *info, void *user_data)
{
	if (!info) {
		TRACE_ERROR("[NULL-CHECK] Agent info");
		return ;
	}
	if (!user_data) {
		TRACE_ERROR("[NULL-CHECK] user_data");
		return ;
	}
	dp_request *request = (dp_request *) user_data;
	if (request->id < 0 || (request->agent_id != info->download_id)) {
		TRACE_ERROR("[NULL-CHECK] agent_id : %d req_id %d",
			request->agent_id, info->download_id);
		return ;
	}

	int request_id = request->id;

	// update info before sending event
	if (info->file_type) {
		TRACE_INFO("[STARTED][%d] [%s]", request_id, info->file_type);
		if (dp_db_replace_column(request_id, DP_DB_TABLE_DOWNLOAD_INFO,
				DP_DB_COL_MIMETYPE,
				DP_DB_COL_TYPE_TEXT, info->file_type) == 0) {

			if (info->tmp_saved_path) {
				TRACE_INFO("[PATH][%d] being written to [%s]",
					request_id, info->tmp_saved_path);
				if (dp_db_set_column
						(request_id, DP_DB_TABLE_DOWNLOAD_INFO,
						DP_DB_COL_TMP_SAVED_PATH, DP_DB_COL_TYPE_TEXT,
						info->tmp_saved_path) < 0)
					TRACE_ERROR("[ERROR][%d][SQL]", request_id);
			}

			if (info->file_size > 0) {
				TRACE_INFO
					("[FILE-SIZE][%d] [%lld]", request_id, info->file_size);
				CLIENT_MUTEX_LOCK(&(request->mutex));
				request->file_size = info->file_size;
				CLIENT_MUTEX_UNLOCK(&(request->mutex));
				if (dp_db_set_column
						(request_id, DP_DB_TABLE_DOWNLOAD_INFO,
						DP_DB_COL_CONTENT_SIZE,
						DP_DB_COL_TYPE_INT64, &info->file_size) < 0)
					TRACE_ERROR("[ERROR][%d][SQL]", request_id);
			}

			if (info->content_name) {
				TRACE_INFO
					("[CONTENTNAME][%d] [%s]", request_id, info->content_name);
				if (dp_db_set_column
						(request_id, DP_DB_TABLE_DOWNLOAD_INFO,
						DP_DB_COL_CONTENT_NAME,
						DP_DB_COL_TYPE_TEXT, info->content_name) < 0)
					TRACE_ERROR("[ERROR][%d][SQL]", request_id);
			}
			if (info->etag) {
				TRACE_INFO("[ETAG][%d] [%s]", request_id, info->etag);
				if (dp_db_replace_column
						(request_id, DP_DB_TABLE_DOWNLOAD_INFO, DP_DB_COL_ETAG,
						DP_DB_COL_TYPE_TEXT, info->etag) < 0)
					TRACE_ERROR("[ERROR][%d][SQL]", request_id);
			}
		} else {
			TRACE_ERROR
				("[ERROR][%d][SQL] failed to insert downloadinfo",
				request_id);
		}
	}

	CLIENT_MUTEX_LOCK(&(request->mutex));

	request->state = DP_STATE_DOWNLOADING;
	if (dp_db_set_column(request->id, DP_DB_TABLE_LOG, DP_DB_COL_STATE,
			DP_DB_COL_TYPE_INT, &request->state) < 0)
		TRACE_ERROR("[ERROR][%d][SQL]", request->id);

	if (request->group && request->group->event_socket >= 0 &&
		request->state_cb)
		dp_ipc_send_event(request->group->event_socket,
			request->id, DP_STATE_DOWNLOADING, DP_ERROR_NONE, 0);

	if (request->auto_notification)
		request->noti_priv_id =
			dp_set_downloadinginfo_notification
				(request->id, request->packagename);

	CLIENT_MUTEX_UNLOCK(&(request->mutex));
}

static void __progress_cb(user_progress_info_t *info, void *user_data)
{
	if (!info) {
		TRACE_ERROR("[NULL-CHECK] Agent info");
		return ;
	}
	if (!user_data) {
		TRACE_ERROR("[NULL-CHECK] user_data");
		return ;
	}
	dp_request *request = (dp_request *) user_data;
	if (request->id < 0 || (request->agent_id != info->download_id)) {
		TRACE_ERROR("[NULL-CHECK][%d] agent_id : %d req_id %d",
			request->id, request->agent_id, info->download_id);
		return ;
	}

	CLIENT_MUTEX_LOCK(&(request->mutex));
	if (request->state == DP_STATE_DOWNLOADING) {
		request->received_size = info->received_size;
		time_t tt = time(NULL);
		struct tm *localTime = localtime(&tt);
		// send event every 1 second.
		if (request->progress_lasttime != localTime->tm_sec) {
			request->progress_lasttime = localTime->tm_sec;
			if (request->progress_cb && request->group &&
				request->group->event_socket >= 0 &&
				request->received_size > 0)
				dp_ipc_send_event(request->group->event_socket,
					request->id, request->state, request->error,
					request->received_size);
			if (request->auto_notification)
				dp_update_downloadinginfo_notification
					(request->noti_priv_id,
					(double)request->received_size,
					(double)request->file_size);
		}
	}
	CLIENT_MUTEX_UNLOCK(&(request->mutex));
}

static void __finished_cb(user_finished_info_t *info, void *user_data)
{
	if (!info) {
		TRACE_ERROR("[NULL-CHECK] Agent info");
		return ;
	}
	TRACE_INFO("Agent ID[%d] err[%d] http_status[%d]",
		info->download_id, info->err, info->http_status);
	if (!user_data) {
		TRACE_ERROR("[NULL-CHECK] user_data");
		return ;
	}
	dp_request *request = (dp_request *) user_data;
	if (request->id < 0 || (request->agent_id != info->download_id)) {
		TRACE_ERROR("[NULL-CHECK][%d] agent_id : %d req_id %d",
			request->id, request->agent_id, info->download_id);
		return ;
	}

	CLIENT_MUTEX_LOCK(&(request->mutex));
	int request_id = request->id;
	dp_credential cred = request->credential;
	CLIENT_MUTEX_UNLOCK(&(request->mutex));
	dp_state_type state = DP_STATE_NONE;
	dp_error_type errorcode = DP_ERROR_NONE;

	// update info before sending event
	if (dp_db_update_date
			(request_id, DP_DB_TABLE_LOG, DP_DB_COL_ACCESS_TIME) < 0)
		TRACE_ERROR("[ERROR][%d][SQL]", request_id);

	if (info->http_status > 0)
		if (dp_db_replace_column(request_id, DP_DB_TABLE_DOWNLOAD_INFO,
				DP_DB_COL_HTTP_STATUS,
				DP_DB_COL_TYPE_INT, &info->http_status) < 0)
			TRACE_ERROR("[ERROR][%d][SQL]", request_id);

	if (info->err == DA_RESULT_OK) {
		if (info->saved_path) {
			char *str = NULL;
			char *content_name = NULL;

			str = strrchr(info->saved_path, '/');
			if (str) {
				str++;
				content_name = dp_strdup(str);
				TRACE_INFO("[PARSE][%d] content_name [%s]",
					request_id, content_name);
			}
			TRACE_INFO
				("[chown][%d] [%d][%d]", request_id, cred.uid, cred.gid);
			if (chown(info->saved_path, cred.uid, cred.gid) < 0)
				TRACE_STRERROR("[ERROR][%d] chown", request_id);
			if (dp_db_replace_column
					(request_id, DP_DB_TABLE_DOWNLOAD_INFO,
					DP_DB_COL_SAVED_PATH,
					DP_DB_COL_TYPE_TEXT, info->saved_path) == 0) {
				if (content_name != NULL) {
					if (dp_db_set_column
							(request_id, DP_DB_TABLE_DOWNLOAD_INFO,
							DP_DB_COL_CONTENT_NAME,
							DP_DB_COL_TYPE_TEXT, content_name) < 0)
						TRACE_ERROR("[ERROR][%d][SQL]", request_id);
				}
			} else {
				TRACE_ERROR("[ERROR][%d][SQL]", request_id);
			}
			if (content_name != NULL)
				free(content_name);

			errorcode = DP_ERROR_NONE;
			state = DP_STATE_COMPLETED;

			TRACE_INFO("[COMPLETED][%d] saved to [%s]",
					request_id, info->saved_path);
		} else {
			TRACE_ERROR("Cannot enter here");
			TRACE_ERROR("[ERROR][%d] No SavedPath", request_id);
			errorcode = DP_ERROR_INVALID_DESTINATION;
			state = DP_STATE_FAILED;
		}
		CLIENT_MUTEX_LOCK(&(request->mutex));
		if (request->file_size == 0) {
			request->file_size = request->received_size;
			if (dp_db_replace_column
					(request_id, DP_DB_TABLE_DOWNLOAD_INFO,
					DP_DB_COL_CONTENT_SIZE,
					DP_DB_COL_TYPE_INT64, &request->file_size ) < 0)
				TRACE_ERROR("[ERROR][%d][SQL]", request_id);
		}
		CLIENT_MUTEX_UNLOCK(&(request->mutex));
	} else if (info->err == DA_RESULT_USER_CANCELED) {
		state = DP_STATE_CANCELED;
		errorcode = DP_ERROR_NONE;
		TRACE_INFO("[CANCELED][%d]", request_id);
	} else {
		state = DP_STATE_FAILED;
		errorcode = __change_error(info->err);
		TRACE_INFO("[FAILED][%d][%s]", request_id,
				dp_print_errorcode(errorcode));
	}

	if (dp_db_set_column
			(request_id, DP_DB_TABLE_LOG, DP_DB_COL_STATE,
			DP_DB_COL_TYPE_INT, &state) < 0)
		TRACE_ERROR("[ERROR][%d][SQL]", request_id);

	if (errorcode != DP_ERROR_NONE) {
		if (dp_db_set_column(request_id, DP_DB_TABLE_LOG,
				DP_DB_COL_ERRORCODE, DP_DB_COL_TYPE_INT,
				&errorcode) < 0)
			TRACE_ERROR("[ERROR][%d][SQL]", request_id);
	}

	// need MUTEX LOCK
	CLIENT_MUTEX_LOCK(&(request->mutex));

	request->state = state;
	request->error = errorcode;

	// stay on memory till called destroy by client or timeout
	if (request->group != NULL && request->group->event_socket >= 0) {
		/* update the received file size.
		* The last received file size cannot update
		* because of reducing update algorithm*/
		if (request->received_size > 0)
			dp_ipc_send_event(request->group->event_socket,
				request->id, DP_STATE_DOWNLOADING, request->error,
				request->received_size);
		if (request->state_cb)
			dp_ipc_send_event(request->group->event_socket,
				request->id, request->state, request->error, 0);
		request->group->queued_count--;
	}

	// to prevent the crash . check packagename of request
	if (request->auto_notification && request->packagename != NULL)
		request->noti_priv_id =
			dp_set_downloadedinfo_notification(request->noti_priv_id,
				request->id, request->packagename, request->state);

	request->stop_time = (int)time(NULL);

	CLIENT_MUTEX_UNLOCK(&(request->mutex));

	dp_thread_queue_manager_wake_up();
}

static void __paused_cb(user_paused_info_t *info, void *user_data)
{
	TRACE_INFO("");
	dp_request *request = (dp_request *) user_data;
	if (!request) {
		TRACE_ERROR("[NULL-CHECK] request");
		return ;
	}
	if (request->id < 0 || (request->agent_id != info->download_id)) {
		TRACE_ERROR("[NULL-CHECK][%d] agent_id : %d req_id %d",
			request->id, request->agent_id, info->download_id);
		return ;
	}

	CLIENT_MUTEX_LOCK(&(request->mutex));
	int request_id = request->id;
	CLIENT_MUTEX_UNLOCK(&(request->mutex));

	if (dp_db_update_date
			(request_id, DP_DB_TABLE_LOG, DP_DB_COL_ACCESS_TIME) < 0)
		TRACE_ERROR("[ERROR][%d][SQL]", request_id);

	// need MUTEX LOCK
	CLIENT_MUTEX_LOCK(&(request->mutex));

	request->state = DP_STATE_PAUSED;

	if (dp_db_set_column
			(request->id, DP_DB_TABLE_LOG, DP_DB_COL_STATE,
			DP_DB_COL_TYPE_INT, &request->state) < 0) {
		TRACE_ERROR("[ERROR][%d][SQL]", request->id);
	}

	if (request->group &&
		request->group->event_socket >= 0 && request->state_cb)
		dp_ipc_send_event(request->group->event_socket,
			request->id, request->state, request->error, 0);

	if (request->group)
		request->group->queued_count--;

	CLIENT_MUTEX_UNLOCK(&(request->mutex));

	dp_thread_queue_manager_wake_up();
}

int dp_init_agent()
{
	int da_ret = 0;
	da_client_cb_t da_cb = {
		__download_info_cb,
		__progress_cb,
		__finished_cb,
		__paused_cb
	};
	da_ret = da_init(&da_cb);
	if (da_ret != DA_RESULT_OK) {
		return DP_ERROR_OUT_OF_MEMORY;
	}
	return DP_ERROR_NONE;
}

void dp_deinit_agent()
{
	da_deinit();
}

// 0 : success
// -1 : failed
dp_error_type dp_cancel_agent_download(int req_id)
{
	if (req_id < 0) {
		TRACE_ERROR("[NULL-CHECK] req_id");
		return -1;
	}
	if (da_cancel_download(req_id) == DA_RESULT_OK)
		return 0;
	return -1;
}

// 0 : success
// -1 : failed
dp_error_type dp_pause_agent_download(int req_id)
{
	if (req_id < 0) {
		TRACE_ERROR("[NULL-CHECK] req_id");
		return -1;
	}
	if (da_suspend_download(req_id) == DA_RESULT_OK)
		return 0;
	return -1;
}


// 0 : success
// -1 : failed
// -2 : pended
dp_error_type dp_start_agent_download(dp_request *request)
{
	int da_ret = -1;
	int req_dl_id = -1;
	dp_error_type errorcode = DP_ERROR_NONE;
	extension_data_t ext_data = {0,};

	TRACE_INFO("");
	if (!request) {
		TRACE_ERROR("[NULL-CHECK] download_clientinfo_slot");
		return DP_ERROR_INVALID_PARAMETER;
	}

	char *url = dp_request_get_url(request->id, request, &errorcode);
	if (url == NULL) {
		TRACE_ERROR("[ERROR][%d] URL is NULL", request->id);
		return DP_ERROR_INVALID_URL;
	}
	char *destination =
		dp_request_get_destination(request->id, request, &errorcode);
	if (destination != NULL)
		ext_data.install_path = destination;

	char *filename =
		dp_request_get_filename(request->id, request, &errorcode);
	if (filename != NULL)
		ext_data.file_name = filename;

	// call start_download() of download-agent

	char *tmp_saved_path =
		dp_request_get_tmpsavedpath(request->id, request, &errorcode);
	if (tmp_saved_path) {
		char *etag = dp_request_get_etag(request->id, request, &errorcode);
		if (etag) {
			TRACE_INFO("[RESUME][%d]", request->id);
			ext_data.etag = etag;
			ext_data.temp_file_path = tmp_saved_path;
		} else {
			/* FIXME later : It is better to handle the unlink function in download agaent module
			 * or in upload the request data to memory after the download provider process is restarted */
			TRACE_INFO("[RESTART][%d] try to remove tmp file [%s]",
				request->id, tmp_saved_path);
			if (dp_is_file_exist(tmp_saved_path) == 0)
				if (unlink(tmp_saved_path) != 0)
					TRACE_STRERROR
						("[ERROR][%d] remove file", request->id);
		}
	}

	// get headers list from httpheaders table(DB)
	int headers_count = dp_db_get_cond_rows_count
			(request->id, DP_DB_TABLE_HTTP_HEADERS, NULL, 0, NULL);
	if (headers_count > 0) {
		ext_data.request_header = calloc(headers_count, sizeof(char*));
		if (ext_data.request_header != NULL) {
			ext_data.request_header_count = dp_db_get_http_headers_list
				(request->id, (char**)ext_data.request_header);
		}
	}

	ext_data.user_data = (void *)request;

	// call start API of agent lib
	da_ret =
		da_start_download_with_extension(url, &ext_data, &req_dl_id);
	if (ext_data.request_header_count > 0) {
		int len = 0;
		int i = 0;
		len = ext_data.request_header_count;
		for (i = 0; i < len; i++) {
			if (ext_data.request_header[i])
				free((void *)(ext_data.request_header[i]));
		}
		free(ext_data.request_header);
	}
	free(url);
	free(destination);
	free(filename);
	free(tmp_saved_path);

	// if start_download() return error cause of maximun download limitation,
	// set state to DOWNLOAD_STATE_PENDED.
	if (da_ret == DA_ERR_ALREADY_MAX_DOWNLOAD) {
		TRACE_INFO("[PENDING][%d] DA_ERR_ALREADY_MAX_DOWNLOAD [%d]",
			request->id, da_ret);
		return DP_ERROR_TOO_MANY_DOWNLOADS;
	} else if (da_ret != DA_RESULT_OK) {
		TRACE_ERROR("[ERROR][%d] DP_ERROR_CONNECTION_FAILED [%d]",
			request->id, da_ret);
		return __change_error(da_ret);
	}
	TRACE_INFO("[SUCCESS][%d] agent_id [%d]", request->id, req_dl_id);
	request->agent_id = req_dl_id;
	return DP_ERROR_NONE;
}

dp_error_type dp_resume_agent_download(int req_id)
{
	int da_ret = -1;
	if (req_id < 0) {
		TRACE_ERROR("[NULL-CHECK] req_id");
		return DP_ERROR_INVALID_PARAMETER;
	}
	da_ret = da_resume_download(req_id);
	if (da_ret == DA_RESULT_OK)
		return DP_ERROR_NONE;
	else if (da_ret == DA_ERR_INVALID_STATE)
		return DP_ERROR_INVALID_STATE;
	return __change_error(da_ret);
}

// 1 : alive
// 0 : not alive
int dp_is_alive_download(int req_id)
{
	int da_ret = 0;
	if (req_id < 0) {
		TRACE_ERROR("[NULL-CHECK] req_id");
		return 0;
	}
	da_ret = da_is_valid_download_id(req_id);
	return da_ret;
}

