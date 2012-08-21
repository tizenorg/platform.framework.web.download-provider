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
 * @file		download-agent-client-mgr.h
 * @brief
 * @author		Keunsoon Lee(keunsoon.lee@samsung.com)
 ***/

#ifndef _Download_Agent_Client_Mgr_H
#define _Download_Agent_Client_Mgr_H

#include <string.h>

#include "download-agent-type.h"
#include "download-agent-interface.h"

#include "download-agent-pthread.h"

typedef enum {
	Q_CLIENT_NOTI_TYPE_UPDATE_DL_INFO,
	Q_CLIENT_NOTI_TYPE_UPDATE_DOWNLOADING_INFO,
	Q_CLIENT_NOTI_TYPE_SEND_STATE,
	Q_CLIENT_NOTI_TYPE_TERMINATE,
} client_noti_type;

typedef struct _client_noti_t client_noti_t;
struct _client_noti_t {
	int download_id;
	void *user_data;
	client_noti_type noti_type;
	union _client_type {
		user_download_info_t update_dl_info ;
		user_downloading_info_t update_downloading_info ;
		user_notify_info_t da_state_info ;
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
	da_bool_t is_using;
	da_bool_t is_manual_download;
	da_client_cb_t client_callback;
	char *client_user_agent;
} client_app_info_t;

typedef struct _client_app_mgr_t {
	da_bool_t is_init;
	da_bool_t is_manual_download;
	client_queue_t client_queue;
	client_app_info_t client_app_info;
	pthread_t thread_id;
	da_bool_t is_thread_init;
	pthread_mutex_t mutex_client_mgr;
} client_app_mgr_t;

da_result_t init_client_app_mgr(void);
da_bool_t is_client_app_mgr_init(void);

da_result_t reg_client_app(da_client_cb_t *da_client_callback,
	da_download_managing_method download_method);
da_result_t dereg_client_app(void);

da_result_t send_client_da_state (int download_id, da_state state, int err);
da_result_t send_client_update_dl_info (int download_id, int dl_req_id,
		char *file_type, unsigned long int file_size, char *tmp_saved_path,
		char *http_response_header, char *http_raw_data);
da_result_t send_client_update_downloading_info (int download_id, int dl_req_id,
		unsigned long int total_received_size, char *saved_path);

da_result_t get_client_download_path(char **out_path);
char *get_client_user_agent_string(void);

da_bool_t is_this_client_available(void);
da_bool_t is_this_client_manual_download_type(void);

void push_client_noti(client_noti_t *client_noti);

#endif
