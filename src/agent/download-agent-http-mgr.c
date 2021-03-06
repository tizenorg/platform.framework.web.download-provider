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
 * @file		download-agent-http-mgr.c
 * @brief		Including functions regarding http mgr
 * @author		Keunsoon Lee(keunsoon.lee@samsung.com)
 * @author		Jungki Kwak(jungki.kwak@samsung.com)
 ***/

#include "download-agent-utils.h"
#include "download-agent-debug.h"
#include "download-agent-client-mgr.h"
#include "download-agent-http-mgr.h"
#include "download-agent-http-misc.h"
#include "download-agent-http-msg-handler.h"
#include "download-agent-file.h"
#include "download-agent-plugin-conf.h"
#include "download-agent-installation.h"
#include "download-agent-plugin-http-interface.h"

da_result_t make_default_http_request_hdr(const char *url,
		char **user_request_header,
		int user_request_heaer_count,
		http_msg_request_t **out_http_msg_request);
da_result_t create_resume_http_request_hdr(stage_info *stage,
		http_msg_request_t **out_resume_request);

da_result_t start_new_transaction(stage_info *stage);
da_result_t set_http_request_hdr(stage_info *stage);
da_result_t make_transaction_info_and_start_transaction(stage_info *stage);

da_result_t pause_for_flow_control(stage_info *stage);
da_result_t unpause_for_flow_control(stage_info *stage);

da_result_t handle_any_input(stage_info *stage);
da_result_t handle_event_control(stage_info *stage, q_event_t *event);
da_result_t handle_event_http(stage_info *stage, q_event_t *event);
da_result_t handle_event_http_packet(stage_info *stage, q_event_t *event);
da_result_t handle_event_http_final(stage_info *stage, q_event_t *event);
da_result_t handle_event_http_abort(stage_info *stage, q_event_t *event);

da_result_t exchange_url_from_header_for_redirection(stage_info *stage,
		http_msg_response_t *http_msg_response);

da_result_t handle_event_abort(stage_info *stage);
da_result_t handle_event_cancel(stage_info *stage);
da_result_t handle_event_suspend(stage_info *stage);
da_result_t handle_event_resume(stage_info *stage);

da_result_t handle_http_hdr(stage_info *stage,
		http_msg_response_t *http_msg_response, int http_status);
da_result_t handle_http_status_code(stage_info *stage,
		http_msg_response_t *http_msg_response, int http_status);
da_result_t handle_http_body(stage_info *stage, char *body, int body_len);

da_result_t set_hdr_fields_on_download_info(stage_info *stage);

da_result_t _check_content_type_is_matched(stage_info *stage);
da_result_t _check_enough_memory_for_this_download(stage_info *stage);
da_result_t _check_downloaded_file_size_is_same_with_header_content_size(
		stage_info *stage);

da_result_t _check_this_partial_download_is_available(stage_info *stage,
		http_msg_response_t *new_http_msg_response);

da_result_t _cancel_transaction(stage_info *stage);
da_result_t _disconnect_transaction(stage_info *stage);

void __parsing_user_request_header(char *user_request_header,
		char **out_field, char **out_value);

http_mgr_t http_mgr;

da_result_t init_http_mgr(void)
{
	da_result_t ret = DA_RESULT_OK;

	DA_LOG_FUNC_START(HTTPManager);

	if (http_mgr.is_http_init == DA_FALSE) {
		http_mgr.is_http_init = DA_TRUE;
		ret = PI_http_init();
	}

	return ret;
}


da_result_t request_to_abort_http_download(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	q_event_t *q_event = DA_NULL;

	DA_LOG_FUNC_START(HTTPManager);
	if (!stage) {
		DA_LOG_ERR(HTTPManager, "Stage is NULL. download info is already destroyed");
		return DA_ERR_INVALID_ARGUMENT;
	}

	DA_LOG(HTTPManager, "Q_EVENT_TYPE_CONTROL_ABORT");
	ret = Q_make_control_event(Q_EVENT_TYPE_CONTROL_ABORT, &q_event);
	if (ret != DA_RESULT_OK) {
		DA_LOG_ERR(HTTPManager, "fail to make q_control_event");
		goto ERR;
	} else {
		DA_LOG(HTTPManager, "queue = %p", GET_DL_QUEUE(GET_STAGE_DL_ID(stage)));
		Q_push_event(GET_DL_QUEUE(GET_STAGE_DL_ID(stage)), q_event);
	}

ERR:
	return ret;
}

void deinit_http_mgr(void)
{
	DA_LOG_FUNC_START(HTTPManager);

	if (http_mgr.is_http_init == DA_TRUE) {
		http_mgr.is_http_init = DA_FALSE;
		PI_http_deinit();
	}

	return;
}

da_result_t request_http_download(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;

	int download_id = DA_INVALID_ID;
	http_state_t http_state = 0;
	da_bool_t need_wait = DA_TRUE;

	queue_t *queue = DA_NULL;
	req_dl_info *req_info = DA_NULL;

	DA_LOG_FUNC_START(HTTPManager);

	download_id = GET_STAGE_DL_ID(stage);
	queue = GET_DL_QUEUE(download_id);
	req_info = GET_STAGE_TRANSACTION_INFO(stage);

	DA_LOG(HTTPManager, "queue = %p", GET_DL_QUEUE(download_id));

	CHANGE_HTTP_STATE(HTTP_STATE_READY_TO_DOWNLOAD, stage);

	do {
		ret = handle_any_input(stage);
		if (ret != DA_RESULT_OK) {
			if (DA_RESULT_OK == GET_REQUEST_HTTP_RESULT(req_info)) {
				GET_REQUEST_HTTP_RESULT(req_info) = ret;
				DA_LOG_CRITICAL(HTTPManager, "setting internal error [%d]", ret);
			}
			_cancel_transaction(stage);
		}

		_da_thread_mutex_lock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));
		http_state = GET_HTTP_STATE_ON_STAGE(stage);
		DA_LOG(HTTPManager, "http_state = %d", http_state);
		_da_thread_mutex_unlock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));

		switch (http_state) {
		case HTTP_STATE_READY_TO_DOWNLOAD:
			ret = start_new_transaction(stage);
			if (ret != DA_RESULT_OK) {
				if (DA_RESULT_OK == GET_REQUEST_HTTP_RESULT(req_info)) {
					GET_REQUEST_HTTP_RESULT(req_info) = ret;
					DA_LOG_CRITICAL(HTTPManager, "setting internal error [%d]", ret);
				}
				DA_LOG(HTTPManager, "exiting with error...");
				need_wait = DA_FALSE;
				break;
			}

			CHANGE_HTTP_STATE(HTTP_STATE_DOWNLOAD_REQUESTED, stage);

			send_client_da_state(download_id,
					DA_STATE_DOWNLOAD_STARTED,
					DA_RESULT_OK);
			break;

		case HTTP_STATE_CANCELED:
		case HTTP_STATE_DOWNLOAD_FINISH:
		case HTTP_STATE_ABORTED:
			DA_LOG(HTTPManager, "exiting...");
			need_wait = DA_FALSE;
			break;

		default:
			break;
		}

		if (need_wait == DA_TRUE) {
			_da_thread_mutex_lock(&(queue->mutex_queue));
			if (DA_FALSE == GET_IS_Q_HAVING_DATA(queue)) {
				unpause_for_flow_control(stage);

//				DA_LOG(HTTPManager, "Waiting for input");
				Q_goto_sleep(queue);
//				DA_LOG(HTTPManager, "Woke up to receive new packet or control event");
			}
			_da_thread_mutex_unlock (&(queue->mutex_queue));

		}

	} while (need_wait == DA_TRUE);

	ret = GET_REQUEST_HTTP_RESULT(req_info);
	DA_LOG(HTTPManager, "--------------Exiting request_http_download! ret = %d", ret);
	return ret;
}

