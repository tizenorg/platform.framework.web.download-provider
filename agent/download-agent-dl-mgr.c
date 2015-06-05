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
#include <sys/syscall.h>
#include <signal.h>

#ifdef _ENABLE_SYS_RESOURCE
#include "resourced.h"
#endif

#include "download-agent-dl-mgr.h"
#include "download-agent-dl-info.h"
#include "download-agent-http-mgr.h"

void __thread_clean_up_handler_for_start_download(void *arg)
{
	DA_LOGI("cleanup for thread id[%lu]", pthread_self());
}

da_ret_t __download_content(da_info_t *da_info)
{
	da_ret_t ret = DA_RESULT_OK;

	DA_LOGV("");
	if (!da_info) {
		DA_LOGE("NULL CHECK!: da_info");
		ret = DA_ERR_INVALID_ARGUMENT;
		return ret;
	}

	ret = request_http_download(da_info);
	return ret;
}


static void *__thread_start_download(void *data)
{
	da_info_t *da_info = DA_NULL;
	req_info_t *req_info = DA_NULL;
	int da_id = DA_INVALID_ID;

//	DA_LOGV("");

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, DA_NULL);

	da_info = (da_info_t *)data;
	NULL_CHECK_RET_OPT(da_info, DA_NULL);
	req_info = da_info->req_info;
	NULL_CHECK_RET_OPT(req_info, DA_NULL);

	da_id = da_info->da_id;
	pthread_cleanup_push(__thread_clean_up_handler_for_start_download, DA_NULL);
#ifdef _ENABLE_SYS_RESOURCE
	if (req_info->pkg_name) {
		pid_t tid = (pid_t) syscall(SYS_gettid);
		da_info->tid = (pid_t) syscall(SYS_gettid);
		DA_SECURE_LOGI("pkg_name[%s] threadid[%lu]",
				req_info->pkg_name,pthread_self());
		if (join_app_performance(req_info->pkg_name, tid) !=
				RESOURCED_ERROR_OK) {
			DA_LOGE("Can not put app to network performance id[%d]", da_id);
		}
	}
#endif
	__download_content(da_info);
	destroy_da_info(da_id);
	pthread_cleanup_pop(0);
	DA_LOGI("=====EXIT thread : da_id[%d]=====", da_id);
	pthread_exit((void *)DA_NULL);
	return DA_NULL;
}

da_ret_t start_download(da_info_t *da_info)
{
	da_ret_t ret = DA_RESULT_OK;
	pthread_attr_t thread_attr;
	pthread_t tid;
	if (pthread_attr_init(&thread_attr) != 0) {
		ret = DA_ERR_FAIL_TO_CREATE_THREAD;
		goto ERR;
	}

	if (pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED) != 0) {
		ret = DA_ERR_FAIL_TO_CREATE_THREAD;
		goto ERR;
	}

	if (pthread_create(&(tid), &thread_attr,
			__thread_start_download, da_info) < 0) {
		DA_LOGE("Fail to make thread:id[%d]", da_info->da_id);
		ret = DA_ERR_FAIL_TO_CREATE_THREAD;
	} else {
		if (tid < 1) {
			DA_LOGE("The thread start is failed before calling this");
// When http resource is leaked, the thread ID is initialized at error handling section of thread_start_download()
// Because the thread ID is initialized, the ptrhead_detach should not be called. This is something like timing issue between threads.
// thread info and basic_dl_input is freed at thread_start_download(). And it should not returns error code in this case.
			ret = DA_ERR_FAIL_TO_CREATE_THREAD;
			goto ERR;
		}
	}
	da_info->thread_id = tid;
	DA_LOGI("Thread create:thread id[%lu]", da_info->thread_id);
ERR:
	if (DA_RESULT_OK != ret) {
		destroy_da_info(da_info->da_id);
	}
	return ret;
}

da_ret_t cancel_download(int dl_id, da_bool_t is_enable_cb)
{
	da_ret_t ret = DA_RESULT_OK;
	da_info_t *da_info = DA_NULL;

	DA_LOGV("");

	ret = get_da_info_with_da_id(dl_id, &da_info);
	if (ret != DA_RESULT_OK || !da_info) {
		return DA_ERR_INVALID_ARGUMENT;
	}
	da_info->is_cb_update = is_enable_cb;
	ret = request_to_cancel_http_download(da_info);
	if (ret != DA_RESULT_OK)
		goto ERR;
	DA_LOGI("Download cancel Successful for download id[%d]", da_info->da_id);

ERR:
	return ret;
}

da_ret_t suspend_download(int dl_id, da_bool_t is_enable_cb)
{
	da_ret_t ret = DA_RESULT_OK;
	da_info_t *da_info = DA_NULL;

	DA_LOGV("");

	ret = get_da_info_with_da_id(dl_id, &da_info);
	if (ret != DA_RESULT_OK || !da_info) {
		return DA_ERR_INVALID_ARGUMENT;
	}
	da_info->is_cb_update = is_enable_cb;
	ret = request_to_suspend_http_download(da_info);
	if (ret != DA_RESULT_OK)
		goto ERR;
	DA_LOGV("Download Suspend Successful for download id[%d]", da_info->da_id);
ERR:
	return ret;

}

da_ret_t resume_download(int dl_id)
{
	da_ret_t ret = DA_RESULT_OK;
	da_info_t *da_info = DA_NULL;

	DA_LOGV("");

	ret = get_da_info_with_da_id(dl_id, &da_info);
	if (ret != DA_RESULT_OK || !da_info) {
		return DA_ERR_INVALID_ARGUMENT;
	}
	da_info->is_cb_update = DA_TRUE;
	ret = request_to_resume_http_download(da_info);
	if (ret != DA_RESULT_OK)
		goto ERR;
	DA_LOGV("Download Resume Successful for download id[%d]", da_info->da_id);
ERR:
	return ret;
}

