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
 * @file		download-agent-interface.c
 * @brief		Interface  for Download Agent.
 * @author		Keunsoon Lee(keunsoon.lee@samsung.com)
 * @author		Jungki Kwak(jungki.kwak@samsung.com)
 ***/

#include "download-agent-interface.h"
#include "download-agent-debug.h"
#include "download-agent-utils.h"
#include "download-agent-http-mgr.h"
#include "download-agent-http-misc.h"
#include "download-agent-client-mgr.h"
#include "download-agent-dl-mgr.h"
#include "download-agent-basic.h"
#include "download-agent-file.h"
#include "download-agent-installation.h"

int da_init(
        da_client_cb_t *da_client_callback,
        da_download_managing_method download_method)
{
	da_result_t ret = DA_RESULT_OK;

	DA_LOG_FUNC_START(Default);

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

	ret = reg_client_app(da_client_callback, download_method);
	if (ret != DA_RESULT_OK)
		goto ERR;

	ret = init_http_mgr();
	if (ret != DA_RESULT_OK)
		goto ERR;

	ret = init_download_mgr();
	if (ret != DA_RESULT_OK)
		goto ERR;

	ret = create_temp_saved_dir();
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
	char *client_dir_path = DA_NULL;

	DA_LOG_FUNC_START(Default);
	if (DA_FALSE == is_this_client_available()) {
		ret = DA_ERR_INVALID_CLIENT;
		return ret;
	}

	deinit_http_mgr();
	deinit_download_mgr();

	ret = get_client_download_path(&client_dir_path);
	if (ret == DA_RESULT_OK) {
		ret = clean_files_from_dir(client_dir_path);

		if (client_dir_path) {
			free(client_dir_path);
			client_dir_path = DA_NULL;
		}
	}

	dereg_client_app();
	DA_LOG(Default, "====== da_deinit EXIT =====");

	return ret;
}

int da_start_download(
        const char *url,
        da_handle_t *da_dl_req_id)
{
	da_result_t ret = DA_RESULT_OK;

	DA_LOG_FUNC_START(Default);

	DA_LOG(Default, "url = %s", url);

	*da_dl_req_id = DA_INVALID_ID;

	if (DA_FALSE == is_this_client_available()) {
		ret = DA_ERR_INVALID_CLIENT;
		goto ERR;
	}

	if (DA_FALSE == is_valid_url(url, &ret))
		goto ERR;

	ret = start_download(url, da_dl_req_id);
	if (ret != DA_RESULT_OK)
		goto ERR;

ERR:
	DA_LOG_CRITICAL(Default, "Return: Dl req id = %d, ret = %d", *da_dl_req_id, ret);
	return ret;
}

