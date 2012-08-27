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
 * @file		download-agent-dl-info-util.c
 * @brief		create and destroy functions for download_info
 * @author		Keunsoon Lee(keunsoon.lee@samsung.com)
 * @author		Jungki Kwak(jungki.kwak@samsung.com)
 ***/

#include <string.h>

#include "download-agent-client-mgr.h"
#include "download-agent-dl-info-util.h"
#include "download-agent-debug.h"
#include "download-agent-utils.h"
#include "download-agent-file.h"
#include "download-agent-http-mgr.h"
#include "download-agent-plugin-conf.h"

pthread_mutex_t mutex_download_state[DA_MAX_DOWNLOAD_ID];
static pthread_mutex_t mutex_download_mgr = PTHREAD_MUTEX_INITIALIZER;
download_mgr_t download_mgr;

void init_state_watcher(state_watcher_t *state_watcher);
void cleanup_state_watcher(state_watcher_t *state_watcher);

da_result_t set_default_header_info(void);

void cleanup_source_info_basic_download(source_info_basic_t *source_info_basic);
void cleanup_req_dl_info_http(req_dl_info *http_download);
void destroy_file_info(file_info *file);

da_result_t init_download_mgr() {
	da_result_t ret = DA_RESULT_OK;
	int i = 0;

	DA_LOG_FUNC_START(Default);

	if (is_client_app_mgr_init() == DA_FALSE) {
		DA_LOG_ERR(Default, "client app manager is not initiated!");
		return DA_ERR_INVALID_CLIENT;
	}

	_da_thread_mutex_lock(&mutex_download_mgr);

	if (download_mgr.is_init == DA_FALSE) {
		download_mgr.is_init = DA_TRUE;

		for (i = 0; i < DA_MAX_DOWNLOAD_ID; i++) {
			_da_thread_mutex_init(&mutex_download_state[i], DA_NULL);
			init_download_info(i);
		}

		init_state_watcher(&(download_mgr.state_watcher));
		init_dl_req_id_history(&(download_mgr.dl_req_id_history));

		download_mgr.default_hdr_info = (default_http_hdr_info_t *) calloc(1,
				sizeof(default_http_hdr_info_t));
		if (download_mgr.default_hdr_info) {
			/* Don't check return value,
			 *  because UA and UAprof is not mandatory field at HTTP request header */
			set_default_header_info();
		} else {
			DA_LOG_ERR(Default, "DA_ERR_FAIL_TO_MEMALLOC");
			ret = DA_ERR_FAIL_TO_MEMALLOC;
		}
	}

	_da_thread_mutex_unlock(&mutex_download_mgr);

	return ret;
}

da_result_t deinit_download_mgr(void) {
	da_result_t ret = DA_RESULT_OK;

	DA_LOG_FUNC_START(Default);

	_da_thread_mutex_lock(&mutex_download_mgr);
	if (download_mgr.is_init == DA_TRUE) {
		int i = 0;
		download_info_t *dl_info = DA_NULL;
		void *t_return = NULL;
		for (i = 0; i < DA_MAX_DOWNLOAD_ID; i++) {
			dl_info = &(download_mgr.download_info[i]);
			if (dl_info && dl_info->is_using) {
				request_to_abort_http_download(GET_DL_CURRENT_STAGE(i));
				DA_LOG_CRITICAL(Thread, "===download id[%d] thread id[%lu] join===",i, GET_DL_THREAD_ID(i));
/* Because the download daemon can call the deinit function, the resources of pthread are not freed
   FIXME later : It is needed to change the termination flow again.
		if (pthread_join(GET_DL_THREAD_ID(i), &t_return) < 0) {
			DA_LOG_ERR(Thread, "join client thread is failed!!!");
		}
*/
		DA_LOG_CRITICAL(Thread, "===download id[%d] thread join return[%d]===",i, (char*)t_return);
}
}
		download_mgr.is_init = DA_FALSE;
		cleanup_state_watcher(&(download_mgr.state_watcher));
		if (NULL != download_mgr.default_hdr_info) {
			if (NULL != download_mgr.default_hdr_info->user_agent_string)
				free(download_mgr.default_hdr_info->user_agent_string);

			download_mgr.default_hdr_info->user_agent_string = NULL;

			free(download_mgr.default_hdr_info);
			download_mgr.default_hdr_info = NULL;
		}
		deinit_dl_req_id_history(&(download_mgr.dl_req_id_history));
	}
	_da_thread_mutex_unlock(&mutex_download_mgr);
	return ret;
}

