/*
 * Download Agent
 *
 * Copyright (c) 2000 - 2012 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Jungki Kwak <jungki.kwak@samsung.com>, Keunsoon Lee <keunsoon.lee@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file		download-agent-dl-mgr.c
 * @brief		common functions for oma and direct download
 * @author		Keunsoon Lee(keunsoon.lee@samsung.com)
 * @author		Jungki Kwak(jungki.kwak@samsung.com)
 ***/

#include "download-agent-client-mgr.h"
#include "download-agent-debug.h"
#include "download-agent-dl-mgr.h"
#include "download-agent-utils.h"
#include "download-agent-http-mgr.h"
#include "download-agent-installation.h"
#include "download-agent-file.h"
#include "download-agent-plugin-drm.h"
#include "download-agent-plugin-conf.h"


static da_result_t __cancel_download_with_download_id(int download_id);
static da_result_t __suspend_download_with_download_id(int download_id);


da_result_t requesting_download(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	req_dl_info *request_session = DA_NULL;
	download_state_t download_state = DA_INVALID_ID;

	DA_LOG_FUNC_START(Default);

	if (!stage) {
		DA_LOG_ERR(Default, "stage is null..");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}

	ret = make_req_dl_info_http(stage, GET_STAGE_TRANSACTION_INFO(stage));
	if (ret != DA_RESULT_OK)
		goto ERR;

	request_session = GET_STAGE_TRANSACTION_INFO(stage);
	ret = request_http_download(stage);
	if (DA_RESULT_OK == ret) {
		DA_LOG(Default, "Http download is complete.");
	} else {
		DA_LOG_ERR(Default, "Http download is failed. ret = %d", ret);
		goto ERR;
	}

	_da_thread_mutex_lock (&mutex_download_state[GET_STAGE_DL_ID(stage)]);
	download_state = GET_DL_STATE_ON_STAGE(stage);
	_da_thread_mutex_unlock (&mutex_download_state[GET_STAGE_DL_ID(stage)]);

	/* Ignore  install content process if the download is failed or paused or canceled */
	if (download_state == DOWNLOAD_STATE_PAUSED ||
		download_state == DOWNLOAD_STATE_CANCELED) {
		return ret;
	}

ERR:
	return ret;
}

da_result_t handle_after_download(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	da_mime_type_id_t mime_type = DA_MIME_TYPE_NONE;

	DA_LOG_FUNC_START(Default);

	if (DA_TRUE == is_this_client_manual_download_type()) {
		if (DA_RESULT_OK == ret) {
			CHANGE_DOWNLOAD_STATE(DOWNLOAD_STATE_FINISH,stage);
		}
		return ret;
	}
	mime_type = get_mime_type_id(
	                GET_CONTENT_STORE_CONTENT_TYPE(GET_STAGE_CONTENT_STORE_INFO(stage)));

	switch (mime_type) {
		case DA_MIME_PLAYREADY_INIT:
			DA_LOG_VERBOSE(Default, "DA_MIME_PLAYREADY_INIT");
			ret = process_play_ready_initiator(stage);
			break;
		case DA_MIME_TYPE_NONE:
			DA_LOG(Default, "DA_MIME_TYPE_NONE");
			ret = DA_ERR_MISMATCH_CONTENT_TYPE;
			break;
		default:
			CHANGE_DOWNLOAD_STATE(DOWNLOAD_STATE_READY_TO_INSTAL,stage);
			break;
	} /* end of switch */

	return ret;
}

da_result_t process_install(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;

	DA_LOG_FUNC_START(Default);

	if (DA_NULL == stage)
		return DA_ERR_INVALID_ARGUMENT;

	/* install process. */
	ret = install_content(stage);

	if (DA_RESULT_OK == ret) {
		int download_id = GET_STAGE_DL_ID(stage);
		char *content_type = DA_NULL;

		if (GET_CONTENT_STORE_CONTENT_TYPE(GET_STAGE_CONTENT_STORE_INFO(stage))) {
			content_type = strdup(
		                GET_CONTENT_STORE_CONTENT_TYPE(GET_STAGE_CONTENT_STORE_INFO(stage)));
		} else {
			/* Fixme : Is Content-Type on HTTP header mandatory? */
			DA_LOG_ERR(Default, "no content type!");
			ret = DA_ERR_MISMATCH_CONTENT_TYPE;
			return ret;
		}

		/* update installed path */
		send_client_update_downloading_info(
		        download_id,
		        GET_DL_REQ_ID(download_id),
		        GET_CONTENT_STORE_CURRENT_FILE_SIZE(GET_STAGE_CONTENT_STORE_INFO(stage)),
		        GET_CONTENT_STORE_ACTUAL_FILE_NAME(GET_STAGE_CONTENT_STORE_INFO(stage))
				);

		if (content_type) {
			free(content_type);
			content_type = DA_NULL;
		}

		return DA_RESULT_OK;
	}
	return ret;
}