da_result_t request_to_cancel_http_download(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	q_event_t *q_event = DA_NULL;

	DA_LOG_FUNC_START(HTTPManager);

	DA_LOG(HTTPManager, "Q_EVENT_TYPE_CONTROL_CANCEL");
	ret = Q_make_control_event(Q_EVENT_TYPE_CONTROL_CANCEL, &q_event);
	if (ret != DA_RESULT_OK) {
		DA_LOG_ERR(HTTPManager, "fail to make q_control_event");
		goto ERR;
	} else {
		DA_LOG(HTTPManager, "queue = %p", GET_DL_QUEUE(GET_STAGE_DL_ID(stage)));
		Q_push_event(GET_DL_QUEUE(GET_STAGE_DL_ID(stage)), q_event);
	}

ERR:
	return ret;
}

da_result_t request_to_suspend_http_download(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	http_state_t http_state = 0;
	q_event_t *q_event = DA_NULL;

	DA_LOG_FUNC_START(HTTPManager);

	_da_thread_mutex_lock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));
	http_state = GET_HTTP_STATE_ON_STAGE(stage);
	DA_LOG(HTTPManager, "http_state = %d", http_state);
	_da_thread_mutex_unlock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));

	switch (http_state) {
	case HTTP_STATE_PAUSED:
	case HTTP_STATE_REQUEST_PAUSE:
		DA_LOG_CRITICAL(HTTPManager, "Already paused. http_state = %d", http_state);
		ret = DA_ERR_ALREADY_SUSPENDED;
		break;

	default:
		DA_LOG(HTTPManager, "Q_EVENT_TYPE_CONTROL_SUSPEND");
		ret = Q_make_control_event(Q_EVENT_TYPE_CONTROL_SUSPEND,
				&q_event);
		if (ret != DA_RESULT_OK) {
			DA_LOG_ERR(HTTPManager, "fail to make q_control_event");
			goto ERR;
		} else {
			DA_LOG(HTTPManager, "queue = %p", GET_DL_QUEUE(GET_STAGE_DL_ID(stage)));
			Q_push_event(GET_DL_QUEUE(GET_STAGE_DL_ID(stage)),
					q_event);
		}

		break;
	}

ERR:
	return ret;
}

da_result_t request_to_resume_http_download(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	http_state_t http_state = 0;
	q_event_t *q_event = DA_NULL;

	DA_LOG_FUNC_START(HTTPManager);

	_da_thread_mutex_lock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));
	http_state = GET_HTTP_STATE_ON_STAGE(stage);
	DA_LOG(HTTPManager, "[%d] http_state = %d", GET_STAGE_DL_ID(stage), http_state);
	_da_thread_mutex_unlock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));

	switch (http_state) {
	case HTTP_STATE_PAUSED:
		DA_LOG(HTTPManager, "Q_EVENT_TYPE_CONTROL_RESUME");
		ret = Q_make_control_event(Q_EVENT_TYPE_CONTROL_RESUME,
				&q_event);
		if (ret != DA_RESULT_OK) {
			DA_LOG_ERR(HTTPManager, "fail to make q_control_event");
			goto ERR;
		} else {
			DA_LOG(HTTPManager, "queue = %p", GET_DL_QUEUE(GET_STAGE_DL_ID(stage)));
			Q_push_event(GET_DL_QUEUE(GET_STAGE_DL_ID(stage)),
					q_event);
		}

		break;

	case HTTP_STATE_REQUEST_PAUSE:
		DA_LOG_ERR(HTTPManager, "[%d] Fail to resume. Previous pause is not finished. http_state = %d", GET_STAGE_DL_ID(stage), http_state);
		ret = DA_ERR_INVALID_STATE;

		break;

	case HTTP_STATE_RESUMED:
		ret = DA_ERR_ALREADY_RESUMED;

		break;

	default:
		DA_LOG_ERR(HTTPManager, "[%d] Fail to resume. This is not a paused ID. http_state = %d", GET_STAGE_DL_ID(stage), http_state);
		ret = DA_ERR_INVALID_STATE;

		break;
	}

ERR:
	return ret;
}

da_result_t start_new_transaction(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;

	ret = set_http_request_hdr(stage);
	if (ret != DA_RESULT_OK)
		return ret;

	ret = make_transaction_info_and_start_transaction(stage);
	return ret;
}

da_result_t make_default_http_request_hdr(const char *url,
		char **user_request_header,
		int user_request_header_count,
		http_msg_request_t **out_http_msg_request)
{
	da_result_t ret = DA_RESULT_OK;

	http_msg_request_t *http_msg_request = NULL;
	char *user_agent = NULL;

	DA_LOG_FUNC_START(HTTPManager);

	if (!url) {
		DA_LOG_ERR(HTTPManager, "DA_ERR_NO_URL");
		ret = DA_ERR_INVALID_URL;
		goto ERR;
	}

	ret = http_msg_request_create(&http_msg_request);
	if (ret != DA_RESULT_OK)
		goto ERR;

	ret = http_msg_request_set_url(http_msg_request, url);
	if (ret != DA_RESULT_OK)
		goto ERR;

	user_agent = get_user_agent();
	if (user_agent)
		http_msg_request_add_field(http_msg_request, HTTP_FIELD_UAGENT,
				user_agent);

	http_msg_request_add_field(http_msg_request, HTTP_FIELD_ACCEPT_LANGUAGE, "en");
	http_msg_request_add_field(http_msg_request, HTTP_FIELD_ACCEPT_CHARSET, "utf-8");

	if (user_request_header && user_request_header_count > 0) {
		int i = 0;
		for (i = 0; i < user_request_header_count; i++)
		{
			char *field = NULL;
			char *value = NULL;
			__parsing_user_request_header(user_request_header[i],
				&field, &value);
			if (field && value) {
				http_msg_request_add_field(http_msg_request, field, value);
				if (field) {
					free(field);
					field = NULL;
				}
				if (*value) {
					free(value);
					value= NULL;
				}
			} else {
				DA_LOG_ERR(HTTPManager, "Fail to parse user request header");
			}
		}
	} else
		DA_LOG(HTTPManager, "no user reqeust header inserted");

	*out_http_msg_request = http_msg_request;

ERR:
	if (ret != DA_RESULT_OK)
		http_msg_request_destroy(&http_msg_request);

	return ret;
}

da_result_t set_http_request_hdr(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	req_dl_info *request_info = DA_NULL;

	char *url = DA_NULL;
	char **user_request_header = DA_NULL;
	int user_request_header_count = 0;
	http_msg_request_t* http_msg_request = NULL;

	DA_LOG_FUNC_START(HTTPManager);

	request_info = GET_STAGE_TRANSACTION_INFO(stage);

	if (DA_NULL ==
			(url = GET_REQUEST_HTTP_REQ_URL(request_info))) {
		DA_LOG_ERR(HTTPManager, "DA_ERR_NO_URL");
		ret = DA_ERR_INVALID_URL;
		goto ERR;
	}

	user_request_header = GET_REQUEST_HTTP_USER_REQUEST_HEADER(
			request_info);
	user_request_header_count = GET_REQUEST_HTTP_USER_REQUEST_HEADER_COUNT(
			request_info);

	ret = make_default_http_request_hdr(url, user_request_header,
		user_request_header_count, &http_msg_request);
	if (ret == DA_RESULT_OK)
		request_info->http_info.http_msg_request = http_msg_request;

ERR:
	return ret;

}

