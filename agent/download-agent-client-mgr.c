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

#include <stdlib.h>

#include "download-agent-client-mgr.h"

da_ret_t send_client_paused_info(da_info_t *da_info)
{
	req_info_t *req_info = DA_NULL;
	NULL_CHECK_RET(da_info);
	req_info = da_info->req_info;
	NULL_CHECK_RET(req_info);

	if (da_info->is_cb_update && da_info->cb_info.paused_cb) {
		da_info->cb_info.paused_cb(da_info->da_id,
				req_info->user_req_data, req_info->user_client_data);
		DA_LOGV("id[%d]", da_info->da_id);
	} else {
		DA_LOGV("No CB:id[%d]", da_info->da_id);
	}

	return DA_RESULT_OK;
}

da_ret_t send_client_update_dl_info(da_info_t *da_info)
{
	download_info_t *info = DA_NULL;
	file_info_t *file_info = DA_NULL;
	http_info_t *http_info = DA_NULL;
	req_info_t *req_info = DA_NULL;
	NULL_CHECK_RET(da_info);
	file_info = da_info->file_info;
	NULL_CHECK_RET(file_info);
	req_info = da_info->req_info;
	NULL_CHECK_RET(req_info);
	http_info = da_info->http_info;
	NULL_CHECK_RET(http_info);

	if (da_info->is_cb_update && da_info->cb_info.download_info_cb) {
		info = (download_info_t *)calloc(1, sizeof(download_info_t));
		if (!info)
			return DA_ERR_FAIL_TO_MEMALLOC;
		info->download_id = da_info->da_id;
		info->file_size = http_info->content_len_from_header;
		if (http_info->content_type_from_header)
			info->file_type = strdup(http_info->content_type_from_header);
		if (file_info->file_path)
			info->tmp_saved_path = strdup(file_info->file_path);
		if (file_info->pure_file_name)
			info->content_name = strdup(file_info->pure_file_name);
		if (http_info->etag_from_header) {
			info->etag = strdup(http_info->etag_from_header);
			//DA_SECURE_LOGI("etag[%s]", info->etag);
		}
		da_info->cb_info.download_info_cb(info,
				req_info->user_req_data, req_info->user_client_data);
		DA_LOGD("id[%d]", info->download_id);
		//DA_LOGI("id[%d]total_size[%lu]", info->download_id, info->file_size);
		//if (http_info->content_type_from_header)
			//DA_SECURE_LOGI("mime_type[%s]", http_info->content_type_from_header);
	} else {
		DA_LOGI("No CB:id[%d]", da_info->da_id);
	}
	return DA_RESULT_OK;
}

da_ret_t send_client_update_progress_info(da_info_t *da_info)
{
	file_info_t *file_info = DA_NULL;
	req_info_t *req_info = DA_NULL;
	NULL_CHECK_RET(da_info);
	file_info = da_info->file_info;
	NULL_CHECK_RET(file_info);
	req_info = da_info->req_info;
	NULL_CHECK_RET(req_info);

	if (!file_info->is_updated)
		return DA_RESULT_OK;

	if (da_info->is_cb_update && da_info->cb_info.progress_cb) {
		da_info->cb_info.progress_cb(da_info->da_id,
				file_info->bytes_written_to_file,
				req_info->user_req_data, req_info->user_client_data);
		DA_LOGV("id[%d],size[%llu]", da_info->da_id,
				file_info->bytes_written_to_file);
	} else {
		DA_LOGI("No CB:id[%d]", da_info->da_id);
	}
	file_info->is_updated = DA_FALSE;
	return DA_RESULT_OK;
}

da_ret_t send_client_finished_info(da_info_t *da_info, int err)
{
	finished_info_t *info = DA_NULL;
	file_info_t *file_info = DA_NULL;
	http_info_t *http_info = DA_NULL;
	req_info_t *req_info = DA_NULL;
	NULL_CHECK_RET(da_info);
	file_info = da_info->file_info;
	NULL_CHECK_RET(file_info);
	req_info = da_info->req_info;
	NULL_CHECK_RET(req_info);
	http_info = da_info->http_info;
	NULL_CHECK_RET(http_info);

	if (da_info->is_cb_update && da_info->cb_info.finished_cb) {
		info = (finished_info_t *)calloc(1, sizeof(finished_info_t));
		if (!info)
			return DA_ERR_FAIL_TO_MEMALLOC;
		info->download_id = da_info->da_id;
		if (http_info->http_msg_response)
			info->http_status = http_info->http_msg_response->status_code;
		else
			DA_LOGE("http_msg_response is NULL");
		if (file_info->file_path)
			info->saved_path = strdup(file_info->file_path);
		if (http_info->etag_from_header)
			info->etag = strdup(http_info->etag_from_header);
		info->err = err;
		da_info->cb_info.finished_cb(info,
				req_info->user_req_data, req_info->user_client_data);
		DA_LOGD("id[%d]", info->download_id);
		//DA_LOGI("id[%d],err[%d], http_status[%d]", info->download_id,
				//info->err, info->http_status);
	} else {
		DA_LOGI("No CB:id[%d]", da_info->da_id);
	}
	return DA_RESULT_OK;
}