int da_start_download_with_extension(
	const char		*url,
	da_handle_t	*da_dl_req_id,
	...
)
{
	da_result_t ret = DA_RESULT_OK;
	va_list argptr;
	char *property_name = NULL;
	int req_header_count = 0;
	int i = 0;

	extension_data_t extension_data;

	DA_LOG_FUNC_START(Default);

	DA_LOG(Default, "url = %s", url);

	*da_dl_req_id = DA_INVALID_ID;
	extension_data.request_header= NULL;
	extension_data.request_header_count = NULL;
	extension_data.install_path = NULL;
	extension_data.file_name = NULL;
	extension_data.user_data = NULL;

	if (DA_FALSE == is_this_client_available()) {
		ret = DA_ERR_INVALID_CLIENT;
		goto ERR;
	}

	if (DA_FALSE == is_valid_url(url, &ret))
		goto ERR;

	va_start(argptr, da_dl_req_id);
	property_name = va_arg(argptr, char*);

	// FIXME How about changing type for property_name enum?
	if (!property_name) {
		DA_LOG_ERR(Default, "No property input!");
		ret = DA_ERR_INVALID_ARGUMENT;
	} else {
		while (property_name && (ret == DA_RESULT_OK)) {
			DA_LOG_VERBOSE(Default, "property_name = [%s]", property_name);

			if (!strncmp(property_name, DA_FEATURE_USER_DATA, strlen(DA_FEATURE_USER_DATA))) {
				extension_data.user_data = va_arg(argptr, void*);
				if (extension_data.user_data) {
					property_name = va_arg(argptr, char*);
				} else {
					DA_LOG_ERR(Default, "No property value for DA_FEATURE_USER_DATA!");
					ret = DA_ERR_INVALID_ARGUMENT;
				}
			} else if (!strncmp(property_name, DA_FEATURE_INSTALL_PATH, strlen(DA_FEATURE_INSTALL_PATH))) {
				extension_data.install_path = va_arg(argptr, const char*);
				if (extension_data.install_path) {
					property_name = va_arg(argptr, char*);
				} else {
					DA_LOG_ERR(Default, "No property value for DA_FEATURE_INSTALL_PATH!");
					ret = DA_ERR_INVALID_ARGUMENT;
				}
			} else if (!strncmp(property_name, DA_FEATURE_FILE_NAME, strlen(DA_FEATURE_FILE_NAME))) {
				extension_data.file_name = va_arg(argptr, const char*);
				if (extension_data.file_name) {
					property_name = va_arg(argptr, char*);
				} else {
					DA_LOG_ERR(Default, "No property value for DA_FEATURE_FILE_NAME!");
					ret = DA_ERR_INVALID_ARGUMENT;
				}
			} else if (!strncmp(property_name, DA_FEATURE_REQUEST_HEADER, strlen(DA_FEATURE_REQUEST_HEADER))) {
				extension_data.request_header = va_arg(argptr, const char **);
				extension_data.request_header_count = va_arg(argptr, const int *);
				if (extension_data.request_header &&
						extension_data.request_header_count) {
					property_name = va_arg(argptr, char *);
				} else {
					DA_LOG_ERR(Default, "No property value for DA_FEATURE_REQUEST_HEADER!");
					ret = DA_ERR_INVALID_ARGUMENT;
				}
			} else {
				DA_LOG_ERR(Default, "Unknown property name; [%s]", property_name);
				ret = DA_ERR_INVALID_ARGUMENT;
			}
		}
	}

	va_end(argptr);

	if (ret != DA_RESULT_OK)
		goto ERR;

	if (extension_data.request_header_count) {
		DA_LOG_VERBOSE(Default, "input request_header_count = [%d]",
			*(extension_data.request_header_count));
		for (i = 0; i < *(extension_data.request_header_count); i++)
		{
			if (extension_data.request_header[i]) {
				req_header_count++;
				DA_LOG_VERBOSE(Default, "request_header = [%s]",
					extension_data.request_header[i]);
			}
		}
		DA_LOG(Default, "actual request_header_count = [%d]", req_header_count);
		if (*(extension_data.request_header_count) != req_header_count) {
			DA_LOG_ERR(Default, "Request header count is not matched with number of request header array");
			extension_data.request_header = NULL;
			extension_data.request_header_count = NULL;
		}
	}

	if (extension_data.install_path)
		DA_LOG_VERBOSE(Default, "install path = [%s]", extension_data.install_path);

	if (extension_data.file_name)
		DA_LOG_VERBOSE(Default, "file_name = [%s]", extension_data.file_name);

	ret = start_download_with_extension(url, da_dl_req_id, &extension_data);

ERR:
	DA_LOG_CRITICAL(Default, "Return: Dl req id = %d, ret = %d", *da_dl_req_id, ret);
	return ret;
}

int da_cancel_download(da_handle_t da_dl_req_id)
{
	da_result_t ret = DA_RESULT_OK;

	DA_LOG_FUNC_START(Default);

	DA_LOG_VERBOSE(Default, "Cancel for dl_req_id = %d", da_dl_req_id);

	if (DA_FALSE == is_this_client_available()) {
		ret = DA_ERR_INVALID_CLIENT;
		goto ERR;
	}

	ret = cancel_download(da_dl_req_id);

ERR:
	DA_LOG_CRITICAL(Default, "Return: Cancel id = %d, ret = %d", da_dl_req_id, ret);
	return ret;
}

int da_suspend_download(da_handle_t da_dl_req_id)
{
	da_result_t ret = DA_RESULT_OK;

	DA_LOG_FUNC_START(Default);

	DA_LOG_VERBOSE(Default, "Suspend for dl_req_id = %d", da_dl_req_id);

	if (DA_FALSE == is_this_client_available()) {
		ret = DA_ERR_INVALID_CLIENT;
		goto ERR;
	}
	ret = suspend_download(da_dl_req_id);

ERR:
	DA_LOG_CRITICAL(Default, "Return: Suspend id = %d, ret = %d", da_dl_req_id, ret);
	return ret;
}

int da_resume_download(da_handle_t da_dl_req_id)
{
	da_result_t ret = DA_RESULT_OK;

	DA_LOG_FUNC_START(Default);

	DA_LOG_VERBOSE(Default, "Resume for dl_req_id = %d", da_dl_req_id);

	if (DA_FALSE == is_this_client_available()) {
		ret = DA_ERR_INVALID_CLIENT;
		goto ERR;
	}
	ret = resume_download(da_dl_req_id);

ERR:
	DA_LOG_CRITICAL(Default, "Return: Resume id = %d, ret = %d", da_dl_req_id, ret);
	return ret;
}

