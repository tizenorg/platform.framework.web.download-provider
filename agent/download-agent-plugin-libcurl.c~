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
#include <string.h>
#include <stdlib.h>

#include "glib.h"

#include "download-agent-dl-info.h"
#include "download-agent-http-msg-handler.h"
#include "download-agent-plugin-libcurl.h"

da_bool_t using_content_sniffing = DA_FALSE;

int __translate_error_code(int curl_error)
{
	switch (curl_error) {
	case CURLE_OPERATION_TIMEDOUT:
		return DA_ERR_HTTP_TIMEOUT;
	case CURLE_SSL_CONNECT_ERROR:
	case CURLE_SSL_ENGINE_NOTFOUND:
	case CURLE_SSL_ENGINE_SETFAILED:
	case CURLE_SSL_CERTPROBLEM:
	case CURLE_SSL_CIPHER:
	case CURLE_SSL_CACERT:
	case CURLE_SSL_ENGINE_INITFAILED:
	case CURLE_SSL_CACERT_BADFILE:
	case CURLE_SSH:
	case CURLE_SSL_SHUTDOWN_FAILED:
	case CURLE_SSL_CRL_BADFILE:
	case CURLE_SSL_ISSUER_ERROR:
		return DA_ERR_SSL_FAIL;
	case CURLE_TOO_MANY_REDIRECTS:
		return DA_ERR_TOO_MANY_REDIRECTS;
	case CURLE_OUT_OF_MEMORY:
		return DA_ERR_FAIL_TO_MEMALLOC;
	case CURLE_UNSUPPORTED_PROTOCOL:
	case CURLE_URL_MALFORMAT:
	case CURLE_COULDNT_RESOLVE_PROXY:
	case CURLE_COULDNT_RESOLVE_HOST:
	case CURLE_COULDNT_CONNECT:
	case CURLE_REMOTE_ACCESS_DENIED:
	case CURLE_HTTP_POST_ERROR:
	case CURLE_BAD_DOWNLOAD_RESUME:
		return DA_ERR_CONNECTION_FAIL;
	case CURLE_ABORTED_BY_CALLBACK:
		return DA_RESULT_USER_CANCELED;
	default:
		return DA_ERR_NETWORK_FAIL;
	}
}

int my_trace(CURL *handle, curl_infotype type, char *data, size_t size, void *user)
{
	switch(type) {
	case CURLINFO_TEXT:
		if (data)
			DA_SECURE_LOGI("[curl] Info:%s", data);
		break;
	case CURLINFO_HEADER_OUT:
		DA_LOGD("[curl] Send header");
		if (data)
			DA_SECURE_LOGI("[curl] %s", data);
		break;
	case CURLINFO_DATA_OUT:
		DA_LOGD("[curl] Send data");
		if (data)
			DA_SECURE_LOGI("[curl] %s", data);
		break;
	case CURLINFO_SSL_DATA_OUT:
		DA_LOGD("[curl] Send SSL data");
		break;
	case CURLINFO_HEADER_IN:
		DA_LOGD("[curl] Recv header");
		if (data)
			DA_SECURE_LOGI("[curl] %s", data);
		break;
#if 0
	case CURLINFO_DATA_IN:
		DA_LOGD("[curl] Recv data");
		if (data)
			DA_SECURE_LOGI("[curl] %d", strlen(data));
		break;
#endif
	case CURLINFO_SSL_DATA_IN:
		DA_SECURE_LOGI("[curl] Recv SSL data");
		break;
	default:
		return 0;
	}
	return 0;
}

