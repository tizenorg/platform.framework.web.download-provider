/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd All Rights Reserved
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

#ifdef SUPPORT_NOTIFICATION
#include <stdlib.h>
#include <signal.h> // pthread_kill
#include <errno.h> // ESRCH

#include "download-provider-log.h"
#include "download-provider-pthread.h"
#include "download-provider-client.h"
#include "download-provider-client-manager.h"
#include "download-provider-notification.h"
#include "download-provider-db-defs.h"
#include "download-provider-db.h"
#endif
#include "download-provider-notification-manager.h"

#ifdef SUPPORT_NOTIFICATION
typedef struct { // manage clients without mutex
	int id;
	int state;
	int noti_priv_id;
	double received_size;
	double file_size;
	dp_noti_type type;
	void *slot; // client can not be NULL. it will exist in dummy
	void *request; // this can be NULL after destroy
	void *next;
} dp_notification_queue_fmt;


pthread_mutex_t g_dp_notification_manager_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_dp_notification_manager_cond = PTHREAD_COND_INITIALIZER;
pthread_t g_dp_notification_manager_tid = 0;

static dp_notification_queue_fmt *g_dp_notification_clear = NULL;
static dp_notification_queue_fmt *g_dp_notification_ongoing = NULL;
static dp_notification_queue_fmt *g_dp_notification = NULL;

pthread_mutex_t g_dp_notification_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_dp_notification_queue_ongoing_mutex = PTHREAD_MUTEX_INITIALIZER;

// normal . push at the tail of queue.
static int __dp_notification_queue_push(dp_notification_queue_fmt **queue, void *slot, void *request, const int id, const int state, const int noti_priv_id, const double received_size, const double file_size, const dp_noti_type type)
{
	if (queue == NULL) {
		TRACE_ERROR("check memory address of queue");
		return -1;
	}
	if (slot == NULL || request == NULL) {
		TRACE_ERROR("check client and request memory address");
		return -1;
	} else if (id <= 0) {
		TRACE_ERROR("check slot or download id", id);
		return -1;
	}

	int ret = -1;
	CLIENT_MUTEX_LOCK(&g_dp_notification_queue_mutex);
	// search the tail of queue
	int i = 0;
	dp_notification_queue_fmt *tailp = *queue;
	dp_notification_queue_fmt *prevp = NULL;
	for (; tailp != NULL; i++) {
		prevp = tailp;
		tailp = tailp->next;
	}
	dp_notification_queue_fmt *new_queue = (dp_notification_queue_fmt *)malloc(sizeof(dp_notification_queue_fmt));
	if (new_queue != NULL) {
		new_queue->slot = slot;
		new_queue->id = id;
		new_queue->state = state;
		new_queue->noti_priv_id = noti_priv_id;
		new_queue->received_size = received_size;
		new_queue->file_size = file_size;
		new_queue->type = type;
		new_queue->request = request;
		new_queue->next = NULL;
		if (prevp == NULL)
			*queue = new_queue;
		else
			prevp->next = new_queue;
		ret = 0;
	}
	//TRACE_DEBUG("queue push %d id:%d", i, id);
	CLIENT_MUTEX_UNLOCK(&g_dp_notification_queue_mutex);
	return ret;
}

