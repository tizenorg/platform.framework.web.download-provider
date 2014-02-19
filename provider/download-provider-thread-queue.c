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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <time.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <signal.h>

#include <pthread.h>

#include "download-provider.h"
#include "download-provider-log.h"
#include "download-provider-config.h"
#include "download-provider-slots.h"
#include "download-provider-socket.h"
#include "download-provider-pthread.h"
#include "download-provider-db.h"
#include "download-provider-queue.h"
#include "download-provider-network.h"
#include "download-provider-da-interface.h"

void dp_terminate(int signo);

pthread_mutex_t g_dp_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_dp_queue_cond = PTHREAD_COND_INITIALIZER;


//////////////////////////////////////////////////////////////////////////
/// @brief		check network status is matched with the type setted by user
/// @return	matched : 0 mispatch : -1
static int __is_matched_network(dp_network_type now_state, dp_network_type setted_state)
{
	if (now_state == setted_state
		|| now_state == DP_NETWORK_TYPE_ETHERNET
		|| setted_state == DP_NETWORK_TYPE_ALL)
		return 0;
	#if 0
	if (setted_state == DP_NETWORK_TYPE_ALL
		|| setted_state == DP_NETWORK_TYPE_DATA_NETWORK
		|| now_state == DP_NETWORK_TYPE_WIFI
		|| now_state == DP_NETWORK_TYPE_ETHERNET
		|| (setted_state == DP_NETWORK_TYPE_WIFI
			&& (now_state == DP_NETWORK_TYPE_WIFI
				|| now_state == DP_NETWORK_TYPE_ETHERNET))
		)
		return 0;
	#endif
	return -1;
}

//////////////////////////////////////////////////////////////////////////
/// @brief		the count of slot downloading currently
static unsigned __get_active_count(dp_request_slots *requests)
{
	unsigned count = 0;
	unsigned i = 0;

	if (requests == NULL)
		return 0;

	for (i = 0; i < DP_MAX_REQUEST; i++) {
		int locked = pthread_mutex_trylock(&requests[i].mutex);
		// locking failure means it used by other thread.
		if (locked == 0) {
			if (requests[i].request != NULL) {
				if (requests[i].request->state == DP_STATE_CONNECTING ||
					requests[i].request->state == DP_STATE_DOWNLOADING)
					count++;
			}
			CLIENT_MUTEX_UNLOCK(&requests[i].mutex);
		}
	}
	return count;
}

//////////////////////////////////////////////////////////////////////////
/// @brief		index of slot which last time is oldest
static int __get_oldest_request_with_network(dp_request_slots *requests, dp_state_type state, dp_network_type now_state)
{
	int i = 0;
	int oldest_time = (int)time(NULL);
	oldest_time++; // most last time
	int oldest_index = -1;

	if (requests == NULL)
		return -1;

	for (i = 0; i < DP_MAX_REQUEST; i++) {
		int locked = pthread_mutex_trylock(&requests[i].mutex);
		// locking failure means it used by other thread.
		if (locked == 0) {
			if (requests[i].request != NULL) {
				if (requests[i].request->state == state &&
					requests[i].request->start_time > 0 &&
					requests[i].request->start_time < oldest_time &&
					__is_matched_network(now_state,
							requests[i].request->network_type) == 0) {
					oldest_time = requests[i].request->start_time;
					oldest_index = i;
				}
			}
			CLIENT_MUTEX_UNLOCK(&requests[i].mutex);
		}
	}
	return oldest_index;
}