void __parse_raw_header(const char *raw_data, http_info_t *http_info)
{
	char *ptr = DA_NULL;
	char *ptr2 = DA_NULL;
	int len = 0;
	char *field = DA_NULL;
	char *value = DA_NULL;
	http_msg_response_t *http_msg_response = NULL;

	if (!raw_data || !http_info) {
		DA_LOGE("NULL Check!: raw_data or http_info");
		return;
	}

	if (!http_info->http_msg_response) {
		http_info->http_msg_response = (http_msg_response_t *)calloc(1,
				sizeof(http_msg_response_t));
		if (!http_info->http_msg_response) {
			DA_LOGE("Fail to calloc");
			return;
		}
		http_info->http_msg_response->head = DA_NULL;
	}
	http_msg_response = http_info->http_msg_response;

	ptr = strchr(raw_data, ':');
	if (!ptr)
		return;
	len = ptr - (char *)raw_data;
	field = (char *)calloc(len + 1, sizeof(char));
	if (!field) {
		DA_LOGE("Fail to calloc");
		return;
	}
	memcpy(field, raw_data, len);
	field[len] = '\0';
		ptr++;
	while(ptr) {
		if (*ptr == ' ')
			ptr++;
		else
			break;
	}
	ptr2 = strchr(raw_data, '\n');
	if (ptr2) {
		len = ptr2 - ptr -1;
	} else {
		len = strlen(ptr);
	}
	value = (char *)calloc(len + 1, sizeof(char));
	if (!value) {
		DA_LOGE("Fail to calloc");
		free(field);
		return;
	}
	memcpy(value, ptr, len);
	value[len] = '\0';
	http_msg_response_add_field(http_msg_response, field, value);
	free(field);
	free(value);
}

void __store_header(void *msg, da_info_t *da_info, size_t header_size,
		const char *sniffed_type)
{
	http_info_t *http_info = DA_NULL;

	if (!da_info || !msg) {
		DA_LOGE("NULL Check!: da_info or msg");
		return;
	}
	http_info = da_info->http_info;
	if (!http_info) {
		DA_LOGE("NULL Check!: http_info");
		return;
	}

	// FIXME later : check status code and redirection case check.

	if (strncmp(msg, HTTP_FIELD_END_OF_FIELD,
			strlen(HTTP_FIELD_END_OF_FIELD)) == 0) {
		long status = 0;
		CURLcode res;
		CURL *curl;
		http_raw_data_t *raw_data = DA_NULL;
		curl = http_info->http_msg->curl;
		res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
		if (res != CURLE_OK) {
			DA_LOGE("Fail to get response status code");
			return;
		}
		DA_LOGV("status code[%d]", (int)status);
		if (http_info->http_msg_response) {
			http_info->http_msg_response->status_code = (int)status;
		}
		raw_data = (http_raw_data_t *)calloc(1, sizeof(http_raw_data_t));
		if (!raw_data) {
			DA_LOGE("Fail to calloc");
			return;
		}

		raw_data->status_code = (int)status;
		raw_data->type = HTTP_EVENT_GOT_HEADER;

		if (http_info->update_cb) {
			http_info->update_cb(raw_data, da_info);
		} else {
			free(raw_data);
		}
		return;
	}
	DA_LOGI("%s",(char *)msg);
	__parse_raw_header((const char *)msg, http_info);
}

size_t __http_gotheaders_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	da_info_t *da_info = DA_NULL;
	if (!ptr || !userdata) {
		DA_LOGE("Check NULL!: ptr, userdata");
		return 0;
	}
	da_info = (da_info_t *)userdata;
	if (da_info->http_info && da_info->http_info->http_msg
			&& da_info->http_info->http_msg->is_cancel_reqeusted) {
		DA_LOGI("Cancel requested");
		return -1;
	}
	if (!using_content_sniffing)
		__store_header(ptr, da_info, (size * nmemb), DA_NULL);
	else
		DA_LOGV("ignore because content sniffing is turned on");
/*
#ifdef _RAF_SUPPORT
	DA_LOGI("[RAF] __http_gotheaders_cb done");
#endif
*/
	return (size * nmemb);
}

#ifdef _RAF_SUPPORT
da_ret_t PI_http_set_file_name_to_curl(http_msg_t *http_msg, char *file_path)
{
	NULL_CHECK_RET(http_msg);
	NULL_CHECK_RET(file_path);
	DA_LOGI("[RAF]set file_path[%s]", file_path);
	curl_easy_setopt(http_msg->curl, CURLOPT_BOOSTER_RAF_FILE, file_path);
	return DA_RESULT_OK;
}
#endif

