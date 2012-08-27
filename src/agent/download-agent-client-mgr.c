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
 * @file		download-agent-client-mgr.c
 * @brief		client manager module for notifying download ststus information
 * @author		Keunsoon Lee(keunsoon.lee@samsung.com)
 * @author		Jungki Kwak(jungki.kwak@samsung.com)
 ***/

#include <unistd.h>

#include "download-agent-client-mgr.h"
#include "download-agent-debug.h"
#include "download-agent-utils.h"
#include "download-agent-file.h"

#define IS_CLIENT_Q_HAVING_DATA(QUEUE)	(QUEUE->having_data)

static client_app_mgr_t client_app_mgr;

static da_result_t __launch_client_thread(void);
static void *__thread_for_client_noti(void *data);
void __thread_clean_up_handler_for_client_thread(void *arg);
static void __pop_client_noti(client_noti_t **out_client_noti);

void __client_q_goto_sleep_without_lock(void);
void __client_q_wake_up_without_lock(void);
void destroy_client_noti(client_noti_t *client_noti);

da_result_t init_client_app_mgr()
{
	DA_LOG_FUNC_START(ClientNoti);

	if(client_app_mgr.is_init)
		return DA_RESULT_OK;

	client_app_mgr.is_init = DA_TRUE;
	client_app_mgr.client_app_info.is_using = DA_FALSE;
	client_app_mgr.client_app_info.is_manual_download = DA_FALSE;
	client_app_mgr.client_app_info.client_user_agent = DA_NULL;
	client_app_mgr.is_thread_init = DA_FALSE;

	return DA_RESULT_OK;
}

da_bool_t is_client_app_mgr_init(void)
{
	return client_app_mgr.is_init;
}

da_bool_t is_client_app_mgr_manual_download(void)
{
	return client_app_mgr.is_manual_download;
}

da_result_t reg_client_app(
		da_client_cb_t *da_client_callback,
		da_download_managing_method download_method
		)
{
	da_result_t ret = DA_RESULT_OK;
	client_queue_t *queue = DA_NULL;
	client_noti_t *client_noti = DA_NULL;

	DA_LOG_FUNC_START(ClientNoti);

	if (client_app_mgr.client_app_info.is_using)
		return DA_ERR_CLIENT_IS_ALREADY_REGISTERED;

	client_app_mgr.client_app_info.is_using = DA_TRUE;
	if (download_method == DA_DOWNLOAD_MANAGING_METHOD_MANUAL)
		client_app_mgr.client_app_info.is_manual_download = DA_TRUE;
	else
		client_app_mgr.client_app_info.is_manual_download = DA_FALSE;

	memset(&(client_app_mgr.client_app_info.client_callback),
			0, sizeof(da_client_cb_t));
	memcpy(&(client_app_mgr.client_app_info.client_callback),
			da_client_callback, sizeof(da_client_cb_t));

	_da_thread_mutex_init(&(client_app_mgr.mutex_client_mgr), DA_NULL);

	/* If some noti is existed at queue, delete all */
	do {
		__pop_client_noti(&client_noti);
		destroy_client_noti(client_noti);
	} while(client_noti != DA_NULL);

	queue = &(client_app_mgr.client_queue);
	DA_LOG_VERBOSE(ClientNoti, "client queue = %p", queue);
	_da_thread_mutex_init(&(queue->mutex_client_queue), DA_NULL);
	_da_thread_cond_init(&(queue->cond_client_queue), DA_NULL);

	ret = __launch_client_thread();

	return ret;
}

