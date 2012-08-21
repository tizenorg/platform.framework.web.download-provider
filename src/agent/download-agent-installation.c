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
 * @file		download-agent-installation.c
 * @brief		Functions for Content Installation
 * @author		Keunsoon Lee(keunsoon.lee@samsung.com)
 * @author		Jungki Kwak(jungki.kwak@samsung.com)
 ***/

#include <sys/time.h>
#include <unistd.h>

#include "download-agent-client-mgr.h"
#include "download-agent-dl-info-util.h"
#include "download-agent-http-mgr.h"
#include "download-agent-http-misc.h"
#include "download-agent-installation.h"
#include "download-agent-file.h"
#include "download-agent-plugin-install.h"

da_result_t _extract_file_path_which_will_be_installed(char *in_file_name, char *in_extension, char *in_install_path_client_wants, char **out_will_install_path);

da_result_t install_content(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;

	file_info *file_storage = DA_NULL;
	char *temp_saved_file_path = DA_NULL;
	char *install_file_path = DA_NULL;
	unsigned int start_time = 0;

	DA_LOG_FUNC_START(InstallManager);

	if (!stage)
		return DA_ERR_INVALID_ARGUMENT;

	file_storage = GET_STAGE_CONTENT_STORE_INFO(stage);
	if (!file_storage) {
		DA_LOG_ERR(InstallManager,"file_storage structure is NULL");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}

	temp_saved_file_path =
			GET_CONTENT_STORE_TMP_FILE_NAME(file_storage);
	DA_LOG(InstallManager,"Source path[%s]",temp_saved_file_path);

	ret = check_enough_storage(stage);
	if (ret != DA_RESULT_OK)
		goto ERR;

	ret = _extract_file_path_which_will_be_installed(
			GET_CONTENT_STORE_PURE_FILE_NAME(file_storage),
			GET_CONTENT_STORE_EXTENSION(file_storage),
			GET_DL_USER_INSTALL_PATH(GET_STAGE_DL_ID(stage)),
			&install_file_path);
	if (ret != DA_RESULT_OK)
		goto ERR;

	DA_LOG(InstallManager,"Installing path [%s]", install_file_path);
	DA_LOG(InstallManager,"Move start time : [%ld]",start_time=time(NULL));
	ret = move_file(temp_saved_file_path, install_file_path);
	if (ret != DA_RESULT_OK)
		goto ERR;

	if (GET_CONTENT_STORE_ACTUAL_FILE_NAME(file_storage))
		free(GET_CONTENT_STORE_ACTUAL_FILE_NAME(file_storage));
	GET_CONTENT_STORE_ACTUAL_FILE_NAME(file_storage) = install_file_path;
	install_file_path = DA_NULL;


ERR:
	if (ret != DA_RESULT_OK) {
		remove_file(GET_CONTENT_STORE_TMP_FILE_NAME(file_storage));
		remove_file(install_file_path);
	} else {
	}
	return ret;
}

da_result_t _extract_file_path_which_will_be_installed(char *in_file_name, char *in_extension, char *in_install_path_client_wants, char **out_will_install_path)
{
	da_result_t ret = DA_RESULT_OK;
	char *install_dir = NULL;
	char *default_install_dir = NULL;
	char *final_path = NULL;
	char *pure_file_name = in_file_name;
	char *extension = in_extension;

	if (!in_file_name || !out_will_install_path)
		return DA_ERR_INVALID_ARGUMENT;

	*out_will_install_path = DA_NULL;

	if (in_install_path_client_wants) {
		install_dir = in_install_path_client_wants;
	} else {
		default_install_dir = PI_get_default_install_dir();
		if (default_install_dir)
			install_dir = default_install_dir;
		else
			return DA_ERR_FAIL_TO_INSTALL_FILE;
	}

	if (DA_FALSE == is_dir_exist(install_dir)) {
		ret = create_dir(install_dir);
		if (ret != DA_RESULT_OK)
			return DA_ERR_FAIL_TO_INSTALL_FILE;
	}

	final_path = get_full_path_avoided_duplication(install_dir, pure_file_name, extension);
	if (!final_path)
		ret = DA_ERR_FAIL_TO_INSTALL_FILE;

	*out_will_install_path = final_path;

	DA_LOG(InstallManager,"Final install path[%s]", *out_will_install_path);

	return ret;
}
