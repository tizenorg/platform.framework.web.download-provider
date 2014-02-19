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

#include "download-agent-http-queue.h"
#include "download-agent-http-mgr.h"
#include "download-agent-debug.h"
#include "download-agent-pthread.h"

void init_q_event_data_http(q_event_t *q_event);
void init_q_event_control(q_event_t *q_event);

void Q_init_queue(queue_t *queue)
{
	queue->having_data = DA_FALSE;
	queue->control_head = DA_NULL;
	queue->data_head = DA_NULL;
	queue->queue_size = 0;

	_da_thread_mutex_init(&(queue->mutex_queue), DA_NULL);
	_da_thread_cond_init(&(queue->cond_queue), DA_NULL);
}

void Q_destroy_queue(queue_t *queue)
{
	q_event_t *event = DA_NULL;

	DA_LOG_FUNC_LOGV(HTTPManager);

	do {
		Q_pop_event(queue, &event);
		Q_destroy_q_event(&event);
	} while(event);

	queue->having_data = DA_FALSE;
	queue->control_head = DA_NULL;
	queue->data_head = DA_NULL;
	queue->queue_size = 0;

	_da_thread_mutex_destroy(&(queue->mutex_queue));
	_da_thread_cond_destroy(&(queue->cond_queue));
}

void Q_init_q_event(q_event_t *q_event)
{
	switch(q_event->event_type) {
		case Q_EVENT_TYPE_DATA_HTTP:
			init_q_event_data_http(q_event);
			break;

		case Q_EVENT_TYPE_DATA_DRM:
			break;

		case Q_EVENT_TYPE_CONTROL:
			init_q_event_control(q_event);
			break;
	}

	q_event->size = 0;
	q_event->next = DA_NULL;
}

void Q_destroy_q_event(q_event_t **in_q_event)
{
	q_event_t *q_event = DA_NULL;
	q_event = *in_q_event;

	if(q_event == DA_NULL)
		return;

	switch(q_event->event_type)	{
		case Q_EVENT_TYPE_DATA_HTTP:
			init_q_event_data_http(q_event);
			q_event->size = 0;
			q_event->next = DA_NULL;
			free(q_event);
			break;

		case Q_EVENT_TYPE_DATA_DRM:
			q_event->size = 0;
			q_event->next = DA_NULL;
			free(q_event);
			break;

		case Q_EVENT_TYPE_CONTROL:
			init_q_event_control(q_event);
			q_event->size = 0;
			q_event->next = DA_NULL;
			free(q_event);
			break;
	}
}

da_result_t  Q_make_control_event(q_event_type_control control_type, q_event_t **out_event)
{
	da_result_t  ret = DA_RESULT_OK;
	q_event_t *q_event = DA_NULL;

	DA_LOG_FUNC_LOGD(HTTPManager);

	q_event = (q_event_t *)calloc(1, sizeof(q_event_t));
	if(q_event == DA_NULL) {
		DA_LOG_ERR(HTTPManager, "calloc fail for q_event");
		ret = DA_ERR_FAIL_TO_MEMALLOC;

		*out_event = DA_NULL;
	} else {
		q_event->event_type = Q_EVENT_TYPE_CONTROL;
		q_event->type.q_event_control.control_type = control_type;
		q_event->next = DA_NULL;

		*out_event = q_event;
	}

	return ret;
}

da_result_t  Q_make_http_data_event(q_event_type_data data_type, q_event_t **out_event)
{
	da_result_t  ret = DA_RESULT_OK;
	q_event_t *q_event = DA_NULL;

	DA_LOG_FUNC_LOGV(HTTPManager);

	q_event = (q_event_t *)calloc(1, sizeof(q_event_t));
	if(q_event == DA_NULL) {
		DA_LOG_ERR(HTTPManager, "calloc fail for q_event");
		ret = DA_ERR_FAIL_TO_MEMALLOC;
		*out_event = DA_NULL;
	} else {
		q_event->event_type = Q_EVENT_TYPE_DATA_HTTP;
		q_event->type.q_event_data_http.data_type = data_type;
		q_event->next = DA_NULL;

		*out_event = q_event;

//		DA_LOG_VERBOSE(HTTPManager, "made event = %x", *out_event);
	}

	return ret;

}

