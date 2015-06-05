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
#include <string.h>
#include <errno.h>
#include <time.h>

#include "download-agent-dl-info.h"
#include "download-agent-file.h"
#include "download-agent-mime-util.h"
#include "download-agent-client-mgr.h"
#include "download-agent-http-msg-handler.h"
#include "download-agent-plugin-conf.h"
#include "download-agent-plugin-drm.h"
#include "download-agent-plugin-libcurl.h"

void __http_update_cb(http_raw_data_t *data, void *user_param);

#define CONVERT_STR(NAME) (#NAME)

static const char *__get_state_str(http_state_t state)
{
	char *str = NULL;
	switch(state) {
	case HTTP_STATE_READY_TO_DOWNLOAD:
		str = CONVERT_STR(HTTP_STATE_READY_TO_DOWNLOAD);
		break;
	case HTTP_STATE_REDIRECTED:
		str = CONVERT_STR(HTTP_STATE_REDIRECTED);
		break;
	case HTTP_STATE_DOWNLOAD_REQUESTED:
		str = CONVERT_STR(HTTP_STATE_DOWNLOAD_REQUESTED);
		break;
	case HTTP_STATE_DOWNLOAD_STARTED:
		str = CONVERT_STR(HTTP_STATE_DOWNLOAD_STARTED);
		break;
	case HTTP_STATE_DOWNLOADING:
		str = CONVERT_STR(HTTP_STATE_DOWNLOADING);
		break;
	case HTTP_STATE_DOWNLOAD_FINISH:
		str = CONVERT_STR(HTTP_STATE_DOWNLOAD_FINISH);
		break;
	case HTTP_STATE_REQUEST_CANCEL:
		str = CONVERT_STR(HTTP_STATE_REQUEST_CANCEL);
		break;
	case HTTP_STATE_REQUEST_PAUSE:
		str = CONVERT_STR(HTTP_STATE_REQUEST_PAUSE);
		break;
	case HTTP_STATE_REQUEST_RESUME:
		str = CONVERT_STR(HTTP_STATE_REQUEST_RESUME);
		break;
	case HTTP_STATE_CANCELED:
		str = CONVERT_STR(HTTP_STATE_CANCELED);
		break;
	case HTTP_STATE_FAILED:
		str = CONVERT_STR(HTTP_STATE_FAILED);
		break;
	case HTTP_STATE_PAUSED:
		str = CONVERT_STR(HTTP_STATE_PAUSED);
		break;
	case HTTP_STATE_RESUMED:
		str = CONVERT_STR(HTTP_STATE_RESUMED);
		break;
	case HTTP_STATE_ABORTED:
		str = CONVERT_STR(HTTP_STATE_ABORTED);
		break;
	case HTTP_STATE_WAIT_FOR_NET_ERR:
		str = CONVERT_STR(HTTP_STATE_WAIT_FOR_NET_ERR);
		break;
	default:
		str = "Unknown State";
		break;
	}
	return str;
}

void __init_http_info(http_info_t *http_info)
{
	DA_LOGV("");

	http_info->state = HTTP_STATE_READY_TO_DOWNLOAD;
	http_info->update_cb = __http_update_cb;
	DA_MUTEX_INIT(&(http_info->mutex_state), DA_NULL);
	DA_MUTEX_INIT(&(http_info->mutex_http), DA_NULL);
	DA_COND_INIT(&(http_info->cond_http), DA_NULL);
}

void __parsing_user_request_header(char *user_request_header,
		char **out_field, char **out_value)
{
	int len = 0;
	char *pos = NULL;
	char *temp_pos = NULL;
	char *field = NULL;
	char *value = NULL;

	DA_LOGV("");

	if (!user_request_header) {
		DA_LOGE("NULL CHECK!: user_request_header");
		goto ERR;
	}

	pos = strchr(user_request_header, ':');
	if (!pos) {
		DA_LOGE("Fail to parse");
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
		DA_LOGE("Wrong field name");
		goto ERR;
	}
	field = (char *)calloc(1, len + 1);
	if (!field) {
		DA_LOGE("Fail to calloc");
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
		DA_LOGE("Fail to calloc");
		goto ERR;
	}
	strncpy(value, pos, len);
	*out_field = field;
	*out_value = value;
	DA_SECURE_LOGD("field[%s], value[%s]", field, value);

	return;
ERR:
	if (field) {
		free(field);
		field = NULL;
	}
	return;
}


da_ret_t __set_http_request_hdr(req_info_t *req_info, http_info_t *http_info, file_info_t *file_info)
{
	da_ret_t ret = DA_RESULT_OK;
	http_msg_request_t *http_msg_request = NULL;
	char *user_agent = NULL;
	int count = 0;

	DA_LOGV("");

	NULL_CHECK_RET(req_info);
	NULL_CHECK_RET(http_info);
	NULL_CHECK_RET(file_info);
	NULL_CHECK_RET_OPT(req_info->url, DA_ERR_INVALID_URL);
	count = req_info->req_header_count;

	ret = http_msg_request_create(&http_msg_request);
	if (ret != DA_RESULT_OK)
		goto ERR;

	ret = http_msg_request_set_url(http_msg_request, req_info->url);
	if (ret != DA_RESULT_OK)
		goto ERR;

	ret = get_user_agent_string(&user_agent);
	if (user_agent && ret == DA_RESULT_OK)
		http_msg_request_add_field(http_msg_request,
				HTTP_FIELD_UAGENT, user_agent);


	http_msg_request_add_field(http_msg_request,
			HTTP_FIELD_ACCEPT_LANGUAGE, "en");
	http_msg_request_add_field(http_msg_request,
			HTTP_FIELD_ACCEPT_CHARSET, "utf-8");

	if (req_info->req_header && count > 0) {
		int i = 0;
		for (i = 0; i < count; i++) {
			char *field = NULL;
			char *value = NULL;
			__parsing_user_request_header(req_info->req_header[i],
					&field, &value);
			if (field && value) {
				http_msg_request_add_field(http_msg_request, field, value);
				if (field) {
					free(field);
					field = NULL;
				}
				if (value) {
					free(value);
					value= NULL;
				}
			} else {
				if (field) {
					free(field);
					field = NULL;
				}
				if (value) {
					free(value);
					value= NULL;
				}
				DA_LOGE("Fail to parse user request header");
			}
		}
	}
	if (req_info->etag) {
		char buff[64] = {0,};
		da_size_t size = 0;
		http_msg_request_add_field(http_msg_request,
				HTTP_FIELD_IF_RANGE, req_info->etag);
		get_file_size(req_info->temp_file_path, &size);
#ifdef _RAF_SUPPORT
		file_info->file_size_of_temp_file = size;
#endif
		snprintf(buff, sizeof(buff)-1, "bytes=%llu-", size);
		http_msg_request_add_field(http_msg_request,
				HTTP_FIELD_RANGE, buff);
	}

	http_info->http_msg_request = http_msg_request;
	free(user_agent);
	return ret;
ERR:
	if (http_msg_request)
		http_msg_request_destroy(&http_msg_request);

	return ret;

}

