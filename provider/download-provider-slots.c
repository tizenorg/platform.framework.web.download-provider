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
#include <stdio.h>
#include <stdlib.h>

#include <time.h>
#include <sys/time.h>

#include "download-provider.h"
#include "download-provider-log.h"
#include "download-provider-pthread.h"

#include "download-provider-slots.h"
#include "download-provider-socket.h"

dp_group_slots *dp_client_group_slots_new(int size)
{
	dp_group_slots *slots = NULL;
	if (size <= 0)
		return NULL;
	slots = (dp_group_slots *) calloc(size,
			sizeof(dp_group_slots));
	return slots;
}

dp_request_slots *dp_request_slots_new(int size)
{
	int i = 0;
	dp_request_slots *slots = NULL;
	if (size <= 0)
		return NULL;
	slots = (dp_request_slots *) calloc(size,
			sizeof(dp_request_slots));
	for (; i < size; i++)
		CLIENT_MUTEX_INIT(&slots[i].mutex, NULL);
	return slots;
}

void dp_request_init(dp_request *request)
{
	if (request == NULL)
		return ;

	request->id = -1;
	request->agent_id = -1;
	request->create_time = 0;
	request->start_time = 0;
	request->pause_time = 0;
	request->stop_time = 0;
	request->state = DP_STATE_NONE;
	request->error = DP_ERROR_NONE;
	request->state_cb = 0;
	request->progress_cb = 0;
	request->progress_lasttime = 0;
	request->received_size = 0;
	request->file_size = 0;
	request->network_type = DP_NETWORK_TYPE_ALL;
	request->startcount = 0;
	request->auto_notification = 0;
	request->noti_priv_id = -1;
	request->packagename = NULL;
	request->group = NULL;
}

dp_request *dp_request_new()
{
	dp_request *request = NULL;
	request = (dp_request *) calloc(1,
			sizeof(dp_request));
	if (!request)
		return NULL;
	dp_request_init(request);
	return request;
}

int dp_request_slot_free(dp_request_slots *request_slot)
{
	if (request_slot == NULL)
		return -1;
	CLIENT_MUTEX_LOCK(&request_slot->mutex);
	dp_request_free(request_slot->request);
	request_slot->request = NULL;
	CLIENT_MUTEX_UNLOCK(&request_slot->mutex);
	return 0;
}

int dp_request_free(dp_request *request)
{
	if (request == NULL)
		return -1;
	free(request->packagename);
	dp_request_init(request);
	free(request);
	return 0;
}

int dp_client_group_free(dp_client_group *group)
{
	if (group != NULL) {
		if (group->cmd_socket > 0)
			dp_socket_free(group->cmd_socket);
		group->cmd_socket = -1;
		if (group->event_socket > 0)
			dp_socket_free(group->event_socket);
		group->event_socket = -1;
		group->queued_count = 0;
		free(group->pkgname);
		free(group->smack_label);
		free(group);
	}
	return 0;
}

int dp_client_group_slots_free(dp_group_slots *slots, int size)
{
	int i = 0;
	if (slots) {
		for (; i < size; i++) {
			if (slots->group)
				dp_client_group_free(slots->group);
			slots->group = NULL;
		}
		free(slots);
	}
	slots = NULL;
	return 0;
}

int dp_request_slots_free(dp_request_slots *slots, int size)
{
	int i = 0;
	if (slots != NULL) {
		for (; i < size; i++) {
			dp_request_free(slots[i].request);
			slots[i].request = NULL;
			CLIENT_MUTEX_DESTROY(&slots[i].mutex);
		}
		free(slots);
	}
	slots = NULL;
	return 0;
}

//////////////////////////////////////////////////////////////////////////
/// @brief	return count of requests in slot
int dp_get_request_count(dp_request_slots *slots)
{
	int i = 0;
	int count = 0;

	if (!slots)
		return -1;

	for (i = 0; i < DP_MAX_REQUEST; i++) {
		if (slots[i].request != NULL)
			count++;
	}
	return count;
}
