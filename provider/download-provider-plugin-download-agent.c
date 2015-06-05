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
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <dlfcn.h> // dlopen
#include <fcntl.h>

#include <download-provider.h>
#include <download-provider-log.h>
#include <download-provider-pthread.h>
#include <download-provider-ipc.h>
#include <download-provider-db-defs.h>
#include <download-provider-db.h>
#include <download-provider-utils.h>
#include <download-provider-notify.h>
#include <download-provider-smack.h>
#include <download-provider-client.h>
#include <download-provider-client-manager.h>
#include <download-provider-plugin-download-agent.h>
#include <download-provider-notification-manager.h>
#include <download-provider-queue-manager.h>

#include <download-agent-defs.h>
#include <download-agent-interface.h>
#include "xdgmime.h"
#include "content/mime_type.h"

#define DP_SDCARD_MNT_POINT "/opt/storage/sdcard"
#define DP_MAX_FILE_PATH_LEN 256
#define DP_MAX_MIME_TABLE_NUM 15

typedef struct {
	const char *mime;
	int content_type;
}mime_table_type;

const char *ambiguous_mime_type_list[] = {
		"text/plain",
		"application/octet-stream"
};

mime_table_type mime_table[]={
		// PDF
		{"application/pdf",DP_CONTENT_PDF},
		// word
		{"application/msword",DP_CONTENT_WORD},
		{"application/vnd.openxmlformats-officedocument.wordprocessingml.document",DP_CONTENT_WORD},
		// ppt
		{"application/vnd.ms-powerpoint",DP_CONTENT_PPT},
		{"application/vnd.openxmlformats-officedocument.presentationml.presentation",DP_CONTENT_PPT},
		// excel
		{"application/vnd.ms-excel",DP_CONTENT_EXCEL},
		{"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",DP_CONTENT_EXCEL},
		// html
		{"text/html",DP_CONTENT_HTML},
		// txt
		{"text/txt",DP_CONTENT_TEXT},
		{"text/plain",DP_CONTENT_TEXT},
		// DRM
		{"application/vnd.oma.drm.content",DP_CONTENT_SD_DRM},
		{"application/vnd.oma.drm.message",DP_CONTENT_DRM},
		{"application/x-shockwave-flash", DP_CONTENT_FLASH},
		{"application/vnd.tizen.package", DP_CONTENT_TPK},
		{"text/calendar",DP_CONTENT_VCAL},
};

static void *g_da_handle = NULL;
static int (*download_agent_init)(void) = NULL; // int da_init(da_client_cb_t *da_client_callback);
static int (*download_agent_deinit)(void) = NULL; //  int da_deinit();
static int (*download_agent_is_alive)(int) = NULL;  // int da_is_valid_download_id(int download_id);
static int (*download_agent_suspend)(int) = NULL; // int da_suspend_download(int download_id);
static int (*download_agent_resume)(int) = NULL; // int da_resume_download(int download_id);
static int (*download_agent_cancel)(int) = NULL; // int da_cancel_download(int download_id);
static int (*download_agent_suspend_without_update)(int) = NULL; // int da_suspend_download_without_update(int download_id);
static int (*download_agent_cancel_without_update)(int) = NULL; // int da_cancel_download_without_update(int download_id);
static int (*download_agent_start)(const char *url, req_data_t *ext_data, da_cb_t *da_cb_data, int *download_id) = NULL; // int da_start_download_with_extension(const char *url, extension_data_t *ext_data, int *download_id);
static dp_content_type __dp_get_content_type(const char *mime, const char *file_path);

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
		ret = DP_ERROR_NETWORK_ERROR;
		break;
	case DA_ERR_CONNECTION_FAIL:
	case DA_ERR_NETWORK_UNAUTHORIZED:
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
	case DA_ERR_FAIL_TO_ACCESS_FILE:
	default:
		ret = DP_ERROR_IO_ERROR;
		break;
	}
	return ret;
}

static int __dp_da_state_feedback(dp_client_slots_fmt *slot, dp_request_fmt *request)
{
	if (slot == NULL || request == NULL) {
		TRACE_ERROR("check address");
		return -1; // try cancel
	}

	TRACE_INFO("[INFO][%d] state:%s error:%s", request->id,
		dp_print_state(request->state), dp_print_errorcode(request->error));

	int errorcode = DP_ERROR_NONE;
	if (dp_db_update_logging(slot->client.dbhandle, request->id,
			request->state, request->error, &errorcode) < 0) {
		TRACE_ERROR("logging failure id:%d error:%d", request->id, errorcode);
		return -1; // try cancel
	}

	request->access_time = (int)time(NULL);

	if (request->state_cb == 1) {
		if (slot->client.notify < 0 ||
				dp_notify_feedback(slot->client.notify, slot, request->id, request->state, request->error, 0) < 0) {
			TRACE_ERROR("id:%d disable state callback by IO_ERROR", request->id);
			request->state_cb = 0;
		}
	}

	return 0;
}

