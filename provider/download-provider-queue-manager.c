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
#include <signal.h> // pthread_kill
#include <errno.h> // ESRCH

#include <download-provider-log.h>
#include <download-provider-pthread.h>

#include <download-provider-db-defs.h>
#include <download-provider-db.h>
#include <download-provider-queue.h>
#include <download-provider-network.h>
#include <download-provider-notify.h>
#include <download-provider-client.h>
#include <download-provider-client-manager.h>
#include <download-provider-plugin-download-agent.h>

pthread_mutex_t g_dp_queue_manager_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_dp_queue_manager_cond = PTHREAD_COND_INITIALIZER;
pthread_t g_dp_queue_manager_tid = 0;

static dp_queue_fmt *g_dp_queue_network_all = NULL;
static dp_queue_fmt *g_dp_queue_network_wifi = NULL;
static dp_queue_fmt *g_dp_queue_network_data_network = NULL;
static dp_queue_fmt *g_dp_queue_network_wifi_direct = NULL;

static dp_queue_fmt **__dp_queue_manager_get_queue(int network)
{
	switch(network) {
	case DP_NETWORK_ALL:
		//TRACE_DEBUG("network all");
		return &g_dp_queue_network_all;
	case DP_NETWORK_WIFI:
		//TRACE_DEBUG("network wifi only");
		return &g_dp_queue_network_wifi;
	case DP_NETWORK_DATA_NETWORK:
		//TRACE_DEBUG("network data network only");
		return &g_dp_queue_network_data_network;
	case DP_NETWORK_WIFI_DIRECT:
		//TRACE_DEBUG("network wifi-direct");
		return &g_dp_queue_network_wifi_direct;
	default:
		break;
	}
	return NULL;
}
int dp_queue_manager_push_queue(void *slot, void *request)
{
	dp_request_fmt *requestp = request;
	if (slot == NULL || request == NULL) {
		TRACE_DEBUG("check address client:%p request:%p id:%d", slot,
				request, (request == NULL ? 0 : requestp->id));
		return -1;
	}
	dp_queue_fmt **queue = __dp_queue_manager_get_queue(requestp->network_type);
	if (requestp->state != DP_STATE_QUEUED) {
		TRACE_ERROR("check id:%d state:%s", requestp->id, dp_print_state(requestp->state));
		return -1;
	}
	if (dp_queue_push(queue, slot, request) < 0) {
		TRACE_ERROR("failed to push to queue id:%d", requestp->id);
		return -1;
	}
	return 0;
}

void dp_queue_manager_clear_queue(void *request)
{
	dp_request_fmt *requestp = request;
	if (request == NULL) {
		TRACE_DEBUG("check address request:%p id:%d",
				request, (request == NULL ? 0 : requestp->id));
		return ;
	}
	dp_queue_fmt **queue = __dp_queue_manager_get_queue(requestp->network_type);
	dp_queue_clear(queue, request);
}