size_t __http_gotchunk_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	http_info_t *http_info = DA_NULL;
	da_info_t *da_info = DA_NULL;
	http_raw_data_t *raw_data = DA_NULL;
	if (!ptr || !userdata) {
		DA_LOGE("Check NULL!: ptr, stream");
		return 0;
	}
	da_info = (da_info_t *)userdata;
	NULL_CHECK_RET_OPT(da_info, 0);
	http_info = da_info->http_info;
	NULL_CHECK_RET_OPT(http_info, 0);
	NULL_CHECK_RET_OPT(http_info->http_msg, 0);
	if (da_info->http_info->http_msg->is_cancel_reqeusted) {
		DA_LOGI("Cancel requested");
		return -1;
	}
	//DA_LOGV("size=%ld, nmemb=%ld, datalen=%ld", size, nmemb, strlen((const char *)ptr));
#ifdef _RAF_SUPPORT
	//DA_LOGI("size=%ld, nmemb=%ld, datalen=%ld", size, nmemb, strlen((const char *)ptr));
	if (http_info->is_raf_mode_confirmed) {
		DA_LOGI("[RAF] return chunked callback");
		return (size * nmemb);
	}
#endif

	if (ptr && size * nmemb > 0) {
		if (http_info->update_cb) {
			raw_data = (http_raw_data_t *)calloc(1, sizeof(http_raw_data_t));
			if (!raw_data) {
				DA_LOGE("Fail to calloc");
				return 0;
			}
			raw_data->body = (char *)calloc(size, nmemb);
			if (!(raw_data->body)) {
				DA_LOGE("Fail to calloc");
				free(raw_data);
				return 0;
			}
			memcpy(raw_data->body, ptr, size * nmemb);
			raw_data->body_len = size*nmemb;
			raw_data->type = HTTP_EVENT_GOT_PACKET;
			http_info->update_cb(raw_data, da_info);
		}
	}
	return (size * nmemb);
}

long __http_finished_cb(void *ptr)
{
	if (!ptr) {
		DA_LOGE("Check NULL!: ptr");
		return CURL_CHUNK_END_FUNC_FAIL;
	}
	DA_LOGI("");
	return CURL_CHUNK_END_FUNC_OK;
}


da_ret_t __set_proxy_on_soup_session(char *proxy_addr, CURL *curl)
{
	da_ret_t ret = DA_RESULT_OK;

	if (proxy_addr && strlen(proxy_addr) > 0) {
		DA_SECURE_LOGI("received proxy[%s]", proxy_addr);
		if (!strstr(proxy_addr, "0.0.0.0")) {
			if (strstr((const char *)proxy_addr, "http") == DA_NULL) {
				char *tmp_str = DA_NULL;
				int needed_len = 0;

				needed_len = strlen(proxy_addr) + strlen(
						SCHEME_HTTP) + 1;
				tmp_str = (char *) calloc(1, needed_len);
				if (!tmp_str) {
					DA_LOGE("DA_ERR_FAIL_TO_MEMALLOC");
					ret = DA_ERR_FAIL_TO_MEMALLOC;
					goto ERR;
				}
				snprintf(tmp_str, needed_len, "%s%s",
						SCHEME_HTTP, proxy_addr);

				curl_easy_setopt(curl, CURLOPT_PROXY, proxy_addr);

				free(tmp_str);
			} else {
				DA_LOGV("There is \"http\" on uri, so, push this address to soup directly.");
				curl_easy_setopt(curl, CURLOPT_PROXY, proxy_addr);
			}
		}
	}
ERR:
	return ret;
}

struct curl_slist *__fill_soup_msg_header(CURL *curl, http_info_t *info)
{
	http_msg_request_t *input_http_msg_request;
	struct curl_slist *headers = DA_NULL;

	if (!curl) {
		DA_LOGE("NULL Check!: curl");
		return DA_NULL;
	}
	input_http_msg_request = info->http_msg_request;

