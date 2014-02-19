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

#include "download-agent-client-mgr.h"
#include "download-agent-debug.h"
#include "download-agent-dl-mgr.h"
#include "download-agent-utils.h"
#include "download-agent-http-mgr.h"
#include "download-agent-file.h"
#include "download-agent-plugin-conf.h"


static da_result_t __cancel_download_with_slot_id(int slot_id);
static da_result_t __suspend_download_with_slot_id(int slot_id);


da_result_t requesting_download(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	req_dl_info *request_session = DA_NULL;

	DA_LOG_FUNC_LOGV(Default);

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
		DA_LOG_VERBOSE(Default, "Http download is complete.");
	} else {
		DA_LOG_ERR(Default, "Http download is failed. ret = %d", ret);
		goto ERR;
	}
ERR:
	return ret;
}

da_result_t handle_after_download(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	da_mime_type_id_t mime_type = DA_MIME_TYPE_NONE;

	DA_LOG_FUNC_LOGV(Default);

	mime_type = get_mime_type_id(
	                GET_CONTENT_STORE_CONTENT_TYPE(GET_STAGE_CONTENT_STORE_INFO(stage)));

	switch (mime_type) {
		case DA_MIME_TYPE_NONE:
			DA_LOG(Default, "DA_MIME_TYPE_NONE");
			ret = DA_ERR_MISMATCH_CONTENT_TYPE;
			break;
		default:
			CHANGE_DOWNLOAD_STATE(DOWNLOAD_STATE_FINISH, stage);
			break;
	} /* end of switch */

	return ret;
}

static da_result_t __cancel_download_with_slot_id(int slot_id)
{
	da_result_t ret = DA_RESULT_OK;
	download_state_t download_state;
	stage_info *stage = DA_NULL;

	DA_LOG_FUNC_LOGD(Default);

	_da_thread_mutex_lock (&mutex_download_state[slot_id]);
	download_state = GET_DL_STATE_ON_ID(slot_id);
	DA_LOG(Default, "download_state = %d", GET_DL_STATE_ON_ID(slot_id));

	if (download_state == DOWNLOAD_STATE_FINISH ||
			download_state == DOWNLOAD_STATE_CANCELED) {
		DA_LOG_CRITICAL(Default, "Already download is finished. Do not send cancel request");
		_da_thread_mutex_unlock (&mutex_download_state[slot_id]);
		return ret;
	}
	_da_thread_mutex_unlock (&mutex_download_state[slot_id]);

	stage = GET_DL_CURRENT_STAGE(slot_id);
	if (!stage)
		return DA_RESULT_OK;

	ret = request_to_cancel_http_download(stage);
	if (ret != DA_RESULT_OK)
		goto ERR;
	DA_LOG(Default, "Download cancel Successful for download id - %d", slot_id);
ERR:
	return ret;
}

da_result_t cancel_download(int dl_id)
{
	da_result_t ret = DA_RESULT_OK;

	int slot_id = DA_INVALID_ID;

	DA_LOG_FUNC_LOGD(Default);

	ret = get_slot_id_for_dl_id(dl_id, &slot_id);
	if (ret != DA_RESULT_OK) {
		DA_LOG_ERR(Default, "dl req ID is not Valid");
		goto ERR;
	}

	if (DA_FALSE == is_valid_slot_id(slot_id)) {
		DA_LOG_ERR(Default, "Download ID is not Valid");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}

	ret = __cancel_download_with_slot_id(slot_id);

ERR:
	return ret;

}

static da_result_t __suspend_download_with_slot_id(int slot_id)
{
	da_result_t ret = DA_RESULT_OK;
	download_state_t download_state;
	stage_info *stage = DA_NULL;

	DA_LOG_FUNC_LOGD(Default);

	_da_thread_mutex_lock (&mutex_download_state[slot_id]);
	download_state = GET_DL_STATE_ON_ID(slot_id);
	DA_LOG(Default, "download_state = %d", GET_DL_STATE_ON_ID(slot_id));
	_da_thread_mutex_unlock (&mutex_download_state[slot_id]);

	stage = GET_DL_CURRENT_STAGE(slot_id);
	if (!stage)
		return DA_ERR_CANNOT_SUSPEND;

	ret = request_to_suspend_http_download(stage);
	if (ret != DA_RESULT_OK)
		goto ERR;
	DA_LOG(Default, "Download Suspend Successful for download id-%d", slot_id);
ERR:
	return ret;
}

