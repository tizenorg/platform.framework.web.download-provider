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
#include "download-agent-dl-mgr.h"

int da_init()
{
	DA_LOGV("");
	da_ret_t ret = DA_RESULT_OK;
	DA_LOGI("Return ret = %d", ret);
	return ret;
}

int da_deinit()
{
	da_ret_t ret = DA_RESULT_OK;

	DA_LOGV("");
	destroy_da_info_list();
	DA_LOGI("====== da_deint EXIT =====");
	return ret;
}

int da_start_download(const char *url, req_data_t *ext_data,
		da_cb_t *da_cb_data,	int *download_id)
{
	da_ret_t ret = DA_RESULT_OK;
	int req_header_count = 0;
	int i = 0;
	int da_id = DA_INVALID_ID;
	da_info_t *da_info = DA_NULL;

	*download_id = DA_INVALID_ID;

	if (ext_data->request_header_count > 0) {
		DA_LOGI("request_header_count[%d]", ext_data->request_header_count);
		for (i = 0; i < ext_data->request_header_count; i++) {
			if (ext_data->request_header[i]) {
				req_header_count++;
				DA_SECURE_LOGI("request_header[%s]", ext_data->request_header[i]);
			}
		}
		DA_LOGI("actual request_header_count[%d]", req_header_count);
		if (ext_data->request_header_count != req_header_count) {
			DA_LOGE("Request header count is not matched with number of request header array");
			ret = DA_ERR_INVALID_ARGUMENT;
			goto ERR;
		}
	}

	if (ext_data->install_path)
		DA_SECURE_LOGI("install path[%s]", ext_data->install_path);
	if (ext_data->file_name)
		DA_SECURE_LOGI("file_name[%s]", ext_data->file_name);
	if (ext_data->temp_file_path)
		DA_SECURE_LOGI("temp_file_path[%s]", ext_data->temp_file_path);
	if (ext_data->etag)
		DA_SECURE_LOGI("etag[%s]", ext_data->etag);
	if (ext_data->pkg_name)
		DA_SECURE_LOGI("pkg_name[%s]", ext_data->pkg_name);
	if (ext_data->network_bonding)
		DA_LOGD("network bonding option[%d]", ext_data->network_bonding);
	if (ext_data->user_req_data)
		DA_LOGI("user_req_data[%p]", ext_data->user_req_data);
	if (ext_data->user_client_data)
			DA_LOGI("user_client_data[%p]", ext_data->user_client_data);

	ret = get_available_da_id(&da_id);
	if (ret != DA_RESULT_OK)
		goto ERR;

	da_info = da_info_list[da_id];
	da_info->da_id = da_id;

	ret = copy_user_input_data(da_info, url, ext_data, da_cb_data);
	if (ret != DA_RESULT_OK)
		goto ERR;
	*download_id = da_id;
	ret = start_download(da_info);
ERR:
	DA_LOGI("Return:id[%d],ret[%d]", *download_id, ret);
	return ret;
}

int da_cancel_download(int download_id)
{
	da_ret_t ret = DA_RESULT_OK;

	DA_LOGV("download_id[%d]", download_id);
	ret = cancel_download(download_id, DA_TRUE);
	DA_LOGI("Return:id[%d],ret[%d]", download_id, ret);
	return ret;
}

int da_cancel_download_without_update(int download_id)
{
	da_ret_t ret = DA_RESULT_OK;
	DA_LOGV("download_id[%d]", download_id);
	ret = cancel_download(download_id, DA_FALSE);
	DA_LOGI("Return:id[%d],ret[%d]", download_id, ret);
	return ret;
}

int da_suspend_download(int download_id)
{
	da_ret_t ret = DA_RESULT_OK;
	DA_LOGV("download_id[%d]", download_id);
	ret = suspend_download(download_id, DA_TRUE);
	DA_LOGI("Return:id[%d],ret[%d]", download_id, ret);
	return ret;
}

int da_suspend_download_without_update(int download_id)
{
	da_ret_t ret = DA_RESULT_OK;
	DA_LOGV("download_id[%d]", download_id);
	ret = suspend_download(download_id, DA_FALSE);
	DA_LOGI("Return:id[%d],ret[%d]", download_id, ret);
	return ret;
}


int da_resume_download(int download_id)
{
	da_ret_t ret = DA_RESULT_OK;
	DA_LOGV("download_id[%d]", download_id);
	ret = resume_download(download_id);
	DA_LOGI("Return:id[%d],ret[%d]", download_id, ret);
	return ret;
}

int da_is_valid_download_id(int download_id)
{
	da_bool_t ret = DA_FALSE;
	ret = is_valid_download_id(download_id);
	DA_LOGI("Return:id[%d],ret[%d]", download_id, ret);
	return ret;
}