da_result_t set_default_header_info(void)
{
	da_result_t ret = DA_RESULT_OK;

	ret = get_user_agent_string(
	                &download_mgr.default_hdr_info->user_agent_string);

	if (DA_RESULT_OK != ret)
		goto ERR;
	else
		DA_LOG(Default, "download_mgr.default_hdr_info->user_agent_string = %s ",
					download_mgr.default_hdr_info->user_agent_string);
ERR:
	return ret;
}

void init_download_info(int download_id)
{
	download_info_t *dl_info = DA_NULL;

//	DA_LOG_FUNC_START(Default);

	_da_thread_mutex_lock(&mutex_download_state[download_id]);
	DA_LOG(Default, "Init download_id [%d] Info", download_id);
	dl_info = &(download_mgr.download_info[download_id]);

	dl_info->is_using = DA_FALSE;
	dl_info->state = DOWNLOAD_STATE_IDLE;
	dl_info->download_stage_data = DA_NULL;
	dl_info->dl_req_id = DA_NULL;
	dl_info->user_install_path = DA_NULL;
	dl_info->user_data = DA_NULL;

	Q_init_queue(&(dl_info->queue));

	DA_LOG(Default, "Init download_id [%d] Info END", download_id);
	_da_thread_mutex_unlock(&mutex_download_state[download_id]);

	return;
}

void destroy_download_info(int download_id)
{
	download_info_t *dl_info = DA_NULL;

	DA_LOG_FUNC_START(Default);

	DA_LOG(Default, "Destroying download_id [%d] Info", download_id);

	if (download_id == DA_INVALID_ID) {
		DA_LOG_ERR(Default, "invalid download_id");
		return;
	}

	dl_info = &(download_mgr.download_info[download_id]);
	if (DA_FALSE == dl_info->is_using) {
/*		DA_LOG_ERR(Default, "invalid download_id"); */
		return;
	}

	_da_thread_mutex_lock (&mutex_download_state[download_id]);
	dl_info->state = DOWNLOAD_STATE_IDLE;
	DA_LOG(Default, "Changed download_state to - [%d] ", dl_info->state);

	dl_info->active_dl_thread_id = 0;

	if (dl_info->download_stage_data != DA_NULL) {
		remove_download_stage(download_id, dl_info->download_stage_data);
		dl_info->download_stage_data = DA_NULL;
	}
	dl_info->dl_req_id = DA_NULL;
	if (dl_info->user_install_path) {
		free(dl_info->user_install_path);
		dl_info->user_install_path = DA_NULL;
	}
	dl_info->user_data = DA_NULL;
	dl_info->cur_da_state = DA_STATE_WAITING;

	Q_destroy_queue(&(dl_info->queue));

	dl_info->is_using = DA_FALSE;

	DA_LOG(Default, "Destroying download_id [%d] Info END", download_id);
	_da_thread_mutex_unlock (&mutex_download_state[download_id]);
	return;
}

void *Add_new_download_stage(int download_id)
{
	stage_info *download_stage_data = NULL;
	stage_info *new_download_stage_data = NULL;

	DA_LOG_FUNC_START(Default);

	new_download_stage_data = (stage_info*)calloc(1, sizeof(stage_info));
	if (!new_download_stage_data)
		goto ERR;

	new_download_stage_data->dl_id = download_id;
	download_stage_data = GET_DL_CURRENT_STAGE(download_id);
	if (download_stage_data) {
		while (download_stage_data->next_stage_info) {
			download_stage_data
			                = download_stage_data->next_stage_info;
		};
		download_stage_data->next_stage_info = new_download_stage_data;
	} else {
		GET_DL_CURRENT_STAGE(download_id) = new_download_stage_data;
	}
	DA_LOG(Default, "NEW STAGE ADDED FOR DOWNLOAD ID[%d] new_stage[%p]", download_id,new_download_stage_data);

ERR:
	return new_download_stage_data;
}