static da_result_t __cancel_download_with_download_id(int download_id)
{
	da_result_t ret = DA_RESULT_OK;
	download_state_t download_state;
	stage_info *stage = DA_NULL;

	DA_LOG_FUNC_START(Default);

	_da_thread_mutex_lock (&mutex_download_state[download_id]);
	download_state = GET_DL_STATE_ON_ID(download_id);
	DA_LOG(Default, "download_state = %d", GET_DL_STATE_ON_ID(download_id));
	if (download_state == DOWNLOAD_STATE_IDLE) {
	/* FIXME later : It is better to check and exit itself at download thread */
		pthread_cancel(GET_DL_THREAD_ID(download_id));
	} else if (download_state >= DOWNLOAD_STATE_READY_TO_INSTAL) {
		DA_LOG_CRITICAL(Default, "Already download is completed. Do not send cancel request");
		_da_thread_mutex_unlock (&mutex_download_state[download_id]);
		return ret;
	}
	_da_thread_mutex_unlock (&mutex_download_state[download_id]);

	if (ret != DA_RESULT_OK)
		goto ERR;

	stage = GET_DL_CURRENT_STAGE(download_id);
	if (!stage)
		return DA_RESULT_OK;

	ret = request_to_cancel_http_download(stage);
	if (ret != DA_RESULT_OK)
		goto ERR;
	DA_LOG(Default, "Download cancel Successful for download id - %d", download_id);
ERR:
	return ret;
}

da_result_t cancel_download(int dl_req_id)
{
	da_result_t ret = DA_RESULT_OK;

	int download_id = DA_INVALID_ID;

	DA_LOG_FUNC_START(Default);

	if ((dl_req_id == DA_DOWNLOAD_REQ_ID_FOR_ALL_ITEMS) ||
			(dl_req_id == DA_DOWNLOAD_REQ_ID_FOR_ALL_ITEMS_WITH_UNIFIED_NOTI)) {
		DA_LOG(Default, "All download items will be cancelled");
		ret = cancel_download_all(dl_req_id);
		return ret;
	}

	ret = get_download_id_for_dl_req_id(dl_req_id, &download_id);
	if (ret != DA_RESULT_OK) {
		DA_LOG_ERR(Default, "dl req ID is not Valid");
		goto ERR;
	}

	if (DA_FALSE == is_valid_dl_ID(download_id)) {
		DA_LOG_ERR(Default, "Download ID is not Valid");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}

	ret = __cancel_download_with_download_id(download_id);

ERR:
	return ret;

}

da_result_t cancel_download_all(int dl_req_id)
{
	da_result_t ret = DA_RESULT_OK;

	int download_id = DA_INVALID_ID;
	int iter = 0;
	da_bool_t b_any_flag_on = DA_FALSE;

	state_watcher_t *state_watcher = DA_NULL;

	DA_LOG_FUNC_START(Default);

	if (dl_req_id == DA_DOWNLOAD_REQ_ID_FOR_ALL_ITEMS_WITH_UNIFIED_NOTI) {
		state_watcher = &(download_mgr.state_watcher);

		_da_thread_mutex_lock(&(state_watcher->mutex));
		state_watcher->type = STATE_WATCHER_TYPE_CANCEL;
		state_watcher->is_progressing_to_all = DA_TRUE;
		_da_thread_mutex_unlock(&(state_watcher->mutex));
	}

	for (iter = 0; iter < DA_MAX_DOWNLOAD_ID; iter++) {
		if (download_mgr.download_info[iter].is_using == DA_TRUE) {
			download_id = iter;

			if (dl_req_id == DA_DOWNLOAD_REQ_ID_FOR_ALL_ITEMS_WITH_UNIFIED_NOTI) {
				state_watcher_flag_ON_for_download_id(state_watcher,
					download_id);
			}
			b_any_flag_on = DA_TRUE;

			/* ToDo: retry if fail, check download_state, don't set flag for state_watcher if state is invalid */
			ret = __cancel_download_with_download_id(download_id);
			/*if (ret == DA_RESULT_OK) {
				state_watcher_flag_ON_for_download_id(
				        state_watcher, download_id);
				b_any_flag_on = DA_TRUE;
			}*/
		}

	}

	if (dl_req_id == DA_DOWNLOAD_REQ_ID_FOR_ALL_ITEMS_WITH_UNIFIED_NOTI) {
		_da_thread_mutex_lock(&(state_watcher->mutex));
		state_watcher->is_progressing_to_all = DA_FALSE;
		_da_thread_mutex_unlock(&(state_watcher->mutex));
	}

	if (b_any_flag_on == DA_FALSE) {
		DA_LOG_ERR(Default, "There is no item to be able to cancel.");
		ret = DA_ERR_INVALID_DL_REQ_ID;
	}

	return ret;
}

