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

#ifndef _Download_Agent_Client_Mgr_H
#define _Download_Agent_Client_Mgr_H

#include <string.h>

#include "download-agent-type.h"
#include "download-agent-interface.h"

#include "download-agent-pthread.h"

typedef enum {
	Q_CLIENT_NOTI_TYPE_STARTED_INFO = 0,
	Q_CLIENT_NOTI_TYPE_PROGRESS_INFO,
	Q_CLIENT_NOTI_TYPE_PAUSED_INFO,
	Q_CLIENT_NOTI_TYPE_FINISHED_INFO,
	Q_CLIENT_NOTI_TYPE_TERMINATE,
} client_noti_type;

typedef struct _client_noti_t client_noti_t;
struct _client_noti_t {
	int slot_id;
	void *user_data;
	client_noti_type noti_type;
	union _client_type {
		user_download_info_t update_dl_info;
		user_progress_info_t update_progress_info;
		user_paused_info_t paused_info;
		user_finished_info_t finished_info;
	} type;

	client_noti_t *next;
};

typedef struct _client_queue_t {
	da_bool_t having_data;
	client_noti_t *client_q_head;
	pthread_mutex_t mutex_client_queue;
	pthread_cond_t cond_client_queue;
} client_queue_t;

typedef struct _client_app_info_t {
	da_client_cb_t client_callback;
	char *client_user_agent;
} client_app_info_t;

typedef struct _client_app_mgr_t {
	da_bool_t is_init;
	client_queue_t client_queue;
	client_app_info_t client_app_info;
	pthread_t thread_id;
	da_bool_t is_thread_init;
	pthread_mutex_t mutex_client_mgr;
} client_app_mgr_t;

da_result_t init_client_app_mgr(void);
da_bool_t is_client_app_mgr_init(void);

da_result_t reg_client_app(da_client_cb_t *da_client_callback);
da_result_t dereg_client_app(void);

da_result_t send_client_paused_info (int slot_id);
da_result_t send_client_update_dl_info (int slot_id, int dl_id,
		char *file_type, unsigned long int file_size, char *tmp_saved_path,
		char *pure_file_name, char *etag, char *extension);
da_result_t send_client_update_progress_info (int slot_id, int dl_id,
		unsigned long int received_size);
da_result_t send_client_finished_info (int slot_id, int dl_id,
		char *saved_path, char *etag, int error, int http_status);

char *get_client_user_agent_string(void);

void push_client_noti(client_noti_t *client_noti);

#endif