void remove_download_stage(int download_id, stage_info *in_stage)
{
	stage_info *stage = DA_NULL;

	DA_LOG_FUNC_START(Default);

	stage = GET_DL_CURRENT_STAGE(download_id);
	if (DA_NULL == stage) {
		DA_LOG_VERBOSE(Default, "There is no stage field on download_id = %d", download_id);
		goto ERR;
	}

	if (DA_NULL == in_stage) {
		DA_LOG_VERBOSE(Default, "There is no in_stage to remove.");
		goto ERR;
	}

	if (in_stage == stage) {
		DA_LOG_VERBOSE(Default, "Base stage will be removed. in_stage[%p]",in_stage);
		DA_LOG_VERBOSE(Default, "next stage[%p]",stage->next_stage_info);
		GET_DL_CURRENT_STAGE(download_id) = stage->next_stage_info;
		empty_stage_info(in_stage);
		free(in_stage);
		in_stage = DA_NULL;
	} else {
		while (in_stage != stage->next_stage_info) {
			stage = stage->next_stage_info;
		}
		if (in_stage == stage->next_stage_info) {
			stage->next_stage_info
				= stage->next_stage_info->next_stage_info;
			DA_LOG_VERBOSE(Default, "Stage will be removed. in_stage[%p]",in_stage);
			DA_LOG_VERBOSE(Default, "next stage[%p]",stage->next_stage_info);
			empty_stage_info(in_stage);
			free(in_stage);
			in_stage = DA_NULL;
		}
	}

ERR:
	return;
}

void empty_stage_info(stage_info *in_stage)
{
	source_info_t *source_information = NULL;
	req_dl_info *request_download_info = NULL;
	file_info *file_information = NULL;

	DA_LOG_FUNC_START(Default);

	DA_LOG(Default, "Stage to Remove:[%p]", in_stage);
	source_information = GET_STAGE_SOURCE_INFO(in_stage);

	cleanup_source_info_basic_download(
	                GET_SOURCE_BASIC(source_information));

	request_download_info = GET_STAGE_TRANSACTION_INFO(in_stage);

	cleanup_req_dl_info_http(request_download_info);

	file_information = GET_STAGE_CONTENT_STORE_INFO(in_stage);
	destroy_file_info(file_information);
}

void cleanup_source_info_basic_download(source_info_basic_t *source_info_basic)
{
	if (NULL == source_info_basic)
		goto ERR;

	DA_LOG_FUNC_START(Default);

	if (NULL != source_info_basic->url) {
		free(source_info_basic->url);
		source_info_basic->url = DA_NULL;
	}

ERR:
	return;

}

void cleanup_req_dl_info_http(req_dl_info *http_download)
{
	DA_LOG_FUNC_START(Default);

	if (http_download->http_info.http_msg_request) {
		http_msg_request_destroy(
		                &(http_download->http_info.http_msg_request));
	}

	if (http_download->http_info.http_msg_response) {
		http_msg_response_destroy(
		                &(http_download->http_info.http_msg_response));
	}

	if (DA_NULL != http_download->location_url) {
		free(http_download->location_url);
		http_download->location_url = DA_NULL;
	}
	if (DA_NULL != http_download->content_type_from_header) {
		free(http_download->content_type_from_header);
		http_download->content_type_from_header = DA_NULL;
	}

	if (DA_NULL != http_download->etag_from_header) {
		free(http_download->etag_from_header);
		http_download->etag_from_header = DA_NULL;
	}

	http_download->invloved_transaction_id = DA_INVALID_ID;
	http_download->content_len_from_header = 0;
	http_download->downloaded_data_size = 0;

	_da_thread_mutex_destroy(&(http_download->mutex_http_state));

	return;
}

void destroy_file_info(file_info *file_information)
{
//	DA_LOG_FUNC_START(Default);

	if (!file_information)
		return;

	if (file_information->file_name_tmp) {
		free(file_information->file_name_tmp);
		file_information->file_name_tmp = NULL;
	}

	if (file_information->file_name_final) {
		free(file_information->file_name_final);
		file_information->file_name_final = NULL;
	}

	if (file_information->content_type) {
		free(file_information->content_type);
		file_information->content_type = NULL;
	}

	if (file_information->pure_file_name) {
		free(file_information->pure_file_name);
		file_information->pure_file_name = NULL;
	}

	if (file_information->extension) {
		free(file_information->extension);
		file_information->extension = NULL;
	}
	return;
}