static int __precheck_request(dp_request_fmt *request, int agentid)
{
	if (request == NULL) {
		TRACE_ERROR("null-check request req_id:%d", agentid);
		return -1;
	}
	if (request->id < 0 || (request->agent_id != agentid)) {
		TRACE_ERROR("id-check request_id:%d agent_id:%d req_id:%d",
			request->id, request->agent_id, agentid);
		return -1;
	}
	return 0;
}

static int __set_file_permission_to_client(dp_client_slots_fmt *slot, dp_request_fmt *request, char *saved_path)
{
	if (slot == NULL || request == NULL) {
		TRACE_ERROR("check address slot:%p request:%p id:%d agentid:%d", slot, request, (request == NULL ? 0 : request->id), (request == NULL ? 0 : request->agent_id));
		return DP_ERROR_INVALID_PARAMETER;
	} else if (saved_path == NULL) {
		TRACE_ERROR("check null saved path");
		return DP_ERROR_INVALID_PARAMETER;
	}

	struct stat lstat_info;
	struct stat fstat_info;
	int fd;
	int errorcode = DP_ERROR_NONE;
	char *str = NULL;
	str = strrchr(saved_path, '/');
	dp_credential cred = slot->credential;
	if (lstat(saved_path, &lstat_info) != -1) {
		fd = open (saved_path, O_RDONLY);
		if (fd != -1) {
			if (fstat(fd, &fstat_info) != -1) {
				if (lstat_info.st_mode == fstat_info.st_mode &&
					lstat_info.st_ino == fstat_info.st_ino &&
					lstat_info.st_dev == fstat_info.st_dev) {
					if ((fchown(fd, cred.uid, cred.gid) != 0) ||
						(fchmod(fd, S_IRUSR | S_IWUSR |
							S_IRGRP | S_IROTH) != 0)) {
						TRACE_STRERROR("[ERROR][%d] permission user:%d group:%d",
							request->id, cred.uid, cred.gid);
						errorcode = DP_ERROR_PERMISSION_DENIED;
					}
				} else {
					TRACE_STRERROR("fstat & lstat info have not matched");
					errorcode = DP_ERROR_PERMISSION_DENIED;
				}
			} else {
				TRACE_STRERROR("fstat call failed");
				errorcode = DP_ERROR_PERMISSION_DENIED;
			}
			close(fd);
		} else {
			TRACE_SECURE_ERROR("open failed for file : %s", saved_path);
			errorcode = DP_ERROR_IO_ERROR;
		}
	} else {
		TRACE_STRERROR("lstat call failed");
		errorcode = DP_ERROR_PERMISSION_DENIED;
	}
	if (errorcode == DP_ERROR_NONE && dp_smack_is_mounted() == 1) {
		// get smack_label from sql
		char *smack_label = dp_db_get_client_smack_label(slot->pkgname);
		if (smack_label == NULL) {
			TRACE_SECURE_ERROR("[SMACK][%d] no label", request->id);
			errorcode = DP_ERROR_PERMISSION_DENIED;
		} else {
			size_t len = str - (saved_path);
			char *dir_path = (char *)calloc(len + 1, sizeof(char));
			if (dir_path != NULL) {
				strncpy(dir_path, saved_path, len);
				errorcode = dp_smack_set_label(smack_label, dir_path, saved_path);
				free(dir_path);
			} else {
				TRACE_STRERROR("[ERROR] calloc");
				errorcode = DP_ERROR_OUT_OF_MEMORY;
			}
			free(smack_label);
		}
	}
	return errorcode;
}