da_result_t dereg_client_app(void)
{
	client_noti_t *client_noti = DA_NULL;

	DA_LOG_FUNC_START(ClientNoti);

	client_noti = (client_noti_t *)calloc(1, sizeof(client_noti_t));
	if (!client_noti) {
		DA_LOG_ERR(ClientNoti, "calloc fail");
		return DA_ERR_FAIL_TO_MEMALLOC;
	}

	client_noti->download_id = DA_INVALID_ID;
	client_noti->noti_type = Q_CLIENT_NOTI_TYPE_TERMINATE;
	client_noti->next = DA_NULL;

	_da_thread_mutex_lock(&(client_app_mgr.mutex_client_mgr));
	if (client_app_mgr.is_thread_init != DA_TRUE) {
		DA_LOG_CRITICAL(ClientNoti, "try to cancel client mgr thread id[%lu]", client_app_mgr.thread_id);
		if (pthread_cancel(client_app_mgr.thread_id) < 0) {
			DA_LOG_ERR(ClientNoti, "cancel thread is failed!!!");
		}
	} else {
		void *t_return = NULL;
		DA_LOG_VERBOSE(ClientNoti, "pushing Q_CLIENT_NOTI_TYPE_TERMINATE");
		push_client_noti(client_noti);
		DA_LOG_CRITICAL(Thread, "===try to join client mgr thread id[%lu]===", client_app_mgr.thread_id);
		if (pthread_join(client_app_mgr.thread_id, &t_return) < 0) {
			DA_LOG_ERR(Thread, "join client thread is failed!!!");
		}
		DA_LOG_CRITICAL(Thread, "===thread join return[%d]===", (char*)t_return);
	}
	_da_thread_mutex_unlock(&(client_app_mgr.mutex_client_mgr));

	/* ToDo: This clean up should be done at the end of client_thread. */
	client_app_mgr.client_app_info.is_using= DA_FALSE;
	client_app_mgr.client_app_info.is_manual_download = DA_FALSE;
	if(client_app_mgr.client_app_info.client_user_agent) {
		free(client_app_mgr.client_app_info.client_user_agent);
		client_app_mgr.client_app_info.client_user_agent = DA_NULL;
	}
	_da_thread_mutex_lock(&(client_app_mgr.mutex_client_mgr));
	client_app_mgr.is_thread_init = DA_FALSE;
	_da_thread_mutex_unlock(&(client_app_mgr.mutex_client_mgr));
	_da_thread_mutex_destroy(&(client_app_mgr.mutex_client_mgr));
	return DA_RESULT_OK;
}

da_result_t  send_client_da_state (int download_id, da_state state, int err)
{
	client_noti_t *client_noti = DA_NULL;
	user_notify_info_t *send_state_info = DA_NULL;
	da_state cur_da_state;

	DA_LOG_FUNC_START(ClientNoti);

	DA_LOG_VERBOSE(ClientNoti, "da_state[%s], download_id[%d]", print_dl_state(state), download_id);

	if (!is_valid_dl_ID(download_id)) {
		DA_LOG_ERR(ClientNoti, "Download ID is not valid");
		/* Returning DA_RESULT_OK if download_id is not valid,
		 * because sending da_state should not effect to download flow. */
		return DA_RESULT_OK;
	}

	if (state_watcher_need_redirect_Q(download_id)) {
		state_watcher_redirect_state(download_id, state, err);
		return DA_RESULT_OK;
	}

	cur_da_state = GET_DL_DA_STATE(download_id);

	if ((DA_STATE_SUSPENDED != state) && (cur_da_state == state)) {
		DA_LOG(ClientNoti, "inserting da_state is same with current da_state! Not inserting! inserting: %d, cur : %d", state, cur_da_state);
		return DA_RESULT_OK;
	}

	GET_DL_DA_STATE(download_id) = state;
	DA_LOG_VERBOSE(ClientNoti, "change da_state to %d", state);

	client_noti = (client_noti_t *)calloc(1, sizeof(client_noti_t));
	if (!client_noti) {
		DA_LOG_ERR(ClientNoti, "calloc fail");
		return DA_ERR_FAIL_TO_MEMALLOC;
	}

	client_noti->download_id = download_id;
	client_noti->user_data = GET_DL_USER_DATA(download_id);
	client_noti->noti_type = Q_CLIENT_NOTI_TYPE_SEND_STATE;
	client_noti->next = DA_NULL;

	send_state_info = (user_notify_info_t *)&(client_noti->type.da_state_info);
	send_state_info->da_dl_req_id = GET_DL_REQ_ID(download_id);
	send_state_info->state = state;
	send_state_info->err = err;

	DA_LOG(ClientNoti, "pushing da_state=%d, download_id=%d, err=%d, dl_req_id=%d",
			state, download_id, err, GET_DL_REQ_ID(download_id));

	push_client_noti(client_noti);

	return DA_RESULT_OK;
}

