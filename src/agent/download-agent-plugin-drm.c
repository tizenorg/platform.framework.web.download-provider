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
 * @file		download-agent-plugin-drm.c
 * @brief		Platform dependent functions for emerald DRM from SISO
 * @author		Keunsoon Lee(keunsoon.lee@samsung.com)
 ***/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <drm_client.h>
#include <drm_client_types.h>
#include <drm_trusted_client.h>
#include <drm_trusted_client_types.h>
#include "download-agent-debug.h"
#include "download-agent-plugin-drm.h"

da_bool_t EDRM_check_dcf_file(const char *file_path)
{
	drm_file_info_s file_info;
	int ret = -1;

	ret = drm_get_file_info(file_path, &file_info);
	if (ret != DRM_RETURN_SUCCESS)	{
		DA_LOG(DRMManager,"error : ret[%d]\n", ret);
		return DA_FALSE;
	}

	if (file_info.oma_info.version == DRM_OMA_DRMV1_RIGHTS &&
			(file_info.oma_info.method == DRM_METHOD_TYPE_COMBINED_DELIVERY ||
			file_info.oma_info.method == DRM_METHOD_TYPE_SEPARATE_DELIVERY)) {
		return DA_TRUE;
	} else {
		return DA_FALSE;
	}
}

da_bool_t EDRM_has_vaild_ro(const char *file_path)
{
	int ret = -1;
	drm_license_status_e status = DRM_LICENSE_STATUS_UNDEFINED;
	ret = drm_get_license_status(file_path, DRM_PERMISSION_TYPE_ANY, &status);

	if (status == DRM_LICENSE_STATUS_VALID)
		return DA_TRUE;
	else
		return DA_FALSE;
}

da_bool_t EDRM_open_convert(const char *file_path, void **fd)
{
	DRM_TRUSTED_CONVERT_HANDLE hConvert;
	drm_trusted_opn_conv_info_s input = {{0,},};

	if (!file_path || !fd) {
		DA_LOG_ERR(DRMManager,"Invalid paramter");
		return DA_FALSE;
	}
	strncpy(input.filePath, file_path, sizeof(input.filePath) - 1);
	if (drm_trusted_open_convert(&input, &hConvert) < 0) {
		DA_LOG_ERR(DRMManager,"Fail to open convert");
		return DA_FALSE;
	}
	*fd = hConvert;
	return DA_TRUE;
}

da_bool_t EDRM_write_convert(void *fd, unsigned char *buffer, int buffer_size)
{
	DRM_TRUSTED_CONVERT_HANDLE hConvert;
	drm_trusted_write_conv_info_s input = {0,};
	drm_trusted_write_conv_resp_s output = {0,};

	if (!fd || !buffer || buffer_size < 0) {
		DA_LOG_ERR(DRMManager,"Invalid paramter");
		return DA_FALSE;
	}
	hConvert = fd;
	input.data_len = buffer_size;
	input.data = buffer;
	if (drm_trusted_write_convert(&input,&output,fd) < 0) {
		DA_LOG_ERR(DRMManager,"Fail to write convert");
		return DA_FALSE;
	}
	if (buffer_size != output.write_size) {
		DA_LOG_ERR(DRMManager,"written size is failed");
		return DA_FALSE;
	}
	return DA_TRUE;
}

da_bool_t EDRM_close_convert(void **fd)
{
	DRM_TRUSTED_CONVERT_HANDLE hConvert;
	if (!fd) {
		DA_LOG_ERR(DRMManager,"Invalid paramter");
		return DA_FALSE;
	}
	hConvert = *fd;
	if (drm_trusted_close_convert(&hConvert) < 0) {
		DA_LOG_ERR(DRMManager,"Fail to close convert");
		return DA_FALSE;
	}
	return DA_TRUE;
}

da_bool_t EDRM_http_user_cancel(void *roap_session)
{
	return DA_FALSE;
}

da_result_t EDRM_wm_get_license(char *rights_url, char **out_content_url)
{
	drm_initiator_info_s init_info;
	drm_web_server_resp_data_s resp_data;
	int ret = 0;
	int len = 0;

	if (rights_url == NULL)
		return DA_ERR_DRM_FAIL;

	memset(&init_info, 0, sizeof(init_info));
	memset(&resp_data, 0, sizeof(resp_data));
	strncpy(init_info.initiator_url, rights_url, DRM_MAX_LEN_INITIATOR_URL - 1);
	len = strlen(rights_url);
	if (len > DRM_MAX_LEN_INITIATOR_URL - 1)
		init_info.initiator_url_len = (unsigned int)len;
	else
		init_info.initiator_url_len = DRM_MAX_LEN_INITIATOR_URL;
	ret = drm_process_request(DRM_REQUEST_TYPE_SUBMIT_INITIATOR_URL,
			&init_info, &resp_data);
	if (DRM_RETURN_SUCCESS == ret) {
		DA_LOG(DRMManager,"resp_data.content_url = %s", resp_data.content_url);
		/* Rights or Domain Certificate are installed successfully */
		/* Check for contentURL */
		if (strlen(resp_data.content_url) > 0) {
			char *content_url = NULL;
			size_t content_url_len = 0;
			content_url_len = strlen(resp_data.content_url);
			if (content_url_len == 0) {
				DA_LOG(DRMManager,"content_url is NULL. Join/Leave Domain, Metering case.");
				*out_content_url = DA_NULL;
				return DA_RESULT_OK;
			} else {
				content_url = (char *)calloc(1, content_url_len + 1);
				if (content_url) {
					strncpy(content_url, resp_data.content_url,
						content_url_len);
					*out_content_url = 	content_url;
					DA_LOG(DRMManager,"drm sumitted initiator url "
							"succeeded with [%s]", *out_content_url);
					return DA_RESULT_OK;
				} else {
					DA_LOG_ERR(DRMManager,"DA_ERR_FAIL_TO_MEMALLOC");
					return DA_ERR_FAIL_TO_MEMALLOC;
				}
			}
		} else {
			DA_LOG_ERR(DRMManager,"resp_data.content url is NULL");
			return DA_ERR_DRM_FAIL;
		}
	} else {
		DA_LOG_ERR(DRMManager,"drm_process_request() failed");
		return DA_ERR_DRM_FAIL;
	}
}