static void __finished_cb(finished_info_t *info, void *user_req_data,
		void *user_client_data)
{
	if (info == NULL) {
		TRACE_ERROR("check download info address");
		return ;
	}
	dp_client_slots_fmt *slot = user_client_data;
	dp_request_fmt *request = user_req_data;
	if (slot == NULL || request == NULL) {
		TRACE_ERROR("check address slot:%p request:%p id:%d agentid:%d", slot, request, (request == NULL ? 0 : request->id), info->download_id);
		free(info->etag);
		free(info->saved_path);
		free(info);
		return ;
	}
	CLIENT_MUTEX_LOCK(&slot->mutex);
	if (__precheck_request(request, info->download_id) < 0) {
		TRACE_ERROR("error request agent_id:%d", info->download_id);
		if (dp_cancel_agent_download(info->download_id) < 0)
			TRACE_ERROR("failed to call cancel_download(%d)", info->download_id);
		free(info->etag);
		free(info->saved_path);
		free(info);
		CLIENT_MUTEX_UNLOCK(&slot->mutex);
		return ;
	}

	int state = DP_STATE_NONE;
	int errorcode = DP_ERROR_NONE;

	if (info->http_status > 0) {
		if (dp_db_replace_property(slot->client.dbhandle, request->id, DP_TABLE_DOWNLOAD, DP_DB_COL_HTTP_STATUS, (void *)&info->http_status, 0, 0, &errorcode) < 0) {
			TRACE_ERROR("id:%d failed to set http_status(%d)", request->id, info->http_status);
		}
	}

	TRACE_SECURE_DEBUG("[FINISH][%d][%s]", request->id, info->saved_path);
	if (info->err == DA_RESULT_OK) {
		if (info->saved_path != NULL) {
			if(strncmp(DP_SDCARD_MNT_POINT, info->saved_path, strlen(DP_SDCARD_MNT_POINT)) != 0) {
				errorcode = __set_file_permission_to_client(slot, request, info->saved_path);
			}
		} else {
			TRACE_ERROR("[ERROR][%d] No SavedPath", request->id);
			errorcode = DP_ERROR_INVALID_DESTINATION;
		}
		if (errorcode == DP_ERROR_NONE)
			state = DP_STATE_COMPLETED;
		else
			state = DP_STATE_FAILED;
	} else {
		if (info->err == DA_RESULT_USER_CANCELED) {

			TRACE_INFO("[CANCELED][%d]", request->id);

			// if state is canceled and error is DP_ERROR_IO_EAGAIN mean ip_changed
			if (request->error == DP_ERROR_IO_EAGAIN) {
				request->error = DP_ERROR_NONE;
			} else {
				state = DP_STATE_CANCELED;
				errorcode = request->error;
			}

		} else {
			state = DP_STATE_FAILED;
			errorcode = __change_error(info->err);
			TRACE_ERROR("[FAILED][%d][%s] agent error:%d", request->id,
					dp_print_errorcode(errorcode), info->err);
		}

	}

	if (errorcode == DP_ERROR_NONE && info->saved_path != NULL) {

		char *content_name = NULL;
		char *str = NULL;
		str = strrchr(info->saved_path, '/');
		if (str != NULL) {
			str++;
			content_name = dp_strdup(str);
		}
		if (request->file_size == 0) {// missed in download_cb
			request->file_size = request->received_size;
			if (dp_db_replace_property(slot->client.dbhandle, request->id, DP_TABLE_DOWNLOAD, DP_DB_COL_CONTENT_SIZE, (void *)&request->file_size, 0, 1, &errorcode) < 0) {
				TRACE_ERROR("id:%d failed to set content_size(%ld)", request->id, request->file_size);
			}
		}
		// update contentname, savedpath
		if (content_name != NULL) {
			if (dp_db_replace_property(slot->client.dbhandle, request->id, DP_TABLE_DOWNLOAD, DP_DB_COL_CONTENT_NAME, (void *)content_name, 0, 2, &errorcode) < 0) {
				TRACE_ERROR("id:%d failed to set content_name", request->id);
			}
		}
		if (dp_db_replace_property(slot->client.dbhandle, request->id, DP_TABLE_DOWNLOAD, DP_DB_COL_SAVED_PATH, (void *)info->saved_path, 0, 2, &errorcode) < 0) {
			TRACE_ERROR("id:%d failed to set saved_path", request->id);
		}
		free(content_name);
		/* update the received file size.
		* The last received file size cannot update
		* because of reducing update algorithm*/
		if (request->progress_cb == 1) {
			if (slot->client.notify < 0 ||
					dp_notify_feedback(slot->client.notify, slot, request->id, DP_STATE_DOWNLOADING, DP_ERROR_NONE, request->received_size) < 0) {
				TRACE_ERROR("id:%d disable progress callback by IO_ERROR", request->id);
				request->progress_cb = 0;
			}
		}
	}

	request->state = state;
	request->error = errorcode;

	if (__dp_da_state_feedback(slot, request) < 0) {
		TRACE_ERROR("id:%d check notify channel", request->id);
		if (dp_cancel_agent_download_without_update(request->agent_id) < 0)
			TRACE_ERROR("[fail][%d]cancel_agent", request->id);
	}
	if (request->noti_type == DP_NOTIFICATION_TYPE_COMPLETE_ONLY ||
			request->noti_type == DP_NOTIFICATION_TYPE_ALL) {
		if (dp_notification_manager_push_notification(slot, request, DP_NOTIFICATION) < 0) {
			TRACE_ERROR("failed to register notification for id:%d", request->id);
		}
	}
	free(info->etag);
	free(info->saved_path);
	free(info);
	CLIENT_MUTEX_UNLOCK(&slot->mutex);
}

static void __paused_cb(int download_id, void *user_req_data, void *user_client_data)
{
	dp_client_slots_fmt *slot = user_client_data;
	dp_request_fmt *request = user_req_data;
	if (slot == NULL || request == NULL) {
		TRACE_ERROR("check address slot:%p request:%p id:%d agentid:%d", slot, request, (request == NULL ? 0 : request->id), download_id);
		return ;
	}
	dp_queue_manager_wake_up();
}