da_result_t  send_client_update_downloading_info (
		int download_id,
		int dl_req_id,
		unsigned long int total_received_size,
		char *saved_path
		)
{
	client_noti_t *client_noti = DA_NULL;
	user_downloading_info_t *downloading_info = DA_NULL;

	DA_LOG_FUNC_START(ClientNoti);

	if (!is_valid_dl_ID(download_id)) {
		DA_LOG_ERR(ClientNoti, "Download ID is not valid");
		return DA_ERR_INVALID_DL_REQ_ID;
	}

	client_noti = (client_noti_t *)calloc(1, sizeof(client_noti_t));
	if (!client_noti) {
		DA_LOG_ERR(ClientNoti, "calloc fail");
		return DA_ERR_FAIL_TO_MEMALLOC;
	}

	client_noti->download_id = download_id;
	client_noti->user_data = GET_DL_USER_DATA(download_id);
	client_noti->noti_type = Q_CLIENT_NOTI_TYPE_UPDATE_DOWNLOADING_INFO;
	client_noti->next = DA_NULL;

	downloading_info = (user_downloading_info_t *)&(client_noti->type.update_downloading_info);
	downloading_info->da_dl_req_id = dl_req_id;
	downloading_info->total_received_size = total_received_size;

	/* These strings MUST be copied to detach __thread_for_client_noti from download_info */
	if (saved_path)
		downloading_info->saved_path = strdup(saved_path);
	DA_LOG(ClientNoti, "pushing received_size=%lu, download_id=%d, dl_req_id=%d",
			total_received_size, download_id, dl_req_id);

	push_client_noti(client_noti);

	return DA_RESULT_OK;
}

da_result_t  send_client_update_dl_info (
		int download_id,
		int dl_req_id,
		char *file_type,
		unsigned long int file_size,
		char *tmp_saved_path,
		char *http_response_header,
		char *http_chunked_data
		)
{
	client_noti_t *client_noti = DA_NULL;
	user_download_info_t *update_dl_info = DA_NULL;

	DA_LOG_FUNC_START(ClientNoti);

	if (!is_valid_dl_ID(download_id)) {
		DA_LOG_ERR(ClientNoti, "Download ID is not valid");
		return DA_ERR_INVALID_DL_REQ_ID;
	}

	client_noti = (client_noti_t *)calloc(1, sizeof(client_noti_t));
	if (!client_noti) {
		DA_LOG_ERR(ClientNoti, "calloc fail");
		return DA_ERR_FAIL_TO_MEMALLOC;
	}

	client_noti->download_id = download_id;
	client_noti->user_data = GET_DL_USER_DATA(download_id);
	client_noti->noti_type = Q_CLIENT_NOTI_TYPE_UPDATE_DL_INFO;
	client_noti->next = DA_NULL;

	update_dl_info = (user_download_info_t *)&(client_noti->type.update_dl_info);
	update_dl_info->da_dl_req_id = dl_req_id;
	update_dl_info->file_size = file_size;

	/* These strings MUST be copied to detach __thread_for_client_noti from download_info */
	if (file_type)
		update_dl_info->file_type = strdup(file_type);

	if (tmp_saved_path)
		update_dl_info->tmp_saved_path = strdup(tmp_saved_path);

	if (http_response_header) {
		update_dl_info->http_response_header = strdup(http_response_header);
	}
	if (http_chunked_data) {
		update_dl_info->http_chunked_data = calloc (1, file_size);
		if (update_dl_info->http_chunked_data)
			memcpy(update_dl_info->http_chunked_data, http_chunked_data,
				file_size);
	}
	DA_LOG(ClientNoti, "pushing file_size=%lu, download_id=%d, dl_req_id=%d",
			file_size, download_id, dl_req_id);

	push_client_noti(client_noti);

	return DA_RESULT_OK;
}

