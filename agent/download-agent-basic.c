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

#include "download-agent-basic.h"
#include "download-agent-debug.h"
#include "download-agent-client-mgr.h"
#include "download-agent-utils.h"
#include "download-agent-http-mgr.h"
#include "download-agent-http-misc.h"
#include "download-agent-dl-mgr.h"
#include "download-agent-pthread.h"
#include "download-agent-file.h"

static void* __thread_start_download(void* data);
void __thread_clean_up_handler_for_start_download(void *arg);

static da_result_t __make_source_info_basic_download(
		stage_info *stage,
		client_input_t *client_input);
static da_result_t __download_content(stage_info *stage);

da_result_t start_download(const char *url , int *dl_id)
{
	DA_LOG_FUNC_LOGD(Default);
	return start_download_with_extension(url, dl_id, NULL);
}

da_result_t start_download_with_extension(
		const char *url,
		int *dl_id,
		extension_data_t *extension_data)
{
	da_result_t  ret = DA_RESULT_OK;
	int slot_id = 0;
	const char **request_header = DA_NULL;
	const char *install_path = DA_NULL;
	const char *file_name = DA_NULL;
	const char *etag = DA_NULL;
	const char *temp_file_path = DA_NULL;
	const char *pkg_name = DA_NULL;
	int request_header_count = 0;
	void *user_data = DA_NULL;
	client_input_t *client_input = DA_NULL;
	client_input_basic_t *client_input_basic = DA_NULL;
	download_thread_input *thread_info = DA_NULL;
	pthread_attr_t thread_attr;

	DA_LOG_FUNC_LOGV(Default);

	if (extension_data) {
		request_header = extension_data->request_header;
		if (extension_data->request_header_count)
			request_header_count = extension_data->request_header_count;
		install_path = extension_data->install_path;
		file_name = extension_data->file_name;
		user_data = extension_data->user_data;
		etag = extension_data->etag;
		temp_file_path = extension_data->temp_file_path;
		pkg_name = extension_data->pkg_name;
	}

	ret = get_available_slot_id(&slot_id);
	if (DA_RESULT_OK != ret)
		return ret;

	*dl_id = GET_DL_ID(slot_id);

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

			client_input->install_path = (char *)calloc(install_path_len+1, sizeof(char));
			if (client_input->install_path)
				strncpy(client_input->install_path, install_path, install_path_len);
		}

		if (file_name) {
			client_input->file_name = (char *)calloc(strlen(file_name)+1, sizeof(char));
			if (client_input->file_name)
				strncpy(client_input->file_name, file_name, strlen(file_name));
		}

		if (etag) {
			client_input->etag = (char *)calloc(strlen(etag)+1, sizeof(char));
			if (client_input->etag)
				strncpy(client_input->etag, etag, strlen(etag));

		}

		if (temp_file_path) {
			client_input->temp_file_path = (char *)calloc(strlen(temp_file_path)+1, sizeof(char));
			if (client_input->temp_file_path)
				strncpy(client_input->temp_file_path, temp_file_path, strlen(temp_file_path));
		}

		if (pkg_name) {
			client_input->pkg_name = (char *)calloc(strlen(pkg_name)+1, sizeof(char));
			if (client_input->pkg_name)
				strncpy(client_input->pkg_name, pkg_name, strlen(pkg_name));
		}
		client_input_basic = &(client_input->client_input_basic);
		client_input_basic->req_url = (char *)calloc(strlen(url)+1, sizeof(char));
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
		thread_info->slot_id = slot_id;
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

	if (pthread_create(&GET_DL_THREAD_ID(slot_id), &thread_attr,
			__thread_start_download, thread_info) < 0) {
		DA_LOG_ERR(Thread, "making thread failed..");
		ret = DA_ERR_FAIL_TO_CREATE_THREAD;
	} else {
		if (GET_DL_THREAD_ID(slot_id) < 1) {
			DA_LOG_ERR(Thread, "The thread start is failed before calling this");
// When http resource is leaked, the thread ID is initialized at error handling section of thread_start_download()
// Because the thread ID is initialized, the ptrhead_detach should not be called. This is something like timing issue between threads.
// thread info and basic_dl_input is freed at thread_start_download(). And it should not returns error code in this case.
			goto ERR;
		}
	}
	DA_LOG_DEBUG(Thread, "download thread create slot_id[%d] thread id[%lu]",
			slot_id,GET_DL_THREAD_ID(slot_id));

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
		destroy_download_info(slot_id);
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

	DA_LOG_FUNC_LOGV(Default);

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