static void __download_info_cb(download_info_t *info, void *user_req_data, void *user_client_data)
{
	if (info == NULL) {
		TRACE_ERROR("check download info address");
		return ;
	}
	dp_client_slots_fmt *slot = user_client_data;
	dp_request_fmt *request = user_req_data;
	if (slot == NULL || request == NULL) {
		TRACE_ERROR("check address slot:%p request:%p id:%d agentid:%d", slot, request, (request == NULL ? 0 : request->id), info->download_id);
		free(info->content_name);
		free(info->etag);
		free(info->file_type);
		free(info->tmp_saved_path);
		free(info);
		return ;
	}
	CLIENT_MUTEX_LOCK(&slot->mutex);
	if (__precheck_request(request, info->download_id) < 0) {
		TRACE_ERROR("error request agent_id:%d", info->download_id);
		if (dp_cancel_agent_download(info->download_id) < 0)
			TRACE_ERROR("failed to call cancel_download(%d)", info->download_id);
		free(info->content_name);
		free(info->etag);
		free(info->file_type);
		free(info->tmp_saved_path);
		free(info);
		CLIENT_MUTEX_UNLOCK(&slot->mutex);
		return ;
	}

	// update info before sending event
	TRACE_SECURE_DEBUG("[DOWNLOAD][%d][%s]", request->id, info->tmp_saved_path);
	if (info->tmp_saved_path != NULL) {
		int errorcode = DP_ERROR_NONE;
		if (dp_db_replace_property(slot->client.dbhandle, request->id, DP_TABLE_DOWNLOAD, DP_DB_COL_MIMETYPE, (void *)info->file_type, 0, 2, &errorcode) < 0) {
			TRACE_ERROR("id:%d failed to set mimetype", request->id);
		}
		if (dp_db_replace_property(slot->client.dbhandle, request->id, DP_TABLE_DOWNLOAD, DP_DB_COL_CONTENT_NAME, (void *)info->content_name, 0, 2, &errorcode) < 0) {
			TRACE_ERROR("id:%d failed to set contentname", request->id);
		}
		if (dp_db_replace_property(slot->client.dbhandle, request->id, DP_TABLE_DOWNLOAD, DP_DB_COL_TMP_SAVED_PATH, (void *)info->tmp_saved_path, 0, 2, &errorcode) < 0) {
			TRACE_ERROR("id:%d failed to set tmp_saved_path", request->id);
		}
		if (info->file_size > 0 && dp_db_replace_property(slot->client.dbhandle, request->id, DP_TABLE_DOWNLOAD, DP_DB_COL_CONTENT_SIZE, (void *)&(info->file_size), 0, 1, &errorcode) < 0) {
			TRACE_ERROR("id:%d failed to set file size", request->id);
		}
		if (info->etag && dp_db_replace_property(slot->client.dbhandle, request->id, DP_TABLE_DOWNLOAD, DP_DB_COL_ETAG, (void *)info->etag, 0, 2, &errorcode) < 0) {
			TRACE_ERROR("id:%d failed to set etag", request->id);
		}
		if(strncmp(DP_SDCARD_MNT_POINT, info->tmp_saved_path, strlen(DP_SDCARD_MNT_POINT)) != 0) {
			errorcode = __set_file_permission_to_client(slot, request, info->tmp_saved_path);
		}
		request->error = errorcode;
	} else {
		request->error = DP_ERROR_IO_ERROR;
	}

	if (request->error != DP_ERROR_NONE) {
		request->state = DP_STATE_FAILED;
		TRACE_ERROR("id:%d try to cancel(%d)", request->id, info->download_id);
		if (dp_cancel_agent_download(request->agent_id) < 0) {
			TRACE_ERROR("[fail][%d] cancel_agent:%d", request->id,
				info->download_id);
		}
	} else {
		request->state = DP_STATE_DOWNLOADING;
		request->file_size = info->file_size; // unsigned
		TRACE_DEBUG("[STARTED] id:%d agentid:%d", request->id, info->download_id);
	}

	if (__dp_da_state_feedback(slot, request) < 0) {
		TRACE_ERROR("id:%d check notify channel", request->id);
		if (dp_cancel_agent_download(request->agent_id) < 0)
			TRACE_ERROR("[fail][%d]cancel_agent", request->id);
	}
	// notification
	if (request->noti_type == DP_NOTIFICATION_TYPE_ALL) {
		if (dp_notification_manager_push_notification(slot, request, DP_NOTIFICATION_ONGOING_UPDATE) < 0) {
			TRACE_ERROR("failed to register notification for id:%d", request->id);
		}
	}
	//get the mime type for dp notification
	if (request->noti_type > DP_NOTIFICATION_TYPE_NONE) {
		request->content_type = __dp_get_content_type(info->file_type, info->tmp_saved_path);
	}
	free(info->content_name);
	free(info->etag);
	free(info->file_type);
	free(info->tmp_saved_path);
	free(info);
	CLIENT_MUTEX_UNLOCK(&slot->mutex);
}