da_result_t  Q_set_status_code_on_http_data_event(q_event_t *q_event, int status_code)
{
	da_result_t  ret = DA_RESULT_OK;

	DA_LOG_FUNC_LOGV(HTTPManager);

	if(q_event->event_type != Q_EVENT_TYPE_DATA_HTTP) {
		DA_LOG_ERR(HTTPManager, "status_code can be set only for Q_EVENT_TYPE_DATA_HTTP.");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}

	q_event->type.q_event_data_http.status_code = status_code;

//	DA_LOG_VERBOSE(HTTPManager, "status_code = %d, q_event = %x", q_event->type.q_event_data_http.status_code, q_event);

ERR:
	return ret;

}

da_result_t  Q_set_http_body_on_http_data_event(q_event_t *q_event, int body_len, char *body_data)
{
	da_result_t  ret = DA_RESULT_OK;

	DA_LOG_FUNC_LOGV(HTTPManager);

	if(q_event->event_type != Q_EVENT_TYPE_DATA_HTTP) {
		DA_LOG_ERR(HTTPManager, "http body can be set only for Q_EVENT_TYPE_DATA_HTTP.");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}

	q_event->type.q_event_data_http.body_len = body_len;
	q_event->type.q_event_data_http.body_data = body_data;
	q_event->size = body_len;

//	DA_LOG_VERBOSE(HTTPManager, "body_len = %d, body_data = %x, q_event = %x", q_event->type.q_event_data_http.body_len, q_event->type.q_event_data_http.body_data, q_event);

ERR:
	return ret;

}

da_result_t  Q_set_error_type_on_http_data_event(q_event_t *q_event, int error_type)
{
	da_result_t  ret = DA_RESULT_OK;

	DA_LOG_FUNC_LOGV(HTTPManager);

	if(q_event->event_type != Q_EVENT_TYPE_DATA_HTTP) {
		DA_LOG_ERR(HTTPManager, "error_type can be set only for Q_EVENT_TYPE_DATA_HTTP.");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}

	q_event->type.q_event_data_http.error_type = error_type;

	DA_LOG_VERBOSE(HTTPManager, "error_type = %d, q_event = %p", q_event->type.q_event_data_http.error_type, q_event);

ERR:
	return ret;

}

da_bool_t Q_push_event(const queue_t *in_queue, const q_event_t *in_event)
{
	da_bool_t b_ret = DA_FALSE;
	queue_t *queue = (queue_t *)in_queue;

	_da_thread_mutex_lock (&(queue->mutex_queue));
	b_ret = Q_push_event_without_lock(in_queue, in_event);
	_da_thread_mutex_unlock (&(queue->mutex_queue));

	return b_ret;
}

da_bool_t Q_push_event_without_lock(const queue_t *in_queue, const q_event_t *in_event)
{
	da_bool_t b_ret = DA_FALSE;
	queue_t *queue = (queue_t *)in_queue;
	q_event_t *event = (q_event_t *)in_event;
	q_event_type event_type;
	q_event_t *head = DA_NULL;
	q_event_t *cur = DA_NULL;

//	DA_LOG_VERBOSE(HTTPManager, "queue = %x", in_queue);

	event_type = event->event_type;

//	_da_thread_mutex_lock (&(queue->mutex_queue));

	if(event_type == Q_EVENT_TYPE_CONTROL) {
		head = queue->control_head;
		if(head == DA_NULL) {
			queue->control_head = event;
		} else {
			cur = head;

			while(cur->next != DA_NULL) {
				cur = cur->next;
			}
			cur->next= event;
		}
		b_ret = DA_TRUE;
	} else {
		if((event->size == 0) || (queue->queue_size < MAX_QUEUE_SIZE)) {
			head = queue->data_head;
			if(head == DA_NULL)	{
				queue->data_head = event;
			} else {
				cur = head;
				while(cur->next != DA_NULL) {
					cur = cur->next;
				}
				cur->next= event ;
			}

			queue->queue_size += event->size;
//			DA_LOG_VERBOSE(HTTPManager, "queue size is %d", queue->queue_size);

			b_ret = DA_TRUE;
		} else {
			DA_LOG_CRITICAL(HTTPManager, "rejected event's size is %d queue_size %d", event->size, queue->queue_size);
			b_ret = DA_FALSE;
		}
	}

	queue->having_data = DA_TRUE;
	Q_wake_up(queue);
//	_da_thread_mutex_unlock (&(queue->mutex_queue));
	return b_ret;
}