int __dp_notification_queue_pop(dp_notification_queue_fmt **queue, void **slot, void **request, int *id, int *state, int *noti_priv_id, double *received_size, double *file_size, dp_noti_type *type)
{
	if (queue == NULL) {
		TRACE_ERROR("check memory address of queue");
		return -1;
	}
	if (slot == NULL) {
		TRACE_ERROR("check client memory address");
		return -1;
	}

	int lock = CLIENT_MUTEX_TRYLOCK(&g_dp_notification_queue_mutex);
	if (lock != 0) {
		TRACE_DEBUG("skip queue is used by other thread");
		return 0;
	}
	if (queue == NULL || *queue == NULL) {
		//TRACE_DEBUG("queue empty");
		CLIENT_MUTEX_UNLOCK(&g_dp_notification_queue_mutex);
		return -1;
	}
	// get a head of queue
	int ret = -1;
	do {
		dp_notification_queue_fmt *popp = *queue;
		*queue = popp->next;
		if (popp->slot == NULL) {
			TRACE_DEBUG("queue error slot:%p id:%d", popp->slot, popp->id);
		} else {
			*slot = popp->slot;
			if (request != NULL) {
				*request = popp->request;
			}
			*id = popp->id;
			*state = popp->state;
			if (noti_priv_id != NULL)
				*noti_priv_id = popp->noti_priv_id;
			if (received_size != NULL)
				*received_size = popp->received_size;
			if (file_size != NULL)
				*file_size = popp->file_size;
			*type = popp->type;
			ret = 0;
			break;
		}
	} while (*queue != NULL); // if meet the tail of queue
	CLIENT_MUTEX_UNLOCK(&g_dp_notification_queue_mutex);
	return ret;
}

int __dp_notification_queue_ongoing_pop(dp_notification_queue_fmt **queue, void **slot, void **request, int *id, int *state, double *received_size, double *file_size, dp_noti_type *type)
{
	if (queue == NULL) {
		TRACE_ERROR("check memory address of queue");
		return -1;
	}
	if (slot == NULL) {
		TRACE_ERROR("check client memory address");
		return -1;
	}

	int lock = CLIENT_MUTEX_TRYLOCK(&g_dp_notification_queue_ongoing_mutex);
	if (lock != 0) {
		TRACE_DEBUG("skip queue is used by other thread");
		return 0;
	}
	if (queue == NULL || *queue == NULL) {
		//TRACE_DEBUG("queue empty");
		CLIENT_MUTEX_UNLOCK(&g_dp_notification_queue_ongoing_mutex);
		return -1;
	}
	// get a head of queue
	int ret = -1;
	do {
		dp_notification_queue_fmt *popp = *queue;
		*queue = popp->next;
		if (popp->slot == NULL) {
			TRACE_DEBUG("queue error slot:%p id:%d", popp->slot, popp->id);
		} else {
			*slot = popp->slot;
			if (request != NULL) {
				*request = popp->request;
			}
			*id = popp->id;
			*state = popp->state;
			if (received_size != NULL)
				*received_size = popp->received_size;
			if (file_size != NULL)
				*file_size = popp->file_size;
			*type = popp->type;
			ret = 0;
			break;
		}
	} while (*queue != NULL); // if meet the tail of queue
	CLIENT_MUTEX_UNLOCK(&g_dp_notification_queue_ongoing_mutex);
	return ret;
}

static int __dp_notification_queue_ongoing_push(dp_notification_queue_fmt **queue, void *slot, void *request, const int id, const int state, const double received_size, const double file_size, const dp_noti_type type)
{
	if (queue == NULL) {
		TRACE_ERROR("check memory address of queue");
		return -1;
	}
	if (slot == NULL || request == NULL) {
		TRACE_ERROR("check client and request memory address");
		return -1;
	} else if (id <= 0) {
		TRACE_ERROR("check slot or download id", id);
		return -1;
	}

	int ret = -1;
	CLIENT_MUTEX_LOCK(&g_dp_notification_queue_ongoing_mutex);
	// search the tail of queue
	int i = 0;
	dp_notification_queue_fmt *tailp = *queue;
	dp_notification_queue_fmt *prevp = NULL;
	for (; tailp != NULL; i++) {
		if (tailp->slot == slot && tailp->request == request) {
			if (tailp->id == id && tailp->state == state && tailp->type == type) {
				CLIENT_MUTEX_UNLOCK(&g_dp_notification_queue_ongoing_mutex);
				return 0;
			}
		}
		prevp = tailp;
		tailp = tailp->next;
	}
	dp_notification_queue_fmt *new_queue = (dp_notification_queue_fmt *)malloc(sizeof(dp_notification_queue_fmt));
	if (new_queue != NULL) {
		new_queue->slot = slot;
		new_queue->id = id;
		new_queue->state = state;
		new_queue->received_size = received_size;
		new_queue->file_size = file_size;
		new_queue->type = type;
		new_queue->request = request;
		new_queue->next = NULL;
		if (prevp == NULL)
			*queue = new_queue;
		else
			prevp->next = new_queue;
		ret = 0;
	}
	//TRACE_DEBUG("queue push %d id:%d", i, id);
	CLIENT_MUTEX_UNLOCK(&g_dp_notification_queue_ongoing_mutex);
	return ret;
}