//	DA_SECURE_LOGI("BASIC HTTP STARTED: URL=%s",
//			source_info->source_info_type.source_info_basic->url);
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
	download_state_t download_state = 0;

	int slot_id = DA_INVALID_ID;

	DA_LOG_FUNC_LOGV(Thread);

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, DA_NULL);

	thread_info = (download_thread_input*)data;
	if (DA_NULL == thread_info) {
		DA_LOG_ERR(Thread, "thread_info is NULL..");
		ret = DA_ERR_INVALID_ARGUMENT;
		return DA_NULL;
	} else {
		slot_id = thread_info->slot_id;
		client_input = thread_info->client_input;

		if(thread_info) {
			free(thread_info);
			thread_info = DA_NULL;
		}
	}

	pthread_cleanup_push(__thread_clean_up_handler_for_start_download, (void *)NULL);

	if (DA_FALSE == is_valid_slot_id(slot_id)) {
		ret = DA_ERR_INVALID_ARGUMENT;
		DA_LOG_ERR(Default, "Invalid Download ID");
		goto ERR;
	}

	if (!client_input) {
		ret = DA_ERR_INVALID_ARGUMENT;
		DA_LOG_ERR(Default, "Invalid client_input");
		goto ERR;
	}

	stage = Add_new_download_stage(slot_id);
	if (!stage) {
		ret = DA_ERR_FAIL_TO_MEMALLOC;
		DA_LOG_ERR(Default, "STAGE ADDITION FAIL!");
		goto ERR;
	}
	DA_LOG_VERBOSE(Default, "new added Stage : %p", stage);

	GET_DL_USER_DATA(slot_id) = client_input->user_data;
	client_input->user_data = DA_NULL;
	GET_DL_USER_INSTALL_PATH(slot_id) = client_input->install_path;
	client_input->install_path = DA_NULL;
	GET_DL_USER_FILE_NAME(slot_id) = client_input->file_name;
	client_input->file_name = DA_NULL;
	GET_DL_USER_ETAG(slot_id) = client_input->etag;
	client_input->etag = DA_NULL;
	GET_DL_USER_TEMP_FILE_PATH(slot_id) = client_input->temp_file_path;
	client_input->temp_file_path = DA_NULL;

	ret = __make_source_info_basic_download(stage, client_input);

	if (ret == DA_RESULT_OK) {
		/* to save memory */
		if (client_input) {
			clean_up_client_input_info(client_input);
			free(client_input);
			client_input = DA_NULL;
		}

		ret = __download_content(stage);
		if (stage != GET_DL_CURRENT_STAGE(slot_id)) {
			DA_LOG_ERR(Default,"Playready download case. The next stage is present stage");
			stage = GET_DL_CURRENT_STAGE(slot_id);
		}
	}
ERR:
	if (client_input) {
		clean_up_client_input_info(client_input);
		free(client_input);
		client_input = DA_NULL;
	}

	if (DA_RESULT_OK == ret) {
		char *installed_path = NULL;
		char *etag = DA_NULL;
		req_dl_info *request_info = NULL;
		file_info *file_storage = NULL;
		DA_LOG_VERBOSE(Default, "Whole download flow is finished.");
		_da_thread_mutex_lock (&mutex_download_state[GET_STAGE_DL_ID(stage)]);
		download_state = GET_DL_STATE_ON_STAGE(stage);
		_da_thread_mutex_unlock (&mutex_download_state[GET_STAGE_DL_ID(stage)]);
		if (download_state == DOWNLOAD_STATE_ABORTED) {
			DA_LOG(Default, "Abort case. Do not call client callback");
#ifdef PAUSE_EXIT
		} else if (download_state == DOWNLOAD_STATE_PAUSED) {
			DA_LOG(Default, "Finish case from paused state");
			destroy_download_info(slot_id);
#endif
		} else {
			request_info = GET_STAGE_TRANSACTION_INFO(stage);
			etag = GET_REQUEST_HTTP_HDR_ETAG(request_info);
			file_storage = GET_STAGE_CONTENT_STORE_INFO(stage);
			installed_path = GET_CONTENT_STORE_ACTUAL_FILE_NAME(file_storage);
			send_user_noti_and_finish_download_flow(slot_id, installed_path,
					etag);
		}
	} else {
		char *etag = DA_NULL;
		req_dl_info *request_info = NULL;
		request_info = GET_STAGE_TRANSACTION_INFO(stage);
		DA_LOG_CRITICAL(Default, "Download Failed -Return = %d", ret);
		if (request_info) {
			etag = GET_REQUEST_HTTP_HDR_ETAG(request_info);
			send_client_finished_info(slot_id, GET_DL_ID(slot_id),
					DA_NULL, etag, ret, get_http_status(slot_id));
		}
		destroy_download_info(slot_id);
	}

	pthread_cleanup_pop(0);
	DA_LOG_CRITICAL(Thread, "==thread_start_download - EXIT==");
	pthread_exit((void *)NULL);
	return DA_NULL;
}

da_result_t __download_content(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	download_state_t download_state = 0;
	da_bool_t isDownloadComplete = DA_FALSE;
	int slot_id = DA_INVALID_ID;

	DA_LOG_FUNC_LOGV(Default);

	slot_id = GET_STAGE_DL_ID(stage);
	CHANGE_DOWNLOAD_STATE(DOWNLOAD_STATE_NEW_DOWNLOAD, stage);

	do {
		stage = GET_DL_CURRENT_STAGE(slot_id);
		_da_thread_mutex_lock (&mutex_download_state[GET_STAGE_DL_ID(stage)]);
		download_state = GET_DL_STATE_ON_STAGE(stage);
		DA_LOG_VERBOSE(Default, "download_state to - [%d] ", download_state);
		_da_thread_mutex_unlock (&mutex_download_state[GET_STAGE_DL_ID(stage)]);

		switch(download_state) {
		case DOWNLOAD_STATE_NEW_DOWNLOAD:
			ret = requesting_download(stage);

			_da_thread_mutex_lock (&mutex_download_state[GET_STAGE_DL_ID(stage)]);
			download_state = GET_DL_STATE_ON_STAGE(stage);
			_da_thread_mutex_unlock (&mutex_download_state[GET_STAGE_DL_ID(stage)]);
			if (download_state == DOWNLOAD_STATE_CANCELED ||
					download_state == DOWNLOAD_STATE_ABORTED ||
					download_state == DOWNLOAD_STATE_PAUSED) {
				break;
			} else {
				if (DA_RESULT_OK == ret) {
					ret = handle_after_download(stage);
				}
			}
			break;
		default:
			isDownloadComplete = DA_TRUE;
			break;
		}
	}while ((DA_RESULT_OK == ret) && (DA_FALSE == isDownloadComplete));

	return ret;
}