//////////////////////////////////////////////////////////////////////////
/// @brief THREAD function for calling da_start_download_with_extension.
/// @warning da_start_download_with_extension can take long time
/// @param the pointer of memory slot allocated for this request.
/// @todo simplify da_start_download_with_extension
static void *__request_download_start_agent(void *args)
{
	dp_error_type errcode = DP_ERROR_NONE;

	dp_request_slots *request_slot = (dp_request_slots *) args;
	if (request_slot == NULL) {
		TRACE_ERROR("[NULL-CHECK] request_slot");
		pthread_exit(NULL);
		return 0;
	}

	CLIENT_MUTEX_LOCK(&request_slot->mutex);

	if (request_slot->request == NULL) {
		TRACE_ERROR("[NULL-CHECK] request");
		CLIENT_MUTEX_UNLOCK(&request_slot->mutex);
		pthread_exit(NULL);
		return 0;
	}

	dp_request *request = request_slot->request;

	if (dp_is_alive_download(request->agent_id)) {
		errcode = dp_resume_agent_download(request->agent_id);
	} else {
		// call agent start function
		errcode = dp_start_agent_download(request_slot);
	}
	// send to state callback.
	if (errcode == DP_ERROR_NONE) {
		// CONNECTING
		request->state = DP_STATE_CONNECTING;
		request->error = DP_ERROR_NONE;
		request->startcount++;
		if (dp_db_set_column
				(request->id, DP_DB_TABLE_LOG, DP_DB_COL_STARTCOUNT,
				DP_DB_COL_TYPE_INT, &request->startcount) < 0)
			TRACE_ERROR("[ERROR][%d][SQL]", request->id);
	} else if (errcode == DP_ERROR_TOO_MANY_DOWNLOADS) {
		// PENDED
		request->state = DP_STATE_QUEUED;
		request->error = DP_ERROR_TOO_MANY_DOWNLOADS;
	} else if (errcode == DP_ERROR_CONNECTION_FAILED) {
		// FAILED
		request->state = DP_STATE_FAILED;
		request->error = DP_ERROR_CONNECTION_FAILED;
		if (request->group != NULL &&
			request->group->event_socket >= 0) {
			dp_ipc_send_event(request->group->event_socket,
				request->id, request->state, request->error, 0);
		}
	} else if (errcode == DP_ERROR_INVALID_STATE) {
		// API FAILED
		request->error = DP_ERROR_INVALID_STATE;
		if (request->group != NULL &&
			request->group->event_socket >= 0) {
			dp_ipc_send_event(request->group->event_socket,
				request->id, request->state, request->error, 0);
		}
	} else {
		request->state = DP_STATE_FAILED;
		request->error = errcode;
		if (request->group != NULL &&
			request->group->event_socket >= 0) {
			dp_ipc_send_event(request->group->event_socket,
				request->id, request->state, request->error, 0);
		}
	}
	if (dp_db_request_update_status(request->id, request->state, request->error) < 0) {
		TRACE_ERROR("[ERROR][%d][SQL]", request->id);
	}

	CLIENT_MUTEX_UNLOCK(&request_slot->mutex);

	if (errcode == DP_ERROR_NONE) {
		TRACE_DEBUG("try other requests -----------------");
		dp_thread_queue_manager_wake_up();
	}

	pthread_exit(NULL);
	return 0;
}

//////////////////////////////////////////////////////////////////////////
/// @brief create thread.
/// @warning if failed to create thread, change to PENED.
/// @param the pointer of memory slot allocated for this request.
static int __request_download_start_thread(dp_request_slots *request_slot)
{
	// declare all resources
	pthread_t thread_pid;
	pthread_attr_t thread_attr;

	if (request_slot == NULL || request_slot->request == NULL) {
		TRACE_ERROR("[CRITICAL] Invalid Address");
		return -1;
	}
	dp_request *request = request_slot->request;

	// initialize
	if (pthread_attr_init(&thread_attr) != 0) {
		TRACE_STRERROR("[ERROR][%d] pthread_attr_init", request->id);
		return -1;
	}
	if (pthread_attr_setdetachstate(&thread_attr,
									PTHREAD_CREATE_DETACHED) != 0) {
		TRACE_STRERROR
			("[ERROR][%d] pthread_attr_setdetachstate", request->id);
		return -1;
	}

	request->state = DP_STATE_CONNECTING;
	if (pthread_create(&thread_pid, &thread_attr,
					__request_download_start_agent, request_slot) != 0) {
		TRACE_STRERROR("[ERROR][%d] pthread_create", request->id);
		pthread_attr_destroy(&thread_attr);
		request->state = DP_STATE_QUEUED;
		return -1;
	}
	pthread_attr_destroy(&thread_attr);
	return 0;
}


void dp_thread_queue_manager_wake_up()
{
	CLIENT_MUTEX_LOCK(&(g_dp_queue_mutex));
	pthread_cond_signal(&g_dp_queue_cond);
	CLIENT_MUTEX_UNLOCK(&(g_dp_queue_mutex));
}


