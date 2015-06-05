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

#ifndef DOWNLOAD_PROVIDER_CLIENT_H
#define DOWNLOAD_PROVIDER_CLIENT_H

#include <pthread.h>
#include <download-provider.h>

typedef struct {
	int state; // downloading state, to prevent the crash, placed at the head of structure.
	int id; // ID created in create request in requests thread.
	int agent_id;
	int error;
	int network_type;
	int access_time;
	unsigned state_cb; // set : 1 unset : 0
	unsigned progress_cb; // set : 1 unset : 0
	unsigned startcount;
	size_t progress_lasttime;
	unsigned long long received_size; // progress
	unsigned long long file_size;
	dp_content_type content_type;
	int noti_type;
	int noti_priv_id;
	void *next;
} dp_request_fmt;

typedef struct {
	int channel; // ipc , if negative means dummy client
	int notify;  // event
	int access_time;
	void *dbhandle;
	dp_request_fmt *requests;
} dp_client_fmt;

void *dp_client_request_thread(void *arg);
char *dp_print_state(int state);
char *dp_print_errorcode(int errorcode);
char *dp_print_section(short section);
char *dp_print_property(unsigned property);
void dp_request_create(dp_client_fmt *client, dp_request_fmt *request);
void dp_request_free(dp_request_fmt *request);
int dp_request_destroy(dp_client_fmt *client, dp_ipc_fmt *ipc_info, dp_request_fmt *requestp);
void dp_client_clear_requests(void *slotp);

#endif