da_result_t make_transaction_info_and_start_transaction(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;

	int download_id = DA_INVALID_ID;
	req_dl_info *request_info = DA_NULL;

	input_for_tranx_t *input_for_tranx = DA_NULL;

	DA_LOG_FUNC_START(HTTPManager);

	download_id = GET_STAGE_DL_ID(stage);

	request_info = GET_STAGE_TRANSACTION_INFO(stage);

	if (GET_REQUEST_HTTP_REQ_URL(request_info) == DA_NULL) {
		DA_LOG_ERR(HTTPManager, "url is NULL");
		ret = DA_ERR_INVALID_URL;
		goto ERR;
	}

	input_for_tranx = (input_for_tranx_t*) calloc(1,
			sizeof(input_for_tranx_t));
	if (input_for_tranx == DA_NULL) {
		ret = DA_ERR_FAIL_TO_MEMALLOC;
		goto ERR;
	} else {
		input_for_tranx->proxy_addr = get_proxy_address();
		input_for_tranx->queue = GET_DL_QUEUE(download_id);

		input_for_tranx->http_method = PI_HTTP_METHOD_GET;
		input_for_tranx->http_msg_request
				= request_info->http_info.http_msg_request;
	}

	ret = PI_http_start_transaction(input_for_tranx,
			&(GET_REQUEST_HTTP_TRANS_ID(request_info)));
	if (ret != DA_RESULT_OK)
		goto ERR;

ERR:
	if (input_for_tranx) {
		free(input_for_tranx);
		input_for_tranx = DA_NULL;
	}

	return ret;
}

da_result_t make_req_dl_info_http(stage_info *stage, req_dl_info *out_info)
{
	char *url = DA_NULL;
	char **user_request_header = DA_NULL;
	int user_request_header_count = 0;
	source_info_t *source_info = DA_NULL;

	DA_LOG_FUNC_START(HTTPManager);

	if (!stage) {
		DA_LOG_ERR(HTTPManager, "stage is NULL");
		return DA_ERR_INVALID_ARGUMENT;
	}

	source_info = GET_STAGE_SOURCE_INFO(stage);

	url = source_info->source_info_type.source_info_basic->url;
	user_request_header =
		source_info->source_info_type.source_info_basic->user_request_header;
	user_request_header_count =
		source_info->source_info_type.source_info_basic->user_request_header_count;
	DA_LOG(HTTPManager, "url [%s]", url);

	if (url) {
		GET_REQUEST_HTTP_REQ_URL(out_info) = url;
		GET_REQUEST_HTTP_USER_REQUEST_HEADER(out_info) = user_request_header;
		GET_REQUEST_HTTP_USER_REQUEST_HEADER_COUNT(out_info) =
			user_request_header_count;
	} else {
		DA_LOG_ERR(HTTPManager, "DA_ERR_NO_URL");
		return DA_ERR_INVALID_URL;
	}

	_da_thread_mutex_init(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)), NULL);

	return DA_RESULT_OK;
}

da_result_t pause_for_flow_control(stage_info *stage)
{
	return DA_RESULT_OK;
}

da_result_t unpause_for_flow_control(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;

	//	DA_LOG_FUNC_START(HTTPManager);

	PI_http_unpause_transaction(
			GET_REQUEST_HTTP_TRANS_ID(GET_STAGE_TRANSACTION_INFO(stage)));
	return ret;
}
da_result_t handle_event_abort(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	http_state_t state = 0;

	DA_LOG_FUNC_START(HTTPManager);

	_da_thread_mutex_lock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));
	state = GET_HTTP_STATE_ON_STAGE(stage);
	DA_LOG(HTTPManager, "http_state = %d", state);
	_da_thread_mutex_unlock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));
	switch (state) {
	case HTTP_STATE_READY_TO_DOWNLOAD:
	case HTTP_STATE_DOWNLOAD_REQUESTED:
	case HTTP_STATE_DOWNLOAD_STARTED:
	case HTTP_STATE_DOWNLOADING:
	case HTTP_STATE_REQUEST_RESUME:
	case HTTP_STATE_REQUEST_PAUSE:
	case HTTP_STATE_RESUMED:
	case HTTP_STATE_PAUSED:
	case HTTP_STATE_REQUEST_CANCEL:
		CHANGE_HTTP_STATE(HTTP_STATE_ABORTED,stage);
		CHANGE_DOWNLOAD_STATE(DOWNLOAD_STATE_CANCELED, stage);
		_disconnect_transaction(stage);
		break;
	case HTTP_STATE_DOWNLOAD_FINISH:
		break;
	default:
		DA_LOG_ERR(HTTPManager, "have to check the flow for this case");
		break;
	}
	return ret;
}
da_result_t handle_event_cancel(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	http_state_t state = 0;

	DA_LOG_FUNC_START(HTTPManager);

	_da_thread_mutex_lock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));
	state = GET_HTTP_STATE_ON_STAGE(stage);
	DA_LOG(HTTPManager, "http_state = %d", state);
	_da_thread_mutex_unlock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));
	switch (state) {
	case HTTP_STATE_READY_TO_DOWNLOAD:
		CHANGE_HTTP_STATE(HTTP_STATE_CANCELED,stage);
		CHANGE_DOWNLOAD_STATE(DOWNLOAD_STATE_CANCELED, stage);
		break;

	case HTTP_STATE_PAUSED:
		discard_download(stage);
		CHANGE_HTTP_STATE(HTTP_STATE_CANCELED,stage);
		CHANGE_DOWNLOAD_STATE(DOWNLOAD_STATE_CANCELED, stage);
		break;

	case HTTP_STATE_DOWNLOAD_REQUESTED:
	case HTTP_STATE_DOWNLOAD_STARTED:
	case HTTP_STATE_DOWNLOADING:
	case HTTP_STATE_REQUEST_RESUME:
	case HTTP_STATE_RESUMED:
		_cancel_transaction(stage);
		CHANGE_HTTP_STATE(HTTP_STATE_REQUEST_CANCEL, stage);
		break;

	case HTTP_STATE_DOWNLOAD_FINISH:
		break;

	case HTTP_STATE_REQUEST_CANCEL:
		DA_LOG(HTTPManager, "HTTP_STATE_REQUEST_CANCEL : cancel is already in progress... ");
		break;

	default:
		DA_LOG_ERR(HTTPManager, "have to check the flow for this case");
		break;
	}

	return ret;
}

da_result_t handle_event_suspend(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	http_state_t http_state = 0;

	_da_thread_mutex_lock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));
	http_state = GET_HTTP_STATE_ON_STAGE(stage);
	_da_thread_mutex_unlock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));

	switch (http_state) {
	case HTTP_STATE_REQUEST_PAUSE:
		DA_LOG(HTTPManager, "already requested to pause! do nothing");
		break;

	case HTTP_STATE_READY_TO_DOWNLOAD:
		CHANGE_HTTP_STATE(HTTP_STATE_PAUSED,stage);
		CHANGE_DOWNLOAD_STATE(DOWNLOAD_STATE_PAUSED, stage);
		send_client_da_state(GET_STAGE_DL_ID(stage),
				DA_STATE_SUSPENDED, DA_RESULT_OK);
		break;

	default:
		_cancel_transaction(stage);
		GET_REQUEST_HTTP_RESULT(GET_STAGE_TRANSACTION_INFO(stage)) = DA_RESULT_OK;
		DA_LOG_CRITICAL(HTTPManager, "[%d] cleanup internal error", GET_STAGE_DL_ID(stage));
		CHANGE_HTTP_STATE(HTTP_STATE_REQUEST_PAUSE,stage);
		break;
	}

	return ret;
}

