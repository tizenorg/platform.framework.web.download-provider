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

#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <openssl/crypto.h>

#include "download-agent-dl-info.h"
#include "download-agent-http-mgr.h"
#include "download-agent-http-msg-handler.h"

static pthread_mutex_t mutex_da_info_list = PTHREAD_MUTEX_INITIALIZER;


/* locking mechnism for safe use of openssl context */
static void openssl_lock_callback(int mode, int type, char *file, int line)
{
	DA_LOGV("type [%d], mode [%d]", type, mode);
	(void)file;
	(void)line;

	if (mode & CRYPTO_LOCK)
		pthread_mutex_lock(&(g_openssl_locks_list[type]));
	else
		pthread_mutex_unlock(&(g_openssl_locks_list[type]));
}

static unsigned long thread_id(void)
{
	unsigned long ret = (unsigned long)pthread_self();
	return ret;
}

da_ret_t init_openssl_locks(void)
{
	DA_LOGD("");
	int index;
	int crypto_num_locks = CRYPTO_num_locks();
	DA_LOGD("crypto_num_locks [%d]", crypto_num_locks);
	g_openssl_locks_list = (pthread_mutex_t *)OPENSSL_malloc(crypto_num_locks * sizeof(pthread_mutex_t));
	if (g_openssl_locks_list == DA_NULL) {
		DA_LOGE("Failed to OPENSSL_malloc");
		return DA_ERR_FAIL_TO_MEMALLOC;
	}
	for (index = 0; index < crypto_num_locks; index++) {
		pthread_mutex_init(&(g_openssl_locks_list[index]), NULL);
	}
	CRYPTO_set_id_callback((unsigned long (*)())thread_id);
	CRYPTO_set_locking_callback((void (*)())openssl_lock_callback);

	return DA_RESULT_OK;
}
da_ret_t deinit_openssl_locks(void)
{
	DA_LOGD("");
	int index;
	int crypto_num_locks = CRYPTO_num_locks();
	for (index = 0; index < crypto_num_locks; index++) {
		pthread_mutex_destroy(&(g_openssl_locks_list[index]));
	}
	CRYPTO_set_id_callback(NULL);
	CRYPTO_set_locking_callback(NULL);
	OPENSSL_free(g_openssl_locks_list);
	g_openssl_locks_list = NULL;

	return DA_RESULT_OK;
}

static void __init_da_info(int id)
{
	da_info_t *da_info = DA_NULL;
	file_info_t *file_info = DA_NULL;
	http_info_t *http_info = DA_NULL;
	req_info_t *req_info = DA_NULL;

	da_info = (da_info_t *)calloc(1, sizeof(da_info_t));
	if (!da_info) {
		DA_LOGE("Fail to calloc. id[%d]",id);
		da_info_list[id] = DA_NULL;
		return;
	}
	file_info = (file_info_t *)calloc(1, sizeof(file_info_t));
	if (!file_info) {
		DA_LOGE("Fail to calloc. id[%d]",id);
		free(da_info);
		da_info_list[id] = DA_NULL;
		return;
	}
	http_info = (http_info_t *)calloc(1, sizeof(http_info_t));
	if (!http_info) {
		DA_LOGE("Fail to calloc. id[%d]",id);
		free(da_info);
		free(file_info);
		da_info_list[id] = DA_NULL;
		return;
	}
	req_info = (req_info_t *)calloc(1, sizeof(req_info_t));
	if (!req_info) {
		DA_LOGE("Fail to calloc. id[%d]",id);
		free(da_info);
		free(file_info);
		free(http_info);
		da_info_list[id] = DA_NULL;
		return;
	}

	da_info->da_id = DA_INVALID_ID;
	da_info->tid = DA_INVALID_ID;
	memset(&(da_info->cb_info), 0x00, sizeof(da_cb_t));
	da_info->is_cb_update = DA_FALSE;
	da_info->http_info = http_info;
	da_info->file_info = file_info;
	da_info->req_info = req_info;
	da_info->update_time = 0;
	da_info_list[id] = da_info;
}

da_ret_t init_http_msg_t(http_msg_t **http_msg)
{
	da_ret_t ret = DA_RESULT_OK;
	http_msg_t *temp = DA_NULL;
	temp = (http_msg_t *)calloc(1, sizeof(http_msg_t));
	if (!temp) {
		DA_LOGE("Fail to calloc. id");
		return DA_ERR_FAIL_TO_MEMALLOC;
	}
	*http_msg = temp;
	return ret;
}