	if (input_http_msg_request) {
		char *field = DA_NULL;
		char *value = DA_NULL;
		char *buff = DA_NULL;
		int len = 0;
		http_header_t *cur = DA_NULL;
		cur = input_http_msg_request->head;
		while (cur) {
			field = cur->field;
			value = cur->value;
			if (field && value) {
				len = strlen(field) + strlen(value) + 1;
				buff = (char *)calloc(len + 1, sizeof(char));
				if (!buff) {
					DA_LOGE("Fail to memalloc");
					break;
				}
//				DA_SECURE_LOGI("[%s] %s", field, value);
				snprintf(buff, len + 1, "%s:%s", field, value);
				headers = curl_slist_append(headers, (const char *)buff);
				free(buff);
				buff = DA_NULL;
			}
			cur = cur->next;
		}
	} else {
		DA_LOGE("NULL Check!: input_http_msg_request");
		return DA_NULL;
	}
	if (input_http_msg_request->http_body) {
		char buff[256] = {0,};
		int body_len = strlen(input_http_msg_request->http_body);
		snprintf(buff, sizeof(buff), "%s:%d", HTTP_FIELD_CONTENT_LENGTH,
				body_len);
		headers = curl_slist_append(headers, buff);
		memset(buff, 0x00, 256);
		snprintf(buff, sizeof(buff), "%s:text/plain", HTTP_FIELD_CONTENT_TYPE);
		headers = curl_slist_append(headers, buff);
		headers = curl_slist_append(headers, input_http_msg_request->http_body);
	}
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	return headers;
}

#ifdef _RAF_SUPPORT
int __http_progress_cb(void *clientp, double dltotal, double dlnow,
		double ultotal, double ulnow)
{
	da_info_t *da_info = DA_NULL;
	http_info_t *http_info = DA_NULL;
	http_raw_data_t *raw_data = DA_NULL;
/*
	if (dlnow > 0 || ulnow > 0)
		DA_LOGI("[RAF]dlnow/ulnow[%llu/%llu][%llu,%llu]", (da_size_t)dlnow, (da_size_t)ulnow, (da_size_t)dltotal, (da_size_t)ultotal);
*/

/*
	if (dlnow == 0) {
		DA_LOGI("[RAF]dlnow is zero. Why is this callback called although there is zero size?");
	}
*/
	NULL_CHECK_RET_OPT(clientp, -1);
	da_info = (da_info_t *)clientp;
	http_info = da_info->http_info;
	NULL_CHECK_RET_OPT(http_info, -1);
	NULL_CHECK_RET_OPT(http_info->http_msg, -1);

	if (http_info->http_msg->is_cancel_reqeusted) {
		DA_LOGI("Cancel requested");
		return -1;
	}

	if (dlnow > 0) {
		if (http_info->update_cb) {
			raw_data = (http_raw_data_t *)calloc(1, sizeof(http_raw_data_t));
			if (!raw_data) {
				DA_LOGE("Fail to calloc");
				return 0;
			}
			raw_data->received_len = (da_size_t)dlnow;
			raw_data->type = HTTP_EVENT_GOT_PACKET;
			http_info->update_cb(raw_data, da_info);
		}
	}
	return CURLE_OK;
}
#endif