da_result_t  __launch_client_thread(void)
{
	pthread_t thread_id = DA_NULL;

	DA_LOG_FUNC_START(Thread);

	if (pthread_create(&thread_id,DA_NULL,__thread_for_client_noti,DA_NULL) < 0) {
		DA_LOG_ERR(Thread, "making thread failed..");
		return DA_ERR_FAIL_TO_CREATE_THREAD;
	}
	DA_LOG(Thread, "client mgr thread id[%d]", thread_id);
	client_app_mgr.thread_id = thread_id;
	return DA_RESULT_OK;
}

void destroy_client_noti(client_noti_t *client_noti)
{
	if (client_noti) {
		if (client_noti->noti_type == Q_CLIENT_NOTI_TYPE_UPDATE_DL_INFO) {
			user_download_info_t *update_dl_info = DA_NULL;
			update_dl_info = (user_download_info_t*)&(client_noti->type.update_dl_info);
			if (update_dl_info->file_type) {
				free(update_dl_info->file_type);
				update_dl_info->file_type = DA_NULL;
			}
			if (update_dl_info->tmp_saved_path) {
				free(update_dl_info->tmp_saved_path);
				update_dl_info->tmp_saved_path = DA_NULL;
			}
			if (update_dl_info->http_response_header) {
				free(update_dl_info->http_response_header);
				update_dl_info->http_response_header = DA_NULL;
			}
			if (update_dl_info->http_chunked_data) {
				free(update_dl_info->http_chunked_data);
				update_dl_info->http_chunked_data = DA_NULL;
			}
		} else if (client_noti->noti_type ==
				Q_CLIENT_NOTI_TYPE_UPDATE_DOWNLOADING_INFO) {
			user_downloading_info_t *downloading_info = DA_NULL;
			downloading_info = (user_downloading_info_t*)&(client_noti->type.update_downloading_info);
			if (downloading_info->saved_path) {
				free(downloading_info->saved_path);
				downloading_info->saved_path = DA_NULL;
			}
		}
		free(client_noti);
	}
}


void push_client_noti(client_noti_t *client_noti)
{
	client_queue_t *queue = DA_NULL;
	client_noti_t *head = DA_NULL;
	client_noti_t *pre = DA_NULL;
	client_noti_t *cur = DA_NULL;

	/* DA_LOG_FUNC_START(ClientNoti); */

	if(!is_this_client_available())	{
		DA_LOG_ERR(ClientNoti, "invalid client");
		return;
	}
	queue = &(client_app_mgr.client_queue);
	_da_thread_mutex_lock (&(queue->mutex_client_queue));

	head = queue->client_q_head;
	if (!head) {
		queue->client_q_head = client_noti;
	} else {
		cur = head;
		while (cur->next) {
			pre = cur;
			cur = pre->next;
		}

		if (DA_TRUE == is_this_client_manual_download_type()) {
			cur->next = client_noti;
		} else if (client_noti->noti_type == Q_CLIENT_NOTI_TYPE_SEND_STATE) {
			cur->next = client_noti;
		} else {
			if (cur->noti_type == Q_CLIENT_NOTI_TYPE_SEND_STATE) {
				cur->next = client_noti;
			} else if (cur->noti_type == Q_CLIENT_NOTI_TYPE_UPDATE_DOWNLOADING_INFO) {
				/* For UI performance. If the update noti info is existed at queue,
				   replace it with new update noti info */
				if (cur->download_id == client_noti->download_id) {
					/* DA_LOG(ClientNoti, "exchange queue's tail and pushing item"); */
					if (pre == DA_NULL)
						queue->client_q_head = client_noti;
					else
						pre->next = client_noti;
					destroy_client_noti(cur);
				} else {
					cur->next = client_noti;
				}
			}
		}
	}

	queue->having_data = DA_TRUE;

	__client_q_wake_up_without_lock();

	_da_thread_mutex_unlock (&(queue->mutex_client_queue));
}