da_result_t suspend_download(int dl_id, da_bool_t is_enable_cb)
{
	da_result_t ret = DA_RESULT_OK;
	int slot_id = DA_INVALID_ID;

	DA_LOG_FUNC_LOGD(Default);

	ret = get_slot_id_for_dl_id(dl_id, &slot_id);
	if (ret != DA_RESULT_OK) {
		DA_LOG_ERR(Default, "dl req ID is not Valid");
		goto ERR;
	}
	GET_DL_ENABLE_PAUSE_UPDATE(slot_id) = is_enable_cb;
	if (DA_FALSE == is_valid_slot_id(slot_id)) {
		DA_LOG_ERR(Default, "Download ID is not Valid");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}

	ret = __suspend_download_with_slot_id(slot_id);

ERR:
	return ret;

}

static da_result_t __resume_download_with_slot_id(int slot_id)
{
	da_result_t ret = DA_RESULT_OK;
	download_state_t download_state;
	stage_info *stage = DA_NULL;

	DA_LOG_FUNC_LOGD(Default);

	_da_thread_mutex_lock (&mutex_download_state[slot_id]);
	download_state = GET_DL_STATE_ON_ID(slot_id);
	DA_LOG(Default, "download_state = %d", GET_DL_STATE_ON_ID(slot_id));
	_da_thread_mutex_unlock (&mutex_download_state[slot_id]);

	stage = GET_DL_CURRENT_STAGE(slot_id);

	ret = request_to_resume_http_download(stage);
	if (ret != DA_RESULT_OK)
		goto ERR;
	DA_LOG(Default, "Download Resume Successful for download id-%d", slot_id);
ERR:
	return ret;
}

da_result_t resume_download(int dl_id)
{
	da_result_t ret = DA_RESULT_OK;
	int slot_id = DA_INVALID_ID;

	DA_LOG_FUNC_LOGD(Default);

	ret = get_slot_id_for_dl_id(dl_id, &slot_id);
	if (ret != DA_RESULT_OK)
		goto ERR;

	if (DA_FALSE == is_valid_slot_id(slot_id)) {
		DA_LOG_ERR(Default, "Download ID is not Valid");
		ret = DA_ERR_INVALID_DL_REQ_ID;
		goto ERR;
	}

	ret = __resume_download_with_slot_id(slot_id);

ERR:
	return ret;
}

da_result_t send_user_noti_and_finish_download_flow(
		int slot_id, char *installed_path, char *etag)
{
	da_result_t ret = DA_RESULT_OK;
	download_state_t download_state = HTTP_STATE_READY_TO_DOWNLOAD;
	da_bool_t need_destroy_download_info = DA_FALSE;

	DA_LOG_FUNC_LOGV(Default);

	_da_thread_mutex_lock (&mutex_download_state[slot_id]);
	download_state = GET_DL_STATE_ON_ID(slot_id);
	DA_LOG_DEBUG(Default, "state = %d", download_state);
	_da_thread_mutex_unlock (&mutex_download_state[slot_id]);

	switch (download_state) {
	case DOWNLOAD_STATE_FINISH:
		send_client_finished_info(slot_id, GET_DL_ID(slot_id),
		        installed_path, DA_NULL, DA_RESULT_OK,
		        get_http_status(slot_id));
		need_destroy_download_info = DA_TRUE;
		break;
	case DOWNLOAD_STATE_CANCELED:
		send_client_finished_info(slot_id, GET_DL_ID(slot_id),
				installed_path, etag, DA_RESULT_USER_CANCELED,
				get_http_status(slot_id));
		need_destroy_download_info = DA_TRUE;
		break;
#ifdef PAUSE_EXIT
	case DOWNLOAD_STATE_PAUSED:
		need_destroy_download_info = DA_TRUE;
		break;
#endif
	default:
		DA_LOG(Default, "download state = %d", download_state);
		break;
	}

	if (need_destroy_download_info == DA_TRUE) {
		destroy_download_info(slot_id);
	} else {
		DA_LOG_CRITICAL(Default, "download info is not destroyed");
	}

	return ret;
}

da_bool_t is_valid_download_id(int dl_id)
{

	da_bool_t ret = DA_TRUE;
	int slot_id = DA_INVALID_ID;

	DA_LOG_VERBOSE(Default, "[is_valid_download_id]download_id : %d", dl_id);

	ret = get_slot_id_for_dl_id(dl_id, &slot_id);
	if (ret != DA_RESULT_OK) {
		DA_LOG_ERR(Default, "dl req ID is not Valid");
		ret = DA_FALSE;
		goto ERR;
	} else {
		ret = DA_TRUE;
	}

	if (DA_FALSE == is_valid_slot_id(slot_id)) {
		DA_LOG_ERR(Default, "Download ID is not Valid");
		ret = DA_FALSE;
		goto ERR;
	}
	if (GET_DL_THREAD_ID(slot_id) < 1) {
		DA_LOG_ERR(Default, "Download thread is not alive");
		ret = DA_FALSE;
		goto ERR;
	}

ERR:
	return ret;
}