void clean_up_client_input_info(client_input_t *client_input)
{
	DA_LOG_FUNC_START(Default);

	if (client_input) {
		client_input->user_data = NULL;

		if (client_input->install_path) {
			free(client_input->install_path);
			client_input->install_path = DA_NULL;
		}

		if (client_input->file_name) {
			free(client_input->file_name);
			client_input->file_name = DA_NULL;
		}

		client_input_basic_t *client_input_basic =
		                &(client_input->client_input_basic);

		if (client_input_basic && client_input_basic->req_url) {
			free(client_input_basic->req_url);
			client_input_basic->req_url = DA_NULL;
		}

		if (client_input_basic && client_input_basic->user_request_header) {
			int i = 0;
			int count = client_input_basic->user_request_header_count;
			for (i = 0; i < count; i++)
			{
				if (client_input_basic->user_request_header[i]) {
					free(client_input_basic->user_request_header[i]);
					client_input_basic->user_request_header[i] = DA_NULL;
				}
			}

			free(client_input_basic->user_request_header);
			client_input_basic->user_request_header = DA_NULL;
			client_input_basic->user_request_header_count = 0;
		}
	} else {
		DA_LOG_ERR(Default, "client_input is NULL.");
	}

	return;
}

da_result_t get_download_id_for_dl_req_id(
        da_handle_t dl_req_id,
        da_handle_t* download_id)
{
	da_result_t ret = DA_ERR_INVALID_DL_REQ_ID;
	int iter = 0;

	if (dl_req_id < 0) {
		DA_LOG_ERR(Default, "dl_req_id is less than 0 - %d", dl_req_id);
		return DA_ERR_INVALID_DL_REQ_ID;
	}

	_da_thread_mutex_lock(&mutex_download_mgr);
	for (iter = 0; iter < DA_MAX_DOWNLOAD_ID; iter++) {
		if (download_mgr.download_info[iter].is_using == DA_TRUE) {
			if (download_mgr.download_info[iter].dl_req_id ==
				dl_req_id) {
				*download_id = iter;
				ret = DA_RESULT_OK;
				break;
			}
		}
	}
	_da_thread_mutex_unlock(&mutex_download_mgr);

	return ret;
}


da_result_t get_available_download_id(da_handle_t *available_id)
{
	da_result_t ret = DA_ERR_ALREADY_MAX_DOWNLOAD;
	int i;

	_da_thread_mutex_lock(&mutex_download_mgr);
	for (i = 0; i < DA_MAX_DOWNLOAD_ID; i++) {
		if (download_mgr.download_info[i].is_using == DA_FALSE) {
			init_download_info(i);

			download_mgr.download_info[i].is_using = DA_TRUE;

			download_mgr.download_info[i].dl_req_id
				= get_available_dl_req_id(&(download_mgr.dl_req_id_history));

			*available_id = i;
			DA_LOG_CRITICAL(Default, "available download id = %d", *available_id);
			ret = DA_RESULT_OK;

			break;
		}
	}
	_da_thread_mutex_unlock(&mutex_download_mgr);

	return ret;
}

da_bool_t is_valid_dl_ID( download_id)
{
	da_bool_t ret = DA_FALSE;

	if (download_id >= 0 && download_id < DA_MAX_DOWNLOAD_ID) {
		if (download_mgr.download_info[download_id].is_using == DA_TRUE)
			ret = DA_TRUE;
	}

	return ret;
}

void init_state_watcher(state_watcher_t *state_watcher)
{
	DA_LOG_FUNC_START(Default);

	_da_thread_mutex_init(&(state_watcher->mutex), DA_NULL);

	_da_thread_mutex_lock(&(state_watcher->mutex));
	state_watcher->type = STATE_WATCHER_TYPE_NONE;
	state_watcher->state_watching_bitmap = 0;
	state_watcher->is_progressing_to_all = DA_FALSE;
	_da_thread_mutex_unlock(&(state_watcher->mutex));
}

void cleanup_state_watcher(state_watcher_t *state_watcher)
{
	DA_LOG_FUNC_START(Default);

	_da_thread_mutex_lock(&(state_watcher->mutex));
	state_watcher->type = STATE_WATCHER_TYPE_NONE;
	state_watcher->state_watching_bitmap = 0;
	state_watcher->is_progressing_to_all = DA_FALSE;
	_da_thread_mutex_unlock(&(state_watcher->mutex));

	_da_thread_mutex_destroy(&(state_watcher->mutex));
}