da_ret_t PI_http_start(da_info_t *da_info)
{
	da_ret_t ret = DA_RESULT_OK;
	http_method_t http_method;
	CURL *curl = DA_NULL;
	CURLcode res;
	http_msg_t *http_msg = DA_NULL;
	char *url = DA_NULL;
	http_info_t *http_info = DA_NULL;
	long http_status = 0;
	struct curl_httppost* post = NULL;
	struct curl_slist *headers = DA_NULL;
	char err_buffer[CURL_ERROR_SIZE] = {0,};

	DA_LOGV("");
#ifdef _RAF_SUPPORT
	// test code
	get_smart_bonding_vconf();
#endif
	NULL_CHECK_GOTO(da_info);
	NULL_CHECK_GOTO(da_info->req_info);
	url = da_info->req_info->url;
	NULL_CHECK_GOTO(url);
	http_info = da_info->http_info;
	NULL_CHECK_GOTO(http_info);

	http_method = http_info->http_method;
	ret = init_http_msg_t(&http_msg);
	if (ret != DA_RESULT_OK)
		goto ERR;
	http_info->http_msg = http_msg;

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();

	if (!curl) {
		DA_LOGE("Fail to create curl");
		return DA_ERR_FAIL_TO_MEMALLOC;
	}
	DA_LOGI("curl[%p]", curl);

	curl_easy_setopt(curl, CURLOPT_MAXCONNECTS, MAX_SESSION_COUNT);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, MAX_TIMEOUT);

	__set_proxy_on_soup_session(http_info->proxy_addr, curl);

	curl_easy_setopt(curl, CURLOPT_URL, url);
	switch (http_method) {
	case HTTP_METHOD_GET:
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
		break;
	case HTTP_METHOD_POST:
		// FIXME later : If the post method is supprot, the post data should be set with curl_fromadd
		curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
		DA_LOGI("Need more information for post filed");
		break;
	case HTTP_METHOD_HEAD:
		DA_LOGI("Donnot implement yet");
		break;
	default:
		DA_LOGE("Cannot enter here");
		break;
	}

	if (using_content_sniffing) {
		/* FIXME later*/
	} else {
		/* FIXME later*/
	}
	headers = __fill_soup_msg_header(curl, http_info);

	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, __http_gotheaders_cb); // can replace to started_cb
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, da_info); // param .. same with CURLOPT_WRITEHEADER
	curl_easy_setopt(curl, CURLOPT_HEADER, 0L); // does not include header to body
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, __http_gotchunk_cb); // can replace to progress_
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, da_info); // param .. same with CURLOPT_WRITEHEADERcb
	curl_easy_setopt(curl, CURLOPT_CHUNK_END_FUNCTION, __http_finished_cb);
	curl_easy_setopt(curl, CURLOPT_CHUNK_DATA, da_info);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
//	curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, my_trace);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err_buffer);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
#ifdef _RAF_SUPPORT
	curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, __http_progress_cb);
	curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, da_info);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
#endif

	if (da_info->req_info->network_bonding) {
#ifdef _DOWNLOAD_BOOSTER_SUPPORT
		DA_LOGI("network bonding enable");
		curl_easy_setopt(curl, CURLOPT_MULTIRAT_NEEDED, 1L);
#endif
#ifdef _RAF_SUPPORT
		curl_easy_setopt(curl, CURLOPT_BOOSTER_RAF_MODE, 1L);
#endif
	}
	http_msg->curl = curl;
	res = curl_easy_perform(curl);
	DA_LOGD("perform done! res[%d]",res);
	if (res != CURLE_OK) {
			//DA_LOGE("Fail to send data :%d[%s]", res, curl_easy_strerror(res));
			DA_LOGE("Fail to perform :%d[%s]", res, curl_multi_strerror(res));
			if (strlen(err_buffer) > 1)
				DA_LOGE("Fail to error buffer[%s]", err_buffer);
	}
	if (res != CURLE_OK) {
		//DA_LOGE("Fail to send data :%d[%s]", res, curl_easy_strerror(res));
		DA_LOGE("Fail to send data :%d[%s]", res, curl_easy_strerror(res));
		if (strlen(err_buffer) > 1)
			DA_LOGE("Fail to error buffer[%s]", err_buffer);
	} else {
		res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);
		if (res != CURLE_OK) {
			//DA_LOGE("Fail to get response code:%d[%s]", res, curl_easy_strerror(res));
			DA_LOGE("Fail to get response code:%d[%s]", res, curl_easy_strerror(res));
			ret = DA_ERR_FAIL_TO_MEMALLOC;;
			goto ERR;
		} else {
			DA_LOGD("Response Http Status code[%d]", (int)http_status);
		}
	}
	if (http_info->update_cb) {
		http_raw_data_t *raw_data = DA_NULL;
		raw_data = (http_raw_data_t *)calloc(1, sizeof(http_raw_data_t));
		if (!raw_data) {
			DA_LOGE("Fail to calloc");
			ret = DA_ERR_FAIL_TO_MEMALLOC;
			goto ERR;
		}
		if (http_msg->is_cancel_reqeusted ||
				res == CURLE_ABORTED_BY_CALLBACK) {
			DA_LOGI("canceled exit. Err[%d]", http_info->error_code);
			if (http_info->error_code < 0)
				ret = http_info->error_code;
			else
				ret = DA_RESULT_USER_CANCELED;
		} else  if ((http_status > 0 && http_status < 100)) {
			raw_data->error = __translate_error_code(res);
			ret = DA_ERR_NETWORK_FAIL;
		} else if (res != CURLE_OK) {
			raw_data->error = __translate_error_code(res);
			ret = DA_ERR_NETWORK_FAIL;
		} else {
			raw_data->status_code = (int)http_status;
		}
		raw_data->type = HTTP_EVENT_FINAL;
		http_info->update_cb(raw_data, da_info);
	}
	if (DA_NULL != headers)
		curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	http_msg->curl = DA_NULL;
	DA_MUTEX_INIT(&(http_msg->mutex), DA_NULL);
