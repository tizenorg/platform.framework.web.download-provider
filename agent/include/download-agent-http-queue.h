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

#ifndef _Download_Agent_Http_Queue_H
#define _Download_Agent_Http_Queue_H


#include "download-agent-type.h"
#include "download-agent-http-msg-handler.h"

#include <pthread.h>
#include <stdlib.h>

#define	MAX_QUEUE_SIZE	1024*64

typedef enum
{
	Q_EVENT_TYPE_DATA_HTTP,
	Q_EVENT_TYPE_DATA_DRM,
	Q_EVENT_TYPE_CONTROL,
}q_event_type;

typedef enum
{
	Q_EVENT_TYPE_CONTROL_NONE = 0,
	Q_EVENT_TYPE_CONTROL_CANCEL,
	Q_EVENT_TYPE_CONTROL_SUSPEND,
	Q_EVENT_TYPE_CONTROL_RESUME,
	Q_EVENT_TYPE_CONTROL_NET_DISCONNECTED,
	Q_EVENT_TYPE_CONTROL_ABORT,
// [090205][jungki]not used yet.
//	Q_EVENT_TYPE_CONTROL_USER_CONFIRM_RESULT,
//	Q_EVENT_TYPE_CONTROL_INSTALL_RESULT,
}q_event_type_control;

typedef enum
{
	Q_EVENT_TYPE_DATA_PACKET,
	Q_EVENT_TYPE_DATA_FINAL,
	Q_EVENT_TYPE_DATA_ABORT,
}q_event_type_data;

typedef struct _q_event_data_http_t
{
	q_event_type_data data_type;

	int status_code;

	http_msg_response_t* http_response_msg;

	int body_len;
	char *body_data;

	da_result_t error_type;
}q_event_data_http_t;

typedef struct _q_event_control_t
{
	q_event_type_control control_type;
}q_event_control_t;

typedef struct _q_event_t q_event_t;
struct _q_event_t
{
	int size;
	q_event_type event_type;
	union _type
	{
		q_event_data_http_t q_event_data_http;
		q_event_control_t q_event_control;
	} type;

	q_event_t *next;
};

typedef struct _queue_t
{
	da_bool_t having_data;

	q_event_t *control_head;
	q_event_t *data_head;

	pthread_mutex_t mutex_queue;
	pthread_cond_t cond_queue;

	int queue_size;
}queue_t;

void Q_init_queue(queue_t *queue);
void Q_destroy_queue(queue_t *queue);

void Q_init_q_event(q_event_t *q_event);
void Q_destroy_q_event(q_event_t **q_event);

da_result_t  Q_make_control_event(q_event_type_control control_type, q_event_t **out_event);

da_result_t  Q_make_http_data_event(q_event_type_data data_type, q_event_t **out_event);
da_result_t  Q_set_status_code_on_http_data_event(q_event_t *q_event, int status_code);
da_result_t  Q_set_http_body_on_http_data_event(q_event_t *q_event, int body_len, char *body_data);
da_result_t  Q_set_error_type_on_http_data_event(q_event_t *q_event, int error_type);


da_bool_t Q_push_event(const queue_t *in_queue, const q_event_t *in_event);
da_bool_t Q_push_event_without_lock(const queue_t *in_queue, const q_event_t *in_event);
void Q_pop_event(const queue_t *in_queue, q_event_t **out_event);

#define GET_IS_Q_HAVING_DATA(QUEUE)		(QUEUE->having_data)

void Q_goto_sleep(const queue_t *in_queue);
void Q_wake_up(const queue_t *in_queue);


#endif