da_result_t handle_event_resume(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	http_msg_request_t *resume_request = NULL;

	http_state_t http_state = 0;

	_da_thread_mutex_lock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));
	http_state = GET_HTTP_STATE_ON_STAGE(stage);
	_da_thread_mutex_unlock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));

	if (http_state != HTTP_STATE_PAUSED) {
		DA_LOG_ERR(HTTPManager, "Not HTTP_STATE_PAUSED! http_state = %d", http_state);
		ret = DA_ERR_INVALID_STATE;
		goto ERR;
	}

	GET_REQUEST_HTTP_RESULT(GET_STAGE_TRANSACTION_INFO(stage)) = DA_RESULT_OK;
	DA_LOG_CRITICAL(HTTPManager, "[%d] cleanup internal error", GET_STAGE_DL_ID(stage));

	CHANGE_HTTP_STATE(HTTP_STATE_REQUEST_RESUME,stage);
	CHANGE_DOWNLOAD_STATE(DOWNLOAD_STATE_NEW_DOWNLOAD,stage);

	ret = create_resume_http_request_hdr(stage, &resume_request);
	if (ret != DA_RESULT_OK)
		goto ERR;

	if (GET_STAGE_TRANSACTION_INFO(stage)->http_info.http_msg_request)
		free(GET_STAGE_TRANSACTION_INFO(stage)->http_info.http_msg_request);

	GET_STAGE_TRANSACTION_INFO(stage)->http_info.http_msg_request
			= resume_request;

	make_transaction_info_and_start_transaction(stage);

ERR:
	return ret;

}

da_result_t create_resume_http_request_hdr(stage_info *stage,
		http_msg_request_t **out_resume_request)
{
	da_result_t ret = DA_RESULT_OK;
	da_bool_t b_ret = DA_FALSE;

	req_dl_info *request_info = NULL;

	http_msg_response_t *first_response = NULL;
	http_msg_request_t *resume_request = NULL;

	char *value = NULL;
	char *url = NULL;
	unsigned int downloaded_data_size = 0;
	char downloaded_data_size_to_str[32] = { 0, };

	char *etag_from_response = NULL;
	char *date_from_response = NULL;

	DA_LOG_FUNC_START(HTTPManager);

	request_info = GET_STAGE_TRANSACTION_INFO(stage);

	if (!(url = GET_REQUEST_HTTP_REQ_URL(GET_STAGE_TRANSACTION_INFO(stage)))) {
		DA_LOG_ERR(HTTPManager, "DA_ERR_NO_URL");
		ret = DA_ERR_INVALID_URL;
		goto ERR;
	}

	first_response = request_info->http_info.http_msg_response;
	if (first_response) {
		b_ret = http_msg_response_get_ETag(first_response, &value);
		if (b_ret) {
			etag_from_response = value;
			value = NULL;
			DA_LOG(HTTPManager, "[ETag][%s]", etag_from_response);
		}

		b_ret = http_msg_response_get_date(first_response, &value);
		if (b_ret) {
			date_from_response = value;
			value = NULL;
			DA_LOG(HTTPManager, "[Date][%s]", date_from_response);
		}

		downloaded_data_size
				= GET_CONTENT_STORE_CURRENT_FILE_SIZE(GET_STAGE_CONTENT_STORE_INFO(stage));
		DA_LOG(HTTPManager, "downloaded_data_size = %u", downloaded_data_size);
		snprintf(downloaded_data_size_to_str, sizeof(downloaded_data_size_to_str), "bytes=%u-",
				downloaded_data_size);
		DA_LOG(HTTPManager, "downloaded_data_size_to_str = %s", downloaded_data_size_to_str);
	}

	ret = make_default_http_request_hdr(url, NULL, 0, &resume_request);
	if (ret != DA_RESULT_OK)
		goto ERR;

	if (etag_from_response) {
		http_msg_request_add_field(resume_request, HTTP_FIELD_IF_RANGE,
				etag_from_response);
	} else {
		if (date_from_response) {
			http_msg_request_add_field(resume_request,
					HTTP_FIELD_IF_RANGE, date_from_response);
		}
	}

	if (strlen(downloaded_data_size_to_str) > 0)
		http_msg_request_add_field(resume_request, HTTP_FIELD_RANGE,
				downloaded_data_size_to_str);

	*out_resume_request = resume_request;

ERR:
	if (etag_from_response) {
		free(etag_from_response);
		etag_from_response = NULL;
	}

	if (date_from_response) {
		free(date_from_response);
		date_from_response = NULL;
	}

	return ret;
}

da_result_t handle_any_input(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;

	int download_id = GET_STAGE_DL_ID(stage);

	queue_t *queue = DA_NULL;
	q_event_t *event = DA_NULL;

	//	DA_LOG_FUNC_START(HTTPManager);

	queue = GET_DL_QUEUE(download_id);

	Q_pop_event(queue, &event);
	if (event == DA_NULL) {
		DA_LOG(HTTPManager, "There is no data on the queue!");
		return DA_RESULT_OK;
	}

	switch (event->event_type) {
	case Q_EVENT_TYPE_CONTROL:
		ret = handle_event_control(stage, event);
		break;

	case Q_EVENT_TYPE_DATA_HTTP:
		ret = handle_event_http(stage, event);
		break;

	default:
		break;
	}

	Q_destroy_q_event(&event);

	return ret;
}

da_result_t handle_event_control(stage_info *stage, q_event_t *event)
{
	da_result_t ret = DA_RESULT_OK;

	DA_LOG_FUNC_START(HTTPManager);

	if (event->event_type == Q_EVENT_TYPE_CONTROL) {
		switch (event->type.q_event_control.control_type) {
		case Q_EVENT_TYPE_CONTROL_CANCEL:
			DA_LOG(HTTPManager, "Q_EVENT_TYPE_CONTROL_CANCEL");
			ret = handle_event_cancel(stage);
			break;

		case Q_EVENT_TYPE_CONTROL_SUSPEND:
			DA_LOG(HTTPManager, "Q_EVENT_TYPE_CONTROL_SUSPEND");
			ret = handle_event_suspend(stage);
			break;

		case Q_EVENT_TYPE_CONTROL_RESUME:
			DA_LOG(HTTPManager, "Q_EVENT_TYPE_CONTROL_RESUME");
			ret = handle_event_resume(stage);
			break;
		case Q_EVENT_TYPE_CONTROL_ABORT:
			DA_LOG(HTTPManager, "Q_EVENT_TYPE_CONTROL_ABORT");
			ret = handle_event_abort(stage);
			break;
			/* Fixme: need to think how we use this type. For now, this type is not used. */
		case Q_EVENT_TYPE_CONTROL_NET_DISCONNECTED:
			DA_LOG(HTTPManager, "Q_EVENT_TYPE_CONTROL_NET_DISCONNECTED");
			break;
		}
	}

	return ret;
}

