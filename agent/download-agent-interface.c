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

#include "download-agent-interface.h"
#include "download-agent-debug.h"
#include "download-agent-utils.h"
#include "download-agent-http-mgr.h"
#include "download-agent-http-misc.h"
#include "download-agent-client-mgr.h"
#include "download-agent-dl-mgr.h"
#include "download-agent-basic.h"
#include "download-agent-file.h"

int da_init(
        da_client_cb_t *da_client_callback)
{
	da_result_t ret = DA_RESULT_OK;

	DA_LOG_FUNC_LOGD(Default);

	if (!da_client_callback) {
		ret = DA_ERR_INVALID_ARGUMENT;
		return ret;
	}

	ret = init_log_mgr();
	if (ret != DA_RESULT_OK)
		goto ERR;

	ret = init_client_app_mgr();
	if (ret != DA_RESULT_OK)
		goto ERR;

	ret = reg_client_app(da_client_callback);
	if (ret != DA_RESULT_OK)
		goto ERR;

	ret = init_http_mgr();
	if (ret != DA_RESULT_OK)
		goto ERR;

	ret = init_download_mgr();
	if (ret != DA_RESULT_OK)
		goto ERR;

ERR:
	if (DA_RESULT_OK != ret)
		da_deinit();

	DA_LOG_CRITICAL(Default, "Return ret = %d", ret);

	return ret;
}

/* TODO:: deinit should clean up all the clients... */
int da_deinit()
{
	da_result_t ret = DA_RESULT_OK;

	DA_LOG_FUNC_LOGV(Default);

	deinit_http_mgr();
	deinit_download_mgr();
	/* Do not clean temporary download path
	 * The client can resume or restart download with temporary file in case of failed download.
	 */
	dereg_client_app();
	DA_LOG(Default, "====== da_deinit EXIT =====");

	return ret;
}

int da_start_download(
        const char *url,
        int *download_id)
{
	da_result_t ret = DA_RESULT_OK;

	DA_LOG_FUNC_LOGD(Default);

	*download_id = DA_INVALID_ID;

	if (DA_FALSE == is_valid_url(url, &ret))
		goto ERR;

	DA_SECURE_LOGI("url = %s", url);

	ret = start_download(url, download_id);
	if (ret != DA_RESULT_OK)
		goto ERR;

ERR:
	DA_LOG_CRITICAL(Default, "Return: Dl req id = %d, ret = %d", *download_id, ret);
	return ret;
}

int da_start_download_with_extension(
	const char		*url,
	extension_data_t *extension_data,
	int	*download_id
)
{
	da_result_t ret = DA_RESULT_OK;
	int req_header_count = 0;
	int i = 0;

	DA_LOG_FUNC_LOGV(Default);

	*download_id = DA_INVALID_ID;

	if (DA_FALSE == is_valid_url(url, &ret))
		goto ERR;

	DA_SECURE_LOGI("url = %s", url);

	if (ret != DA_RESULT_OK)
		goto ERR;
	if (!extension_data) {
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}

	if (extension_data->request_header_count > 0) {
		DA_LOG_VERBOSE(Default, "input request_header_count = [%d]",
			extension_data->request_header_count);
		for (i = 0; i < extension_data->request_header_count; i++) {
			if (extension_data->request_header[i]) {
				req_header_count++;
				DA_SECURE_LOGI("request_header = [%s]",
					extension_data->request_header[i]);
			}
		}
		DA_LOG_VERBOSE(Default, "actual request_header_count = [%d]", req_header_count);
		if (extension_data->request_header_count != req_header_count) {
			DA_LOG_ERR(Default, "Request header count is not matched with number of request header array");
			extension_data->request_header = NULL;
			extension_data->request_header_count = 0;
			ret = DA_ERR_INVALID_ARGUMENT;
			goto ERR;
		}
	}

	if (extension_data->install_path) {
		if (!is_dir_exist(extension_data->install_path))
			return DA_ERR_INVALID_INSTALL_PATH;
		DA_SECURE_LOGI("install_path = [%s]", extension_data->install_path);
	}

	if (extension_data->file_name)
		DA_SECURE_LOGI("file_name = [%s]", extension_data->file_name);
	if (extension_data->temp_file_path)
		DA_SECURE_LOGI("temp_file_path = [%s]", extension_data->temp_file_path);
	if (extension_data->etag)
		DA_SECURE_LOGI("etag = [%s]", extension_data->etag);
	if (extension_data->pkg_name)
		DA_SECURE_LOGI("pkg_name = [%s]", extension_data->pkg_name);
	if (extension_data->user_data)
		DA_LOG_VERBOSE(Default, "user_data = [%p]", extension_data->user_data);

	ret = start_download_with_extension(url, download_id, extension_data);

ERR:
	DA_LOG_CRITICAL(Default, "Return: Dl req id = %d, ret = %d", *download_id, ret);
	return ret;
}

int da_cancel_download(int download_id)
{
	da_result_t ret = DA_RESULT_OK;

	DA_LOG_VERBOSE(Default, "Cancel for dl_id = %d", download_id);

	ret = cancel_download(download_id);

	DA_LOG_CRITICAL(Default, "Return: Cancel id = %d, ret = %d", download_id, ret);
	return ret;
}

int da_suspend_download(int download_id)
{
	da_result_t ret = DA_RESULT_OK;

	DA_LOG_VERBOSE(Default, "Suspend for dl_id = %d", download_id);

	ret = suspend_download(download_id, DA_TRUE);

	DA_LOG_CRITICAL(Default, "Return: Suspend id = %d, ret = %d", download_id, ret);
	return ret;
}

int da_suspend_download_without_update(int download_id)
{
	da_result_t ret = DA_RESULT_OK;

	DA_LOG_VERBOSE(Default, "Suspend for dl_id = %d", download_id);

	ret = suspend_download(download_id, DA_FALSE);

	DA_LOG_CRITICAL(Default, "Return: Suspend id = %d, ret = %d", download_id, ret);
	return ret;
}


int da_resume_download(int download_id)
{
	da_result_t ret = DA_RESULT_OK;

	DA_LOG_VERBOSE(Default, "Resume for dl_id = %d", download_id);

	ret = resume_download(download_id);

	DA_LOG_CRITICAL(Default, "Return: Resume id = %d, ret = %d", download_id, ret);
	return ret;
}

int da_is_valid_download_id(int download_id)
{
	da_bool_t ret = DA_FALSE;
	ret = is_valid_download_id(download_id);
	return ret;
}