static void __progress_cb(int download_id, unsigned long long received_size,
		void *user_req_data, void *user_client_data)
{
	dp_client_slots_fmt *slot = user_client_data;
	dp_request_fmt *request = user_req_data;
	if (slot == NULL || request == NULL) {
		TRACE_ERROR("check address slot:%p request:%p id:%d agentid:%d", slot, request, (request == NULL ? 0 : request->id), download_id);
		return ;
	}
	CLIENT_MUTEX_LOCK(&slot->mutex);
	/*
	if (CLIENT_MUTEX_TRYLOCK(&slot->mutex) != 0) {
		TRACE_ERROR("slot busy agent_id:%d", download_id);
		return ;
	}
	*/
	if (__precheck_request(request, download_id) < 0) {
		TRACE_ERROR("error request agent_id:%d", download_id);
		if (dp_cancel_agent_download(download_id) < 0)
			TRACE_ERROR("failed to call cancel_download(%d)", download_id);
		CLIENT_MUTEX_UNLOCK(&slot->mutex);
		return ;
	}

	// For resume case after pause, it change state from connecting to downloading
	if (request->state == DP_STATE_CONNECTING) {
		request->state = DP_STATE_DOWNLOADING;
		if (__dp_da_state_feedback(slot, request) < 0) {
			TRACE_ERROR("id:%d check notify channel", request->id);
			if (dp_cancel_agent_download(request->agent_id) < 0)
				TRACE_ERROR("[fail][%d]cancel_agent", request->id);
		}
	}

	if (request->state == DP_STATE_DOWNLOADING) {
		request->received_size = received_size;
		time_t tt = time(NULL);
		struct tm *localTime = localtime(&tt);
		// send event every 1 second.
		if (request->progress_lasttime != localTime->tm_sec) {
			request->progress_lasttime = localTime->tm_sec;

			if (request->progress_cb == 1) {
				if (slot->client.notify < 0 ||
						dp_notify_feedback(slot->client.notify, slot,
							request->id, DP_STATE_DOWNLOADING, DP_ERROR_NONE, received_size) < 0) {
					// failed to read from socket // ignore this status
					TRACE_ERROR("id:%d disable progress callback by IO_ERROR", request->id);
					request->progress_cb = 0;
				}
			}

			if (request->noti_type == DP_NOTIFICATION_TYPE_ALL) {
				if (dp_notification_manager_push_notification(slot, request, DP_NOTIFICATION_ONGOING_PROGRESS) < 0) {
					TRACE_ERROR("failed to register notification for id:%d", request->id);
				}
			}

		}
	}
	CLIENT_MUTEX_UNLOCK(&slot->mutex);
}

static int __dp_is_ambiguous_mime_type(const char *mime_type)
{
	if (mime_type == NULL)
		return -1;

	int index = 0;
	int listSize = sizeof(ambiguous_mime_type_list) / sizeof(const char *);
	for (index = 0; index < listSize; index++) {
		if (0 == strncmp(mime_type, ambiguous_mime_type_list[index],
				strlen(ambiguous_mime_type_list[index]))) {
			TRACE_DEBUG("It is ambiguous");
			return 0;
		}
	}
	return -1;
}

static dp_content_type __dp_get_content_type(const char *mime, const char *file_path)
{
	int i = 0;
	int type = DP_CONTENT_UNKNOWN;
	char *temp_mime = NULL;
	if (mime == NULL || strlen(mime) < 1)
		return DP_CONTENT_UNKNOWN;

	if ((file_path != NULL) && (strlen(file_path) > 0) &&
			(__dp_is_ambiguous_mime_type(mime) == 0)) {
		const char *ext = strrchr(file_path, '.');
		if (ext == NULL) {
			TRACE_ERROR("File Extension is NULL");
			return type;
		}
		mime_type_get_mime_type(ext + 1, &temp_mime);
	}
	if (temp_mime == NULL) {
		temp_mime = (char *)calloc(1, DP_MAX_FILE_PATH_LEN);
		if (temp_mime == NULL) {
			TRACE_ERROR("Fail to call calloc");
			return type;
		}
		strncpy(temp_mime, mime, DP_MAX_FILE_PATH_LEN - 1);
	}
	TRACE_SECURE_DEBUG("mime type [%s]", temp_mime);

	/* Search a content type from mime table. */
	for (i = 0; i < DP_MAX_MIME_TABLE_NUM; i++) {
		if (strncmp(mime_table[i].mime, temp_mime, strlen(temp_mime)) == 0){
			type = mime_table[i].content_type;
			break;
		}
	}
	if (type == DP_CONTENT_UNKNOWN) {
		const char *unaliased_mime = NULL;
		/* unaliased_mimetype means representative mime among similar types */
		unaliased_mime = xdg_mime_unalias_mime_type(temp_mime);

		if (unaliased_mime != NULL) {
			TRACE_SECURE_DEBUG("unaliased mime type[%s]",unaliased_mime);
			if (strstr(unaliased_mime,"video/") != NULL)
				type = DP_CONTENT_VIDEO;
			else if (strstr(unaliased_mime,"audio/") != NULL)
				type = DP_CONTENT_MUSIC;
			else if (strstr(unaliased_mime,"image/") != NULL)
				type = DP_CONTENT_IMAGE;
		}
	}
	free(temp_mime);
	TRACE_DEBUG("type[%d]", type);
	return type;
}


