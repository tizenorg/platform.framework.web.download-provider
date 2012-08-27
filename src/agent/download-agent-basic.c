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
 * @file		download-agent-basic.c
 * @brief		functions for basic (http) download
 * @author		Keunsoon Lee(keunsoon.lee@samsung.com)
 * @author		Jungki Kwak(jungki.kwak@samsung.com)
 ***/

#include <stdlib.h>

#include "download-agent-basic.h"
#include "download-agent-debug.h"
#include "download-agent-client-mgr.h"
#include "download-agent-utils.h"
#include "download-agent-http-mgr.h"
#include "download-agent-http-misc.h"
#include "download-agent-dl-mgr.h"
#include "download-agent-installation.h"
#include "download-agent-pthread.h"

static void* __thread_start_download(void* data);
void __thread_clean_up_handler_for_start_download(void *arg);

static da_result_t __make_source_info_basic_download(
		stage_info *stage,
		client_input_t *client_input);
static da_result_t __download_content(stage_info *stage);

da_result_t start_download(const char *url , da_handle_t *dl_req_id)
{
	DA_LOG_FUNC_START(Default);
	return start_download_with_extension(url, dl_req_id, NULL);
}

da_result_t start_download_with_extension(
		const char *url,
		da_handle_t *dl_req_id,
		extension_data_t *extension_data)
{
	da_result_t  ret = DA_RESULT_OK;
	int download_id = 0;
	const char **request_header = DA_NULL;
	const char *install_path = DA_NULL;
	const char *file_name = DA_NULL;
	int request_header_count = 0;
	void *user_data = DA_NULL;
	client_input_t *client_input = DA_NULL;
	client_input_basic_t *client_input_basic = DA_NULL;
	download_thread_input *thread_info = DA_NULL;
	pthread_attr_t thread_attr;

	DA_LOG_FUNC_START(Default);

	if (extension_data) {
		request_header = extension_data->request_header;
		if (extension_data->request_header_count)
			request_header_count = *(extension_data->request_header_count);
		install_path = extension_data->install_path;
		file_name = extension_data->file_name;
		user_data = extension_data->user_data;
	}

	ret = get_available_download_id(&download_id);
	if (DA_RESULT_OK != ret)
		return ret;

	*dl_req_id = GET_DL_REQ_ID(download_id);

	client_input = (client_input_t *)calloc(1, sizeof(client_input_t));
	if (!client_input) {
		DA_LOG_ERR(Default, "DA_ERR_FAIL_TO_MEMALLOC");
		ret = DA_ERR_FAIL_TO_MEMALLOC;
		goto ERR;
	} else {
		client_input->user_data = user_data;
		if (install_path) {
			int install_path_len = strlen(install_path);
			if (install_path[install_path_len-1] == '/')
				install_path_len--;

			client_input->install_path = (char *)calloc(1, install_path_len+1);
			if (client_input->install_path)
				snprintf(client_input->install_path, install_path_len+1, install_path);
		}

		if (file_name) {
			client_input->file_name = (char *)calloc(1, strlen(file_name)+1);
			if (client_input->file_name)
				strncpy(client_input->file_name, file_name, strlen(file_name));
		}

		client_input_basic = &(client_input->client_input_basic);
		client_input_basic->req_url = (char *)calloc(1, strlen(url)+1);
		if(DA_NULL == client_input_basic->req_url) {
			DA_LOG_ERR(Default, "DA_ERR_FAIL_TO_MEMALLOC");
			ret = DA_ERR_FAIL_TO_MEMALLOC;
			goto ERR;
		}
		strncpy(client_input_basic->req_url ,url,strlen(url));

		if (request_header_count > 0) {
			int i = 0;
			client_input_basic->user_request_header =
				(char **)calloc(1, sizeof(char *)*request_header_count);
			if(DA_NULL == client_input_basic->user_request_header) {
				DA_LOG_ERR(Default, "DA_ERR_FAIL_TO_MEMALLOC");
				ret = DA_ERR_FAIL_TO_MEMALLOC;
				goto ERR;
			}
			for (i = 0; i < request_header_count; i++)
			{
				client_input_basic->user_request_header[i] = strdup(request_header[i]);
			}
			client_input_basic->user_request_header_count = request_header_count;
		}
	}

	thread_info = (download_thread_input *)calloc(1, sizeof(download_thread_input));
	if (!thread_info) {
		DA_LOG_ERR(Default, "DA_ERR_FAIL_TO_MEMALLOC");
		ret = DA_ERR_FAIL_TO_MEMALLOC;
		goto ERR;
	} else {
		thread_info->download_id = download_id;
		thread_info->client_input = client_input;
	}
	if (pthread_attr_init(&thread_attr) != 0) {
		ret = DA_ERR_FAIL_TO_CREATE_THREAD;
		goto ERR;
	}

	if (pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED) != 0) {
		ret = DA_ERR_FAIL_TO_CREATE_THREAD;
		goto ERR;
	}

	if (pthread_create(&GET_DL_THREAD_ID(download_id), &thread_attr,
			__thread_start_download, thread_info) < 0) {
		DA_LOG_ERR(Thread, "making thread failed..");
		ret = DA_ERR_FAIL_TO_CREATE_THREAD;
	} else {
		if (GET_DL_THREAD_ID(download_id) < 1) {
			DA_LOG_ERR(Thread, "The thread start is failed before calling this");
// When http resource is leaked, the thread ID is initialized at error handling section of thread_start_download()
// Because the thread ID is initialized, the ptrhead_detach should not be called. This is something like timing issue between threads.
// thread info and basic_dl_input is freed at thread_start_download(). And it should not returns error code in this case.
			goto ERR;
		}
	}
	DA_LOG_CRITICAL(Thread, "download thread create download_id[%d] thread id[%lu]",
			download_id,GET_DL_THREAD_ID(download_id));

