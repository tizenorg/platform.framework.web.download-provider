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

#include "drm_client.h"
#include "drm_client_types.h"
#include "drm_trusted_client.h"
#include "drm_trusted_client_types.h"

#include "download-agent-debug.h"
#include "download-agent-plugin-drm.h"


void __EDRM_clean_up()
{
	int ret = 0;
	ret = drm_trusted_handle_request(DRM_TRUSTED_REQ_TYPE_CLIENT_CLEAN_UP, NULL, NULL);
   if (DRM_RETURN_SUCCESS == ret) {
	   DA_LOGD( "Clean up successfull");
   } else {
	   DA_LOGE("ret[%0x%x]",ret);
   }
}

da_bool_t EDRM_convert(const char *in_file_path, char **out_file_path)
{
	drm_trusted_conv_info_s input;
	drm_trusted_conv_resp_info_s output;
	size_t len = 0;
	int ret = 0;

	memset(&input, 0x0, sizeof(drm_trusted_conv_info_s));
	memset(&output, 0x0, sizeof(drm_trusted_conv_resp_info_s));

	len = strlen(in_file_path);
	if (len >= sizeof(input.filePath))
		len = sizeof(input.filePath) - 1;
	memcpy(input.filePath, in_file_path, len);

	ret = drm_trusted_convert_dm(&input, &output);

	if (DRM_TRUSTED_RETURN_SUCCESS != ret) {
		DA_LOGE("ret[%0x%x]",ret);
		__EDRM_clean_up();
		return DA_FALSE;
	} else {
		DA_SECURE_LOGD("Returned filePath[%s]", output.filePath);
		*out_file_path = strdup(output.filePath);
	}
	__EDRM_clean_up();
	return DA_TRUE;
}

da_ret_t EDRM_wm_get_license(char *rights_url, char **out_content_url)
{
	int ret = 0;
	int len = 0;
	drm_initiator_info_s init_info;
	drm_web_server_resp_data_s resp_data;

	if (rights_url == NULL)
		return DA_ERR_DRM_FAIL;

	memset(&init_info, 0, sizeof(init_info));
	memset(&resp_data, 0, sizeof(resp_data));
	strncpy(init_info.initiator_url, rights_url,
			DRM_MAX_LEN_INITIATOR_URL - 1);
	len = strlen(rights_url);
	if (len > DRM_MAX_LEN_INITIATOR_URL - 1)
		init_info.initiator_url_len = (unsigned int)len;
	else
		init_info.initiator_url_len = DRM_MAX_LEN_INITIATOR_URL;
	ret = drm_process_request(DRM_REQUEST_TYPE_SUBMIT_INITIATOR_URL,
			&init_info, &resp_data);
	if (DRM_RETURN_SUCCESS == ret) {
		DA_SECURE_LOGD("resp_data.content_url = %s", resp_data.content_url);
		/* Rights or Domain Certificate are installed successfully */
		/* Check for contentURL */
		if (strlen(resp_data.content_url) > 0) {
			char *content_url = NULL;
			size_t content_url_len = 0;
			content_url_len = strlen(resp_data.content_url);
			content_url = (char *)calloc(1, content_url_len + 1);
			if (content_url) {
				strncpy(content_url, resp_data.content_url,
					content_url_len);
				*out_content_url = content_url;
				DA_SECURE_LOGD("drm sumitted initiator url "
						"succeeded with [%s]", *out_content_url);
				__EDRM_clean_up();
				return DA_RESULT_OK;
			} else {
				DA_LOGE("DA_ERR_FAIL_TO_MEMALLOC");
				__EDRM_clean_up();
				return DA_ERR_FAIL_TO_MEMALLOC;
			}
		} else {
			DA_LOGV("content_url is NULL.\
					Join/Leave Domain, Metering case.");
			*out_content_url = DA_NULL;
			__EDRM_clean_up();
			return DA_RESULT_OK;
		}
	} else {
		DA_LOGE("drm_process_request() failed");
		__EDRM_clean_up();
		return DA_ERR_DRM_FAIL;
	}
}