int dp_init_agent()
{

	g_da_handle = dlopen("/usr/lib/libdownloadagent2.so", RTLD_LAZY | RTLD_GLOBAL);
	if (!g_da_handle) {
		TRACE_ERROR("[dlopen] %s", dlerror());
		g_da_handle = NULL;
		return DP_ERROR_OUT_OF_MEMORY;
	}
	dlerror();    /* Clear any existing error */

	*(void **) (&download_agent_init) = dlsym(g_da_handle, "da_init");
	if (download_agent_init == NULL ) {
		TRACE_ERROR("[dlsym] da_init:%s", dlerror());
		dlclose(g_da_handle);
		g_da_handle = NULL;
		return DP_ERROR_OUT_OF_MEMORY;
	}

	*(void **) (&download_agent_deinit) = dlsym(g_da_handle, "da_deinit");
	if (download_agent_deinit == NULL ) {
		TRACE_ERROR("[dlsym] da_deinit:%s", dlerror());
		dlclose(g_da_handle);
		g_da_handle = NULL;
		return DP_ERROR_OUT_OF_MEMORY;
	}

	*(void **) (&download_agent_is_alive) =
			dlsym(g_da_handle, "da_is_valid_download_id");
	if (download_agent_is_alive == NULL ) {
		TRACE_ERROR("[dlsym] da_is_valid_download_id:%s", dlerror());
		dlclose(g_da_handle);
		g_da_handle = NULL;
		return DP_ERROR_OUT_OF_MEMORY;
	}

	*(void **) (&download_agent_suspend) =
			dlsym(g_da_handle, "da_suspend_download");
	if (download_agent_suspend == NULL ) {
		TRACE_ERROR("[dlsym] da_suspend_download:%s", dlerror());
		dlclose(g_da_handle);
		g_da_handle = NULL;
		return DP_ERROR_OUT_OF_MEMORY;
	}

	*(void **) (&download_agent_resume) =
			dlsym(g_da_handle, "da_resume_download");
	if (download_agent_resume == NULL) {
		TRACE_ERROR("[dlsym] da_resume_download:%s", dlerror());
		dlclose(g_da_handle);
		g_da_handle = NULL;
		return DP_ERROR_OUT_OF_MEMORY;
	}

//	*(void **) (&download_agent_cancel) = dlsym(g_da_handle, "da_cancel_download_without_update");
	*(void **) (&download_agent_cancel) =
			dlsym(g_da_handle, "da_cancel_download");
	if (download_agent_cancel == NULL) {
		TRACE_ERROR("[dlsym] da_cancel_download:%s", dlerror());
		dlclose(g_da_handle);
		g_da_handle = NULL;
		return DP_ERROR_OUT_OF_MEMORY;
	}

	*(void **) (&download_agent_start) =
			dlsym(g_da_handle, "da_start_download");
	if (download_agent_start == NULL) {
		TRACE_ERROR("[dlsym] da_start_download:%s", dlerror());
		dlclose(g_da_handle);
		g_da_handle = NULL;
		return DP_ERROR_OUT_OF_MEMORY;
	}

	*(void **) (&download_agent_cancel_without_update) = dlsym(g_da_handle, "da_cancel_download_without_update");
	if (download_agent_cancel_without_update == NULL) {
		TRACE_ERROR("[dlsym] da_cancel_download_without_update:%s", dlerror());
		dlclose(g_da_handle);
		g_da_handle = NULL;
		return DP_ERROR_OUT_OF_MEMORY;
	}

	*(void **) (&download_agent_suspend_without_update) = dlsym(g_da_handle, "da_suspend_download_without_update");
	if (download_agent_suspend_without_update == NULL) {
		TRACE_ERROR("[dlsym] da_suspend_download_without_update:%s", dlerror());
		dlclose(g_da_handle);
		g_da_handle = NULL;
		return DP_ERROR_OUT_OF_MEMORY;
	}

	int da_ret = -1;
	da_ret = (*download_agent_init)();
	if (da_ret != DA_RESULT_OK) {
		return DP_ERROR_OUT_OF_MEMORY;
	}
	return DP_ERROR_NONE;
}