da_ret_t __create_http_resume_hdr(req_info_t *req_info, http_info_t *http_info,
		file_info_t *file_info)
{
	da_ret_t ret = DA_RESULT_OK;
	da_bool_t b_ret = DA_FALSE;
	char *value = NULL;
	char temp_size_str[32] = { 0, };
	char *etag_from_response = NULL;
	char *date_from_response = NULL;
	http_msg_response_t *first_response = NULL;
	http_msg_request_t *resume_request = NULL;
	http_msg_request_t *old_request = NULL;

	DA_LOGV("");

	first_response = http_info->http_msg_response;
	if (first_response) {
		b_ret = http_msg_response_get_ETag(first_response, &value);
		if (b_ret) {
			etag_from_response = value;
			value = NULL;
			DA_SECURE_LOGD("[ETag][%s]", etag_from_response);
		}
		b_ret = http_msg_response_get_date(first_response, &value);
		if (b_ret) {
			date_from_response = value;
			value = NULL;
			DA_LOGV("[Date][%s]", date_from_response);
		}
		DA_SECURE_LOGD("downloaded_size[%u]", file_info->bytes_written_to_file);
		snprintf(temp_size_str, sizeof(temp_size_str), "bytes=%llu-",
				file_info->bytes_written_to_file);
		DA_SECURE_LOGD("size str[%s]", temp_size_str);
		free(first_response);
		http_info->http_msg_response = DA_NULL;
	}
	old_request = http_info->http_msg_request;
	free(old_request);
	http_info->http_msg_request = DA_NULL;

	ret = __set_http_request_hdr(req_info, http_info, file_info);
	if (ret != DA_RESULT_OK)
		goto ERR;

	resume_request = http_info->http_msg_request;
	if (etag_from_response) {
		http_msg_request_add_field(resume_request, HTTP_FIELD_IF_RANGE,
				etag_from_response);
	} else {
		if (date_from_response) {
			http_msg_request_add_field(resume_request,
					HTTP_FIELD_IF_RANGE, date_from_response);
		}
	}

	if (strlen(temp_size_str) > 0)
		http_msg_request_add_field(resume_request, HTTP_FIELD_RANGE,
				temp_size_str);

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


da_ret_t __start_transaction(da_info_t *da_info)
{
	da_ret_t ret = DA_RESULT_OK;
	http_info_t *http_info;
	DA_LOGV("");

	if (!da_info) {
		DA_LOGE("NULL CHECK!: da_info");
		return DA_ERR_INVALID_ARGUMENT;
	}
	http_info = da_info->http_info;
	if (!http_info) {
		DA_LOGE("NULL CHECK!: http_info");
		return DA_ERR_INVALID_ARGUMENT;
	}
	http_info->http_method = HTTP_METHOD_GET;
	http_info->proxy_addr = get_proxy_address();

	ret = PI_http_start(da_info);

	return ret;
}

da_ret_t __start_resume_transaction(da_info_t *da_info)
{
	da_ret_t ret = DA_RESULT_OK;
	http_info_t *http_info = DA_NULL;
	file_info_t *file_info = DA_NULL;
	req_info_t *req_info = DA_NULL;

	NULL_CHECK_RET(da_info);
	http_info = da_info->http_info;
	NULL_CHECK_RET(http_info);
	file_info = da_info->file_info;
	NULL_CHECK_RET(file_info);
	req_info = da_info->req_info;
	NULL_CHECK_RET(req_info);

	ret = __create_http_resume_hdr(req_info, http_info,
			file_info);
	if (ret != DA_RESULT_OK)
		return ret;

	reset_http_info_for_resume(http_info);
	if (file_info->file_path) {
		req_info->temp_file_path = strdup(file_info->file_path);
	} else {
		DA_LOGE("file_path cannot be NULL in resume case");
		return DA_ERR_INVALID_ARGUMENT;
	}
	ret = __start_transaction(da_info);
	return ret;
}


da_ret_t __start_new_transaction(da_info_t *da_info)
{
	da_ret_t ret = DA_RESULT_OK;

	if (!da_info) {
		DA_LOGE("NULL CHECK!: da_info");
		return DA_ERR_INVALID_ARGUMENT;
	}

	ret = __set_http_request_hdr(da_info->req_info, da_info->http_info, da_info->file_info);
	if (ret != DA_RESULT_OK)
		return ret;

	ret = __start_transaction(da_info);
	return ret;
}

int __check_wait_for_auto_retry(http_info_t *http_info)
{
	da_ret_t ret = DA_RESULT_OK;
	struct timespec ts;
	struct timeval tp;
	NULL_CHECK_RET_OPT(http_info, 0);
	gettimeofday(&tp, NULL);
	ts.tv_sec = tp.tv_sec + DA_MAX_TIME_OUT;
	ts.tv_nsec = tp.tv_usec * 1000;
	DA_LOGI("Network Fail case, wait for a while");

	DA_MUTEX_LOCK(&(http_info->mutex_state));
	http_info->state = HTTP_STATE_WAIT_FOR_NET_ERR;
	DA_MUTEX_UNLOCK(&(http_info->mutex_state));
	DA_MUTEX_LOCK(&(http_info->mutex_http));
	ret = pthread_cond_timedwait(&(http_info->cond_http),
			&(http_info->mutex_http), &ts);
	DA_MUTEX_UNLOCK(&(http_info->mutex_http));
	if (ret == ETIMEDOUT) {
		DA_LOGI("Waiting is done by timeout");
	} else if (ret != 0) {
		DA_LOGE("fail to pthread_cond_waittime[%d][%s]",ret, strerror(ret));
	} else {
		DA_LOGI("Waiting is done by control");
		DA_MUTEX_LOCK(&(http_info->mutex_state));
		DA_LOGI("http_state[%s]", __get_state_str(http_info->state));
		DA_MUTEX_UNLOCK(&(http_info->mutex_state));
		return 1;
	}

	return 0;
}

// In download thread
da_ret_t request_http_download(da_info_t *da_info)
{
	da_ret_t ret = DA_RESULT_OK;
	req_info_t *req_info = DA_NULL;
	http_info_t *http_info = DA_NULL;
	http_state_t http_state = 0;
	da_bool_t need_wait = DA_TRUE;

	DA_LOGV("");

	NULL_CHECK_RET(da_info);
	req_info = da_info->req_info;
	NULL_CHECK_RET(req_info);
	http_info = da_info->http_info;
	NULL_CHECK_RET(http_info);
	__init_http_info(http_info);

	do {
		DA_MUTEX_LOCK(&(http_info->mutex_state));
		http_state = http_info->state;
		DA_MUTEX_UNLOCK(&(http_info->mutex_state));
		DA_LOGD("http_state[%s][%d]",__get_state_str(http_info->state), da_info->da_id);
		switch (http_state) {
		case HTTP_STATE_READY_TO_DOWNLOAD:
			DA_MUTEX_LOCK(&(http_info->mutex_state));
			http_info->state = HTTP_STATE_DOWNLOAD_REQUESTED;
			DA_MUTEX_UNLOCK(&(http_info->mutex_state));
			ret = __start_new_transaction(da_info);
			DA_MUTEX_LOCK(&(http_info->mutex_state));
			http_state = http_info->state;
			DA_LOGD("http_state[%s][%d]",__get_state_str(http_info->state), da_info->da_id);
			http_info->error_code = ret;
			DA_MUTEX_UNLOCK(&(http_info->mutex_state));
			if (ret == DA_ERR_NETWORK_FAIL && http_state != HTTP_STATE_PAUSED) {
				DA_LOGE("Network failed");
				if (__check_wait_for_auto_retry(http_info) == 1)
					need_wait = DA_TRUE;
				else
					need_wait = DA_FALSE;
			}
			break;
		case HTTP_STATE_REDIRECTED:
		case HTTP_STATE_DOWNLOAD_REQUESTED:
		case HTTP_STATE_DOWNLOAD_STARTED:
		case HTTP_STATE_DOWNLOADING:
		case HTTP_STATE_REQUEST_PAUSE:
			DA_LOGE("Cannot enter here:[%s][id]",
					__get_state_str(http_info->state), da_info->da_id);
			break;
		case HTTP_STATE_REQUEST_CANCEL:
			break;
		case HTTP_STATE_REQUEST_RESUME:
			DA_MUTEX_LOCK(&(http_info->mutex_state));
			http_info->state = HTTP_STATE_READY_TO_DOWNLOAD;
			DA_MUTEX_UNLOCK(&(http_info->mutex_state));
			need_wait = DA_TRUE;
			break;
		case HTTP_STATE_CANCELED:
			need_wait = DA_FALSE;
			ret = DA_RESULT_USER_CANCELED;
			break;
		case HTTP_STATE_PAUSED:
			DA_LOGD("error_code[%d]", http_info->error_code);
			send_client_paused_info(da_info);
			DA_LOGD("Waiting thread for paused state");
			DA_MUTEX_LOCK(&(http_info->mutex_http));
			pthread_cond_wait(&(http_info->cond_http),&(http_info->mutex_http));
			DA_MUTEX_UNLOCK(&(http_info->mutex_http));
			DA_LOGD("Wake up thread due to resume");
			break;
		case HTTP_STATE_RESUMED:
			DA_MUTEX_LOCK(&(http_info->mutex_state));
			http_info->state = HTTP_STATE_DOWNLOAD_REQUESTED;
			DA_MUTEX_UNLOCK(&(http_info->mutex_state));
			ret = __start_resume_transaction(da_info);
			DA_MUTEX_LOCK(&(http_info->mutex_state));
			http_state = http_info->state;
			DA_MUTEX_UNLOCK(&(http_info->mutex_state));
			if (ret == DA_ERR_NETWORK_FAIL && http_state != HTTP_STATE_PAUSED) {
				DA_LOGE("Network failed");
				if (__check_wait_for_auto_retry(http_info) == 1)
					need_wait = DA_TRUE;
				else
					need_wait = DA_FALSE;
			}
			break;
		case HTTP_STATE_DOWNLOAD_FINISH:
			need_wait = DA_FALSE;
			if (ret == DA_RESULT_OK)
				ret = check_drm_convert(da_info->file_info);
			break;
		case HTTP_STATE_FAILED:
			if (ret == DA_ERR_NETWORK_FAIL) {
				if (__check_wait_for_auto_retry(http_info) == 1)
					need_wait = DA_TRUE;
				else
					need_wait = DA_FALSE;
			} else {
				need_wait = DA_FALSE;
			}
			break;
		case HTTP_STATE_ABORTED:
			need_wait = DA_FALSE;
			break;
		default:
			break;
		}
	} while (need_wait == DA_TRUE);
	DA_LOGD("Final http_state[%s][%d] err[%d]",__get_state_str(http_info->state), da_info->da_id, ret);
	if (http_info->state != HTTP_STATE_PAUSED)
		send_client_finished_info(da_info ,ret);
	DA_LOGI("=== Exiting http_download ret[%d] ===", ret);
	return ret;
}

da_ret_t __disconnect_transaction(http_info_t *http_info)
{
	da_ret_t ret = DA_RESULT_OK;
	DA_LOGD("");
	ret = PI_http_disconnect(http_info);
	return ret;
}

da_ret_t __handle_event_abort(http_info_t *http_info)
{
	da_ret_t ret = DA_RESULT_OK;
	http_state_t state = 0;

	DA_LOGD("");

	DA_MUTEX_LOCK(&(http_info->mutex_state));
	state = http_info->state;
	DA_MUTEX_UNLOCK(&(http_info->mutex_state));
	DA_LOGV("http_state[%s]", __get_state_str(state));

	switch (state) {
	case HTTP_STATE_READY_TO_DOWNLOAD:
	case HTTP_STATE_REDIRECTED:
	case HTTP_STATE_DOWNLOAD_REQUESTED:
	case HTTP_STATE_DOWNLOAD_STARTED:
	case HTTP_STATE_DOWNLOADING:
	case HTTP_STATE_REQUEST_CANCEL:
	case HTTP_STATE_REQUEST_PAUSE:
	case HTTP_STATE_REQUEST_RESUME:
	case HTTP_STATE_CANCELED:
	case HTTP_STATE_PAUSED:
	case HTTP_STATE_RESUMED:
	case HTTP_STATE_ABORTED:
	case HTTP_STATE_WAIT_FOR_NET_ERR:
		/* IF the network session is terminated due to some error,
		 * the state can be aborted.(data aborted case) */
		DA_MUTEX_LOCK(&(http_info->mutex_state));
		http_info->state = HTTP_STATE_ABORTED;
		DA_MUTEX_UNLOCK(&(http_info->mutex_state));
		__disconnect_transaction(http_info);
		break;
	case HTTP_STATE_DOWNLOAD_FINISH:
		break;
	default:
		DA_LOGE("Cannot enter here");
		break;
	}
	return ret;
}

da_ret_t __check_enough_memory(http_info_t *http_info, char *user_install_path)
{
	da_ret_t ret = DA_RESULT_OK;
	da_size_t cont_len = 0;
	char *dir_path = DA_DEFAULT_INSTALL_PATH_FOR_PHONE;

	DA_LOGV("");
	NULL_CHECK_RET(http_info);
	cont_len = http_info->content_len_from_header;
	if (cont_len > 0) {
		if (user_install_path)
			dir_path = user_install_path;
		ret = get_available_memory(dir_path, cont_len);
	}
	return ret;
}

da_ret_t request_to_abort_http_download(da_info_t *da_info)
{
	da_ret_t ret = DA_RESULT_OK;
	ret = __handle_event_abort(da_info->http_info);
	return ret;
}

da_ret_t request_to_cancel_http_download(da_info_t *da_info)
{
	da_ret_t ret = DA_RESULT_OK;
	http_info_t *http_info = DA_NULL;
	http_state_t http_state = 0;
	DA_LOGV("");

	NULL_CHECK_RET(da_info);
	http_info = da_info->http_info;
	NULL_CHECK_RET(http_info);

	DA_MUTEX_LOCK(&(http_info->mutex_state));
	http_state = http_info->state;
	DA_MUTEX_UNLOCK(&(http_info->mutex_state));
	DA_LOGD("http_state[%s]", __get_state_str(http_state));
	switch (http_state) {
	case HTTP_STATE_READY_TO_DOWNLOAD:
		ret = PI_http_cancel(http_info);
		DA_MUTEX_LOCK(&(http_info->mutex_state));
		http_info->state = HTTP_STATE_CANCELED;
		DA_MUTEX_UNLOCK(&(http_info->mutex_state));
		if (da_info->thread_id > 0) {
			if (pthread_cancel(da_info->thread_id) != 0) {
				DA_LOGE("Fail to cancel thread id[%d] err[%s]",
						da_info->thread_id, strerror(errno));
			} else {
				DA_LOGI("====Exit thread with cancel:da_id[%d]===",
						da_info->da_id);
			}
		}
		break;
	case HTTP_STATE_WAIT_FOR_NET_ERR:
		DA_MUTEX_LOCK(&(http_info->mutex_state));
		http_info->state = HTTP_STATE_CANCELED;
		DA_MUTEX_UNLOCK(&(http_info->mutex_state));
		DA_MUTEX_LOCK(&(http_info->mutex_http));
		DA_COND_SIGNAL(&(http_info->cond_http));
		DA_MUTEX_UNLOCK(&(http_info->mutex_http));
		break;
	case HTTP_STATE_PAUSED:
		reset_http_info(http_info);
		DA_MUTEX_LOCK(&(http_info->mutex_state));
		http_info->state = HTTP_STATE_CANCELED;
		DA_MUTEX_UNLOCK(&(http_info->mutex_state));
		break;
	case HTTP_STATE_DOWNLOAD_REQUESTED:
	case HTTP_STATE_DOWNLOAD_STARTED:
	case HTTP_STATE_DOWNLOADING:
	case HTTP_STATE_REQUEST_RESUME:
	case HTTP_STATE_RESUMED:
		ret = PI_http_cancel(http_info);
		DA_MUTEX_LOCK(&(http_info->mutex_state));
		http_info->state = HTTP_STATE_REQUEST_CANCEL;
		DA_MUTEX_UNLOCK(&(http_info->mutex_state));
		break;
	case HTTP_STATE_DOWNLOAD_FINISH:
		ret = DA_ERR_INVALID_STATE;
		break;
	case HTTP_STATE_REQUEST_CANCEL:
		DA_LOGV("cancel is already in progress... ");
		ret = DA_ERR_INVALID_STATE;
		break;
	default:
		ret = DA_ERR_INVALID_STATE;
		DA_LOGE("Cannot enter here");
		break;
	}
	return ret;
}

da_ret_t request_to_suspend_http_download(da_info_t *da_info)
{

	da_ret_t ret = DA_RESULT_OK;
	http_info_t *http_info = DA_NULL;
	http_state_t http_state = 0;

	DA_LOGV("");

	NULL_CHECK_RET(da_info);
	http_info = da_info->http_info;
	NULL_CHECK_RET(http_info);

	DA_MUTEX_LOCK(&(http_info->mutex_state));
	http_state = http_info->state;
	DA_MUTEX_UNLOCK(&(http_info->mutex_state));
	DA_LOGD("http_state[%s]", __get_state_str(http_state));

	switch (http_state) {
	case HTTP_STATE_PAUSED:
	case HTTP_STATE_REQUEST_PAUSE:
		DA_LOGI("Already paused. http_state[%s]", __get_state_str(http_state));
		ret = DA_ERR_ALREADY_SUSPENDED;
		break;
	case HTTP_STATE_READY_TO_DOWNLOAD:
		DA_LOGE("Download has not been started yet");
		ret = DA_ERR_INVALID_STATE;
		break;
	case HTTP_STATE_WAIT_FOR_NET_ERR:
		DA_LOGD("error_code[%d]", http_info->error_code);
		DA_MUTEX_LOCK(&(http_info->mutex_state));
		http_info->state = HTTP_STATE_PAUSED;
		DA_MUTEX_UNLOCK(&(http_info->mutex_state));
		DA_MUTEX_LOCK(&(http_info->mutex_http));
		DA_COND_SIGNAL(&(http_info->cond_http));
		DA_MUTEX_UNLOCK(&(http_info->mutex_http));
		break;
	default:
		DA_MUTEX_LOCK(&(http_info->mutex_state));
		http_info->state = HTTP_STATE_REQUEST_PAUSE;
		DA_MUTEX_UNLOCK(&(http_info->mutex_state));
		DA_LOGD("error_code[%d]", http_info->error_code);
		if (http_info->error_code != DA_ERR_NETWORK_FAIL)
			ret = PI_http_pause(http_info);
		DA_MUTEX_LOCK(&(http_info->mutex_state));
		http_info->state = HTTP_STATE_PAUSED;
		DA_MUTEX_UNLOCK(&(http_info->mutex_state));

		break;
	}
	return ret;
}

da_ret_t request_to_resume_http_download(da_info_t *da_info)
{
	da_ret_t ret = DA_RESULT_OK;
	http_info_t *http_info = DA_NULL;
	http_state_t http_state = 0;
	int retry_count = 0;

	DA_LOGV("");

	NULL_CHECK_RET(da_info);
	http_info = da_info->http_info;
	NULL_CHECK_RET(http_info);

	DA_MUTEX_LOCK(&(http_info->mutex_state));
	http_state = http_info->state;
	DA_MUTEX_UNLOCK(&(http_info->mutex_state));
	DA_LOGD("http_state[%s]", __get_state_str(http_state));

	switch (http_state) {
	case HTTP_STATE_PAUSED:
		DA_MUTEX_LOCK(&(http_info->mutex_state));
		http_info->state = HTTP_STATE_RESUMED;
		DA_MUTEX_UNLOCK(&(http_info->mutex_state));
		DA_LOGD("Wake up thread for paused state");
		DA_MUTEX_LOCK(&(http_info->mutex_http));
		DA_COND_SIGNAL(&(http_info->cond_http));
		DA_MUTEX_UNLOCK(&(http_info->mutex_http));
		DA_LOGD("error_code[%d]", http_info->error_code);
		if (http_info->error_code != DA_ERR_NETWORK_FAIL) {
			ret = PI_http_unpause(http_info);
			if (ret != DA_RESULT_OK)
				PI_http_cancel(http_info);
			DA_MUTEX_LOCK(&(http_info->mutex_state));
			http_info->state = HTTP_STATE_DOWNLOADING;
			DA_MUTEX_UNLOCK(&(http_info->mutex_state));

		}
		break;
	case HTTP_STATE_REQUEST_PAUSE:
		DA_LOGD("Waiting to handle pause request");
		do {
			DA_MUTEX_LOCK(&(http_info->mutex_state));
			http_state = http_info->state;
			DA_MUTEX_UNLOCK(&(http_info->mutex_state));
			if (http_state == HTTP_STATE_PAUSED) {
				DA_LOGD("Change to paused state");
				ret = PI_http_unpause(http_info);
				break;
			}
			retry_count++;
		} while(retry_count < 10000);
		if (ret != DA_RESULT_OK || retry_count >= 10000)
			PI_http_cancel(http_info);
		break;
	case HTTP_STATE_RESUMED:
		ret = DA_ERR_ALREADY_RESUMED;
		break;
	default:
		DA_LOGE("Fail to resume. Invalid state check. http_state[%s]",
				__get_state_str(http_state));
		ret = DA_ERR_INVALID_STATE;
		// If resume is failed due to invalid state, the previous pause should be canceled.
		PI_http_cancel(http_info);
		break;
	}
	return ret;
}

da_ret_t __check_resume_download_is_available(
		req_info_t *req_info, http_info_t *http_info, file_info_t *file_info)
{
	da_ret_t ret = DA_RESULT_OK;
	da_bool_t b_ret = DA_FALSE;
	char *origin_ETag = NULL;
	char *new_ETag = NULL;
	da_size_t remained_content_len = 0;
	char *value = NULL;
	da_size_t size = 0;
	char *temp_file_path = DA_NULL;
	char *dir_path = DA_DEFAULT_INSTALL_PATH_FOR_PHONE;

	DA_LOGV("");

	origin_ETag = req_info->etag;

	b_ret = http_msg_response_get_content_length(http_info->http_msg_response,
			&size);
	if (b_ret) {
		remained_content_len = size;
		size = 0;
		DA_SECURE_LOGD("remained_content_len[%llu]", remained_content_len);
	}

	b_ret = http_msg_response_get_ETag(http_info->http_msg_response, &value);
	if (b_ret) {
		new_ETag = value;
		value = NULL;
		DA_SECURE_LOGD("new ETag[%s]", new_ETag);
	} else {
		goto ERR;
	}

	if (origin_ETag && new_ETag &&
			0 != strncmp(origin_ETag, new_ETag, strlen(new_ETag))) {
		DA_LOGE("ETag is not identical! revoke!");
		/* FIXME Later : Need to detail error exception handling */
		ret = DA_ERR_NETWORK_FAIL;
		/*ret = DA_ERR_MISMATCH_HTTP_HEADER; */
		goto ERR;
	}

	if (remained_content_len > 0) {
		if (req_info->install_path)
				dir_path = req_info->install_path;
		ret = get_available_memory(dir_path, remained_content_len);
		if (ret != DA_RESULT_OK)
			goto ERR;
	}

	if (!http_info->content_type_from_header) {
		b_ret = http_msg_response_get_content_type(http_info->http_msg_response,
				&value);
		if (b_ret) {
			http_info->content_type_from_header = value;
			value = NULL;
			DA_SECURE_LOGD("Content-Type[%s]",
				http_info->content_type_from_header);
		}
	}
	temp_file_path = req_info->temp_file_path;
	if (!temp_file_path) {
		DA_LOGE("Temporary file path cannot be NULL");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}
	get_file_size(temp_file_path, &size);
	http_info->content_len_from_header = remained_content_len + size;
	DA_SECURE_LOGD("Content-Length[%llu]", http_info->content_len_from_header);
ERR:
	if (new_ETag) {
		free(new_ETag);
		new_ETag = DA_NULL;
	}
	return ret;
}


da_ret_t __check_content_type_is_matched(http_info_t *http_info)
{
	da_ret_t ret = DA_RESULT_OK;
	char *content_type_from_server = DA_NULL;

	DA_LOGV("");

	content_type_from_server = http_info->content_type_from_header;
	if (content_type_from_server == DA_NULL) {
		DA_LOGV("http header has no Content-Type field, no need to compare");
		return DA_RESULT_OK;
	}
	return ret;
}

da_ret_t __handle_http_status_code(http_info_t *http_info,
		file_info_t *file_info, req_info_t *req_info)
{
	da_ret_t ret = DA_RESULT_OK;
	http_state_t http_state = 0;
	http_msg_response_t *http_msg_response = DA_NULL;
	char *location = DA_NULL;
	char *if_range_str = DA_NULL;
	char *range_str = DA_NULL;
	int http_status = 0;

	NULL_CHECK_RET(http_info);
	NULL_CHECK_RET(file_info);
	NULL_CHECK_RET(req_info);
	DA_MUTEX_LOCK(&(http_info->mutex_state));
	http_state = http_info->state;
	DA_MUTEX_UNLOCK(&(http_info->mutex_state));
	DA_LOGD("http_state[%s]", __get_state_str(http_state));
	http_msg_response = http_info->http_msg_response;
	NULL_CHECK_RET(http_msg_response);
	http_status = http_msg_response->status_code;
	switch (http_status) {
	case 200:
	case 201:
	case 202:
	case 203:
// Although expecting 206, 200 response is received. Remove temporary file and reset file info
		if (http_info->http_msg_request &&
				http_msg_request_get_if_range(http_info->http_msg_request, &if_range_str) == DA_TRUE &&
				http_msg_request_get_range(http_info->http_msg_request, &range_str) == DA_TRUE) {
			DA_LOGI("Server do not support if-range option");
			clean_paused_file(file_info);
		}
		free(if_range_str);
		free(range_str);
		if (http_state == HTTP_STATE_REQUEST_RESUME)
			clean_paused_file(file_info);
		ret = __check_content_type_is_matched(http_info);
		if (ret != DA_RESULT_OK)
			goto ERR;
		ret = __check_enough_memory(http_info, req_info->install_path);
		if (ret != DA_RESULT_OK)
			goto ERR;
		DA_MUTEX_LOCK(&(http_info->mutex_state));
		http_info->state =  HTTP_STATE_DOWNLOAD_STARTED;
		DA_MUTEX_UNLOCK(&(http_info->mutex_state));
		break;

	case 206:
		DA_LOGV("HTTP Status is %d - Partial download for resume!", http_status);
		/* The resume can be started with start API.
		 * So the state should be not HTTP_STATE_RESUME_REQUESTED but HTTP_STATE_DOWNLOAD_REQUESTED*/
		if (http_state == HTTP_STATE_DOWNLOAD_REQUESTED) {
			ret = __check_resume_download_is_available(req_info, http_info, file_info);
			if (ret != DA_RESULT_OK)
				goto ERR;
			DA_MUTEX_LOCK(&(http_info->mutex_state));
			http_info->state = HTTP_STATE_DOWNLOAD_STARTED;
			DA_MUTEX_UNLOCK(&(http_info->mutex_state));

		} else if (http_state == HTTP_STATE_REQUEST_RESUME) {
			///FIXME later : how get previous response header
			///ret = __check_this_partial_download_is_available(http_info,
				///	previous_ http_msg_response);
			//if (ret != DA_RESULT_OK)
				//goto ERR;
			DA_MUTEX_LOCK(&(http_info->mutex_state));
			http_info->state = HTTP_STATE_RESUMED;
			DA_MUTEX_UNLOCK(&(http_info->mutex_state));
		} else {
			DA_LOGE("This download is not resumed, revoke");
			ret = DA_ERR_INVALID_STATE;
			goto ERR;
		}
		break;

	case 300:
	case 301:
	case 302:
	case 303:
	case 305:
	case 306:
	case 307:
		DA_LOGV("HTTP Status is %d - redirection!",http_status);
		if (http_msg_response_get_location(http_msg_response, &location)) {
			DA_SECURE_LOGD("location  = %s\n", location);
			http_info->location_url = location;
			DA_LOGI("[TEST] location_url[%p]",http_info->location_url);
		}
		DA_MUTEX_LOCK(&(http_info->mutex_state));
		http_info->state = HTTP_STATE_REDIRECTED;
		DA_MUTEX_UNLOCK(&(http_info->mutex_state));
		http_msg_response_destroy(&http_msg_response);
		http_info->http_msg_response = DA_NULL;
		break;

	case 100:
	case 101:
	case 102:
	case 204:
	case 304:
		DA_LOGV("HTTP Status is %d - 204 server got the request, \
				but no content to reply back, \
				304 means not modified!", http_status);
		ret = DA_ERR_SERVER_RESPOND_BUT_SEND_NO_CONTENT;
		break;

	case 416: // Requested range not satisfiable
	case 503:
	case 504:
	default:
///		GET_REQUEST_HTTP_RESULT(request_info)
///			= DA_ERR_UNREACHABLE_SERVER;
		DA_LOGI("set internal error code : DA_ERR_UNREACHABLE_SERVER");
		break;
	}

ERR:
	return ret;
}

da_ret_t __check_before_downloading(da_info_t *da_info, http_state_t state)
{
	da_ret_t ret = DA_RESULT_OK;
	http_info_t *http_info = DA_NULL;
	req_info_t *req_info = DA_NULL;
	file_info_t *file_info = DA_NULL;
	NULL_CHECK_RET(da_info);
	http_info = da_info->http_info;
	NULL_CHECK_RET(http_info);
	req_info = da_info->req_info;
	NULL_CHECK_RET(req_info);
	file_info = da_info->file_info;
	NULL_CHECK_RET(file_info);
	DA_LOGD("state:%s",__get_state_str(state));
	// resume case
	if (req_info->temp_file_path && file_info->bytes_written_to_file > 0) {
		ret = start_file_append(file_info);
	} else if (state == HTTP_STATE_DOWNLOAD_STARTED) {
		ret = start_file_writing(da_info);
	} else {
		DA_LOGE("Cannot enter here!");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}
	if (DA_RESULT_OK != ret)
		goto ERR;

	DA_MUTEX_LOCK(&(http_info->mutex_state));
	http_info->state = HTTP_STATE_DOWNLOADING;
	DA_MUTEX_UNLOCK(&(http_info->mutex_state));

	ret = send_client_update_dl_info(da_info);
ERR:
	return ret;
}

da_ret_t __handle_event_http_header(http_raw_data_t *raw_data, da_info_t *da_info)
{
	da_ret_t ret = DA_RESULT_OK;
	http_state_t http_state = 0;
	http_info_t *http_info = DA_NULL;
	file_info_t *file_info = DA_NULL;
	req_info_t *req_info = DA_NULL;
	http_msg_response_t *http_msg_response = DA_NULL;
	da_size_t size = 0;
	char *mime_type = DA_NULL;
	char *etag = DA_NULL;
	char *file_name = DA_NULL;

	NULL_CHECK_RET(da_info);
	http_info = da_info->http_info;
	NULL_CHECK_RET(http_info);
	file_info = da_info->file_info;
	NULL_CHECK_RET(file_info);
	req_info = da_info->req_info;
	NULL_CHECK_RET(req_info);
	NULL_CHECK_RET(raw_data);

	DA_MUTEX_LOCK(&(http_info->mutex_state));
	http_state = http_info->state;
	DA_MUTEX_UNLOCK(&(http_info->mutex_state));
	DA_LOGV("http_state[%s]", __get_state_str(http_state));
	http_msg_response = http_info->http_msg_response;
	switch (http_state) {
	case HTTP_STATE_DOWNLOAD_REQUESTED:
	case HTTP_STATE_REQUEST_PAUSE:
	case HTTP_STATE_REQUEST_RESUME:
	case HTTP_STATE_REDIRECTED:
		http_msg_response_get_content_length(http_msg_response, &size);
		http_info->content_len_from_header = size;
		http_msg_response_get_content_type(http_msg_response, &mime_type);
		http_info->content_type_from_header = mime_type;
		if (mime_type)
			file_info->mime_type = strdup(mime_type);
		http_msg_response_get_ETag(http_msg_response, &etag);
		http_info->etag_from_header = etag;
		http_msg_response_get_content_disposition(
				http_msg_response, DA_NULL, &file_name);
		http_info->file_name_from_header = file_name;
		ret = __handle_http_status_code(http_info, file_info, req_info);
		if (ret != DA_RESULT_OK) {
			DA_LOGE("Fail to handle http status code");
			goto ERR;
		}
#ifdef _RAF_SUPPORT
		char *val = NULL;
		http_msg_response_get_RAF_mode(http_msg_response, &val);
		if (!val) {
			DA_LOGE("Fail to raf mode value from response header");
		} else {
			DA_LOGI("[RAF] val[%s:%s]", HTTP_FIELD_RAF_MODE, val);
			if (strncmp(val, "yes", strlen("yes")) == 0) {
				DA_MUTEX_LOCK(&(http_info->mutex_state));
				http_info->state = HTTP_STATE_DOWNLOAD_STARTED;
				DA_MUTEX_UNLOCK(&(http_info->mutex_state));
				ret = __check_before_downloading(da_info, http_info->state);
				if (ret != DA_RESULT_OK) {
					free(val);
					goto ERR;
				}
				http_info->is_raf_mode_confirmed = DA_TRUE;
				ret = PI_http_set_file_name_to_curl(http_info->http_msg, file_info->file_path);
				if (ret != DA_RESULT_OK) {
					DA_LOGE("Fail to set file name to curl");
					free(val);
					goto ERR;
				}
			}
			free(val);
		}
#endif
		break;
	case HTTP_STATE_REQUEST_CANCEL:
		DA_LOGV("Cancel is in progress.. http_state[%s]",
				__get_state_str(http_state));
		break;

	default:
		DA_LOGE("http_state[%s]", __get_state_str(http_state));
		goto ERR;
	}

ERR:
	if (ret != DA_RESULT_OK) {
		DA_LOGE("Request to cancel due to error[%d]", ret);
		PI_http_cancel(http_info);
		http_info->error_code = ret;
		discard_download(file_info);
		DA_MUTEX_LOCK(&(http_info->mutex_state));
		http_info->state = HTTP_STATE_FAILED;
		DA_MUTEX_UNLOCK(&(http_info->mutex_state));
	}
	free(raw_data);
	return ret;
}

da_ret_t __handle_event_http_packet(http_raw_data_t *raw_data, da_info_t *da_info)
{
	da_ret_t ret = DA_RESULT_OK;
	http_state_t http_state = 0;
	http_info_t *http_info = DA_NULL;
	file_info_t *file_info = DA_NULL;
	time_t t;
	struct tm *lc_time;
	DA_LOGV("");

	NULL_CHECK_RET(da_info);
	http_info = da_info->http_info;
	NULL_CHECK_RET(http_info);
	file_info = da_info->file_info;
	NULL_CHECK_RET(file_info);
	NULL_CHECK_RET(raw_data);

	DA_MUTEX_LOCK(&(http_info->mutex_state));
	http_state = http_info->state;
	DA_MUTEX_UNLOCK(&(http_info->mutex_state));

	switch (http_state) {
	case HTTP_STATE_DOWNLOAD_STARTED:
#ifdef _RAF_SUPPORT
		if (http_info->is_raf_mode_confirmed == DA_FALSE) {
#endif
		ret = __check_before_downloading(da_info, http_state);
		if (ret != DA_RESULT_OK)
			goto ERR;
		ret = file_write_ongoing(file_info,
				raw_data->body, raw_data->body_len);
		if (ret != DA_RESULT_OK)
			goto ERR;
#ifdef _RAF_SUPPORT
		} else {
			file_info->bytes_written_to_file =
					raw_data->received_len + file_info->file_size_of_temp_file;
			file_info->is_updated = DA_TRUE;
		}
#endif
		ret = send_client_update_progress_info(da_info);
		DA_MUTEX_LOCK(&(http_info->mutex_state));
		http_state = HTTP_STATE_DOWNLOADING;
		DA_MUTEX_UNLOCK(&(http_info->mutex_state));
		break;
	case HTTP_STATE_RESUMED:
#ifdef _RAF_SUPPORT
		if (http_info->is_raf_mode_confirmed == DA_FALSE) {
#endif
		__check_before_downloading(da_info, http_state);
		ret = file_write_ongoing(file_info,
				raw_data->body, raw_data->body_len);
		if (ret != DA_RESULT_OK)
			goto ERR;
#ifdef _RAF_SUPPORT
		} else {
			file_info->bytes_written_to_file =
					raw_data->received_len + file_info->file_size_of_temp_file;
			file_info->is_updated = DA_TRUE;
		}
#endif
		ret = send_client_update_progress_info(da_info);
		break;
	case HTTP_STATE_REDIRECTED:
		DA_LOGV("http_state[%s]", __get_state_str(http_state));
		break;
	case HTTP_STATE_DOWNLOADING:
#ifdef _RAF_SUPPORT
		if (http_info->is_raf_mode_confirmed == DA_FALSE) {
#endif
		/* Should this function before updating download info
		 * Because it extract mime type at once only if first download updating at client */
		ret = file_write_ongoing(file_info,
				raw_data->body, raw_data->body_len);
		if (ret != DA_RESULT_OK)
			goto ERR;
#ifdef _RAF_SUPPORT
		} else {
			file_info->bytes_written_to_file =
					raw_data->received_len + file_info->file_size_of_temp_file;
			file_info->is_updated = DA_TRUE;
		}
#endif
		// send event every 1 second.
		if ((t = time(DA_NULL)) > 0) {
			if ((lc_time = localtime(&t)) != DA_NULL) {
				if (da_info->update_time != lc_time->tm_sec) {
					da_info->update_time = lc_time->tm_sec;
					ret = send_client_update_progress_info(da_info);
				}
			} else {
				DA_LOGE("Fail to call localtime[%s]",strerror(errno));
				ret = send_client_update_progress_info(da_info);
			}
		} else {
			DA_LOGE("Fail to call time[%s]",strerror(errno));
			ret = send_client_update_progress_info(da_info);
		}
		break;
	case HTTP_STATE_REQUEST_PAUSE:
#ifdef _RAF_SUPPORT
		if (http_info->is_raf_mode_confirmed == DA_FALSE) {
#endif
		DA_LOGV("http_state[%s]", __get_state_str(http_state));
		ret = file_write_ongoing(file_info,
				raw_data->body, raw_data->body_len);
		if (ret != DA_RESULT_OK)
			goto ERR;
#ifdef _RAF_SUPPORT
		} else {
			file_info->bytes_written_to_file =
					raw_data->received_len + file_info->file_size_of_temp_file;
			file_info->is_updated = DA_TRUE;
		}
#endif

		break;
	default:
		DA_LOGE("Do nothing! http_state is in case[%s]",
				__get_state_str(http_state));
		goto ERR;
	}
ERR:
	if (ret != DA_RESULT_OK) {
		DA_LOGE("Request to cancel due to error[%d]", ret);
		PI_http_cancel(http_info);
		http_info->error_code = ret;
		discard_download(da_info->file_info);
		DA_MUTEX_LOCK(&(http_info->mutex_state));
		http_info->state = HTTP_STATE_FAILED;
		DA_MUTEX_UNLOCK(&(http_info->mutex_state));
	}
	if (raw_data->body)
		free(raw_data->body);
	free(raw_data);
	return ret;
}

da_ret_t __check_file_size_with_header_content_size(file_info_t *file_info)
{
	da_ret_t ret = DA_RESULT_OK;
	unsigned long long tmp_file_size = 0;

	DA_LOGV("");

	if (file_info->file_size > 0) {

#ifdef _ENABLE_OMA_DRM
		if (is_content_drm_dm(file_info->mime_type)) {
			/* FIXME Later : How can get the file size of DRM file. */
			return ret;
		}
#endif

		get_file_size(file_info->file_path, &tmp_file_size);

		if (tmp_file_size != file_info->file_size) {
			DA_SECURE_LOGE("Real file size[%llu], MISMATCH CONTENT SIZE",
					tmp_file_size);
			ret = DA_ERR_MISMATCH_CONTENT_SIZE;
		}
	}
	return ret;
}

da_ret_t __handle_event_http_final(http_raw_data_t *raw_data, da_info_t *da_info)
{
	da_ret_t ret = DA_RESULT_OK;
	http_state_t http_state = 0;
	http_info_t *http_info = DA_NULL;
	file_info_t *file_info = DA_NULL;

	DA_LOGV("");

	NULL_CHECK_RET(da_info);
	http_info = da_info->http_info;
	NULL_CHECK_RET(http_info);
	file_info = da_info->file_info;
	NULL_CHECK_RET(file_info);
	NULL_CHECK_RET(raw_data);

	DA_MUTEX_LOCK(&(http_info->mutex_state));
	http_state = http_info->state;
	DA_MUTEX_UNLOCK(&(http_info->mutex_state));
	DA_LOGD("http_state[%s]", __get_state_str(http_state));

	switch (http_state) {
	case HTTP_STATE_REDIRECTED:
		DA_MUTEX_LOCK(&(http_info->mutex_state));
		http_info->state = HTTP_STATE_READY_TO_DOWNLOAD;
		DA_MUTEX_UNLOCK(&(http_info->mutex_state));
		break;
	case HTTP_STATE_DOWNLOAD_REQUESTED:
		DA_LOGV("case HTTP_STATE_DOWNLOAD_REQUESTED");
		DA_MUTEX_LOCK(&(http_info->mutex_state));
		http_info->state = HTTP_STATE_DOWNLOAD_FINISH;
		DA_MUTEX_UNLOCK(&(http_info->mutex_state));
		break;
	case HTTP_STATE_DOWNLOADING:
		DA_LOGD("case HTTP_STATE_DOWNLOADING");
#ifdef _RAF_SUPPORT
		if (http_info->is_raf_mode_confirmed == DA_TRUE) {
			ret = file_write_complete_for_raf(file_info);
		} else {
			ret = file_write_complete(file_info);
		}
#else
		ret = file_write_complete(file_info);
#endif
		if (ret != DA_RESULT_OK) {
			discard_download(file_info);
			goto ERR;
		}
		ret = __check_file_size_with_header_content_size(file_info);
		if(ret != DA_RESULT_OK) {
			discard_download(file_info) ;
			goto ERR;
		}
		DA_MUTEX_LOCK(&(http_info->mutex_state));
		http_info->state = HTTP_STATE_DOWNLOAD_FINISH;
		DA_MUTEX_UNLOCK(&(http_info->mutex_state));
		ret = send_client_update_progress_info(da_info);
		break;
	case HTTP_STATE_REQUEST_PAUSE:
#ifdef _RAF_SUPPORT
		if (http_info->is_raf_mode_confirmed == DA_TRUE) {
			if (file_info->file_handle)
				ret = file_write_complete_for_raf(file_info);
		} else {
			ret = file_write_complete(file_info);
		}
#else
		if (file_info->file_handle) {
			ret = file_write_complete(file_info);
//			send_client_update_progress_info(da_info);
		}
#endif
		DA_MUTEX_LOCK(&(http_info->mutex_state));
		http_info->state = HTTP_STATE_PAUSED;
		DA_MUTEX_UNLOCK(&(http_info->mutex_state));
		DA_LOGV("Server Notification code is set to NULL");
		break;
	case HTTP_STATE_ABORTED:
	case HTTP_STATE_CANCELED:
		discard_download(file_info);
		break;
	case HTTP_STATE_REQUEST_CANCEL:
#ifdef _RAF_SUPPORT
		if (http_info->is_raf_mode_confirmed == DA_TRUE) {
			ret = file_write_complete_for_raf(file_info);
		} else {
			ret = file_write_complete(file_info);
		}
#else
		ret = file_write_complete(file_info);
#endif
		if (ret != DA_RESULT_OK)
			goto ERR;
		discard_download(file_info);
		DA_MUTEX_LOCK(&(http_info->mutex_state));
		http_info->state = HTTP_STATE_CANCELED;
		DA_MUTEX_UNLOCK(&(http_info->mutex_state));
		break;
	case HTTP_STATE_PAUSED:
		DA_LOGD("Remain paused stated");
		break;
	default:
#ifdef _RAF_SUPPORT
		if (http_info->is_raf_mode_confirmed == DA_TRUE) {
			ret = file_write_complete_for_raf(file_info);
		} else {
			ret = file_write_complete(file_info);
		}
#else
		ret = file_write_complete(file_info);
#endif
		if (ret != DA_RESULT_OK)
			goto ERR;
		discard_download(file_info);
		DA_MUTEX_LOCK(&(http_info->mutex_state));
		http_info->state = HTTP_STATE_FAILED;
		DA_MUTEX_UNLOCK(&(http_info->mutex_state));
		break;
	}

ERR:
	/* When file complete is failed */
	if (DA_RESULT_OK != ret) {
		DA_MUTEX_LOCK(&(http_info->mutex_state));
		http_info->state = HTTP_STATE_DOWNLOAD_FINISH;
		DA_MUTEX_UNLOCK(&(http_info->mutex_state));
	}
	if (raw_data->body)
		free(raw_data->body);
	free(raw_data);
	return ret;
}

void __http_update_cb(http_raw_data_t *data, void *user_param)
{
	http_raw_data_t *raw_data = DA_NULL;
	da_info_t *da_info = DA_NULL;
	if (!data || !user_param) {
		DA_LOGE("NULL CHECK!: data, user_param");
		return;
	}
	DA_LOGV("");
	raw_data = data;
	da_info = (da_info_t *)user_param;

	switch(data->type) {
	case HTTP_EVENT_GOT_HEADER:
		__handle_event_http_header(raw_data, da_info);
		break;
	case HTTP_EVENT_GOT_PACKET:
		__handle_event_http_packet(raw_data, da_info);
		break;
	case HTTP_EVENT_FINAL:
		__handle_event_http_final(raw_data, da_info);
		break;
/*
	case HTTP_EVENT_ABORT:
		ret = __handle_event_http_abort(raw_data, da_info);
		break;
*/
	}
}

da_bool_t is_stopped_state(da_info_t *da_info)
{
	http_info_t *http_info = DA_NULL;
	http_state_t http_state;
	NULL_CHECK_RET_OPT(da_info, DA_FALSE);
	http_info = da_info->http_info;
	NULL_CHECK_RET_OPT(http_info, DA_FALSE);
	DA_MUTEX_LOCK(&(http_info->mutex_state));
	http_state = http_info->state;
	DA_MUTEX_UNLOCK(&(http_info->mutex_state));
	switch (http_state) {
	case HTTP_STATE_REQUEST_CANCEL:
	case HTTP_STATE_CANCELED:
	case HTTP_STATE_FAILED:
	case HTTP_STATE_ABORTED:
	//case HTTP_STATE_REQUEST_PAUSE:
	//case HTTP_STATE_REQUEST_RESUME:
	//case HTTP_STATE_WAIT_FOR_NET_ERR:
		return DA_TRUE;
	default:
		return DA_FALSE;
	}
	return DA_FALSE;
}