da_result_t handle_event_http(stage_info *stage, q_event_t *event)
{
	da_result_t ret = DA_RESULT_OK;
	q_event_data_http_t *q_event_data_http = DA_NULL;

	//	DA_LOG_FUNC_START(HTTPManager);

	if (event->event_type == Q_EVENT_TYPE_DATA_HTTP) {
		q_event_data_http = &(event->type.q_event_data_http);
		switch (q_event_data_http->data_type) {
		case Q_EVENT_TYPE_DATA_PACKET:
			ret = handle_event_http_packet(stage, event);

			break;

		case Q_EVENT_TYPE_DATA_FINAL:
			DA_LOG(HTTPManager, "Q_EVENT_TYPE_DATA_FINAL");
			ret = handle_event_http_final(stage, event);

			break;

		case Q_EVENT_TYPE_DATA_ABORT:
			DA_LOG(HTTPManager, "Q_EVENT_TYPE_DATA_ABORT");
			ret = handle_event_http_abort(stage, event);

			break;
		}
	}

	return ret;
}

da_result_t handle_event_http_packet(stage_info *stage, q_event_t *event)
{
	da_result_t ret = DA_RESULT_OK;
	da_bool_t is_handle_hdr_success = DA_TRUE;
	q_event_data_http_t *received_data = DA_NULL;

	//	DA_LOG_FUNC_START(HTTPManager);

	received_data = &(event->type.q_event_data_http);

	if (received_data->http_response_msg) {
		ret = handle_http_hdr(stage, received_data->http_response_msg,
				received_data->http_response_msg->status_code);
		if (DA_RESULT_OK != ret) {
			is_handle_hdr_success = DA_FALSE;
		}

		received_data->http_response_msg = NULL;
	}

	if (received_data->body_len > 0) {
		if (is_handle_hdr_success == DA_TRUE) {
			ret = handle_http_body(stage, received_data->body_data,
					received_data->body_len);
		}
		/*For all cases body_data should be deleted*/
		free(received_data->body_data);
		received_data->body_data = DA_NULL;
	}

	return ret;
}

da_result_t handle_event_http_final(stage_info *stage, q_event_t *event)
{
	da_result_t ret = DA_RESULT_OK;

	http_state_t http_state = 0;
	int download_id = DA_INVALID_ID;

	DA_LOG_FUNC_START(HTTPManager);

	download_id = GET_STAGE_DL_ID(stage);
	_disconnect_transaction(stage);

	_da_thread_mutex_lock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));
	http_state = GET_HTTP_STATE_ON_STAGE(stage);
	DA_LOG(HTTPManager, "http_state = %d", http_state);
	_da_thread_mutex_unlock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));

	switch (http_state) {
	case HTTP_STATE_REDIRECTED:
		CHANGE_HTTP_STATE(HTTP_STATE_READY_TO_DOWNLOAD,stage);
		break;

	case HTTP_STATE_DOWNLOAD_REQUESTED:
		DA_LOG(HTTPManager, "case HTTP_STATE_DOWNLOAD_REQUESTED");
		CHANGE_HTTP_STATE(HTTP_STATE_DOWNLOAD_FINISH, stage);
		break;

	case HTTP_STATE_DOWNLOADING:
		DA_LOG(HTTPManager, "case HTTP_STATE_DOWNLOADING");
		ret = file_write_complete(stage);
		if (ret != DA_RESULT_OK) {
			discard_download(stage);
			goto ERR;
		}
		/*			ret = _check_downloaded_file_size_is_same_with_header_content_size(stage);
		 if(ret != DA_RESULT_OK)
		 {
		 discard_download(stage) ;
		 goto ERR;
		 }
		 */
		CHANGE_HTTP_STATE(HTTP_STATE_DOWNLOAD_FINISH, stage);
		send_client_update_downloading_info(
				download_id,
				GET_DL_REQ_ID(download_id),
				GET_CONTENT_STORE_CURRENT_FILE_SIZE(GET_STAGE_CONTENT_STORE_INFO(stage)),
				DA_NULL);
		break;

	case HTTP_STATE_REQUEST_PAUSE:
		if (GET_CONTENT_STORE_FILE_HANDLE(GET_STAGE_CONTENT_STORE_INFO(stage))) {
			ret = file_write_complete(stage);
			send_client_update_downloading_info(
					download_id,
					GET_DL_REQ_ID(download_id),
					GET_CONTENT_STORE_CURRENT_FILE_SIZE(GET_STAGE_CONTENT_STORE_INFO(stage)),
					GET_CONTENT_STORE_ACTUAL_FILE_NAME(GET_STAGE_CONTENT_STORE_INFO(stage)));

			IS_CONTENT_STORE_FILE_BYTES_WRITTEN_TO_FILE(GET_STAGE_CONTENT_STORE_INFO(stage))
					= DA_FALSE;
		}
		CHANGE_HTTP_STATE(HTTP_STATE_PAUSED,stage);
		CHANGE_DOWNLOAD_STATE(DOWNLOAD_STATE_PAUSED, stage);
		send_client_da_state(download_id, DA_STATE_SUSPENDED,
				DA_RESULT_OK);
		DA_LOG(HTTPManager, "Server Notification code is set to NULL");
		break;

	case HTTP_STATE_ABORTED:
	case HTTP_STATE_CANCELED:
		discard_download(stage);
		break;

	case HTTP_STATE_REQUEST_CANCEL:
		ret = file_write_complete(stage);
		if (ret != DA_RESULT_OK)
			goto ERR;
		discard_download(stage);
		CHANGE_HTTP_STATE(HTTP_STATE_CANCELED, stage);
		CHANGE_DOWNLOAD_STATE(DOWNLOAD_STATE_CANCELED, stage);
		break;

	default:
		ret = file_write_complete(stage);
		if (ret != DA_RESULT_OK)
			goto ERR;
		discard_download(stage);
		CHANGE_HTTP_STATE(HTTP_STATE_ABORTED,stage);
		break;
	}

ERR:
	/* When file complete is failed */
	if (DA_RESULT_OK != ret) {
		CHANGE_HTTP_STATE(HTTP_STATE_DOWNLOAD_FINISH, stage);
	}
	return ret;
}

da_result_t handle_event_http_abort(stage_info *stage, q_event_t *event)
{
	da_result_t ret = DA_RESULT_OK;
	http_state_t http_state = 0;

	DA_LOG_FUNC_START(HTTPManager);

	GET_REQUEST_HTTP_RESULT(GET_STAGE_TRANSACTION_INFO(stage))
		= event->type.q_event_data_http.error_type;
	DA_LOG_CRITICAL(HTTPManager, "set internal error code : [%d]", GET_REQUEST_HTTP_RESULT(GET_STAGE_TRANSACTION_INFO(stage)));
	_disconnect_transaction(stage);

	_da_thread_mutex_lock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));
	http_state = GET_HTTP_STATE_ON_STAGE(stage);
	DA_LOG(HTTPManager, "http_state = %d", http_state);
	_da_thread_mutex_unlock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));

	switch (http_state) {
	case HTTP_STATE_REQUEST_PAUSE:
		CHANGE_HTTP_STATE(HTTP_STATE_PAUSED,stage);
		CHANGE_DOWNLOAD_STATE(DOWNLOAD_STATE_PAUSED, stage);
		send_client_da_state(GET_STAGE_DL_ID(stage),
				DA_STATE_SUSPENDED, DA_RESULT_OK);
		ret = file_write_complete(stage);
		if (ret != DA_RESULT_OK)
			goto ERR;
		break;

	case HTTP_STATE_REQUEST_CANCEL:
		CHANGE_HTTP_STATE(HTTP_STATE_CANCELED,stage);
		CHANGE_DOWNLOAD_STATE(DOWNLOAD_STATE_CANCELED, stage);
		ret = file_write_complete(stage);
		if (ret != DA_RESULT_OK)
			goto ERR;
		discard_download(stage);
		break;

	default:
		CHANGE_HTTP_STATE(HTTP_STATE_ABORTED,stage);
		ret = file_write_complete(stage);
		if (ret != DA_RESULT_OK)
			goto ERR;
		discard_download(stage);
		break;
	}