void __dp_notification_queue_clear(dp_notification_queue_fmt **queue, const int id)
{
	if (queue == NULL) {
		TRACE_ERROR("check memory address of queue");
		return ;
	}
	CLIENT_MUTEX_LOCK(&g_dp_notification_queue_mutex);
	int i = 0;
	dp_notification_queue_fmt *tailp = *queue;
	dp_notification_queue_fmt *prevp = NULL;
	for (; tailp != NULL; i++) {
		if (tailp->id == id) {
			// clear.
			if (prevp == NULL)
				*queue = tailp->next;
			else
				prevp->next = tailp->next;
			TRACE_DEBUG("queue clear this %d id:%d", i, tailp->id);
			free(tailp);
			break;
		}
		prevp = tailp;
		tailp = tailp->next;
	}
	CLIENT_MUTEX_UNLOCK(&g_dp_notification_queue_mutex);
}

int dp_notification_manager_clear_notification(void *slot, void *request, const dp_noti_type type)
{
	dp_request_fmt *requestp = request;
	if (request == NULL) {
		TRACE_DEBUG("check address request:%p id:%d",
				request, (request == NULL ? 0 : requestp->id));
		return -1;
	}
	__dp_notification_queue_clear(&g_dp_notification, requestp->id);
	TRACE_DEBUG("push clear id:%d noti_priv_id:%d", requestp->id, requestp->noti_priv_id);
	if (__dp_notification_queue_push(&g_dp_notification_clear, slot, request, requestp->id, requestp->state, -1, 0, 0, type) < 0) {
		TRACE_ERROR("failed to push to notification id:%d", requestp->id);
		return -1;
	}
	dp_notification_manager_wake_up();
	return 0;
}

int dp_notification_manager_push_notification(void *slot, void *request, const dp_noti_type type)
{
	dp_request_fmt *requestp = request;
	if (slot == NULL || request == NULL) {
		TRACE_DEBUG("check address client:%p request:%p id:%d", slot,
				request, (request == NULL ? 0 : requestp->id));
		return -1;
	}
//	TRACE_DEBUG("push noti id:%d noti_priv_id:%d type:%d", requestp->id, requestp->noti_priv_id, type);
	if (type == DP_NOTIFICATION) {
		__dp_notification_queue_clear(&g_dp_notification_ongoing, requestp->id);
		if (__dp_notification_queue_push(&g_dp_notification, slot, request, requestp->id, requestp->state, requestp->noti_priv_id, 0, (double)requestp->file_size, type) < 0) {
			TRACE_ERROR("failed to push to notification id:%d", requestp->id);
			return -1;
		}
	} else {
		__dp_notification_queue_clear(&g_dp_notification, requestp->id);
		if (__dp_notification_queue_ongoing_push(&g_dp_notification_ongoing, slot, request, requestp->id, requestp->state, (double)requestp->received_size, (double)requestp->file_size, type) < 0) {
			TRACE_ERROR("failed to push to notification id:%d", requestp->id);
			return -1;
		}
	}
	dp_notification_manager_wake_up();
	return 0;
}

