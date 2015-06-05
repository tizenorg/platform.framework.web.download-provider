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

#include <stdlib.h>

#include <download-provider-queue.h>
#include <download-provider-log.h>
#include <download-provider-pthread.h>
#include <download-provider-client.h>
#include <download-provider-client-manager.h>

/* queue
 * 1. push : at the tail of linked list
 * 2. pop  : at the head of linked list
 * 3. priority push : at the head of linked list
 * 4. in pop, check client of slot, search request by download_id
 */

//dp_queue_fmt *g_dp_queue = NULL; // head of linked list
pthread_mutex_t g_dp_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

// normal . push at the tail of queue.
int dp_queue_push(dp_queue_fmt **queue, void *slot, void *request)
{
	if (queue == NULL) {
		TRACE_ERROR("check memory address of queue");
		return -1;
	}
	dp_client_slots_fmt *baseslot = slot;
	dp_request_fmt *new_request = request;
	if (slot == NULL || request == NULL || new_request->id <= 0) {
		TRACE_ERROR("check client and request memory address");
		return -1;
	} else if (new_request->id <= 0) {
		TRACE_ERROR("check slot or download id", new_request->id);
		return -1;
	} else if (new_request->state != DP_STATE_QUEUED) {
		TRACE_ERROR("check id:%d state:%s", new_request->id, dp_print_state(new_request->state));
		return -1;
	}

	CLIENT_MUTEX_LOCK(&g_dp_queue_mutex);
	// search the tail of queue
	int i = 0;
	dp_queue_fmt *tailp = *queue;
	dp_queue_fmt *prevp = NULL;
	TRACE_DEBUG("queue req slot:%p request:%p id:%d", slot, request, (request == NULL ? 0 : new_request->id));
	for (; tailp != NULL; i++) {
		dp_request_fmt *qrequestp = tailp->request;
		if (tailp->slot == NULL || tailp->request == NULL ||
				qrequestp->state != DP_STATE_QUEUED) {
			TRACE_DEBUG("queue error %d slot:%p request:%p id:%d", i, tailp->slot, tailp->request, (tailp->request == NULL ? 0 : ((dp_request_fmt *)tailp->request)->id));
		} else if (tailp->slot == slot && tailp->request == request && qrequestp->id == new_request->id) {
			TRACE_INFO("queue duplicte %d slot:%p request:%p id:%d", i, slot, request, new_request->id);
			CLIENT_MUTEX_UNLOCK(&g_dp_queue_mutex);
			return 0;
		} else {
			TRACE_INFO("queue info %d slot:%p request:%p id:%d %s", i, tailp->slot, tailp->request, ((dp_request_fmt *)tailp->request)->id, ((dp_client_slots_fmt *)tailp->slot)->pkgname);
		}
		prevp = tailp;
		tailp = tailp->next;
	}
	dp_queue_fmt *new_queue = (dp_queue_fmt *)malloc(sizeof(dp_queue_fmt));
	if (new_queue != NULL) {
		new_queue->slot = slot;
		new_queue->request = request;
		new_queue->next = NULL;
		if (prevp == NULL)
			*queue = new_queue;
		else
			prevp->next = new_queue;
	} else {
		CLIENT_MUTEX_UNLOCK(&g_dp_queue_mutex);
		return -1;
	}
	TRACE_DEBUG("queue push %d info:%s id:%d", i, baseslot->pkgname, new_request->id);
	CLIENT_MUTEX_UNLOCK(&g_dp_queue_mutex);
	return 0;
}

int dp_queue_pop(dp_queue_fmt **queue, void **slot, void **request)
{
	if (queue == NULL) {
		TRACE_ERROR("check memory address of queue");
		return -1;
	}
	if (slot == NULL || request == NULL) {
		TRACE_ERROR("check client and request memory address");
		return -1;
	}

	int lock = CLIENT_MUTEX_TRYLOCK(&g_dp_queue_mutex);
	if (lock != 0) {
		TRACE_DEBUG("skip queue is used by other thread");
		return 0;
	}
	if (*queue == NULL) {
		//TRACE_DEBUG("queue empty");
		CLIENT_MUTEX_UNLOCK(&g_dp_queue_mutex);
		return -1;
	}
	// get a head of queue
	int ret = -1;
	dp_queue_fmt *popp;
	do {
		 popp = *queue;
		*queue = popp->next;
		dp_client_slots_fmt *slotp = popp->slot;
		dp_request_fmt *requestp = popp->request;
		if (slotp == NULL || requestp == NULL ||
				requestp->state != DP_STATE_QUEUED) {
			TRACE_DEBUG("queue error slot:%p request:%p id:%d", popp->slot, popp->request, (requestp == NULL ? 0 : requestp->id));
			free(popp);
		} else {
			TRACE_INFO("queue pop slot:%p request:%p id:%d %s", popp->slot, popp->request, requestp->id, slotp->pkgname);
			*slot = popp->slot;
			*request = popp->request;
			ret = 0;
			free(popp);
			break;
		}
	} while (*queue != NULL); // if meet the tail of queue
	CLIENT_MUTEX_UNLOCK(&g_dp_queue_mutex);
	return ret;
}

void dp_queue_clear(dp_queue_fmt **queue, void *request)
{
	if (queue == NULL) {
		TRACE_ERROR("check memory address of queue");
		return ;
	}
	if (request == NULL) {
		TRACE_ERROR("check client and request memory address");
		return ;
	}
	CLIENT_MUTEX_LOCK(&g_dp_queue_mutex);
	int i = 0;
	dp_queue_fmt *tailp = *queue;
	dp_queue_fmt *prevp = NULL;
	TRACE_DEBUG("queue clear req request:%p id:%d", request, (request == NULL ? 0 : ((dp_request_fmt *)request)->id));
	for (; tailp != NULL; i++) {
		dp_request_fmt *requestp = tailp->request;
		TRACE_DEBUG("queue info %d request:%p id:%d", i, requestp, (requestp == NULL ? 0 : requestp->id));
		if (requestp == request) {
			// clear.
			if (prevp == NULL)
				*queue = tailp->next;
			else
				prevp->next = tailp->next;
			TRACE_DEBUG("queue clear this %d request:%p id:%d", i, requestp, (requestp == NULL ? 0 : requestp->id));
			free(tailp);
			break;
		}
		prevp = tailp;
		tailp = tailp->next;
	}
	CLIENT_MUTEX_UNLOCK(&g_dp_queue_mutex);
}

void dp_queue_clear_all(dp_queue_fmt **queue)
{
	if (queue == NULL) {
		TRACE_ERROR("check memory address of queue");
		return ;
	}
	CLIENT_MUTEX_LOCK(&g_dp_queue_mutex);
	if (queue == NULL || *queue == NULL) {
		TRACE_DEBUG("queue empty");
		CLIENT_MUTEX_UNLOCK(&g_dp_queue_mutex);
		return ;
	}
	// get a head of queue
	do {
		dp_queue_fmt *popp = *queue;
		*queue = popp->next;
		TRACE_DEBUG("queue clear slot:%p request:%p id:%d", popp->slot, popp->request, (popp->request == NULL ? 0 : ((dp_request_fmt *)popp->request)->id));
		free(popp);
	} while (*queue != NULL); // if meet the tail of queue
	CLIENT_MUTEX_UNLOCK(&g_dp_queue_mutex);
}