static da_result_t __suspend_download_with_download_id(int download_id)
{
	da_result_t ret = DA_RESULT_OK;
	download_state_t download_state;
	stage_info *stage = DA_NULL;

	DA_LOG_FUNC_START(Default);

	_da_thread_mutex_lock (&mutex_download_state[download_id]);
	download_state = GET_DL_STATE_ON_ID(download_id);
	DA_LOG(Default, "download_state = %d", GET_DL_STATE_ON_ID(download_id));
	_da_thread_mutex_unlock (&mutex_download_state[download_id]);

	if (ret != DA_RESULT_OK)
		goto ERR;

	stage = GET_DL_CURRENT_STAGE(download_id);
	if (!stage)
		return DA_ERR_CANNOT_SUSPEND;

	ret = request_to_suspend_http_download(stage);
	if (ret != DA_RESULT_OK)
		goto ERR;
	DA_LOG(Default, "Download Suspend Successful for download id-%d", download_id);
ERR:
	return ret;
}

da_result_t suspend_download(int dl_req_id)
{
	da_result_t ret = DA_RESULT_OK;
	int download_id = DA_INVALID_ID;

	DA_LOG_FUNC_START(Default);

	if ((dl_req_id == DA_DOWNLOAD_REQ_ID_FOR_ALL_ITEMS) ||
			(dl_req_id == DA_DOWNLOAD_REQ_ID_FOR_ALL_ITEMS_WITH_UNIFIED_NOTI)) {
		DA_LOG(Default, "All download items will be suspended");
		ret = suspend_download_all(dl_req_id);
		return ret;
	}

	ret = get_download_id_for_dl_req_id(dl_req_id, &download_id);
	if (ret != DA_RESULT_OK) {
		DA_LOG_ERR(Default, "dl req ID is not Valid");
		goto ERR;
	}

	if (DA_FALSE == is_valid_dl_ID(download_id)) {
		DA_LOG_ERR(Default, "Download ID is not Valid");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}

	ret = __suspend_download_with_download_id(download_id);

ERR:
	return ret;

}

da_result_t suspend_download_all(int dl_req_id)
{
	da_result_t ret = DA_RESULT_OK;

	int download_id = DA_INVALID_ID;
	int iter = 0;
	da_bool_t b_any_flag_on = DA_FALSE;

	state_watcher_t *state_watcher = DA_NULL;

	DA_LOG_FUNC_START(Default);

	if (dl_req_id == DA_DOWNLOAD_REQ_ID_FOR_ALL_ITEMS_WITH_UNIFIED_NOTI) {
		state_watcher = &(download_mgr.state_watcher);

		_da_thread_mutex_lock(&(state_watcher->mutex));
		state_watcher->type = STATE_WATCHER_TYPE_SUSPEND;
		state_watcher->is_progressing_to_all = DA_TRUE;
		_da_thread_mutex_unlock(&(state_watcher->mutex));
	}

	for (iter = 0; iter < DA_MAX_DOWNLOAD_ID; iter++) {
		if (download_mgr.download_info[iter].is_using == DA_TRUE) {
			download_id = iter;
			/* ToDo: retry if fail, check download_state, don't set flag for state_watcher if state is invalid */
			ret = __suspend_download_with_download_id(download_id);
			if (ret == DA_RESULT_OK) {
				if (dl_req_id == DA_DOWNLOAD_REQ_ID_FOR_ALL_ITEMS_WITH_UNIFIED_NOTI) {
					state_watcher_flag_ON_for_download_id(
				        state_watcher, download_id);
				}
				b_any_flag_on = DA_TRUE;
			}
		}

	}

	if (dl_req_id == DA_DOWNLOAD_REQ_ID_FOR_ALL_ITEMS_WITH_UNIFIED_NOTI) {
		_da_thread_mutex_lock(&(state_watcher->mutex));
		state_watcher->is_progressing_to_all = DA_FALSE;
		_da_thread_mutex_unlock(&(state_watcher->mutex));
	}

	if (b_any_flag_on == DA_FALSE) {
		DA_LOG_ERR(Default, "There is no item to be able to suspend.");
		ret = DA_ERR_CANNOT_SUSPEND;
	}

	return ret;
}