ERR:
	if (DA_RESULT_OK != ret) {
		if (client_input) {
			clean_up_client_input_info(client_input);
			free(client_input);
			client_input = DA_NULL;
		}
		if (thread_info) {
			free(thread_info);
			thread_info = DA_NULL;
		}
		destroy_download_info(download_id);
	}
	return ret;
}

da_result_t __make_source_info_basic_download(
		stage_info *stage,
		client_input_t *client_input)
{
	da_result_t ret = DA_RESULT_OK;
	client_input_basic_t *client_input_basic = DA_NULL;
	source_info_t *source_info = DA_NULL;
	source_info_basic_t *source_info_basic = DA_NULL;

	DA_LOG_FUNC_START(Default);

	if (!stage) {
		DA_LOG_ERR(Default, "no stage; DA_ERR_INVALID_ARGUMENT");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}

	client_input_basic = &(client_input->client_input_basic);
	if (DA_NULL == client_input_basic->req_url) {
		DA_LOG_ERR(Default, "DA_ERR_INVALID_URL");
		ret = DA_ERR_INVALID_URL;
		goto ERR;
	}

	source_info_basic = (source_info_basic_t*)calloc(1, sizeof(source_info_basic_t));
	if (DA_NULL == source_info_basic) {
		DA_LOG_ERR(Default, "DA_ERR_FAIL_TO_MEMALLOC");
		ret = DA_ERR_FAIL_TO_MEMALLOC;
		goto ERR;
	}

	source_info_basic->url = client_input_basic->req_url;
	client_input_basic->req_url = DA_NULL;

	if (client_input_basic->user_request_header) {
		source_info_basic->user_request_header =
			client_input_basic->user_request_header;
		source_info_basic->user_request_header_count =
			client_input_basic->user_request_header_count;
		client_input_basic->user_request_header = DA_NULL;
		client_input_basic->user_request_header_count = 0;
	}

	source_info = GET_STAGE_SOURCE_INFO(stage);
	memset(source_info, 0, sizeof(source_info_t));

	source_info->source_info_type.source_info_basic = source_info_basic;

	DA_LOG(Default, "BASIC HTTP STARTED: URL=%s",
			source_info->source_info_type.source_info_basic->url);
ERR:
	return ret;
}

void __thread_clean_up_handler_for_start_download(void *arg)
{
       DA_LOG_CRITICAL(Default, "cleanup for thread id = %d", pthread_self());
}