ERR:
	return ret;
}

da_result_t handle_http_hdr(stage_info *stage,
		http_msg_response_t *http_msg_response, int http_status)
{
	da_result_t ret = DA_RESULT_OK;
	int download_id = DA_INVALID_ID;
	http_state_t http_state = 0;
	char *response_header_data = DA_NULL;

	DA_LOG_FUNC_START(HTTPManager);

	download_id = GET_STAGE_DL_ID(stage);

	_da_thread_mutex_lock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));
	http_state = GET_HTTP_STATE_ON_STAGE(stage);
	DA_LOG(HTTPManager, "http_state = %d", http_state);
	_da_thread_mutex_unlock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));

	switch (http_state) {
	case HTTP_STATE_DOWNLOAD_REQUESTED:
	case HTTP_STATE_REQUEST_PAUSE:
	case HTTP_STATE_REQUEST_RESUME:
	case HTTP_STATE_REDIRECTED:
		/* In case of manual download, this is first update.
		 * So, the client can know the mime type at second update */
		if (DA_TRUE == is_this_client_manual_download_type() &&
				http_msg_response) {
			// SHOULD free response_header_data when it is not used any more
			response_header_data = get_http_response_header_raw(http_msg_response);
			send_client_update_dl_info(
				download_id,
				GET_DL_REQ_ID(download_id),
				GET_CONTENT_STORE_CONTENT_TYPE(GET_STAGE_CONTENT_STORE_INFO(stage)),
				GET_CONTENT_STORE_FILE_SIZE(GET_STAGE_CONTENT_STORE_INFO(stage)),
				DA_NULL,
				response_header_data,
				DA_NULL);
			if (response_header_data) {
				free(response_header_data);
				response_header_data = NULL;
			}
		}
		ret = handle_http_status_code(stage, http_msg_response,
				http_status);
		if (ret != DA_RESULT_OK)
			goto ERR;
		break;

	case HTTP_STATE_REQUEST_CANCEL:
		DA_LOG(HTTPManager, "Cancel is in progress.. http_state = %d", http_state);
		break;

	default:
		DA_LOG_ERR(HTTPManager, "http_state = %d", http_state);
		goto ERR;
	}

ERR:
	return ret;
}

da_result_t handle_http_status_code(stage_info *stage,
		http_msg_response_t *http_msg_response, int http_status)
{
	da_result_t ret = DA_RESULT_OK;

	int download_id = DA_INVALID_ID;
	req_dl_info *request_info = DA_NULL;
	http_state_t http_state = 0;

	DA_LOG_FUNC_START(HTTPManager);

	download_id = GET_STAGE_DL_ID(stage);
	request_info = GET_STAGE_TRANSACTION_INFO(stage);

	GET_STAGE_TRANSACTION_INFO(stage)->http_info.http_msg_response
			= http_msg_response;

	_da_thread_mutex_lock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));
	http_state = GET_HTTP_STATE_ON_STAGE(stage);
	DA_LOG(HTTPManager, "http_state = %d", http_state);
	_da_thread_mutex_unlock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));

	switch (http_status) {
	case 200:
	case 201:
	case 202:
	case 203:
		if (http_state == HTTP_STATE_REQUEST_RESUME)
			clean_paused_file(stage);
		ret = set_hdr_fields_on_download_info(stage);
		if (ret != DA_RESULT_OK)
			goto ERR;
		ret = _check_content_type_is_matched(stage);
		if (ret != DA_RESULT_OK)
			goto ERR;
		ret = _check_enough_memory_for_this_download(stage);
		if (ret != DA_RESULT_OK)
			goto ERR;
		CHANGE_HTTP_STATE(HTTP_STATE_DOWNLOAD_STARTED,stage);
		CHANGE_DOWNLOAD_STATE(DOWNLOAD_STATE_NEW_DOWNLOAD,stage); // ?
		break;

	case 206:
		DA_LOG(HTTPManager, "HTTP Status is %d - Partial download for resume!",http_status);
		if (http_state != HTTP_STATE_REQUEST_RESUME) {
			DA_LOG_ERR(HTTPManager, "This download is not resumed, revoke");
			ret = DA_ERR_INVALID_STATE;
			goto ERR;
		}
		ret = _check_this_partial_download_is_available(stage,
				http_msg_response);
		if (ret != DA_RESULT_OK)
			goto ERR;
		CHANGE_HTTP_STATE(HTTP_STATE_RESUMED,stage);
		CHANGE_DOWNLOAD_STATE(DOWNLOAD_STATE_NEW_DOWNLOAD,stage);
		break;

	case 300:
	case 301:
	case 302:
	case 303:
	case 305:
	case 306:
	case 307:
		DA_LOG(HTTPManager, "HTTP Status is %d - redirection!",http_status);
		ret = exchange_url_from_header_for_redirection(stage, http_msg_response);
		if (ret != DA_RESULT_OK)
			goto ERR;
		CHANGE_HTTP_STATE(HTTP_STATE_REDIRECTED,stage);
		http_msg_response_destroy(&http_msg_response);
		break;

	case 100:
	case 101:
	case 102:
	case 204:
	case 304:
		DA_LOG(HTTPManager, "HTTP Status is %d - 204 means server got the request, but no content to reply back, 304 means not modified!",http_status);
		ret = DA_ERR_SERVER_RESPOND_BUT_SEND_NO_CONTENT;
		break;

	case 416: // Requested range not satisfiable
	case 503:
	case 504:
	default:
		GET_REQUEST_HTTP_RESULT(request_info)
			= DA_ERR_UNREACHABLE_SERVER;
		DA_LOG_CRITICAL(HTTPManager, "set internal error code : DA_ERR_UNREACHABLE_SERVER [%d]", DA_ERR_UNREACHABLE_SERVER);
		break;
	}

ERR:
	return ret;
}

da_result_t exchange_url_from_header_for_redirection(stage_info *stage,
		http_msg_response_t *http_msg_response)
{
	da_result_t ret = DA_RESULT_OK;
	char *location = DA_NULL;

	DA_LOG_FUNC_START(HTTPManager);

	if (http_msg_response_get_location(http_msg_response, &location)) {
		DA_LOG(HTTPManager, "location  = %s\n", location);
		GET_REQUEST_HTTP_REQ_LOCATION(GET_STAGE_TRANSACTION_INFO(stage)) = location;
	}

	return ret;
}