void dp_deinit_agent()
{
	if (g_da_handle != NULL) {
		if (download_agent_deinit != NULL)
			(*download_agent_deinit)();

		dlclose(g_da_handle);
		g_da_handle = NULL;
	}
}

// 1 : alive
// 0 : not alive
int dp_is_alive_download(int req_id)
{
	int da_ret = 0;
	if (req_id < 0)
		return 0;
	if (download_agent_is_alive != NULL)
		da_ret = (*download_agent_is_alive)(req_id);
	return da_ret;
}

// 0 : success
// -1 : failed
int dp_cancel_agent_download(int req_id)
{
	if (req_id < 0) {
		TRACE_ERROR("[NULL-CHECK] req_id");
		return -1;
	}
	if (dp_is_alive_download(req_id) == 0) {
		TRACE_ERROR("[CHECK agent-id:%d] dead request", req_id);
		return -1;
	}
	if (download_agent_cancel != NULL) {
		if ((*download_agent_cancel)(req_id) == DA_RESULT_OK)
			return 0;
	}
	return -1;
}

// 0 : success
// -1 : failed
int dp_pause_agent_download(int req_id)
{
	if (req_id < 0) {
		TRACE_ERROR("[NULL-CHECK] req_id");
		return -1;
	}
	if (dp_is_alive_download(req_id) == 0) {
		TRACE_ERROR("[CHECK agent-id:%d] dead request", req_id);
		return -1;
	}
	if (download_agent_suspend != NULL) {
		if ((*download_agent_suspend)(req_id) == DA_RESULT_OK)
			return 0;
	}
	return -1;
}


// 0 : success
// -1 : failed
int dp_cancel_agent_download_without_update(int req_id)
{
	if (req_id < 0) {
		TRACE_ERROR("[NULL-CHECK] req_id");
		return -1;
	}
	if (dp_is_alive_download(req_id) == 0) {
		TRACE_ERROR("[CHECK agent-id:%d] dead request", req_id);
		return -1;
	}
	if (download_agent_cancel_without_update != NULL) {
		if ((*download_agent_cancel_without_update)(req_id) == DA_RESULT_OK)
			return 0;
	}
	return -1;
}

// 0 : success
// -1 : failed
int dp_pause_agent_download_without_update(int req_id)
{
	if (req_id < 0) {
		TRACE_ERROR("[NULL-CHECK] req_id");
		return -1;
	}
	if (dp_is_alive_download(req_id) == 0) {
		TRACE_ERROR("[CHECK agent-id:%d] dead request", req_id);
		return -1;
	}
	if (download_agent_suspend_without_update != NULL) {
		if ((*download_agent_suspend_without_update)(req_id) == DA_RESULT_OK)
			return 0;
	}
	return -1;
}