// Main role : start download, check status of queue.
// No timeout. Wake up by Signal be sent from other thread
void *dp_thread_queue_manager(void *arg)
{
	int i;
	int active_count;
	dp_client_group *group = NULL;
	dp_request *request = NULL;

	dp_privates *privates = (dp_privates*)arg;
	if (!privates) {
		TRACE_ERROR("[CRITICAL] Invalid Address");
		dp_terminate(SIGTERM);
		pthread_exit(NULL);
		return 0;
	}

	pthread_cond_init(&g_dp_queue_cond, NULL);
	while (privates != NULL && privates->listen_fd >= 0) {

		CLIENT_MUTEX_LOCK(&(g_dp_queue_mutex));
		pthread_cond_wait(&g_dp_queue_cond, &g_dp_queue_mutex);

		// request thread response instantly
		CLIENT_MUTEX_UNLOCK(&(g_dp_queue_mutex));

		if (privates == NULL || privates->requests == NULL ||
			privates->listen_fd < 0) {
			TRACE_DEBUG("Terminate Thread");
			break;
		}

		// if failed to initialize the callback for checking connection
		if (!privates->connection)
			privates->network_status =
				dp_get_network_connection_instant_status();

#ifdef SUPPORT_WIFI_DIRECT
		if (privates->is_connected_wifi_direct == 0) {
			if (dp_network_wifi_direct_is_connected() == 0) {
				// wifi-direct activated. pass.
				privates->is_connected_wifi_direct = 1;
			}
		}
#endif

		if (privates->network_status == DP_NETWORK_TYPE_OFF &&
			privates->is_connected_wifi_direct == 0) {
			TRACE_DEBUG("[CHECK NETWORK STATE]");
			continue;
		}

		active_count = __get_active_count(privates->requests);

		TRACE_DEBUG("Status Queue: now active[%d] max[%d]",
					active_count, DP_MAX_DOWNLOAD_AT_ONCE);

		// Start Conditions
		// 1. state is QUEUED
		// 2. 1 QUEUED per 1 Group : need not to check max limitation.!!
		//    if no group, it will be started later.
		// 3. most old last time : below conditions need max limitation.
		// 4. match network connection type
		// 5. wifi-direct on

		// search group having 1 queued_count
		// guarantee 1 instant download per 1 group
		if (active_count >= DP_MAX_DOWNLOAD_AT_ONCE) {
			for (i = 0; i < DP_MAX_REQUEST; i++) {
				CLIENT_MUTEX_LOCK(&privates->requests[i].mutex);
				request = privates->requests[i].request;
				if (request != NULL && request->state == DP_STATE_QUEUED) {
					group = privates->requests[i].request->group;
					if (group && group->queued_count == 1) {
						if (__is_matched_network
							(privates->network_status,
							request->network_type) == 0 ||
							(privates->is_connected_wifi_direct == 1 &&
								request->network_type ==
									DP_NETWORK_TYPE_WIFI_DIRECT)) {
							if (__request_download_start_thread(&privates->requests[i]) == 0) {
								TRACE_DEBUG
									("[Guarantee Intant Download] Group [%s]", group->pkgname);
								active_count++;
							}
						}
					}
				}
				CLIENT_MUTEX_UNLOCK(&privates->requests[i].mutex);
			}
		}

		if (active_count >= DP_MAX_DOWNLOAD_AT_ONCE) {
			TRACE_DEBUG("[BUSY] Active[%d] Max[%d]",
				active_count, DP_MAX_DOWNLOAD_AT_ONCE);
			continue;
		}

		// can start download more.
		// search oldest request
		while(active_count < DP_MAX_DOWNLOAD_AT_ONCE) {

#ifdef SUPPORT_WIFI_DIRECT
			// WIFI-Direct first
			if (privates->is_connected_wifi_direct == 1) {
				i = __get_oldest_request_with_network(privates->requests,
						DP_STATE_QUEUED, DP_NETWORK_TYPE_WIFI_DIRECT);
				if (i >= 0) {
					TRACE_DEBUG("Found WIFI-Direct request %d", i);
					if (__request_download_start_thread(&privates->requests[i]) == 0)
						active_count++;
					continue;
				}
			}
#endif

			i = __get_oldest_request_with_network(privates->requests,
					DP_STATE_QUEUED, privates->network_status);
			if (i < 0) {
				TRACE_DEBUG
					("No Request now active[%d] max[%d]",
					active_count, DP_MAX_DOWNLOAD_AT_ONCE);
				break;
			}
			TRACE_DEBUG("QUEUE Status now %d active %d/%d", i,
				active_count, DP_MAX_DOWNLOAD_AT_ONCE);
			__request_download_start_thread(&privates->requests[i]);
			active_count++;
		}
	}
	pthread_cond_destroy(&g_dp_queue_cond);
	pthread_exit(NULL);
	return 0;
}
