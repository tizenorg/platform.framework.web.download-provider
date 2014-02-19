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
	DA_LOG_FUNC_LOGV(ClientNoti);

	if(client_app_mgr.is_init)
		return DA_RESULT_OK;

	client_app_mgr.is_init = DA_TRUE;
	client_app_mgr.client_app_info.client_user_agent = DA_NULL;
	client_app_mgr.is_thread_init = DA_FALSE;
	client_app_mgr.thread_id = 0;

	return DA_RESULT_OK;
}

da_bool_t is_client_app_mgr_init(void)
{
	return client_app_mgr.is_init;
}

da_result_t reg_client_app(
		da_client_cb_t *da_client_callback)
{
	da_result_t ret = DA_RESULT_OK;
	client_queue_t *queue = DA_NULL;
	client_noti_t *client_noti = DA_NULL;

	DA_LOG_FUNC_LOGD(ClientNoti);

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

	DA_LOG_FUNC_LOGV(ClientNoti);

	client_noti = (client_noti_t *)calloc(1, sizeof(client_noti_t));
	if (!client_noti) {
		DA_LOG_ERR(ClientNoti, "calloc fail");
		return DA_ERR_FAIL_TO_MEMALLOC;
	}

	client_noti->slot_id = DA_INVALID_ID;
	client_noti->noti_type = Q_CLIENT_NOTI_TYPE_TERMINATE;
	client_noti->next = DA_NULL;

	_da_thread_mutex_lock(&(client_app_mgr.mutex_client_mgr));
	if (client_app_mgr.is_thread_init != DA_TRUE) {
		DA_LOG_CRITICAL(ClientNoti, "try to cancel client mgr thread id[%lu]",
				client_app_mgr.thread_id);
		if (client_app_mgr.thread_id &&
				pthread_cancel(client_app_mgr.thread_id) < 0) {
			DA_LOG_ERR(ClientNoti, "cancel thread is failed!!!");
		}
		free(client_noti);
	} else {
		void *t_return = NULL;
		DA_LOG_VERBOSE(ClientNoti, "pushing Q_CLIENT_NOTI_TYPE_TERMINATE");
		push_client_noti(client_noti);
		DA_LOG_DEBUG(Thread, "===try to join client mgr thread id[%lu]===",
				client_app_mgr.thread_id);
		if (client_app_mgr.thread_id &&
				pthread_join(client_app_mgr.thread_id, &t_return) < 0) {
			DA_LOG_ERR(Thread, "join client thread is failed!!!");
		}
		DA_LOG_DEBUG(Thread, "===thread join return[%d]===", (char*)t_return);
	}
	_da_thread_mutex_unlock(&(client_app_mgr.mutex_client_mgr));

	/* ToDo: This clean up should be done at the end of client_thread. */
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

da_result_t send_client_paused_info(int slot_id)
{
	client_noti_t *client_noti = DA_NULL;
	user_paused_info_t *paused_info = DA_NULL;
	download_state_t state = GET_DL_STATE_ON_ID(slot_id);

	DA_LOG_FUNC_LOGD(ClientNoti);

	if (!GET_DL_ENABLE_PAUSE_UPDATE(slot_id)) {
		DA_LOG(ClientNoti, "Do not call pause cb");
		return DA_RESULT_OK;
	}
	if (!is_valid_slot_id(slot_id)) {
		DA_LOG_ERR(ClientNoti, "Download ID is not valid");
		return DA_RESULT_OK;
	}

	DA_LOG_VERBOSE(ClientNoti, "slot_id[%d]", slot_id);
	if ((DOWNLOAD_STATE_PAUSED != state)) {
		DA_LOG(ClientNoti, "The state is not paused. state:%d", state);
		return DA_ERR_INVALID_STATE;
	}

	client_noti = (client_noti_t *)calloc(1, sizeof(client_noti_t));
	if (!client_noti) {
		DA_LOG_ERR(ClientNoti, "calloc fail");
		return DA_ERR_FAIL_TO_MEMALLOC;
	}

	client_noti->slot_id = slot_id;
	client_noti->user_data = GET_DL_USER_DATA(slot_id);
	client_noti->noti_type = Q_CLIENT_NOTI_TYPE_PAUSED_INFO;
	client_noti->next = DA_NULL;

	paused_info = (user_paused_info_t *)&(client_noti->type.paused_info);
	paused_info->download_id= GET_DL_ID(slot_id);
	DA_LOG(ClientNoti, "pushing paused info. slot_id=%d, dl_id=%d",
			slot_id, GET_DL_ID(slot_id));

	push_client_noti(client_noti);

	return DA_RESULT_OK;
}

da_result_t  send_client_update_progress_info (
		int slot_id,
		int dl_id,
		unsigned long int received_size
		)
{
	client_noti_t *client_noti = DA_NULL;
	user_progress_info_t *progress_info = DA_NULL;

	DA_LOG_FUNC_LOGV(ClientNoti);

	if (!is_valid_slot_id(slot_id)) {
		DA_LOG_ERR(ClientNoti, "Download ID is not valid");
		return DA_ERR_INVALID_DL_REQ_ID;
	}

	client_noti = (client_noti_t *)calloc(1, sizeof(client_noti_t));
	if (!client_noti) {
		DA_LOG_ERR(ClientNoti, "calloc fail");
		return DA_ERR_FAIL_TO_MEMALLOC;
	}

	client_noti->slot_id = slot_id;
	client_noti->user_data = GET_DL_USER_DATA(slot_id);
	client_noti->noti_type = Q_CLIENT_NOTI_TYPE_PROGRESS_INFO;
	client_noti->next = DA_NULL;

	progress_info = (user_progress_info_t *)&(client_noti->type.update_progress_info);
	progress_info->download_id= dl_id;
	progress_info->received_size = received_size;

	DA_LOG_VERBOSE(ClientNoti, "pushing received_size=%lu, slot_id=%d, dl_id=%d",
			received_size, slot_id, dl_id);

	push_client_noti(client_noti);

	return DA_RESULT_OK;
}

da_result_t  send_client_update_dl_info (
		int slot_id,
		int dl_id,
		char *file_type,
		unsigned long int file_size,
		char *tmp_saved_path,
		char *pure_file_name,
		char *etag,
		char *extension)
{
	client_noti_t *client_noti = DA_NULL;
	user_download_info_t *update_dl_info = DA_NULL;
	int len = 0;

	DA_LOG_FUNC_LOGV(ClientNoti);

	if (!is_valid_slot_id(slot_id)) {
		DA_LOG_ERR(ClientNoti, "Download ID is not valid");
		return DA_ERR_INVALID_DL_REQ_ID;
	}

	client_noti = (client_noti_t *)calloc(1, sizeof(client_noti_t));
	if (!client_noti) {
		DA_LOG_ERR(ClientNoti, "calloc fail");
		return DA_ERR_FAIL_TO_MEMALLOC;
	}

	client_noti->slot_id = slot_id;
	client_noti->user_data = GET_DL_USER_DATA(slot_id);
	client_noti->noti_type = Q_CLIENT_NOTI_TYPE_STARTED_INFO;
	client_noti->next = DA_NULL;

	update_dl_info = (user_download_info_t *)&(client_noti->type.update_dl_info);
	update_dl_info->download_id = dl_id;
	update_dl_info->file_size = file_size;
	if (pure_file_name && extension) {
		len = strlen(pure_file_name) + strlen(extension) + 1;
		update_dl_info->content_name = (char *)calloc(len + 1, sizeof(char));
		if (!update_dl_info->content_name) {
			free(client_noti);
			return DA_ERR_FAIL_TO_MEMALLOC;
		}
		snprintf(update_dl_info->content_name, len + 1, "%s.%s",
				pure_file_name, extension);
	}

	/* These strings MUST be copied to detach __thread_for_client_noti from download_info */
	if (file_type)
		update_dl_info->file_type = strdup(file_type);

	if (tmp_saved_path)
		update_dl_info->tmp_saved_path = strdup(tmp_saved_path);

	if (etag)
		update_dl_info->etag = strdup(etag);
	DA_LOG_DEBUG(ClientNoti, "pushing slot_id=%d, dl_id=%d", slot_id, dl_id);

	push_client_noti(client_noti);

	return DA_RESULT_OK;
}

da_result_t  send_client_finished_info (
		int slot_id,
		int dl_id,
		char *saved_path,
		char *etag,
		int error,
		int http_status
		)
{
	client_noti_t *client_noti = DA_NULL;
	user_finished_info_t *finished_info = DA_NULL;

	DA_LOG_FUNC_LOGV(ClientNoti);

	if (!is_valid_slot_id(slot_id)) {
		DA_LOG_ERR(ClientNoti, "Download ID is not valid");
		return DA_ERR_INVALID_DL_REQ_ID;
	}

	client_noti = (client_noti_t *)calloc(1, sizeof(client_noti_t));
	if (!client_noti) {
		DA_LOG_ERR(ClientNoti, "calloc fail");
		return DA_ERR_FAIL_TO_MEMALLOC;
	}

	client_noti->slot_id = slot_id;
	client_noti->user_data = GET_DL_USER_DATA(slot_id);
	client_noti->noti_type = Q_CLIENT_NOTI_TYPE_FINISHED_INFO;
	client_noti->next = DA_NULL;

	finished_info = (user_finished_info_t *)&(client_noti->type.finished_info);
	finished_info->download_id = dl_id;
	finished_info->err = error;
	finished_info->http_status = http_status;

	if (saved_path) {
		finished_info->saved_path = strdup(saved_path);
		DA_SECURE_LOGD("saved path=%s", saved_path);
	}
	if (etag) {
		finished_info->etag = strdup(etag);
		DA_SECURE_LOGD("pushing finished info. etag[%s]", etag);
	}
	DA_LOG_VERBOSE(ClientNoti, "user_data=%p", client_noti->user_data);
	DA_LOG_VERBOSE(ClientNoti, "http_status=%d", http_status);
	DA_LOG(ClientNoti, "pushing slot_id=%d, dl_id=%d err=%d", slot_id, dl_id, error);

	push_client_noti(client_noti);

	return DA_RESULT_OK;
}

da_result_t  __launch_client_thread(void)
{
	pthread_t thread_id = 0;

	DA_LOG_FUNC_LOGV(Thread);

	if (pthread_create(&thread_id, DA_NULL,
			__thread_for_client_noti,DA_NULL) < 0) {
		DA_LOG_ERR(Thread, "making thread failed..");
		return DA_ERR_FAIL_TO_CREATE_THREAD;
	}
	DA_LOG_VERBOSE(Thread, "client mgr thread id[%d]", thread_id);
	client_app_mgr.thread_id = thread_id;
	return DA_RESULT_OK;
}

void destroy_client_noti(client_noti_t *client_noti)
{
	if (client_noti) {
		if (client_noti->noti_type == Q_CLIENT_NOTI_TYPE_STARTED_INFO) {
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
			if (update_dl_info->etag) {
				free(update_dl_info->etag);
				update_dl_info->etag = DA_NULL;
			}
		} else if (client_noti->noti_type ==
				Q_CLIENT_NOTI_TYPE_FINISHED_INFO) {
			user_finished_info_t *finished_info = DA_NULL;
			finished_info = (user_finished_info_t*)
				&(client_noti->type.finished_info);
			if (finished_info->saved_path) {
				free(finished_info->saved_path);
				finished_info->saved_path = DA_NULL;
			}
			if (finished_info->etag) {
				free(finished_info->etag);
				finished_info->etag = DA_NULL;
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
#if 0
		if (cur->noti_type == Q_CLIENT_NOTI_TYPE_PROGRESS_INFO) {
			/* For UI performance. If the update noti info is existed at queue,
			   replace it with new update noti info */
			if (cur->slot_id == client_noti->slot_id) {
				/* DA_LOG(ClientNoti, "exchange queue's tail and pushing item"); */
				if (pre == DA_NULL)
					queue->client_q_head = client_noti;
				else
					pre->next = client_noti;
				destroy_client_noti(cur);
			} else {
				cur->next = client_noti;
			}
		} else {
			cur->next = client_noti;
		}
#else
	cur->next = client_noti;
#endif
	}

	queue->having_data = DA_TRUE;

	__client_q_wake_up_without_lock();
	if (queue->client_q_head->next) {
		DA_LOG_VERBOSE(ClientNoti, "client noti[%p] next noti[%p]",
				queue->client_q_head, queue->client_q_head->next);
	} else {
		DA_LOG_VERBOSE(ClientNoti, "client noti[%p] next noti is NULL",
				queue->client_q_head);
	}

	_da_thread_mutex_unlock (&(queue->mutex_client_queue));
}

void __pop_client_noti(client_noti_t **out_client_noti)
{
	client_queue_t *queue = DA_NULL;

	queue = &(client_app_mgr.client_queue);

	_da_thread_mutex_lock (&(queue->mutex_client_queue));

	if (queue->client_q_head) {
		*out_client_noti = queue->client_q_head;
		queue->client_q_head = queue->client_q_head->next;
		if (queue->client_q_head) {
			DA_LOG_VERBOSE(ClientNoti, "client noti[%p] next noti[%p]",
					*out_client_noti, queue->client_q_head);
		} else {
			DA_LOG_VERBOSE(ClientNoti, "client noti[%p] next noti is NULL",
					*out_client_noti);
		}
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
	queue = &(client_app_mgr.client_queue);
	_da_thread_cond_wait(&(queue->cond_client_queue), &(queue->mutex_client_queue));
}

void __client_q_wake_up_without_lock(void)
{
	client_queue_t *queue = DA_NULL;
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

	DA_LOG_FUNC_LOGV(Thread);

	_da_thread_mutex_lock(&(client_app_mgr.mutex_client_mgr));
	client_app_mgr.is_thread_init = DA_TRUE;
	_da_thread_mutex_unlock(&(client_app_mgr.mutex_client_mgr));

	queue = &(client_app_mgr.client_queue);
	DA_LOG_VERBOSE(ClientNoti, "client queue = %p", queue);

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, DA_NULL);
	pthread_cleanup_push(__thread_clean_up_handler_for_client_thread, (void *)DA_NULL);

	do {
		_da_thread_mutex_lock(&(queue->mutex_client_queue));
		if (DA_FALSE == IS_CLIENT_Q_HAVING_DATA(queue)) {
			DA_LOG_VERBOSE(Thread, "Sleep @ thread_for_client_noti!");
			__client_q_goto_sleep_without_lock();
			DA_LOG_VERBOSE(Thread, "Woke up @ thread_for_client_noti");
		}
		_da_thread_mutex_unlock(&(queue->mutex_client_queue));

		do {
			__pop_client_noti(&client_noti);
			if (client_noti == DA_NULL) {
				DA_LOG_ERR(ClientNoti, "There is no data on client queue!");
				ret = DA_ERR_INVALID_STATE;
				need_wait = DA_FALSE;
			} else {
				DA_LOG_VERBOSE(ClientNoti, "noti type[%d]",
						client_noti->noti_type);
				switch (client_noti->noti_type) {
				case Q_CLIENT_NOTI_TYPE_STARTED_INFO:
				{
					user_download_info_t *update_dl_info = DA_NULL;;
					update_dl_info = (user_download_info_t*)(&(client_noti->type.update_dl_info));
					if (client_app_mgr.client_app_info.client_callback.update_dl_info_cb) {
						client_app_mgr.client_app_info.client_callback.update_dl_info_cb(update_dl_info, client_noti->user_data);
						if (update_dl_info->etag)
							DA_SECURE_LOGD("Etag:[%s]", update_dl_info->etag);
						DA_SECURE_LOGD("file size=%lu", update_dl_info->file_size);
						DA_LOG(ClientNoti, "Update download info for slot_id=%d, dl_id=%d- DONE",
								client_noti->slot_id,
								update_dl_info->download_id
								);
					}
				}
				break;
				case Q_CLIENT_NOTI_TYPE_PROGRESS_INFO:
				{
					user_progress_info_t *progress_info = DA_NULL;;
					progress_info = (user_progress_info_t*)(&(client_noti->type.update_progress_info));
					if (client_app_mgr.client_app_info.client_callback.update_progress_info_cb) {
						client_app_mgr.client_app_info.client_callback.update_progress_info_cb(progress_info, client_noti->user_data);
						DA_LOG_VERBOSE(ClientNoti, "Update downloading info for slot_id=%d, dl_id=%d, received size=%lu - DONE",
								client_noti->slot_id,
								progress_info->download_id,
								progress_info->received_size);
					}
				}
				break;
				case Q_CLIENT_NOTI_TYPE_FINISHED_INFO:
				{
					user_finished_info_t *finished_info = DA_NULL;;
					finished_info = (user_finished_info_t*)(&(client_noti->type.finished_info));
					if (client_app_mgr.client_app_info.client_callback.finished_info_cb) {
						client_app_mgr.client_app_info.client_callback.finished_info_cb(
							finished_info, client_noti->user_data);
						DA_LOG(ClientNoti, "Completed info for slot_id=%d, dl_id=%d, err=%d http_state=%d user_data=%p- DONE",
								client_noti->slot_id,
								finished_info->download_id,
								finished_info->err,
								finished_info->http_status,
								client_noti->user_data);
						if (finished_info->etag)
							DA_SECURE_LOGD("Completed info for etag=%s - DONE",
									finished_info->etag);

					}
				}
				break;
				case Q_CLIENT_NOTI_TYPE_PAUSED_INFO:
				{
					user_paused_info_t *da_paused_info = DA_NULL;
					da_paused_info = (user_paused_info_t *)(&(client_noti->type.paused_info));

					if (client_app_mgr.client_app_info.client_callback.paused_info_cb) {
						DA_LOG(ClientNoti, "User Paused info for slot_id=%d, dl_id=%d - Done",
								client_noti->slot_id,
								da_paused_info->download_id);
						client_app_mgr.client_app_info.client_callback.paused_info_cb(
							da_paused_info, client_noti->user_data);
					}
				}
				break;
				case Q_CLIENT_NOTI_TYPE_TERMINATE:
					DA_LOG_VERBOSE(ClientNoti, "Q_CLIENT_NOTI_TYPE_TERMINATE");
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
	DA_LOG_DEBUG(Thread, "=====thread_for_client_noti- EXIT=====");
	pthread_exit((void *)NULL);
	return DA_NULL;
}

char *get_client_user_agent_string(void)
{
	if (!client_app_mgr.is_init)
		return DA_NULL;

	return client_app_mgr.client_app_info.client_user_agent;
}