static void *__thread_start_download(void *data)
{
	da_result_t ret = DA_RESULT_OK;
	download_thread_input *thread_info = DA_NULL;
	client_input_t *client_input = DA_NULL;
	stage_info *stage = DA_NULL;
	int download_id = DA_INVALID_ID;

	DA_LOG_FUNC_START(Thread);

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, DA_NULL);

	thread_info = (download_thread_input*)data;
	if (DA_NULL == thread_info) {
		DA_LOG_ERR(Thread, "thread_info is NULL..");
		ret = DA_ERR_INVALID_ARGUMENT;
		return DA_NULL;
	} else {
		download_id = thread_info->download_id;
		client_input = thread_info->client_input;

		if(thread_info) {
			free(thread_info);
			thread_info = DA_NULL;
		}
	}

	pthread_cleanup_push(__thread_clean_up_handler_for_start_download, (void *)NULL);

	if (DA_FALSE == is_valid_dl_ID(download_id)) {
		ret = DA_ERR_INVALID_ARGUMENT;
		DA_LOG_ERR(Default, "Invalid Download ID");
		goto ERR;
	}

	if (!client_input) {
		ret = DA_ERR_INVALID_ARGUMENT;
		DA_LOG_ERR(Default, "Invalid client_input");
		goto ERR;
	}

	stage = Add_new_download_stage(download_id);
	if (!stage) {
		ret = DA_ERR_FAIL_TO_MEMALLOC;
		DA_LOG_ERR(Default, "STAGE ADDITION FAIL!");
		goto ERR;
	}
	DA_LOG(Default, "new added Stage : %p", stage);

	GET_DL_USER_DATA(download_id) = client_input->user_data;
	GET_DL_USER_INSTALL_PATH(download_id) = client_input->install_path;
	client_input->install_path = DA_NULL;
	GET_DL_USER_FILE_NAME(download_id) = client_input->file_name;
	client_input->file_name = DA_NULL;

	ret = __make_source_info_basic_download(stage, client_input);
	/* to save memory */
	if (client_input) {
		clean_up_client_input_info(client_input);
		free(client_input);
		client_input = DA_NULL;
	}

	if (ret == DA_RESULT_OK)
		ret = __download_content(stage);

ERR:
	if (client_input) {
		clean_up_client_input_info(client_input);
		free(client_input);
		client_input = DA_NULL;
	}

	if (DA_RESULT_OK == ret) {
		DA_LOG_CRITICAL(Default, "Whole download flow is finished.");
		send_user_noti_and_finish_download_flow(download_id);
	} else {
		DA_LOG_CRITICAL(Default, "DA_STATE_FAILED -Return = %d", ret);
		send_client_da_state(download_id, DA_STATE_FAILED, ret);
		destroy_download_info(download_id);
	}

	pthread_cleanup_pop(0);
	DA_LOG_CRITICAL(Thread, "=====thread_start_download - EXIT=====");
	pthread_exit((void *)NULL);
	return DA_NULL;
}

da_result_t __download_content(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	download_state_t download_state = 0;
	da_bool_t isDownloadComplete = DA_FALSE;
	int download_id = DA_INVALID_ID;

	DA_LOG_FUNC_START(Default);

	download_id = GET_STAGE_DL_ID(stage);
	CHANGE_DOWNLOAD_STATE(DOWNLOAD_STATE_NEW_DOWNLOAD, stage);

	do {
		stage = GET_DL_CURRENT_STAGE(download_id);
		_da_thread_mutex_lock (&mutex_download_state[GET_STAGE_DL_ID(stage)]);
		download_state = GET_DL_STATE_ON_STAGE(stage);
		DA_LOG(Default, "download_state to - [%d] ", download_state);
		_da_thread_mutex_unlock (&mutex_download_state[GET_STAGE_DL_ID(stage)]);

		switch(download_state) {
		case DOWNLOAD_STATE_NEW_DOWNLOAD:
			ret = requesting_download(stage);

			_da_thread_mutex_lock (&mutex_download_state[GET_STAGE_DL_ID(stage)]);
			download_state = GET_DL_STATE_ON_STAGE(stage);
			_da_thread_mutex_unlock (&mutex_download_state[GET_STAGE_DL_ID(stage)]);

			if (download_state == DOWNLOAD_STATE_CANCELED) {
				break;
			}	else {
				if (DA_RESULT_OK == ret) {
					ret = handle_after_download(stage);
				}
			}
			break;
		case DOWNLOAD_STATE_READY_TO_INSTAL:
			send_client_da_state(download_id, DA_STATE_DOWNLOAD_COMPLETE, DA_RESULT_OK);
			ret = process_install(stage);
			if (DA_RESULT_OK == ret) {
				CHANGE_DOWNLOAD_STATE(DOWNLOAD_STATE_FINISH,stage);
			}
			break;
		default:
			isDownloadComplete = DA_TRUE;
			break;
		}
	}while ((DA_RESULT_OK == ret) && (DA_FALSE == isDownloadComplete));

	return ret;
}