static da_result_t __resume_download_with_download_id(int download_id)
{
	da_result_t ret = DA_RESULT_OK;
	download_state_t download_state;
	stage_info *stage = DA_NULL;

	DA_LOG_FUNC_START(Default);

	_da_thread_mutex_lock (&mutex_download_state[download_id]);
	download_state = GET_DL_STATE_ON_ID(download_id);
	DA_LOG(Default, "download_state = %d", GET_DL_STATE_ON_ID(download_id));
	_da_thread_mutex_unlock (&mutex_download_state[download_id]);

	if (ret != DA_RESULT_OK)
		goto ERR;

	stage = GET_DL_CURRENT_STAGE(download_id);

	ret = request_to_resume_http_download(stage);
	if (ret != DA_RESULT_OK)
		goto ERR;
	DA_LOG(Default, "Download Resume Successful for download id-%d", download_id);
ERR:
	return ret;
}

da_result_t resume_download(int dl_req_id)
{
	da_result_t ret = DA_RESULT_OK;
	int download_id = DA_INVALID_ID;

	DA_LOG_FUNC_START(Default);

	if ((dl_req_id == DA_DOWNLOAD_REQ_ID_FOR_ALL_ITEMS) ||
			(dl_req_id == DA_DOWNLOAD_REQ_ID_FOR_ALL_ITEMS_WITH_UNIFIED_NOTI)) {
		DA_LOG(Default, "All download items will be resumed");
		ret = resume_download_all(dl_req_id);
		return ret;
	}

	ret = get_download_id_for_dl_req_id(dl_req_id, &download_id);
	if (ret != DA_RESULT_OK)
		goto ERR;

	if (DA_FALSE == is_valid_dl_ID(download_id)) {
		DA_LOG_ERR(Default, "Download ID is not Valid");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}

	ret = __resume_download_with_download_id(download_id);

ERR:
	return ret;
}

da_result_t resume_download_all(int dl_req_id)
{
	da_result_t ret = DA_RESULT_OK;

	int download_id = DA_INVALID_ID;
	int iter = 0;
	da_bool_t b_any_flag_on = DA_FALSE;

	state_watcher_t *state_watcher = DA_NULL;

	DA_LOG_FUNC_START(Default);

	if (dl_req_id == DA_DOWNLOAD_REQ_ID_FOR_ALL_ITEMS_WITH_UNIFIED_NOTI) {
		state_watcher = &(download_mgr.state_watcher);

		_da_thread_mutex_lock(&(state_watcher->mutex));
		state_watcher->type = STATE_WATCHER_TYPE_RESUME;
		state_watcher->is_progressing_to_all = DA_TRUE;
		_da_thread_mutex_unlock(&(state_watcher->mutex));
	}

	for (iter = 0; iter < DA_MAX_DOWNLOAD_ID; iter++) {
		if (download_mgr.download_info[iter].is_using == DA_TRUE) {
			download_id = iter;

			if (dl_req_id == DA_DOWNLOAD_REQ_ID_FOR_ALL_ITEMS_WITH_UNIFIED_NOTI) {
				state_watcher_flag_ON_for_download_id(state_watcher,
					download_id);
			}
			b_any_flag_on = DA_TRUE;

			/* ToDo: retry if fail, check download_state, don't set flag for state_watcher if state is invalid */
			ret = __resume_download_with_download_id(download_id);
			/*if (ret == DA_RESULT_OK) {
				state_watcher_flag_ON_for_download_id(
				        state_watcher, download_id);
				b_any_flag_on = DA_TRUE;
			}*/
		}

	}

	if (dl_req_id == DA_DOWNLOAD_REQ_ID_FOR_ALL_ITEMS_WITH_UNIFIED_NOTI) {
		_da_thread_mutex_lock(&(state_watcher->mutex));
		state_watcher->is_progressing_to_all = DA_FALSE;
		_da_thread_mutex_unlock(&(state_watcher->mutex));
	}

	if (b_any_flag_on == DA_FALSE) {
		DA_LOG(Default, "There is no item to be able to cancel.");
		ret = DA_ERR_INVALID_DL_REQ_ID;
	}

	return ret;
}

