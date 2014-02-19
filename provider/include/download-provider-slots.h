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

#ifndef DOWNLOAD_PROVIDER2_SLOTS_H
#define DOWNLOAD_PROVIDER2_SLOTS_H

#include "download-provider.h"
#include "download-provider-pthread.h"

// Backgound Daemon should has the limitation of resource.
#define DP_MAX_GROUP 15
#define DP_MAX_REQUEST 64

typedef struct {
	pid_t pid;
	uid_t uid;
	gid_t gid;
} dp_credential;

typedef struct {
	// send command * get return value.
	int cmd_socket;
	// send event to client
	int event_socket;
	unsigned queued_count; // start ++, finish or failed -- ( queue & active )
	// fill by app-manager
	char *pkgname;
	dp_credential credential;
	char *smack_label;
} dp_client_group;

typedef struct {
	int id; // ID created in create request in requests thread.
	int agent_id;
	int create_time;
	int start_time; // setted by START command
	int pause_time; // setted by PAUSE command
	int stop_time; // time setted by finished_cb
	int noti_priv_id;
	unsigned state_cb; // set : 1 unset : 0
	unsigned progress_cb; // set : 1 unset : 0
	unsigned startcount;
	unsigned auto_notification;
	unsigned ip_changed;
	dp_state_type state; // downloading state
	dp_error_type error;
	dp_network_type network_type;
	size_t progress_lasttime;
	unsigned long long received_size; // progress
	unsigned long long file_size;
	char *packagename;
	dp_client_group *group; // indicate dp_client_group included this request
} dp_request;

typedef struct {
	dp_client_group *group;
} dp_group_slots;

typedef struct {
	pthread_mutex_t mutex;
	dp_request *request;
} dp_request_slots;


// functions
dp_group_slots *dp_client_group_slots_new(int size);
dp_request_slots *dp_request_slots_new(int size);
int dp_client_group_free(dp_client_group *group);
int dp_client_group_slots_free(dp_group_slots *slots, int size);
dp_request *dp_request_new();
void dp_request_init(dp_request *request);
int dp_request_free(dp_request *request);
int dp_request_slot_free(dp_request_slots *request_slot);
int dp_request_slots_free(dp_request_slots *slots, int size);
int dp_get_request_count(dp_request_slots *slots);

#endif