da_result_t handle_http_body(stage_info *stage, char *body, int body_len)
{
	da_result_t ret = DA_RESULT_OK;
	http_state_t http_state = 0;
	int download_id = DA_INVALID_ID;

	//	DA_LOG_FUNC_START(HTTPManager);

	download_id = GET_STAGE_DL_ID(stage);

	if (DA_RESULT_OK
			!= GET_REQUEST_HTTP_RESULT(GET_STAGE_TRANSACTION_INFO(stage))) {
		DA_LOG_CRITICAL(HTTPManager, "ignore because internal error code is set with [%d]",
				GET_REQUEST_HTTP_RESULT(GET_STAGE_TRANSACTION_INFO(stage)));
		return ret;
	}

	_da_thread_mutex_lock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));
	http_state = GET_HTTP_STATE_ON_STAGE(stage);
	//	DA_LOG(HTTPManager, "http_state = %d", http_state);
	_da_thread_mutex_unlock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));

	if (http_state == HTTP_STATE_DOWNLOAD_STARTED) {
		ret = start_file_writing(stage);
		if (DA_RESULT_OK != ret)
			goto ERR;

		CHANGE_HTTP_STATE(HTTP_STATE_DOWNLOADING, stage);
		send_client_da_state(download_id, DA_STATE_DOWNLOADING,
				DA_RESULT_OK);
		send_client_update_dl_info(
				download_id,
				GET_DL_REQ_ID(download_id),
				GET_CONTENT_STORE_CONTENT_TYPE(GET_STAGE_CONTENT_STORE_INFO(stage)),
				GET_CONTENT_STORE_FILE_SIZE(GET_STAGE_CONTENT_STORE_INFO(stage)),
				GET_CONTENT_STORE_TMP_FILE_NAME(GET_STAGE_CONTENT_STORE_INFO(stage)),
				DA_NULL,
				DA_NULL);
	} else if (http_state == HTTP_STATE_RESUMED) {
		ret = start_file_writing_append(stage);
		if (DA_RESULT_OK != ret)
			goto ERR;

		CHANGE_HTTP_STATE(HTTP_STATE_DOWNLOADING,stage);
		send_client_da_state(download_id, DA_STATE_RESUMED,
				DA_RESULT_OK);
		send_client_da_state(download_id, DA_STATE_DOWNLOADING,
				DA_RESULT_OK);
	}

	_da_thread_mutex_lock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));
	http_state = GET_HTTP_STATE_ON_STAGE(stage);
	//	DA_LOG(HTTPManager, "http_state = %d", http_state);
	_da_thread_mutex_unlock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));

	switch (http_state) {
	case HTTP_STATE_REDIRECTED:
		DA_LOG(HTTPManager, "Just ignore http body, because this body is not for redirection one.");
		break;

	case HTTP_STATE_DOWNLOADING:
		/* Should this function before updating download info
		 * Because it extract mime type at once only if first download updating at client */
		ret = file_write_ongoing(stage, body, body_len);
		if (ret != DA_RESULT_OK)
			goto ERR;
		if (DA_TRUE == is_this_client_manual_download_type()) {
			send_client_update_dl_info(
					download_id,
					GET_DL_REQ_ID(download_id),
					GET_CONTENT_STORE_CONTENT_TYPE(GET_STAGE_CONTENT_STORE_INFO(stage)),
					body_len,
					GET_CONTENT_STORE_TMP_FILE_NAME(GET_STAGE_CONTENT_STORE_INFO(stage)),
					DA_NULL,
					body);
		} else if ((DA_TRUE ==
				IS_CONTENT_STORE_FILE_BYTES_WRITTEN_TO_FILE(GET_STAGE_CONTENT_STORE_INFO(stage)))) {
			send_client_update_downloading_info(
					download_id,
					GET_DL_REQ_ID(download_id),
					GET_CONTENT_STORE_CURRENT_FILE_SIZE(GET_STAGE_CONTENT_STORE_INFO(stage)),
					DA_NULL);

			IS_CONTENT_STORE_FILE_BYTES_WRITTEN_TO_FILE(GET_STAGE_CONTENT_STORE_INFO(stage))
					= DA_FALSE;
		}
		break;

	case HTTP_STATE_REQUEST_PAUSE:
		if (DA_TRUE == is_this_client_manual_download_type()) {
			send_client_update_dl_info(
					download_id,
					GET_DL_REQ_ID(download_id),
					GET_CONTENT_STORE_CONTENT_TYPE(GET_STAGE_CONTENT_STORE_INFO(stage)),
					GET_CONTENT_STORE_FILE_SIZE(GET_STAGE_CONTENT_STORE_INFO(stage)),
					GET_CONTENT_STORE_TMP_FILE_NAME(GET_STAGE_CONTENT_STORE_INFO(stage)),
					body,
					DA_NULL);
		} else {
			ret = file_write_ongoing(stage, body, body_len);
			if (ret != DA_RESULT_OK)
			goto ERR;
		}
		break;

	default:
		DA_LOG(HTTPManager, "Do nothing! http_state is in case %d", http_state);

		goto ERR;
	}

ERR:
	return ret;
}

/* Function should be renamed , as it is actually not setting the header fields in download info */
da_result_t set_hdr_fields_on_download_info(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	da_bool_t b_ret = DA_FALSE;

	req_dl_info *request_info = DA_NULL;
	http_msg_response_t *http_msg_response = NULL;

	char *value = NULL;
	int int_value = 0;

	DA_LOG_FUNC_START(HTTPManager);

	request_info = GET_STAGE_TRANSACTION_INFO(stage);

	http_msg_response
			= request_info->http_info.http_msg_response;
	if (!http_msg_response) {
		DA_LOG_ERR(HTTPManager, "There is no header data!!");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}

	b_ret = http_msg_response_get_content_type(http_msg_response, &value);
	if (b_ret) {
		GET_REQUEST_HTTP_HDR_CONT_TYPE(request_info) = value;
		value = NULL;
		DA_LOG(HTTPManager, "[Content-Type][%s] - stored", GET_REQUEST_HTTP_HDR_CONT_TYPE(request_info));
	}

	b_ret = http_msg_response_get_content_length(http_msg_response,
			&int_value);
	if (b_ret) {
		GET_REQUEST_HTTP_HDR_CONT_LEN(request_info) = int_value;
		int_value = 0;
		DA_LOG(HTTPManager, "[Content-Length][%d] - stored", GET_REQUEST_HTTP_HDR_CONT_LEN(request_info));
	}

	b_ret = http_msg_response_get_ETag(http_msg_response, &value);
	if (b_ret) {
		GET_REQUEST_HTTP_HDR_ETAG(request_info) = value;
		value = NULL;
		DA_LOG(HTTPManager, "[ETag][%s] - stored ", GET_REQUEST_HTTP_HDR_ETAG(request_info));
	}

ERR:
	return ret;
}

da_result_t _check_content_type_is_matched(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	req_dl_info *request_info = DA_NULL;
	source_info_t *source_info = DA_NULL;
	char *content_type_from_server = DA_NULL;

	DA_LOG_FUNC_START(HTTPManager);

	request_info = GET_STAGE_TRANSACTION_INFO(stage);
	source_info = GET_STAGE_SOURCE_INFO(stage);

	content_type_from_server = GET_REQUEST_HTTP_HDR_CONT_TYPE(request_info);
	if (content_type_from_server == DA_NULL) {
		DA_LOG(HTTPManager, "http header has no Content-Type field, no need to compare");
		return DA_RESULT_OK;
	}

	return ret;
}