static void __dp_notification_manager_check_notification()
{
	int pop_queue = 0;
	do {
		int errorcode = DP_ERROR_NONE;
		int download_id = -1;
		int state = -1;
		dp_noti_type noti_type = -1;
		dp_client_slots_fmt *slot = NULL;
		dp_request_fmt *request = NULL;
		pop_queue = 0;
		if (__dp_notification_queue_pop(&g_dp_notification_clear, (void *)&slot, (void *)&request, &download_id, &state, NULL, NULL, NULL, &noti_type) == 0) {
			if (slot != NULL) {
				int noti_priv_id = -1;
				if (CLIENT_MUTEX_CHECKLOCK(&slot->mutex) == 0) {
					if (request != NULL && request->id == download_id && request->noti_priv_id >= 0) {
						noti_priv_id = request->noti_priv_id;
						request->noti_priv_id = -1;
						if (dp_db_replace_property(slot->client.dbhandle, download_id, DP_TABLE_NOTIFICATION, DP_DB_COL_NOTI_PRIV_ID, (void *)&request->noti_priv_id, 0, 0, &errorcode) < 0) {
							TRACE_ERROR("failed to set priv_id id:%d error:%s", download_id, dp_print_errorcode(errorcode));
						}
					}
					CLIENT_MUTEX_UNLOCK(&slot->mutex);
				}
				TRACE_DEBUG("clear ongoing id:%d noti_priv_id:%d type:%d", download_id, noti_priv_id, noti_type);
				if (noti_priv_id >= 0) {
					if (noti_type > DP_NOTIFICATION)
						dp_notification_delete_ongoing(noti_priv_id);
					else
						dp_notification_delete(noti_priv_id);
				}
			}
			pop_queue++;
			continue;
		}
		int noti_priv_id = -1;
		if (__dp_notification_queue_pop(&g_dp_notification, (void *)&slot, (void *)&request, &download_id, &state, &noti_priv_id, NULL, NULL, &noti_type) == 0) {
			if (slot != NULL) {
				__dp_notification_queue_clear(&g_dp_notification_ongoing, download_id); // prevent new ongoing
				if (noti_priv_id >= 0) {
					TRACE_DEBUG("clear ongoing(%d) id:%d type:%d state:%d", noti_priv_id, download_id, noti_type, state);
					dp_notification_delete_ongoing(noti_priv_id);
					noti_priv_id = -1;
				}
				if (CLIENT_MUTEX_CHECKLOCK(&slot->mutex) == 0) {
					TRACE_DEBUG("notification id:%d type:%d state:%d", download_id, noti_type, state);
					if (request != NULL && request->id == download_id && request->noti_priv_id >= 0) {
						dp_notification_delete_ongoing(request->noti_priv_id);
						request->noti_priv_id = -1;
					}
					dp_content_type content_type = DP_CONTENT_UNKNOWN;
					if (request != NULL)
						content_type = request->content_type;
					noti_priv_id = dp_notification_new(slot->client.dbhandle, download_id, state, content_type, slot->pkgname); // lazy API
					TRACE_DEBUG("new notification(%d) id:%d type:%d state:%d", noti_priv_id, download_id, noti_type, state);
					if (noti_priv_id < 0) {
						TRACE_ERROR("failed to register notification for id:%d", download_id);
					} else {
						if (request != NULL && request->id == download_id) {
							request->noti_priv_id = noti_priv_id;
						}
						if (dp_db_replace_property(slot->client.dbhandle, download_id, DP_TABLE_NOTIFICATION, DP_DB_COL_NOTI_PRIV_ID, (void *)&noti_priv_id, 0, 0, &errorcode) < 0) {
							TRACE_ERROR("failed to set priv_id id:%d error:%s", download_id, dp_print_errorcode(errorcode));
						}
					}
					CLIENT_MUTEX_UNLOCK(&slot->mutex);
				}
			}
			pop_queue++;
		}
		double received_size = 0;
		double file_size = 0;
		if (__dp_notification_queue_ongoing_pop(&g_dp_notification_ongoing, (void *)&slot, (void *)&request, &download_id, &state, &received_size, &file_size, &noti_type) == 0) {
			if (slot != NULL) {
				int noti_priv_id = -1;
				int request_id = -1;
				if (CLIENT_MUTEX_CHECKLOCK(&slot->mutex) == 0) {
					if (request != NULL && request->id == download_id) {
						request_id = download_id;
						if (request->noti_priv_id >= 0) {
							noti_priv_id = request->noti_priv_id;
						}
					}
					CLIENT_MUTEX_UNLOCK(&slot->mutex);
				} else {
					TRACE_ERROR("ongoing wrong address id:%d noti_priv_id:%d type:%d state:%d", download_id, noti_priv_id, noti_type, state);
					continue;
				}

				if (request_id < 0) {
					TRACE_ERROR("ongoing wrong info id:%d noti_priv_id:%d type:%d state:%d", download_id, noti_priv_id, noti_type, state);
					slot = NULL;
					request = NULL;
					continue;
				}
				if (noti_priv_id < 0 && noti_type > DP_NOTIFICATION_ONGOING) {
					TRACE_DEBUG("ongoing precheck id:%d noti_priv_id:%d type:%d state:%d", download_id, noti_priv_id, noti_type, state);
					noti_type = DP_NOTIFICATION_ONGOING;
				}

				TRACE_DEBUG("ongoing id:%d noti_priv_id:%d type:%d state:%d", download_id, noti_priv_id, noti_type, state);

				char *subject = NULL;
				if (noti_type == DP_NOTIFICATION || noti_type == DP_NOTIFICATION_ONGOING_UPDATE) {
					unsigned length = 0;
					if (CLIENT_MUTEX_CHECKLOCK(&slot->mutex) == 0) {
						if (request != NULL) {
							if (dp_db_get_property_string(slot->client.dbhandle, download_id, DP_TABLE_NOTIFICATION, DP_DB_COL_NOTI_SUBJECT, (unsigned char **)&subject, &length, &errorcode) < 0) {
								TRACE_ERROR("failed to get subject id:%d error:%s", download_id, dp_print_errorcode(errorcode));
							} else if (subject == NULL && dp_db_get_property_string(slot->client.dbhandle, download_id, DP_TABLE_DOWNLOAD, DP_DB_COL_CONTENT_NAME, (unsigned char **)&subject, &length, &errorcode) < 0) {
								TRACE_ERROR("failed to get content_name id:%d error:%s", download_id, dp_print_errorcode(errorcode));
							}
						}
						CLIENT_MUTEX_UNLOCK(&slot->mutex);
					}
				}

				if (noti_type > DP_NOTIFICATION_ONGOING) { // update
					if (noti_priv_id >= 0 && dp_notification_ongoing_update(noti_priv_id, received_size, file_size, subject) < 0) {
						TRACE_ERROR("failed to update ongoing for id:%d", download_id);
					}
				} else { // new ongoing
					if (noti_priv_id >= 0) {
						TRACE_DEBUG("clear ongoing id:%d noti_priv_id:%d", download_id, noti_priv_id);
						dp_notification_delete(noti_priv_id);
						dp_notification_delete_ongoing(noti_priv_id);
						noti_priv_id = -1;
					}
					unsigned char *raws_buffer = NULL;
					unsigned length = 0;
					if (CLIENT_MUTEX_CHECKLOCK(&slot->mutex) == 0) {
						if (dp_db_get_property_string(slot->client.dbhandle, download_id, DP_TABLE_NOTIFICATION, DP_DB_COL_NOTI_RAW_ONGOING, &raws_buffer, &length, &errorcode) < 0) {
							TRACE_DEBUG("failed to get bundle raws id:%d error:%s", download_id, dp_print_errorcode(errorcode));
						}
						CLIENT_MUTEX_UNLOCK(&slot->mutex);
					}
					noti_priv_id = dp_notification_ongoing_new(slot->pkgname, subject, raws_buffer, length);
					TRACE_DEBUG("new ongoing(%d) id:%d type:%d state:%d", noti_priv_id, download_id, noti_type, state);
					free(raws_buffer);
					if (noti_priv_id < 0) {
						TRACE_ERROR("failed to update ongoing for id:%d", download_id);
					} else {
						if (CLIENT_MUTEX_CHECKLOCK(&slot->mutex) == 0) {
							if (request != NULL)
								request->noti_priv_id = noti_priv_id;
							if (dp_db_replace_property(slot->client.dbhandle, download_id, DP_TABLE_NOTIFICATION, DP_DB_COL_NOTI_PRIV_ID, (void *)&noti_priv_id, 0, 0, &errorcode) < 0) {
								TRACE_ERROR("failed to set priv_id id:%d error:%s", download_id, dp_print_errorcode(errorcode));
							}
							CLIENT_MUTEX_UNLOCK(&slot->mutex);
						}
					}
				}
				free(subject);
			}
			pop_queue++;
		}
	} while (pop_queue > 0);
}