da_result_t send_user_noti_and_finish_download_flow(int download_id)
{
	da_result_t ret = DA_RESULT_OK;
	download_state_t download_state = DA_NULL;
	da_bool_t need_destroy_download_info = DA_FALSE;

	DA_LOG_FUNC_START(Default);

	_da_thread_mutex_lock (&mutex_download_state[download_id]);
	download_state = GET_DL_STATE_ON_ID(download_id);
	DA_LOG(Default, "state = %d", download_state);
	_da_thread_mutex_unlock (&mutex_download_state[download_id]);

	switch (download_state) {
	case DOWNLOAD_STATE_FINISH:
		send_client_da_state(download_id, DA_STATE_FINISHED,
				DA_RESULT_OK);
		need_destroy_download_info = DA_TRUE;
		break;
	case DOWNLOAD_STATE_CANCELED:
		send_client_da_state(download_id, DA_STATE_CANCELED,
				DA_RESULT_OK);
		need_destroy_download_info = DA_TRUE;
		break;
	default:
		DA_LOG(Default, "download state = %d", download_state);
		break;
	}

	if (need_destroy_download_info == DA_TRUE) {
		destroy_download_info(download_id);
	} else {
		DA_LOG_CRITICAL(Default, "download info is not destroyed");
	}

	return ret;
}

da_result_t process_play_ready_initiator(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	int download_id = GET_STAGE_DL_ID(stage);
	stage_info *next_stage = DA_NULL;
	source_info_t *source_info = DA_NULL;
	char *rights_url = DA_NULL;
	char *content_url = DA_NULL;

	DA_LOG_FUNC_START(Default);

	discard_download(stage);

	rights_url = GET_SOURCE_BASIC(GET_STAGE_SOURCE_INFO(stage))->url;
	DA_LOG_VERBOSE(Default, "len = %d, rights_url = %s \n", strlen(rights_url), rights_url);

	ret = EDRM_wm_get_license(rights_url, &content_url);
	if (ret != DA_RESULT_OK) {
		DA_LOG_ERR(Default, "EDRM_wm_get_license() failed.");
		goto ERR;
	}

	if (content_url) {
		DA_LOG_VERBOSE(Default, "Starting new download with content_url = [%s]", content_url);

		next_stage = Add_new_download_stage(download_id);
		if (!next_stage) {
			DA_LOG_ERR(Default, "STAGE ADDITION FAIL!");
			ret = DA_ERR_FAIL_TO_MEMALLOC;
			goto ERR;
		}

		source_info = GET_STAGE_SOURCE_INFO(next_stage);
		source_info->source_info_type.source_info_basic
		        = (source_info_basic_t*)calloc(1,
		                sizeof(source_info_basic_t));
		if (DA_NULL == source_info->source_info_type.source_info_basic) {
			DA_LOG_ERR(Default, "DA_ERR_INVALID_ARGUMENT");
			ret = DA_ERR_INVALID_ARGUMENT;
			goto ERR;
		}
		source_info->source_info_type.source_info_basic->url
		        = content_url;

		CHANGE_DOWNLOAD_STATE(DOWNLOAD_STATE_NEW_DOWNLOAD, next_stage);
		remove_download_stage(download_id, stage);
	} else {
		DA_LOG_VERBOSE(Default, "content url is null. So, do nothing with normal termination.");

		CHANGE_DOWNLOAD_STATE(DOWNLOAD_STATE_FINISH, stage);
	}

ERR:
	if (ret != DA_RESULT_OK) {
		if (content_url)
			free(content_url);
	}

	return ret;
}