// if return negative, queue-manager try again.
static int __dp_queue_manager_try_download(dp_client_slots_fmt *slot, dp_request_fmt *request)
{
	int errorcode = DP_ERROR_NONE;
	int result = 0;

	if (slot == NULL || request == NULL) {
		TRACE_DEBUG("check address client:%p request:%p id:%d", slot,
				request, (request == NULL ? 0 : request->id));
		// return 0 to ignore this call.
		return 0;
	}

	if (request->state != DP_STATE_QUEUED) {
		TRACE_ERROR("check id %d state:%d", request->id, request->state);
		return 0;
	}

	// check startcount

	request->startcount++;
	request->access_time = (int)time(NULL);

	if (dp_db_replace_property(slot->client.dbhandle, request->id,
			DP_TABLE_LOGGING, DP_DB_COL_STARTCOUNT,
			(void *)&request->startcount, 0, 0, &errorcode) < 0) {
		TRACE_ERROR("failed to set startcount");
		return -1;
	}

	errorcode = DP_ERROR_NONE;

	if (dp_is_alive_download(request->agent_id) > 0)
		errorcode = dp_resume_agent_download(request->agent_id);
	else
		// call agent start function
		errorcode = dp_start_agent_download(slot, request);

	if (errorcode == DP_ERROR_NONE) {
		request->state = DP_STATE_CONNECTING;
	} else if (errorcode == DP_ERROR_TOO_MANY_DOWNLOADS ||
			errorcode == DP_ERROR_DISK_BUSY ||
			errorcode == DP_ERROR_OUT_OF_MEMORY) {
		TRACE_ERROR("push queue id:%d error:%s", request->id, dp_print_errorcode(errorcode));
		// PENDED
		request->state = DP_STATE_QUEUED;
		result = -1; // try again.
	} else if (errorcode == DP_ERROR_INVALID_STATE) { // by resume
		// ignore this request
		result = -1;
		TRACE_ERROR("failed to resume id:%d", request->id);
		request->agent_id = -1; // workaround. da_agent will an object for this agent_id later
	} else if (errorcode != DP_ERROR_NONE) {
		request->state = DP_STATE_FAILED;
	}

	request->error = errorcode;

	if (result == 0) { // it's not for retrying
		int sqlerror = DP_ERROR_NONE;
		if (dp_db_update_logging(slot->client.dbhandle, request->id,
				request->state, request->error, &sqlerror) < 0) {
			TRACE_ERROR("logging failure id:%d error:%d", request->id, sqlerror);
		}
		if (errorcode != DP_ERROR_NONE && request->state_cb == 1) { // announce state

			TRACE_ERROR("notify id:%d error:%s", request->id, dp_print_errorcode(errorcode));
			if (dp_notify_feedback(slot->client.notify, slot,
					request->id, request->state, request->error, 0) < 0) {
				TRACE_ERROR("disable state callback by IO_ERROR id:%d", request->id);
				request->state_cb = 0;
			}
		}
	}

	return result;

}

static int __dp_queue_manager_check_queue(dp_queue_fmt **queue)
{
	dp_client_slots_fmt *slot = NULL;
	dp_request_fmt *request = NULL;
	while (dp_queue_pop(queue, (void *)&slot, (void *)&request) == 0) { // pop a request from queue.
		TRACE_DEBUG("queue-manager pop a request");
		if (slot == NULL || request == NULL) {
			TRACE_DEBUG("queue error client:%p request:%p id:%d", slot, request, (request == NULL ? 0 : request->id));
			continue;
		}

		CLIENT_MUTEX_LOCK(&slot->mutex);

		TRACE_DEBUG("queue info slot:%p request:%p id:%d", slot, request, (request == NULL ? 0 : request->id));

		int errorcode = DP_ERROR_NONE;
		int download_state = DP_STATE_NONE;
		if (slot != NULL && request != NULL &&
				dp_db_get_property_int(slot->client.dbhandle, request->id, DP_TABLE_LOGGING, DP_DB_COL_STATE, &download_state, &errorcode) < 0) {
			TRACE_ERROR("deny checking id:%d db state:%s memory:%s", request->id, dp_print_state(download_state), dp_print_state(request->state));
			errorcode = DP_ERROR_ID_NOT_FOUND;
		}

		if (download_state == DP_STATE_QUEUED && __dp_queue_manager_try_download(slot, request) < 0) {
			// if failed to start, push at the tail of queue. try again.
			if (dp_queue_push(queue, slot, request) < 0) {
				TRACE_ERROR("failed to push to queue id:%d", request->id);
				int errorcode = DP_ERROR_NONE;
				if (dp_db_update_logging(slot->client.dbhandle, request->id, DP_STATE_FAILED, DP_ERROR_QUEUE_FULL, &errorcode) < 0) {
					TRACE_ERROR("failed to update log id:%d", request->id);
				}
				request->state = DP_STATE_FAILED;
				request->error = DP_ERROR_QUEUE_FULL;
			}
			CLIENT_MUTEX_UNLOCK(&slot->mutex);
			return -1; // return negative for taking a break
		}

		CLIENT_MUTEX_UNLOCK(&slot->mutex);

		slot = NULL;
		request = NULL;
	}
	return 0;
}