ERR:
	DA_LOGD("Done");
	return ret;

}

da_ret_t PI_http_disconnect(http_info_t *info)
{
	da_ret_t ret = DA_RESULT_OK;
	http_msg_t *http_msg = DA_NULL;

	DA_LOGD("");
	NULL_CHECK_RET(info);
	http_msg = info->http_msg;
	NULL_CHECK_RET(http_msg);
	DA_LOGV("session [%p]", http_msg->curl);
	DA_MUTEX_LOCK(&(http_msg->mutex));
	if (http_msg->is_paused)
		PI_http_unpause(info);
	 if (http_msg->curl)
		 curl_easy_cleanup(http_msg->curl);

	http_msg->curl = DA_NULL;
	http_msg->is_paused = DA_FALSE;
	http_msg->is_cancel_reqeusted = DA_FALSE;
	DA_MUTEX_UNLOCK(&(http_msg->mutex));
	DA_MUTEX_DESTROY(&(http_msg->mutex));
	destroy_http_msg_t(http_msg);
	info->http_msg = DA_NULL;
	return ret;
}

da_ret_t PI_http_cancel(http_info_t *info)
{
	da_ret_t ret = DA_RESULT_OK;
	http_msg_t *http_msg = DA_NULL;

	DA_LOGV("");

	NULL_CHECK_RET(info);
	http_msg = info->http_msg;
	NULL_CHECK_RET(http_msg);
	NULL_CHECK_RET(http_msg->curl);
	DA_MUTEX_LOCK(&(http_msg->mutex));
	DA_LOGI("curl[%p]", http_msg->curl);
	http_msg->is_cancel_reqeusted = DA_TRUE;
	DA_MUTEX_UNLOCK(&(http_msg->mutex));
	DA_LOGD("Done - soup cancel");
	return ret;
}

da_ret_t PI_http_pause(http_info_t *info)
{
	da_ret_t ret = DA_RESULT_OK;
	http_msg_t *http_msg = DA_NULL;
	CURLcode res = CURLE_OK;
	DA_LOGV("");

	NULL_CHECK_RET(info);
	http_msg = info->http_msg;
	NULL_CHECK_RET(http_msg);
	DA_LOGD("curl [%p]", http_msg->curl);
	NULL_CHECK_RET(http_msg->curl);
	DA_MUTEX_LOCK(&(http_msg->mutex));
	DA_LOGE("curl_easy_pause call");
	curl_easy_pause(http_msg->curl, CURLPAUSE_ALL);
	DA_LOGE("curl_easy_pause:%d", res);
	if (res == CURLE_OK) {
		http_msg->is_paused = DA_TRUE;
	} else {
		ret = DA_ERR_CANNOT_SUSPEND;
	}
	DA_MUTEX_UNLOCK(&(http_msg->mutex));
	return ret;
}

da_ret_t PI_http_unpause(http_info_t *info)
{
	da_ret_t ret = DA_RESULT_OK;
	http_msg_t *http_msg = DA_NULL;
	CURLcode res = CURLE_OK;
	DA_LOGV("");

	NULL_CHECK_RET(info);
	http_msg = info->http_msg;
	DA_LOGV("curl [%p]", http_msg->curl);
	NULL_CHECK_RET(http_msg->curl);
	DA_MUTEX_LOCK(&(http_msg->mutex));
	res = curl_easy_pause(http_msg->curl, CURLPAUSE_CONT);
	if (res == CURLE_OK)
		http_msg->is_paused = DA_FALSE;
	else
		ret = DA_ERR_CANNOT_RESUME;
	DA_MUTEX_UNLOCK(&(http_msg->mutex));
	return ret;
}