void __pop_client_noti(client_noti_t **out_client_noti)
{
	client_queue_t *queue = DA_NULL;

	/* DA_LOG_FUNC_START(ClientNoti); */

	queue = &(client_app_mgr.client_queue);

	_da_thread_mutex_lock (&(queue->mutex_client_queue));

	if (queue->client_q_head) {
		*out_client_noti = queue->client_q_head;
		queue->client_q_head = queue->client_q_head->next;
	} else {
		*out_client_noti = DA_NULL;
	}

	if (queue->client_q_head == DA_NULL) {
		queue->having_data = DA_FALSE;
	}

	_da_thread_mutex_unlock (&(queue->mutex_client_queue));
}

void __client_q_goto_sleep_without_lock(void)
{
	client_queue_t *queue = DA_NULL;

	/* DA_LOG_FUNC_START(ClientNoti); */

	queue = &(client_app_mgr.client_queue);
	_da_thread_cond_wait(&(queue->cond_client_queue), &(queue->mutex_client_queue));
}

void __client_q_wake_up_without_lock(void)
{
	client_queue_t *queue = DA_NULL;

	/* DA_LOG_FUNC_START(ClientNoti); */

	queue = &(client_app_mgr.client_queue);
	_da_thread_cond_signal(&(queue->cond_client_queue));
}

void __thread_clean_up_handler_for_client_thread(void *arg)
{
	DA_LOG_CRITICAL(Thread, "cleanup for thread id = %d", pthread_self());
}