// 0 : success
// -1 : failed
// -2 : pended
int dp_start_agent_download(void *slot, void *request)
{
	int da_ret = -1;
	int req_dl_id = -1;
	req_data_t *req_data = NULL;

	dp_client_slots_fmt *base_slot = slot;
	dp_request_fmt *base_req = request;

	if (slot == NULL || request == NULL) {
		TRACE_ERROR("check slot or request address");
		return DP_ERROR_INVALID_PARAMETER;
	}

	da_cb_t da_cb = {
		__download_info_cb,
		__progress_cb,
		__finished_cb,
		__paused_cb
	};

	req_data = (req_data_t *)calloc(1, sizeof(req_data_t));
	if (req_data == NULL) {
		TRACE_ERROR("[ERROR] calloc");
		return DP_ERROR_OUT_OF_MEMORY;
	}

	int errorcode = DP_ERROR_NONE;
	unsigned length = 0;
	char *url = NULL;
	char *destination = NULL;
	char *tmp_saved_path = NULL;
	char *user_tmp_file_path = NULL;
	char *filename = NULL;
	char *etag = NULL;
	int user_network_bonding = 0;

	if (dp_db_get_property_string(base_slot->client.dbhandle, base_req->id, DP_TABLE_REQUEST, DP_DB_COL_URL, (unsigned char **)&url, &length, &errorcode) < 0 ||
			url == NULL) {
		free(req_data);
		if (errorcode == DP_ERROR_NO_DATA) {
			TRACE_ERROR("url id:%d NO_DATA", base_req->id);
			return DP_ERROR_INVALID_URL;
		} else {
			TRACE_ERROR("url id:%d DISK_BUSY", base_req->id);
			return DP_ERROR_DISK_BUSY;
		}
	}
	if (dp_db_get_property_string(base_slot->client.dbhandle, base_req->id, DP_TABLE_REQUEST, DP_DB_COL_TEMP_FILE_PATH, (unsigned char **)&user_tmp_file_path, &length, &errorcode) < 0 ||
			user_tmp_file_path == NULL) {
		TRACE_DEBUG("user_tmp_file_path id:%d NO_DATA", base_req->id);
	}

	if (user_tmp_file_path != NULL)
		req_data->temp_file_path = user_tmp_file_path;
	else {
		if (dp_db_get_property_string(base_slot->client.dbhandle, base_req->id, DP_TABLE_REQUEST, DP_DB_COL_FILENAME, (unsigned char **)&filename, &length, &errorcode) < 0 ||
				filename == NULL) {
			TRACE_DEBUG("filename id:%d NO_DATA", base_req->id);
		} else
			req_data->file_name = filename;
		if (dp_db_get_property_string(base_slot->client.dbhandle, base_req->id, DP_TABLE_REQUEST, DP_DB_COL_DESTINATION, (unsigned char **)&destination, &length, &errorcode) < 0 ||
			destination == NULL) {
			TRACE_DEBUG("destination id:%d NO_DATA", base_req->id);
		} else
			req_data->install_path = destination;
		if (dp_db_get_property_string(base_slot->client.dbhandle, base_req->id, DP_TABLE_DOWNLOAD, DP_DB_COL_TMP_SAVED_PATH, (unsigned char **)&tmp_saved_path, &length, &errorcode) < 0 ||
				tmp_saved_path == NULL) {
			TRACE_DEBUG("tmp_saved_path id:%d NO_DATA", base_req->id);
		}
	}

	if (tmp_saved_path != NULL) {
		if (dp_db_get_property_string(base_slot->client.dbhandle, base_req->id, DP_TABLE_DOWNLOAD, DP_DB_COL_ETAG, (unsigned char **)&etag, &length, &errorcode) < 0 ||
				etag == NULL) {
			TRACE_DEBUG("etag id:%d NO_DATA", base_req->id);
		}
		if (etag != NULL) {
			TRACE_DEBUG("try to resume id:%d", base_req->id);
			req_data->etag = etag;
			req_data->temp_file_path = tmp_saved_path;
		} else {
			/* FIXME later : It is better to handle the unlink function in download agaent module
			 * or in upload the request data to memory after the download provider process is restarted */
			TRACE_SECURE_INFO("try to restart id:%d remove tmp file:%s",
				base_req->id, tmp_saved_path);
			if (dp_is_file_exist(tmp_saved_path) == 0) {
				if (unlink(tmp_saved_path) != 0)
					TRACE_STRERROR("failed to remove file id:%d", base_req->id);
			}
		}
	}
	if( dp_db_get_property_int(base_slot->client.dbhandle, base_req->id, DP_TABLE_REQUEST, DP_DB_COL_NETWORK_BONDING, (int *)&user_network_bonding, &errorcode) < 0 ||
			user_network_bonding < 0) {
		TRACE_DEBUG("unable to get network bonding value for id:%d", base_req->id);
	} else
		req_data->network_bonding = user_network_bonding;

	req_data->pkg_name = base_slot->pkgname;

	// get headers list from header table(DB)
	int headers_count = dp_db_check_duplicated_int(base_slot->client.dbhandle, DP_TABLE_HEADERS, DP_DB_COL_ID, base_req->id, &errorcode);
	if (headers_count > 0) {
		req_data->request_header = calloc(headers_count, sizeof(char *));
		if (req_data->request_header != NULL) {
				headers_count = dp_db_get_http_headers_list(base_slot->client.dbhandle, base_req->id, (char **)req_data->request_header, &errorcode);
				if (headers_count > 0)
					req_data->request_header_count = headers_count;
		}
	}

	req_data->user_client_data = (void *)slot;
	req_data->user_req_data = (void *)request;

	// call start API of agent lib
	if (download_agent_start != NULL)
		da_ret = (*download_agent_start)(url, req_data, &da_cb, &req_dl_id);
	if (req_data->request_header_count > 0) {
		int len = 0;
		int i = 0;
		len = req_data->request_header_count;
		for (i = 0; i < len; i++)
			free((void *)(req_data->request_header[i]));
		free(req_data->request_header);
	}
	free(url);
	free(destination);
	free(tmp_saved_path);
	free(user_tmp_file_path);
	free(filename);
	free(etag);
	free(req_data);

	if (da_ret == DA_RESULT_OK) {
		TRACE_DEBUG("request id :%d SUCCESS agent_id:%d", base_req->id, req_dl_id);
		base_req->agent_id = req_dl_id;
	}
	return __change_error(da_ret);
}

int dp_resume_agent_download(int req_id)
{
	int da_ret = -1;
	if (req_id < 0) {
		TRACE_ERROR("[NULL-CHECK] req_id");
		return DP_ERROR_INVALID_PARAMETER;
	}
	if (download_agent_resume != NULL)
		da_ret = (*download_agent_resume)(req_id);
	if (da_ret == DA_RESULT_OK)
		return DP_ERROR_NONE;
	else if (da_ret == DA_ERR_INVALID_STATE)
		return DP_ERROR_INVALID_STATE;
	return __change_error(da_ret);
}