void state_watcher_flag_ON_for_download_id(
                state_watcher_t *state_watcher,
                int download_id)
{
	unsigned short mask = 0;

	_da_thread_mutex_lock(&(state_watcher->mutex));

	mask = 1 << download_id;

	state_watcher->state_watching_bitmap |= mask;

	DA_LOG(Default, "state_watcher [%d], download_id [%d]", state_watcher->state_watching_bitmap, download_id);

	_da_thread_mutex_unlock(&(state_watcher->mutex));
}

void state_watcher_flag_OFF_for_download_id(
                state_watcher_t *state_watcher,
                int download_id)
{
	unsigned short mask = 0;

	_da_thread_mutex_lock(&(state_watcher->mutex));

	mask = ~(1 << download_id);

	state_watcher->state_watching_bitmap &= mask;

	DA_LOG(Default, "state_watcher [%d], download_id [%d]", state_watcher->state_watching_bitmap, download_id);

	_da_thread_mutex_unlock(&(state_watcher->mutex));
}

da_bool_t state_watcher_is_flag_on_for_download_id_Q(
                state_watcher_t *state_watcher,
                int download_id)
{
	da_bool_t b_ret = DA_FALSE;
	unsigned short mask = 0;
	unsigned short result = 0;

	_da_thread_mutex_lock(&(state_watcher->mutex));

	mask = 1 << download_id;

	result = state_watcher->state_watching_bitmap & mask;

	/*	DA_LOG(Default, "state_watcher after [%d]", state_watcher->state_watching_bitmap); */

	_da_thread_mutex_unlock(&(state_watcher->mutex));

	if (result)
		b_ret = DA_TRUE;
	else
		b_ret = DA_FALSE;

	return b_ret;
}

da_bool_t state_watcher_need_redirect_Q(int download_id)
{
	da_bool_t b_ret = DA_FALSE;
	state_watcher_t *state_watcher = DA_NULL;

	state_watcher = &(download_mgr.state_watcher);

	b_ret = state_watcher_is_flag_on_for_download_id_Q(state_watcher,
	                download_id);

	DA_LOG(Default, "state_watcher - need to redirect? [%d]", b_ret);

	return b_ret;
}

void state_watcher_redirect_state(int download_id, da_state state, int err)
{
	state_watcher_t *state_watcher = DA_NULL;

	DA_LOG_FUNC_START(Default);

	DA_LOG(Default, "download_id = [%d], receiving state = [%d]", download_id, state);

	state_watcher = &(download_mgr.state_watcher);

	switch (state) {
	case DA_STATE_FAILED:
	case DA_STATE_FINISHED:
		state_watcher_flag_OFF_for_download_id(state_watcher,
		                download_id);
		send_client_da_state(download_id, state, err);

		break;

	case DA_STATE_CANCELED:
		state_watcher_flag_OFF_for_download_id(state_watcher,
		                download_id);

		if (state_watcher->type == STATE_WATCHER_TYPE_CANCEL) {
			if (!(state_watcher->state_watching_bitmap) &&
				(state_watcher->is_progressing_to_all
							== DA_FALSE)) {
				send_client_da_state(download_id,
						DA_STATE_CANCELED_ALL,
						DA_RESULT_OK);
			}
		} else {
			send_client_da_state(download_id, state, err);
		}

		break;

	case DA_STATE_SUSPENDED:
		state_watcher_flag_OFF_for_download_id(state_watcher,
		                download_id);

		if (state_watcher->type == STATE_WATCHER_TYPE_SUSPEND) {
			if (!(state_watcher->state_watching_bitmap) &&
				(state_watcher->is_progressing_to_all
			                                == DA_FALSE)) {
				send_client_da_state(download_id,
				                DA_STATE_SUSPENDED_ALL,
				                DA_RESULT_OK);
			}
		}

		break;

	case DA_STATE_RESUMED:
		state_watcher_flag_OFF_for_download_id(state_watcher,
		                download_id);

		if (state_watcher->type == STATE_WATCHER_TYPE_RESUME) {
			if (!(state_watcher->state_watching_bitmap) &&
				(state_watcher->is_progressing_to_all
			                                == DA_FALSE)) {
				send_client_da_state(download_id,
						DA_STATE_RESUMED_ALL,
						DA_RESULT_OK);
			}
		}

		break;

	default:
		DA_LOG(Default, "blocked...");
		break;
	}
}