static void *__thread_for_client_noti(void *data)
{
	da_result_t  ret = DA_RESULT_OK;
	da_bool_t need_wait = DA_TRUE;
	client_queue_t *queue = DA_NULL;
	client_noti_t *client_noti = DA_NULL;

	DA_LOG_FUNC_START(Thread);

	_da_thread_mutex_lock(&(client_app_mgr.mutex_client_mgr));
	client_app_mgr.is_thread_init = DA_TRUE;
	_da_thread_mutex_unlock(&(client_app_mgr.mutex_client_mgr));

	queue = &(client_app_mgr.client_queue);
	DA_LOG(ClientNoti, "client queue = %p", queue);

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, DA_NULL);
	pthread_cleanup_push(__thread_clean_up_handler_for_client_thread, (void *)DA_NULL);

	do {
		_da_thread_mutex_lock(&(queue->mutex_client_queue));
		if (DA_FALSE == IS_CLIENT_Q_HAVING_DATA(queue)) {
			DA_LOG(Thread, "Sleep @ thread_for_client_noti!");
			__client_q_goto_sleep_without_lock();
			DA_LOG(Thread, "Woke up @ thread_for_client_noti");
		}
		_da_thread_mutex_unlock(&(queue->mutex_client_queue));

		do {
			__pop_client_noti(&client_noti);
			if (client_noti == DA_NULL) {
				DA_LOG_ERR(ClientNoti, "There is no data on client queue!");
				ret = DA_ERR_INVALID_STATE;
				need_wait = DA_FALSE;
			} else {
				switch (client_noti->noti_type) {
				case Q_CLIENT_NOTI_TYPE_UPDATE_DL_INFO:
				{
					user_download_info_t *update_dl_info = DA_NULL;;
					update_dl_info = (user_download_info_t*)(&(client_noti->type.update_dl_info));
					if (client_app_mgr.client_app_info.client_callback.update_dl_info_cb) {
						client_app_mgr.client_app_info.client_callback.update_dl_info_cb(update_dl_info, client_noti->user_data);
						DA_LOG(ClientNoti, "Update download info for download_id=%d, dl_req_id=%d, received size=%lu- DONE",
								client_noti->download_id,
								update_dl_info->da_dl_req_id,
								update_dl_info->file_size
								);
					}
				}
				break;
				case Q_CLIENT_NOTI_TYPE_UPDATE_DOWNLOADING_INFO:
				{
					user_downloading_info_t *downloading_info = DA_NULL;;
					downloading_info = (user_downloading_info_t*)(&(client_noti->type.update_downloading_info));
					if (client_app_mgr.client_app_info.client_callback.update_progress_info_cb) {
						client_app_mgr.client_app_info.client_callback.update_progress_info_cb(downloading_info, client_noti->user_data);
						DA_LOG(ClientNoti, "Update downloading info for download_id=%d, dl_req_id=%d, received size=%lu - DONE",
								client_noti->download_id,
								downloading_info->da_dl_req_id,
								downloading_info->total_received_size);
					}
				}
				break;
				case Q_CLIENT_NOTI_TYPE_SEND_STATE:
				{
					user_notify_info_t *da_state_info = DA_NULL;
					da_state_info = (user_notify_info_t *)(&(client_noti->type.da_state_info));

					if (client_app_mgr.client_app_info.client_callback.user_noti_cb) {
						DA_LOG(ClientNoti, "User Noti info for download_id=%d, dl_req_id=%d, da_state=%d, err=%d",
								client_noti->download_id,
								da_state_info->da_dl_req_id, da_state_info->state,
								da_state_info->err);
						client_app_mgr.client_app_info.client_callback.user_noti_cb(da_state_info, client_noti->user_data);
						DA_LOG(ClientNoti, "User Noti info for download_id=%d, dl_req_id=%d, da_state=%d, err=%d - DONE",
								client_noti->download_id,
								da_state_info->da_dl_req_id,
								da_state_info->state, da_state_info->err);
					}
				}
				break;
				case Q_CLIENT_NOTI_TYPE_TERMINATE:
					DA_LOG_CRITICAL(ClientNoti, "Q_CLIENT_NOTI_TYPE_TERMINATE");
					need_wait = DA_FALSE;
					break;
				}
				destroy_client_noti(client_noti);
			}

			if(DA_TRUE == need_wait) {
				_da_thread_mutex_lock(&(queue->mutex_client_queue));
				if (DA_FALSE == IS_CLIENT_Q_HAVING_DATA(queue)) {
					_da_thread_mutex_unlock (&(queue->mutex_client_queue));
					break;
				} else {
					_da_thread_mutex_unlock (&(queue->mutex_client_queue));
				}
			} else {
				break;
			}
		} while (1);
	} while (DA_TRUE == need_wait);

	_da_thread_mutex_destroy(&(queue->mutex_client_queue));
	_da_thread_cond_destroy(&(queue->cond_client_queue));

	pthread_cleanup_pop(0);
	DA_LOG_CRITICAL(Thread, "=====thread_for_client_noti- EXIT=====");
	pthread_exit((void *)NULL);
	return DA_NULL;
}

da_bool_t is_this_client_available(void)
{
	return client_app_mgr.client_app_info.is_using;
}

da_bool_t is_this_client_manual_download_type(void)
{
	return client_app_mgr.client_app_info.is_manual_download;
}

da_result_t  get_client_download_path(char **out_path)
{
	if (!out_path) {
		DA_LOG_ERR(ClientNoti, "DA_ERR_INVALID_ARGUMENT");
		return DA_ERR_INVALID_ARGUMENT;
	}

	/* change the directory policy. it doesn't clean the temp direcoty when deinitializing */
	*out_path = strdup(DA_DEFAULT_TMP_FILE_DIR_PATH);
	DA_LOG_VERBOSE(ClientNoti, "client download path = [%s]", *out_path);

	return DA_RESULT_OK;
}

char *get_client_user_agent_string(void)
{
	if (!client_app_mgr.is_init)
		return DA_NULL;

	return client_app_mgr.client_app_info.client_user_agent;
}