static void *__dp_notification_manager(void *arg)
{
	pthread_cond_init(&g_dp_notification_manager_cond, NULL);

	dp_notification_set_locale();

	while (g_dp_notification_manager_tid > 0) {

		if (g_dp_notification_manager_tid <= 0) {
			TRACE_DEBUG("notification-manager is closed by other thread");
			break;
		}

		// check wifi_direct first
		__dp_notification_manager_check_notification();

		CLIENT_MUTEX_LOCK(&g_dp_notification_manager_mutex);
		pthread_cond_wait(&g_dp_notification_manager_cond, &g_dp_notification_manager_mutex);
		CLIENT_MUTEX_UNLOCK(&g_dp_notification_manager_mutex);
	}

	TRACE_DEBUG("notification-manager's working is done");
	dp_notification_clear_locale();
	pthread_cond_destroy(&g_dp_notification_manager_cond);
	pthread_exit(NULL);
	return 0;
}

static int __dp_notification_manager_start()
{
	if (g_dp_notification_manager_tid == 0 ||
			pthread_kill(g_dp_notification_manager_tid, 0) == ESRCH) {
		TRACE_DEBUG("try to create notification-manager");
		if (pthread_create(&g_dp_notification_manager_tid, NULL,
				__dp_notification_manager, NULL) != 0) {
			TRACE_STRERROR("failed to create notification-manager");
			return -1;
		}
	}
	return 0;
}