void destroy_http_msg_t(http_msg_t *http_msg)
{
	if (http_msg)
		free(http_msg);
	http_msg = DA_NULL;
	return;
}

da_ret_t get_available_da_id(int *available_id)
{
	da_ret_t ret = DA_ERR_ALREADY_MAX_DOWNLOAD;
	int i = 0;

	DA_MUTEX_LOCK(&mutex_da_info_list);
	for (i = 0; i < DA_MAX_ID; i++) {
		if (da_info_list[i] == DA_NULL) {
			*available_id = i;
			DA_LOGV("available download id[%d]", *available_id);
			__init_da_info(i);
			ret = DA_RESULT_OK;
			break;
		}
	}
	DA_MUTEX_UNLOCK(&mutex_da_info_list);

	return ret;
}

da_ret_t get_da_info_with_da_id(int id, da_info_t **out_info)
{
	da_ret_t ret = DA_ERR_INVALID_ARGUMENT;
	int i = 0;

	DA_MUTEX_LOCK(&mutex_da_info_list);
	for (i = 0; i < DA_MAX_ID; i++) {
		if (DA_NULL != da_info_list[i] && da_info_list[i]->da_id == id) {
			*out_info = da_info_list[i];
			ret = DA_RESULT_OK;
			break;
		}
	}
	DA_MUTEX_UNLOCK(&mutex_da_info_list);

	return ret;
}

da_ret_t __is_supporting_protocol(const char *url)
{
	da_ret_t ret = DA_RESULT_OK;
	int wanted_str_len = 0;
	char *protocol = NULL;
	char *wanted_str_start = NULL;
	char *wanted_str_end = NULL;

	if (DA_NULL == url || strlen(url) < 1)
		return DA_ERR_INVALID_URL;

	wanted_str_start = (char*)url;
	wanted_str_end = strstr(url, "://");
	if (!wanted_str_end) {
		DA_LOGE("No protocol on this url");
		return DA_ERR_INVALID_URL;
	}

	wanted_str_len = wanted_str_end - wanted_str_start;
	protocol = (char*)calloc(1, wanted_str_len + 1);
	if (!protocol) {
		DA_LOGE("DA_ERR_FAIL_TO_MEMALLOC");
		return DA_ERR_FAIL_TO_MEMALLOC;
	}
	strncpy(protocol, wanted_str_start, wanted_str_len);

	if (strlen(protocol) < 1)
		ret = DA_ERR_UNSUPPORTED_PROTOCAL;
	else if (strcasecmp(protocol, "http") != 0 &&
			strcasecmp(protocol, "https") != 0)
		ret = DA_ERR_UNSUPPORTED_PROTOCAL;

	free(protocol);
	return ret;
}

da_ret_t copy_user_input_data(da_info_t *da_info, const char *url,
		req_data_t *ext_data, da_cb_t *da_cb_data)
{
	da_ret_t ret = DA_RESULT_OK;

	if (!url || !da_info) {
		DA_LOGE("Invalid Param");
		return DA_ERR_INVALID_ARGUMENT;
	}

	ret = __is_supporting_protocol(url);
	if (ret != DA_RESULT_OK) {
		DA_SECURE_LOGE("url[%s]", url);
		return ret;
	}

	if (ext_data) {
		req_info_t *req_info = da_info->req_info;

		if (ext_data->request_header_count > 0) {
			int i = 0;
			int count = ext_data->request_header_count;
			req_info->req_header = (char **)calloc(count, sizeof(char *));
			if(DA_NULL == req_info->req_header) {
				DA_LOGE("Fail to calloc");
				free(req_info);
				da_info->req_info = DA_NULL;
				return DA_ERR_FAIL_TO_MEMALLOC;
			}
			for (i = 0; i < count; i++) {
				if (ext_data->request_header[i])
					req_info->req_header[i] =
							strdup(ext_data->request_header[i]);
			}
			req_info->req_header_count = count;
		}

		if (url)
			req_info->url = strdup(url);
		if (ext_data->install_path)
			req_info->install_path = strdup(ext_data->install_path);
		if (ext_data->file_name)
			req_info->file_name = strdup(ext_data->file_name);
		if (ext_data->etag)
			req_info->etag = strdup(ext_data->etag);
		if (ext_data->temp_file_path)
			req_info->temp_file_path = strdup(ext_data->temp_file_path);
		if (ext_data->pkg_name)
			req_info->pkg_name = strdup(ext_data->pkg_name);
		req_info->network_bonding = ext_data->network_bonding;
		if (ext_data->user_req_data)
			req_info->user_req_data = ext_data->user_req_data;
		if (ext_data->user_client_data)
			req_info->user_client_data = ext_data->user_client_data;
		da_info->req_info = req_info;
	}
	if (da_cb_data) {
		da_info->is_cb_update = DA_TRUE;
		memcpy(&(da_info->cb_info), da_cb_data, sizeof(da_cb_t));
	}
	return ret;
}

