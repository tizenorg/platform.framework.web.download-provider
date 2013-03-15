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

#ifndef _Download_Agent_Plugin_Http_Interface_H
#define _Download_Agent_Plugin_Http_Interface_H

#include "download-agent-type.h"
#include "download-agent-http-msg-handler.h"

typedef enum {
	PI_HTTP_METHOD_GET = 1,
	PI_HTTP_METHOD_POST = 2,
	PI_HTTP_METHOD_HEAD = 3
} pi_http_method_t;


typedef struct _input_for_tranx_t {
	pi_http_method_t http_method;

	char *proxy_addr;
	queue_t *queue;

	http_msg_request_t* http_msg_request;
} input_for_tranx_t;



da_result_t  PI_http_init(void);
void PI_http_deinit(void);

da_result_t  PI_http_start_transaction(const input_for_tranx_t *input_for_tranx, int *out_tranx_id);
da_result_t  PI_http_cancel_transaction(int in_tranx_id, da_bool_t abort_option);
da_result_t  PI_http_disconnect_transaction(int in_tranx_id);
void PI_http_pause_transaction(int transaction_id);
void PI_http_unpause_transaction(int transaction_id);

#endif