void dp_notification_manager_wake_up()
{
	if (g_dp_notification_manager_tid > 0) {
		int locked = CLIENT_MUTEX_TRYLOCK(&g_dp_notification_manager_mutex);
		if (locked == 0) {
			pthread_cond_signal(&g_dp_notification_manager_cond);
			CLIENT_MUTEX_UNLOCK(&g_dp_notification_manager_mutex);
		}
	} else {
		__dp_notification_manager_start();
	}
}

void dp_notification_manager_kill()
{
	if (g_dp_notification_manager_tid > 0 &&
			pthread_kill(g_dp_notification_manager_tid, 0) != ESRCH) {
		//send signal to notification thread
		g_dp_notification_manager_tid = 0;
		CLIENT_MUTEX_LOCK(&g_dp_notification_manager_mutex);
		pthread_cond_signal(&g_dp_notification_manager_cond);
		CLIENT_MUTEX_UNLOCK(&g_dp_notification_manager_mutex);
		pthread_cancel(g_dp_notification_manager_tid);
		int status;
		pthread_join(g_dp_notification_manager_tid, (void **)&status);
	}
}
#else

int dp_notification_manager_clear_notification(void *slot, void *request, const dp_noti_type type)
{
	return 0;
}

int dp_notification_manager_push_notification(void *slot, void *request, const dp_noti_type type)
{
	return 0;
}

void dp_notification_manager_wake_up()
{
	return;
}

void dp_notification_manager_kill()
{
	return;
}
#endif