static void *__dp_queue_manager(void *arg)
{
	pthread_cond_init(&g_dp_queue_manager_cond, NULL);

	if (dp_init_agent() != DP_ERROR_NONE) {
		TRACE_ERROR("failed to init agent");
		pthread_cond_destroy(&g_dp_queue_manager_cond);
		pthread_exit(NULL);
		return 0;
	}

	do {

		if (g_dp_queue_manager_tid <= 0) {
			TRACE_INFO("queue-manager is closed by other thread");
			break;
		}

		// check wifi_direct first
		if (dp_network_is_wifi_direct() == 1 && __dp_queue_manager_check_queue(&g_dp_queue_network_wifi_direct) < 0) {
			TRACE_ERROR("download-agent is busy, try again after 15 seconds");
		} else { // enter here if disable wifi-direct or download-agent is available
			int network_status = dp_network_get_status();
			if (network_status != DP_NETWORK_OFF) {
				TRACE_INFO("queue-manager try to check queue network:%d", network_status);
				if (g_dp_queue_network_all != NULL && __dp_queue_manager_check_queue(&g_dp_queue_network_all) < 0) {
					TRACE_ERROR("download-agent is busy, try again after 15 seconds");
				} else {
					dp_queue_fmt **queue = __dp_queue_manager_get_queue(network_status);
					if (__dp_queue_manager_check_queue(queue) < 0) {
						TRACE_ERROR("download-agent is busy, try again after 15 seconds");
					}
				}
			}
		}

		struct timeval now;
		struct timespec ts;
		gettimeofday(&now, NULL);
		ts.tv_sec = now.tv_sec + 5;
		ts.tv_nsec = now.tv_usec * 1000;
		CLIENT_MUTEX_LOCK(&g_dp_queue_manager_mutex);
		pthread_cond_timedwait(&g_dp_queue_manager_cond, &g_dp_queue_manager_mutex, &ts);
		CLIENT_MUTEX_UNLOCK(&g_dp_queue_manager_mutex);

	} while (g_dp_queue_manager_tid > 0);

	TRACE_DEBUG("queue-manager's working is done");
	dp_deinit_agent();
	dp_queue_clear_all(&g_dp_queue_network_all);
	pthread_cond_destroy(&g_dp_queue_manager_cond);
	pthread_exit(NULL);
	return 0;
}

static int __dp_queue_manager_start()
{
	if (g_dp_queue_manager_tid == 0 ||
			pthread_kill(g_dp_queue_manager_tid, 0) == ESRCH) {
		TRACE_DEBUG("try to create queue-manager");
		if (pthread_create(&g_dp_queue_manager_tid, NULL,
				__dp_queue_manager, NULL) != 0) {
			TRACE_STRERROR("failed to create queue-manager");
			return -1;
		}
	}
	return 0;
}

void dp_queue_manager_wake_up()
{
	if (g_dp_queue_manager_tid > 0 &&
			pthread_kill(g_dp_queue_manager_tid, 0) != ESRCH) {
		int locked = CLIENT_MUTEX_TRYLOCK(&g_dp_queue_manager_mutex);
		if (locked == 0) {
			pthread_cond_signal(&g_dp_queue_manager_cond);
			CLIENT_MUTEX_UNLOCK(&g_dp_queue_manager_mutex);
		}
	} else {
		__dp_queue_manager_start();
	}
}

void dp_queue_manager_kill()
{
	if (g_dp_queue_manager_tid > 0 &&
			pthread_kill(g_dp_queue_manager_tid, 0) != ESRCH) {
		//send signal to queue thread
		g_dp_queue_manager_tid = 0;
		CLIENT_MUTEX_LOCK(&g_dp_queue_manager_mutex);
		pthread_cond_signal(&g_dp_queue_manager_cond);
		CLIENT_MUTEX_UNLOCK(&g_dp_queue_manager_mutex);
		pthread_cancel(g_dp_queue_manager_tid);
		int status;
		pthread_join(g_dp_queue_manager_tid, (void **)&status);
	}
}