static void __destroy_http_msg(http_msg_t *msg)
{
	msg->curl = DA_NULL;
	free(msg);
}

static void __destroy_http_msg_request(http_msg_request_t *msg)
{
	if (msg) {
		http_msg_request_destroy(&msg);
		free(msg);
	}
}

static void __destroy_http_msg_response(http_msg_response_t *msg)
{
	if (msg) {
		http_msg_response_destroy(&msg);
		free(msg);
	}
}

static void __destroy_req_info(req_info_t *req_info)
{
	if (req_info) {
		free(req_info->url);
		if (req_info->req_header && req_info->req_header_count > 0) {
			int i = 0;
			int count = req_info->req_header_count;
			for (i = 0; i < count; i++)	{
				free(req_info->req_header[i]);
				req_info->req_header[i] = DA_NULL;
			}
			free(req_info->req_header);
			req_info->req_header = DA_NULL;
			req_info->req_header_count = 0;
		}
		free(req_info->install_path);
		free(req_info->file_name);
		free(req_info->etag);
		free(req_info->temp_file_path);
		free(req_info->pkg_name);
		req_info->user_req_data = DA_NULL;
		req_info->user_client_data = DA_NULL;
		free(req_info);
	}
}

void destroy_http_info(http_info_t *http_info)
{
	if (http_info) {
		DA_LOGI("[TEST] location_url[%p]",http_info->location_url);
		free(http_info->location_url);
		free(http_info->proxy_addr);
		free(http_info->content_type_from_header);
		free(http_info->etag_from_header);
		free(http_info->file_name_from_header);
		if (http_info->http_msg_request) {
			__destroy_http_msg_request(http_info->http_msg_request);
			http_info->http_msg_request = DA_NULL;
		}
		if (http_info->http_msg_response) {
			__destroy_http_msg_response(http_info->http_msg_response);
			http_info->http_msg_response = DA_NULL;
		}
		if (http_info->http_msg) {
			__destroy_http_msg(http_info->http_msg);
			http_info->http_msg = DA_NULL;
		}
		DA_MUTEX_DESTROY(&(http_info->mutex_state));
		DA_MUTEX_DESTROY(&(http_info->mutex_http));
		DA_COND_DESTROY(&(http_info->cond_http));
		http_info->state = HTTP_STATE_READY_TO_DOWNLOAD;
		http_info->http_method = HTTP_METHOD_GET;
		http_info->content_len_from_header = 0;
		http_info->total_size = 0;
		http_info->error_code = 0;
		free(http_info);
	}
}

void destroy_file_info(file_info_t *file_info)
{
	if (file_info) {
		file_info->file_handle = DA_NULL;
		free(file_info->pure_file_name);
		free(file_info->extension);
		free(file_info->file_path);
		free(file_info->mime_type);
		free(file_info->buffer);
		file_info->buffer_len = 0;
		file_info->file_size = 0;
#ifdef _RAF_SUPPORT
		file_info->file_size_of_temp_file = 0;
#endif
		file_info->bytes_written_to_file = 0;
		file_info->is_updated = DA_FALSE;
		free(file_info);
	}
}