void Q_pop_event(const queue_t *in_queue, q_event_t **out_event)
{
	queue_t *queue = (queue_t*)in_queue;

//	DA_LOG_VERBOSE(HTTPManager, "queue = %x", in_queue);

	/** Pop Priority
	  * 1. If there are control event, control event should pop first
	  * 2. If there is no control event, data event should pop
	  * 3. If there is no control and data event on queue, pop NULL
	 */

	_da_thread_mutex_lock (&(queue->mutex_queue));

	if(queue->control_head != DA_NULL) {/* Priority 1 */
		*out_event = queue->control_head;
		queue->control_head = queue->control_head->next;
	} else {
		if(queue->data_head != DA_NULL) {/* Priority 2 */
			*out_event = queue->data_head;
			queue->data_head = queue->data_head->next;
			queue->queue_size -= (*out_event)->size;
			DA_LOG_VERBOSE(HTTPManager, "queue size is %d", queue->queue_size);
		} else {/* Priority 3 */
			*out_event = DA_NULL;
		}
	}

	if((queue->control_head == DA_NULL) && (queue->data_head == DA_NULL)) {
		queue->having_data = DA_FALSE;
	} else {
		queue->having_data = DA_TRUE;
	}

	_da_thread_mutex_unlock (&(queue->mutex_queue));

}

void Q_goto_sleep(const queue_t *in_queue)
{
	DA_LOG_VERBOSE(HTTPManager, "sleep for %p", in_queue);

//** SHOULD NOT use mutex **//

//	_da_thread_mutex_lock (&(in_queue->mutex_queue));
	_da_thread_cond_wait((pthread_cond_t*)(&(in_queue->cond_queue)),(pthread_mutex_t*) (&(in_queue->mutex_queue)));
//	_da_thread_mutex_unlock (&(in_queue->mutex_queue));
}

void Q_wake_up(const queue_t *in_queue)
{
	DA_LOG_VERBOSE(HTTPManager, "wake up for %p", in_queue);

//** SHOULD NOT use mutex **//

//	_da_thread_mutex_lock (&(in_queue->mutex_queue));
	_da_thread_cond_signal((pthread_cond_t*)(&(in_queue->cond_queue)));
//	_da_thread_mutex_unlock (&(in_queue->mutex_queue));
}

void init_q_event_data_http(q_event_t *q_event)
{
	q_event_data_http_t *q_event_data_http;

	DA_LOG_FUNC_LOGV(HTTPManager);

	if(q_event->event_type == Q_EVENT_TYPE_DATA_HTTP) {
		q_event_data_http = &(q_event->type.q_event_data_http);

		if(q_event_data_http) {
			q_event_data_http->status_code = 0;
			if(q_event_data_http->http_response_msg) {
				http_msg_response_destroy(&(q_event_data_http->http_response_msg));
				q_event_data_http->http_response_msg = DA_NULL;
			}

			if(q_event_data_http->body_len > 0 ) {
				if (q_event_data_http->body_data) {
					free(q_event_data_http->body_data);
					q_event_data_http->body_data = DA_NULL;
				}
			}
			q_event_data_http->error_type = 0;
		}
	}
}

void init_q_event_control(q_event_t *q_event)
{
	q_event_control_t *q_event_control;

	DA_LOG_FUNC_LOGV(HTTPManager);

	if(q_event->event_type == Q_EVENT_TYPE_CONTROL) {
		q_event_control = &(q_event->type.q_event_control);
		if(q_event_control) {
			q_event_control->control_type = Q_EVENT_TYPE_CONTROL_NONE;
		}
	}

}
