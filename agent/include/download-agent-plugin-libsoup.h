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

#ifndef _Download_Agent_Plugin_Libsoup_H
#define _Download_Agent_Plugin_Libsoup_H

#include <string.h>
#include <libsoup/soup.h>

#include "download-agent-http-queue.h"
#include "download-agent-pthread.h"
#include "download-agent-plugin-http-interface.h"

typedef struct  _pi_session_table_t {
	da_bool_t is_using;
	SoupSession *session;
	SoupMessage *msg;
	queue_t *queue;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	da_bool_t is_paused;
} pi_session_table_t;

extern pi_session_table_t pi_session_table[];

#define MAX_SESSION_COUNT	DA_MAX_DOWNLOAD_REQ_AT_ONCE
#define MAX_TIMEOUT		60	// second

#define IS_VALID_SESSION_TABLE_ENTRY(ENTRY)		((((ENTRY) < 0) || ((ENTRY) > MAX_SESSION_COUNT-1)) ? 0 : 1)


#define GET_SESSION_FROM_TABLE_ENTRY(ENTRY)	(pi_session_table[ENTRY].session)
#define GET_MSG_FROM_TABLE_ENTRY(ENTRY)	(pi_session_table[ENTRY].msg)
#define GET_QUEUE_FROM_TABLE_ENTRY(ENTRY)		(pi_session_table[ENTRY].queue)


da_bool_t _pi_http_is_valid_input_for_tranx(const input_for_tranx_t *input_for_tranx);

void _pi_http_init_session_table_entry(const int in_session_table_entry);
void _pi_http_destroy_session_table_entry(const int in_session_table_entry);
int _pi_http_get_avaiable_session_table_entry(void);

da_bool_t _pi_http_register_queue_to_session_table(const int session_table_entry, const queue_t *in_queue);
da_bool_t _pi_http_register_session_to_session_table(const int in_session_table_entry, SoupSession *session);
da_bool_t _pi_http_register_msg_to_session_table(const int in_session_table_entry, SoupMessage *msg);

queue_t *_pi_http_get_queue_from_session_table_entry(const int in_session_table_entry);
int _pi_http_get_session_table_entry_from_message(SoupMessage *msg);

void _pi_http_store_read_data_to_queue(SoupMessage *msg, const char *body_data, int received_body_len);
void _pi_http_store_read_header_to_queue(SoupMessage *msg, const char *sniffedType);
void _pi_http_store_neterr_to_queue(SoupMessage *msg);


void _pi_http_finished_cb(SoupSession *session, SoupMessage *msg, gpointer data);
void _pi_http_restarted_cb(SoupMessage *msg, gpointer data);
void _pi_http_gotheaders_cb(SoupMessage *msg, gpointer data);
void _pi_http_contentsniffed_cb(SoupMessage *msg, const char *sniffedType, GHashTable *params, gpointer data);
void _pi_http_gotchunk_cb(SoupMessage *msg, SoupBuffer *chunk, gpointer data);


#endif