// For pause and resume case
void reset_http_info_for_resume(http_info_t *http_info)
{
	if (http_info) {
		DA_LOGI("[TEST] location_url[%p]",http_info->location_url);
		free(http_info->location_url);
		http_info->location_url = DA_NULL;
		free(http_info->proxy_addr);
		http_info->proxy_addr = DA_NULL;
		free(http_info->content_type_from_header);
		http_info->content_type_from_header = DA_NULL;
		if (http_info->http_msg_response) {
			__destroy_http_msg_response(http_info->http_msg_response);
			http_info->http_msg_response = DA_NULL;
		}
		if (http_info->http_msg) {
			__destroy_http_msg(http_info->http_msg);
			http_info->http_msg = DA_NULL;
		}
		http_info->http_method = HTTP_METHOD_GET;
		http_info->content_len_from_header = 0;
		http_info->total_size = 0;
	}
}

void reset_http_info(http_info_t *http_info)
{
	if (http_info) {
		DA_LOGI("[TEST] location_url[%p]",http_info->location_url);
		free(http_info->location_url);
		http_info->location_url = DA_NULL;
		free(http_info->proxy_addr);
		http_info->proxy_addr = DA_NULL;
		free(http_info->content_type_from_header);
		http_info->content_type_from_header = DA_NULL;
		if (http_info->http_msg_request) {
			__destroy_http_msg_request(http_info->http_msg_request);
			http_info->http_msg_request = DA_NULL;
		}
		if (http_info->http_msg_response) {
			__destroy_http_msg_response(http_info->http_msg_response);
			http_info->http_msg_response = DA_NULL;
		}
		if (http_info->http_msg) {
			__destroy_http_msg(http_info->http_msg);
			http_info->http_msg = DA_NULL;
		}
		http_info->http_method = HTTP_METHOD_GET;
		http_info->content_len_from_header = 0;
		http_info->total_size = 0;
	}
}

da_bool_t is_valid_download_id(int download_id)
{
	da_ret_t ret = DA_RESULT_OK;

	DA_LOGV("");
	DA_MUTEX_LOCK(&mutex_da_info_list);
	if (DA_NULL == da_info_list[download_id]) {
		DA_MUTEX_UNLOCK(&mutex_da_info_list);
		return DA_FALSE;
	}
	if (is_stopped_state(da_info_list[download_id])) {
		DA_MUTEX_UNLOCK(&mutex_da_info_list);
		return DA_TRUE;
	}
	if (da_info_list[download_id]->da_id != DA_INVALID_ID) {
		DA_MUTEX_UNLOCK(&mutex_da_info_list);
		return DA_TRUE;
	} else {
		DA_MUTEX_UNLOCK(&mutex_da_info_list);
		return DA_FALSE;
	}
	DA_MUTEX_UNLOCK(&mutex_da_info_list);
	return ret;
}

void destroy_da_info_list()
{
	int i = 0;
	DA_MUTEX_LOCK(&mutex_da_info_list);
	for (i = 0; i < DA_MAX_ID; i++) {
		if(DA_NULL != da_info_list[i]) {
			if (da_info_list[i]->req_info) {
				__destroy_req_info(da_info_list[i]->req_info);
				da_info_list[i]->req_info = DA_NULL;
			}
			if (da_info_list[i]->http_info) {
				destroy_http_info(da_info_list[i]->http_info);
				da_info_list[i]->http_info = DA_NULL;
			}
			if (da_info_list[i]->file_info) {
				destroy_file_info(da_info_list[i]->file_info);
				da_info_list[i]->file_info = DA_NULL;
			}
			free(da_info_list[i]);
			da_info_list[i] = DA_NULL;
		}
	}
	DA_MUTEX_UNLOCK(&mutex_da_info_list);
}

void destroy_da_info(int id)
{
	da_info_t *da_info = DA_NULL;
	DA_MUTEX_LOCK(&mutex_da_info_list);
	da_info = da_info_list[id];
	if (da_info) {
		if (da_info->req_info) {
			__destroy_req_info(da_info->req_info);
			da_info->req_info = DA_NULL;
		}
		if (da_info->http_info) {
			destroy_http_info(da_info->http_info);
			da_info->http_info = DA_NULL;
		}
		if (da_info->file_info) {
			destroy_file_info(da_info->file_info);
			da_info->file_info = DA_NULL;
		}
		da_info->da_id = DA_INVALID_ID;
		da_info->tid = DA_INVALID_ID;
		memset(&(da_info->cb_info), 0x00, sizeof(da_cb_t));
		free(da_info);
		da_info_list[id] = DA_NULL;
	}
	DA_MUTEX_UNLOCK(&mutex_da_info_list);
}