da_result_t _check_enough_memory_for_this_download(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;

	req_dl_info *request_info = DA_NULL;

	long long cont_len = 0;
	da_storage_size_t memory;

	DA_LOG_FUNC_START(HTTPManager);

	memset(&memory, 0x00, sizeof(da_storage_size_t));
	request_info = GET_STAGE_TRANSACTION_INFO(stage);

	cont_len = (long long) GET_REQUEST_HTTP_HDR_CONT_LEN(request_info);
	if (cont_len) {
		ret = get_available_memory(DA_STORAGE_PHONE, &memory);
		if (DA_RESULT_OK == ret) {
			DA_LOG(HTTPManager, "Memory avail: %lu, Memory block :%lu Content: %lld",memory.b_available,memory.b_size, cont_len);
			if (memory.b_available < ((cont_len
					+ SAVE_FILE_BUFFERING_SIZE_50KB)
					/ memory.b_size)) /* 50KB buffering */
			{
				ret = DA_ERR_DISK_FULL;
				goto ERR;
			}
		}
	}

ERR:
	return ret;
}

da_result_t _check_this_partial_download_is_available(stage_info *stage,
		http_msg_response_t *new_http_msg_response)
{
	da_result_t ret = DA_RESULT_OK;
	da_bool_t b_ret = DA_FALSE;

	char *origin_ETag = NULL;
	char *new_ETag = NULL;

	int remained_content_len = 0;
	da_storage_size_t memory;

	char *value = NULL;
	int int_value = 0;

	DA_LOG_FUNC_START(HTTPManager);

	origin_ETag
			= GET_REQUEST_HTTP_HDR_ETAG(GET_STAGE_TRANSACTION_INFO(stage));

	b_ret = http_msg_response_get_content_length(new_http_msg_response,
			&int_value);
	if (b_ret) {
		remained_content_len = int_value;
		int_value = 0;
		DA_LOG(HTTPManager, "[remained_content_len][%d]", remained_content_len);
	}

	b_ret = http_msg_response_get_ETag(new_http_msg_response, &value);
	if (b_ret) {
		new_ETag = value;
		value = NULL;
		DA_LOG(HTTPManager, "[new ETag][%s]", new_ETag);
	} else {
		goto ERR;
	}

	if (0 != strcmp(origin_ETag, new_ETag)) {
		DA_LOG_ERR(HTTPManager, "ETag is not identical! revoke!");
		/* FIXME Later : Need to detail error exception handling */
		ret = DA_ERR_NETWORK_FAIL;
		/*ret = DA_ERR_MISMATCH_HTTP_HEADER; */
		goto ERR;
	}

	if (remained_content_len) {
		ret = get_available_memory(DA_STORAGE_PHONE, &memory);
		if (DA_RESULT_OK == ret) {
			DA_LOG(HTTPManager, "Memory avail: %lu, Memory block :%lu Content: %lld",memory.b_available,memory.b_size, remained_content_len );
			if (memory.b_available < ((remained_content_len
					+ SAVE_FILE_BUFFERING_SIZE_50KB)
					/ memory.b_size)) /* 50KB buffering */
			{
				ret = DA_ERR_DISK_FULL;
				goto ERR;
			}
		}
	}

ERR:
	if (new_ETag) {
		free(new_ETag);
		new_ETag = DA_NULL;
	}

	return ret;
}

da_result_t _check_downloaded_file_size_is_same_with_header_content_size(
		stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;

	req_dl_info *request_info = DA_NULL;
	file_info *file_info_data = DA_NULL;

	char *real_file_path = DA_NULL;
	int content_size_from_real_file = DA_INVALID_ID;
	unsigned int content_size_from_http_header = DA_INVALID_ID;

	DA_LOG_FUNC_START(HTTPManager);

	request_info = GET_STAGE_TRANSACTION_INFO(stage);
	file_info_data = GET_STAGE_CONTENT_STORE_INFO(stage);

	content_size_from_http_header
			= GET_CONTENT_STORE_FILE_SIZE(file_info_data);

	if (content_size_from_http_header > 0) {
		real_file_path
				= GET_CONTENT_STORE_TMP_FILE_NAME(file_info_data);
		get_file_size(real_file_path,
				&content_size_from_real_file);

		if ((unsigned int) content_size_from_real_file
				!= content_size_from_http_header) {
			DA_LOG_ERR(HTTPManager, "size from header = %d, real size = %d, DA_ERR_MISMATCH_CONTENT_SIZE", content_size_from_http_header, content_size_from_real_file);
			ret = DA_ERR_MISMATCH_CONTENT_SIZE;
		}
	}

	return ret;
}

da_result_t _disconnect_transaction(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	int transaction_id = DA_INVALID_ID;

	DA_LOG_FUNC_START(HTTPManager);

	transaction_id
			= GET_REQUEST_HTTP_TRANS_ID(GET_STAGE_TRANSACTION_INFO(stage));

	DA_LOG(HTTPManager, "transaction_id = %d download_id = %d", transaction_id, GET_STAGE_DL_ID(stage));

	if (transaction_id != DA_INVALID_ID) {
		ret = PI_http_disconnect_transaction(transaction_id);
		GET_REQUEST_HTTP_TRANS_ID(GET_STAGE_TRANSACTION_INFO(stage))
				= DA_INVALID_ID;
	}

	return ret;
}

da_result_t _cancel_transaction(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	int transaction_id = DA_INVALID_ID;

	DA_LOG_FUNC_START(HTTPManager);

	transaction_id
			= GET_REQUEST_HTTP_TRANS_ID(GET_STAGE_TRANSACTION_INFO(stage));

	DA_LOG(HTTPManager, "transaction_id = %d", transaction_id);

	if (transaction_id != DA_INVALID_ID) {
		http_state_t state = 0;
		_da_thread_mutex_lock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));
		state = GET_HTTP_STATE_ON_STAGE(stage);
		if (state <= HTTP_STATE_DOWNLOAD_REQUESTED)
			ret = PI_http_cancel_transaction(transaction_id, DA_TRUE);
		else
			ret = PI_http_cancel_transaction(transaction_id, DA_FALSE);
		_da_thread_mutex_unlock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(stage)));
	}

	return ret;
}

void __parsing_user_request_header(char *user_request_header,
		char **out_field, char **out_value)
{
	int len = 0;
	char *pos = NULL;
	char *temp_pos = NULL;
	char *field = NULL;
	char *value = NULL;

	DA_LOG_FUNC_START(HTTPManager);

	if (!user_request_header) {
		DA_LOG_ERR(HTTPManager, "user_request_header is NULL");
		goto ERR;
	}

	pos = strchr(user_request_header, ':');
	if (!pos) {
		DA_LOG_ERR(HTTPManager, "Fail to parse");
		goto ERR;
	}
	temp_pos = (char *)user_request_header;
	while (*temp_pos)
	{
		if (temp_pos == pos || *temp_pos == ' ') {
			len =  temp_pos - user_request_header;
			break;
		}
		temp_pos++;
	}
	if (len < 1) {
		DA_LOG_ERR(HTTPManager, "Wrong field name");
		goto ERR;
	}
	field = (char *)calloc(1, len + 1);
	if (!field) {
		DA_LOG_ERR(HTTPManager, "Fail to calloc");
		goto ERR;
	}
	strncpy(field, user_request_header, len);
	pos++;
	while (*pos)
	{
		if (*pos != ' ')
			break;
		pos++;
	}
	len = strlen(pos) + 1;
	value = (char *)calloc(1, len + 1);
	if (!value) {
		DA_LOG_ERR(HTTPManager, "Fail to calloc");
		goto ERR;
	}
	strncpy(value, pos, len);
	*out_field = field;
	*out_value = value;
	DA_LOG(HTTPManager, "field[%s], value[%s]", field, value);

	return;
ERR:
	if (field) {
		free(field);
		field = NULL;
	}
	return;
}

