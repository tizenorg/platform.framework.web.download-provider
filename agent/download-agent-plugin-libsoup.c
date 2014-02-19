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
#include <string.h>

#include "download-agent-debug.h"
#include "download-agent-plugin-libsoup.h"
#include "download-agent-http-misc.h"
#include "download-agent-utils.h"
#include "download-agent-pthread.h"

pthread_mutex_t mutex_for_session_table = PTHREAD_MUTEX_INITIALIZER;

pi_session_table_t pi_session_table[MAX_SESSION_COUNT] = { { 0, }, };
da_bool_t using_content_sniffing;

da_bool_t _pi_http_is_this_session_table_entry_using(
		const int in_session_table_entry);

da_result_t PI_http_init(void)
{
	DA_LOG_FUNC_LOGV(HTTPManager);

	using_content_sniffing = DA_TRUE;

	return DA_RESULT_OK;
}

void PI_http_deinit(void)
{
	DA_LOG_FUNC_LOGV(HTTPManager);

	return;
}

da_result_t _set_proxy_on_soup_session(SoupSession *session, char *proxy_addr)
{
	da_result_t ret = DA_RESULT_OK;


	if (proxy_addr && strlen(proxy_addr) > 0) {
		DA_SECURE_LOGD("received proxy = %s \n", proxy_addr);
		if (!strstr(proxy_addr, "0.0.0.0")) {
			if (strstr((const char *)proxy_addr, "http") == DA_NULL) {
				DA_LOG_VERBOSE(Default,"There is no \"http://\" on received uri, so, add it.");

				char *tmp_str = DA_NULL;
				int needed_len = 0;

				needed_len = strlen(proxy_addr) + strlen(
						SCHEME_HTTP) + 1;
				tmp_str = (char *) calloc(1, needed_len);
				if (!tmp_str) {
					DA_LOG_ERR(HTTPManager,"DA_ERR_FAIL_TO_MEMALLOC");
					ret = DA_ERR_FAIL_TO_MEMALLOC;
					goto ERR;
				}
				snprintf(tmp_str, needed_len, "%s%s",
						SCHEME_HTTP, proxy_addr);

				g_object_set(session, SOUP_SESSION_PROXY_URI,
						soup_uri_new(tmp_str), NULL);

				free(tmp_str);
			} else {
				DA_LOG(HTTPManager,"There is \"http\" on uri, so, push this address to soup directly.");
				g_object_set(session, SOUP_SESSION_PROXY_URI,
						soup_uri_new(proxy_addr), NULL);
			}
		}
	} else {
		DA_LOG_VERBOSE(HTTPManager,"There is no proxy value");
	}
ERR:
	return ret;
}

void _fill_soup_msg_header(SoupMessage *msg,
		const input_for_tranx_t *input_for_tranx)
{
	SoupMessageHeaders *headers = msg->request_headers;

	http_msg_request_t *input_http_msg_request;
	http_msg_iter_t http_msg_iter;
	http_msg_iter_t http_msg_iter_pre;

	char *field;
	char *value;

	input_http_msg_request = input_for_tranx->http_msg_request;

	http_msg_request_get_iter(input_http_msg_request, &http_msg_iter);
	http_msg_iter_pre = http_msg_iter;
	while (http_msg_get_field_with_iter(&http_msg_iter, &field, &value)) {
		if ((field != DA_NULL) && (value != DA_NULL)) {
			DA_SECURE_LOGD("[%s] %s", field, value);
			soup_message_headers_append(headers, field, value);
		}
		http_msg_iter_pre = http_msg_iter;
	}

	if (input_http_msg_request->http_body) {
		char body_len_str[16] = { 0, };
		int body_len = strlen(input_http_msg_request->http_body);

		snprintf(body_len_str, sizeof(body_len_str), "%d", body_len);

		soup_message_headers_append(headers, "Content-Length",
				body_len_str);
		soup_message_headers_append(headers, "Content-Type",
				"text/plain");
		soup_message_body_append(msg->request_body, SOUP_MEMORY_COPY,
				input_http_msg_request->http_body, body_len);
	}
}

da_result_t PI_http_start_transaction(const input_for_tranx_t *input_for_tranx,
		int *out_tranx_id)
{
	da_result_t ret = DA_RESULT_OK;
	int session_table_entry = -1;
	pi_http_method_t pi_http_method = PI_HTTP_METHOD_GET;
	queue_t *queue = DA_NULL;
	char *url = DA_NULL;
	SoupSession *session = DA_NULL;
	SoupMessage *msg = DA_NULL;

	DA_LOG_FUNC_LOGV(HTTPManager);

	if (DA_FALSE == _pi_http_is_valid_input_for_tranx(input_for_tranx)) {
		DA_LOG_ERR(HTTPManager,"input_for_tranx is invalid");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	} else {
		queue = input_for_tranx->queue;
		pi_http_method = input_for_tranx->http_method;
		url = input_for_tranx->http_msg_request->url;
	}

	session_table_entry = _pi_http_get_avaiable_session_table_entry();
	if (session_table_entry == -1) {
		ret = DA_ERR_ALREADY_MAX_DOWNLOAD;
		goto ERR;
	}
	DA_LOG_VERBOSE(HTTPManager,"session_table_entry = %d", session_table_entry);

	if (DA_FALSE == _pi_http_register_queue_to_session_table(
			session_table_entry, queue)) {
		_pi_http_destroy_session_table_entry(session_table_entry);
		ret = DA_ERR_ALREADY_MAX_DOWNLOAD;
		goto ERR;
	}

	/* modified by keunsoon.lee 2010-09-20 use sync_new() instead of async_new() for make different soup thread from UI main thread*/
	session = soup_session_sync_new();
	/* session=soup_session_async_new(); */
	if (!session) {
		DA_LOG_ERR(HTTPManager,"Fail to create session");
		return DA_ERR_INVALID_URL;
	}
	DA_LOG(HTTPManager,"session[%p]", session);
/*
	SoupLogger* logger = soup_logger_new(SOUP_LOGGER_LOG_BODY, -1);
	soup_logger_attach(logger, session);
	g_object_unref(logger);
*/
	if (DA_FALSE == _pi_http_register_session_to_session_table(
			session_table_entry, session)) {
		_pi_http_init_session_table_entry(session_table_entry);
		ret = DA_ERR_ALREADY_MAX_DOWNLOAD;
		goto ERR;
	}

	g_object_set(session, SOUP_SESSION_MAX_CONNS, MAX_SESSION_COUNT, NULL);
	/* Set timeout unlimited time to resume a download which has ETag when the network is re-connected
	 * => This is changed to 180 seconds due to limitation of max downloading items.
	 */
	g_object_set(session, SOUP_SESSION_TIMEOUT, MAX_TIMEOUT, NULL);

	_set_proxy_on_soup_session(session, input_for_tranx->proxy_addr);

	switch (pi_http_method) {
	case PI_HTTP_METHOD_GET:
		msg = soup_message_new(METHOD_GET, url);
		break;
	case PI_HTTP_METHOD_POST:
		msg = soup_message_new(METHOD_POST, url);
		break;
	case PI_HTTP_METHOD_HEAD:
		msg = soup_message_new(METHOD_HEAD, url);
		break;
	default:
		DA_LOG_ERR(HTTPManager,"Cannot enter here");
		break;
	}
	DA_LOG_VERBOSE(HTTPManager,"msg[%p]", msg);
	/* if it is failed to create a msg, the url can be invalid, becasue of the input argument of soup_message_new API */
	if (msg == NULL) {
		DA_LOG_ERR(HTTPManager,"Fail to create message");
		ret = DA_ERR_INVALID_URL;
		goto ERR;
	}

	_fill_soup_msg_header(msg, input_for_tranx);

	g_signal_connect(msg, "restarted", G_CALLBACK(_pi_http_restarted_cb),
			NULL); /* for redirection case */
	g_signal_connect(msg, "got-headers",
			G_CALLBACK(_pi_http_gotheaders_cb), NULL);
	g_signal_connect(msg, "got-chunk", G_CALLBACK(_pi_http_gotchunk_cb),
			NULL);

	if (using_content_sniffing) {
		soup_session_add_feature_by_type(session, SOUP_TYPE_CONTENT_SNIFFER);
		g_signal_connect(msg, "content-sniffed",
				G_CALLBACK(_pi_http_contentsniffed_cb), NULL);
	} else {
		soup_message_disable_feature(msg, SOUP_TYPE_CONTENT_SNIFFER);
	}

	soup_session_queue_message(session, msg, _pi_http_finished_cb, NULL);
//	g_signal_connect(msg, "finished", G_CALLBACK(_pi_http_finished_cb),	NULL);

	if (DA_FALSE == _pi_http_register_msg_to_session_table(
			session_table_entry, msg)) {
		_pi_http_destroy_session_table_entry(session_table_entry);
		ret = DA_ERR_ALREADY_MAX_DOWNLOAD;
		goto ERR;
	}

	*out_tranx_id = session_table_entry;
	DA_LOG(HTTPManager,"*out_tranx_id = %d", *out_tranx_id);

ERR:
	return ret;
}

da_result_t PI_http_disconnect_transaction(int in_tranx_id)
{
	da_result_t ret = DA_RESULT_OK;
	int session_table_entry = -1;

	DA_LOG_VERBOSE(HTTPManager,"in_tranx_id = %d", in_tranx_id);

	session_table_entry = in_tranx_id;

	_pi_http_destroy_session_table_entry(session_table_entry);

	return ret;
}

da_result_t PI_http_cancel_transaction(int in_tranx_id, da_bool_t abort_option)
{
	da_result_t ret = DA_RESULT_OK;
	SoupSession *session;
	SoupMessage *msg;
	int session_table_entry = -1;

	DA_LOG_FUNC_LOGV(HTTPManager);

	session_table_entry = in_tranx_id;
	if (!_pi_http_is_this_session_table_entry_using(session_table_entry)) {
		DA_LOG_CRITICAL(HTTPManager,"not using session");
		return ret;
	}
	session = GET_SESSION_FROM_TABLE_ENTRY(session_table_entry);
	msg = GET_MSG_FROM_TABLE_ENTRY(session_table_entry);

	if (DA_NULL == session) {
		DA_LOG_ERR(HTTPManager,"invalid session = %p", session);
		goto ERR;
	}

	if (DA_NULL == msg) {
		DA_LOG_ERR(HTTPManager,"invalid message = %p", msg);
		goto ERR;
	}
	DA_LOG(HTTPManager,"soup cancel API:abort option[%d] tranx_id[%d]",
			abort_option, in_tranx_id);
	if (abort_option)
		soup_session_abort(session);
	else
		soup_session_cancel_message(session, msg, SOUP_STATUS_CANCELLED);
	DA_LOG(HTTPManager,"soup cancel API-Done");
ERR:
	return ret;
}

void PI_http_pause_transaction(int transaction_id)
{
	int session_table_entry = -1;
	pthread_mutex_t *mutex;
	pthread_cond_t *cond;

	DA_LOG_FUNC_LOGD(HTTPManager);

	DA_LOG(HTTPManager,"in_tranx_id = %d", transaction_id);

	session_table_entry = transaction_id;

	if (!_pi_http_is_this_session_table_entry_using(session_table_entry))
		return;

	mutex = &(pi_session_table[session_table_entry].mutex);
	cond = &(pi_session_table[session_table_entry].cond);

	_da_thread_mutex_lock (mutex);

	if (pi_session_table[session_table_entry].is_paused == DA_FALSE) {
		DA_LOG_CRITICAL(HTTPManager,"paused!");
		pi_session_table[session_table_entry].is_paused = DA_TRUE;
		_da_thread_cond_wait(cond, mutex);
	} else {
		DA_LOG_CRITICAL(HTTPManager,"NOT paused!");
	}

	_da_thread_mutex_unlock (mutex);

}

void PI_http_unpause_transaction(int transaction_id)
{
	int session_table_entry = -1;
	pthread_mutex_t *mutex;
	pthread_cond_t *cond;

	DA_LOG_FUNC_LOGV(Default);

	session_table_entry = transaction_id;

	if (!_pi_http_is_this_session_table_entry_using(session_table_entry))
		return;

	mutex = &(pi_session_table[session_table_entry].mutex);
	cond = &(pi_session_table[session_table_entry].cond);

	_da_thread_mutex_lock (mutex);

	if (pi_session_table[session_table_entry].is_paused == DA_TRUE) {
		DA_LOG_CRITICAL(HTTPManager,"wake up!");
		pi_session_table[session_table_entry].is_paused = DA_FALSE;
		_da_thread_cond_signal(cond);
	}

	_da_thread_mutex_unlock (mutex);

}

da_bool_t _pi_http_is_valid_input_for_tranx(
		const input_for_tranx_t *input_for_tranx)
{
	if (!(input_for_tranx->http_msg_request)) {
		DA_LOG_ERR(HTTPManager,"http_msg_request is NULL");
		return DA_FALSE;
	}

	if (!((input_for_tranx->http_method == PI_HTTP_METHOD_GET) ||
			(input_for_tranx->http_method == PI_HTTP_METHOD_POST) ||
			(input_for_tranx->http_method == PI_HTTP_METHOD_HEAD))) {
		DA_LOG_ERR(HTTPManager,"http_method is neither GET or POST or HEAD");
		return DA_FALSE;
	}

	return DA_TRUE;
}

da_bool_t _pi_http_is_this_session_table_entry_using(
		const int in_session_table_entry)
{
	da_bool_t is_using = DA_FALSE;

	if (DA_FALSE == IS_VALID_SESSION_TABLE_ENTRY(in_session_table_entry))
		return DA_FALSE;

	_da_thread_mutex_lock (&mutex_for_session_table);

	is_using = pi_session_table[in_session_table_entry].is_using;

	_da_thread_mutex_unlock (&mutex_for_session_table);

	return is_using;
}

void _pi_http_init_session_table_entry(const int in_session_table_entry)
{
	int entry = in_session_table_entry;

	if (DA_FALSE == IS_VALID_SESSION_TABLE_ENTRY(entry))
		return;

//	_da_thread_mutex_lock (&mutex_for_session_table);

	pi_session_table[entry].is_using = DA_TRUE;
	pi_session_table[entry].msg = NULL;
	pi_session_table[entry].session = NULL;
	pi_session_table[entry].queue = NULL;

	_da_thread_mutex_init(&(pi_session_table[entry].mutex), DA_NULL);
	_da_thread_cond_init(&(pi_session_table[entry].cond), NULL);
	pi_session_table[entry].is_paused = DA_FALSE;

//	_da_thread_mutex_unlock (&mutex_for_session_table);

	return;
}

void _pi_http_destroy_session_table_entry(const int in_session_table_entry)
{
	int entry = in_session_table_entry;

	if (DA_FALSE == IS_VALID_SESSION_TABLE_ENTRY(entry))
		return;

	_da_thread_mutex_lock (&mutex_for_session_table);

	if (pi_session_table[entry].is_paused == DA_TRUE)
		PI_http_unpause_transaction(entry);

	/* Warning! Do not g_object_unref(msg) here!
	 * soup_session_queue_message() steals msg's reference count,
	 * so, we don't need to do anything for memory management.
	 *
	 * But, if using soup_session_send_message(), MUST call g_object_unref(msg). */
	/* if (pi_session_table[entry].msg)
		g_object_unref(pi_session_table[entry].msg); */

	pi_session_table[entry].msg = NULL;

	/* FIXME Cannot g_object_unref(session) here,
	 * because msg inside this session is not destoryed yet.
	 * The msg's reference count is stealed by soup_session_queue_message(),
	 * and it will be destroyed when _pi_http_finished_cb() is returned.
	 * For now, this _pi_http_destroy_session_table_entry() is called inside
	 * _pi_http_finished_cb(), so, g_object_unref(session) is not working.
	 * Should find out call this function after _pi_http_finished_cb(). */
	if (pi_session_table[entry].session)
		g_object_unref(pi_session_table[entry].session);
	else
		DA_LOG_ERR(HTTPManager,"session is NULL. Cannot unref this.");
	DA_LOG(HTTPManager,"unref session [%p]",pi_session_table[entry].session);

	pi_session_table[entry].session = NULL;

	pi_session_table[entry].queue = NULL;
	pi_session_table[entry].is_paused = DA_FALSE;
	pi_session_table[entry].is_using = DA_FALSE;

	_da_thread_mutex_destroy(&(pi_session_table[entry].mutex));
	_da_thread_cond_destroy(&(pi_session_table[entry].cond));

	_da_thread_mutex_unlock (&mutex_for_session_table);

	return;
}

int _pi_http_get_avaiable_session_table_entry(void)
{
	int i;
	int avaiable_entry = -1;

	_da_thread_mutex_lock (&mutex_for_session_table);

	for (i = 0; i < MAX_SESSION_COUNT; i++) {
		if (pi_session_table[i].is_using == DA_FALSE) {
			/*	pi_session_table[i].is_using = DA_TRUE; */
			DA_LOG_VERBOSE(HTTPManager,"available entry = %d", i);

			avaiable_entry = i;

			break;
		}
	}
	_pi_http_init_session_table_entry(avaiable_entry);
	_da_thread_mutex_unlock (&mutex_for_session_table);

	return avaiable_entry;
}

da_bool_t _pi_http_register_queue_to_session_table(
		const int in_session_table_entry, const queue_t *in_queue)
{
	int entry = in_session_table_entry;
	queue_t *queue = (queue_t *) in_queue;
	da_bool_t ret = DA_FALSE;

	if (DA_FALSE == IS_VALID_SESSION_TABLE_ENTRY(entry)) {
		DA_LOG_ERR(HTTPManager,"invalid entry = %d", entry);
		return DA_FALSE;
	}

	_da_thread_mutex_lock (&mutex_for_session_table);

	if (pi_session_table[entry].is_using == DA_FALSE) {
		DA_LOG_ERR(HTTPManager,"this entry [%d] is not using", entry);
		ret = DA_FALSE;
	} else {
		pi_session_table[entry].queue = queue;
		DA_LOG_VERBOSE(HTTPManager,"queue = %p", pi_session_table[entry].queue);
		ret = DA_TRUE;
	}

	_da_thread_mutex_unlock (&mutex_for_session_table);

	return ret;
}

da_bool_t _pi_http_register_session_to_session_table(
		const int in_session_table_entry, SoupSession *session)
{
	int entry = in_session_table_entry;
	da_bool_t ret = DA_FALSE;

	if (DA_FALSE == IS_VALID_SESSION_TABLE_ENTRY(entry)) {
		DA_LOG_ERR(HTTPManager,"invalid entry = %d", entry);
		return DA_FALSE;
	}

	if (DA_NULL == session) {
		DA_LOG_ERR(HTTPManager,"invalid session = %p",session);
		return DA_FALSE;
	}

	_da_thread_mutex_lock (&mutex_for_session_table);

	if (pi_session_table[entry].is_using == DA_FALSE) {
		ret = DA_FALSE;
	} else {
		pi_session_table[entry].session = session;
		ret = DA_TRUE;
	}

	_da_thread_mutex_unlock (&mutex_for_session_table);

	return ret;
}

da_bool_t _pi_http_register_msg_to_session_table(
		const int in_session_table_entry, SoupMessage *msg)
{
	int entry = in_session_table_entry;
	da_bool_t ret = DA_FALSE;

	if (DA_FALSE == IS_VALID_SESSION_TABLE_ENTRY(entry)) {
		DA_LOG_ERR(HTTPManager,"invalid entry = %d", entry);
		return DA_FALSE;
	}

	if (DA_NULL == msg) {
		DA_LOG_ERR(HTTPManager,"invalid msg = %p",msg);
		return DA_FALSE;
	}

	_da_thread_mutex_lock (&mutex_for_session_table);

	if (pi_session_table[entry].is_using == DA_FALSE) {
		ret = DA_FALSE;
	} else {
		pi_session_table[entry].msg = msg;
		ret = DA_TRUE;
	}

	_da_thread_mutex_unlock (&mutex_for_session_table);

	return ret;
}

queue_t *_pi_http_get_queue_from_session_table_entry(
		const int in_session_table_entry)
{
	int entry = in_session_table_entry;
	queue_t *out_queue = NULL;

	if (DA_FALSE == IS_VALID_SESSION_TABLE_ENTRY(entry)) {
		DA_LOG_ERR(HTTPManager,"invalid entry = %d", entry);
		return out_queue;
	}

	_da_thread_mutex_lock (&mutex_for_session_table);

	out_queue = pi_session_table[entry].queue;
	_da_thread_mutex_unlock (&mutex_for_session_table);

	return out_queue;
}

void _pi_http_store_read_header_to_queue(SoupMessage *msg, const char *sniffedType)
{
	da_result_t ret = DA_RESULT_OK;

	queue_t *da_queue = NULL;
	q_event_t *da_event = NULL;
	q_event_type_data da_event_type_data;

	int session_table_entry = -1;
	SoupMessageHeadersIter headers_iter;

	const char *header_name;
	const char *header_value;

	http_msg_response_t *http_msg_response = NULL;

	if (msg->response_headers) {
		ret = http_msg_response_create(&http_msg_response);
		if (ret != DA_RESULT_OK)
			return;

		http_msg_response_set_status_code(http_msg_response,
				msg->status_code);

		DA_LOG_VERBOSE(HTTPManager,"\n----raw header---------------------------------------------");
		DA_LOG(HTTPManager,"status code = %d", msg->status_code);
		soup_message_headers_iter_init(&headers_iter,
				msg->response_headers);
		while (soup_message_headers_iter_next(&headers_iter,
				&header_name, &header_value)) {
			if ((header_name != DA_NULL) && (header_value
					!= DA_NULL)) {
				http_msg_response_add_field(http_msg_response,
						header_name, header_value);
				DA_SECURE_LOGI("[%s][%s]", header_name, header_value);
			}
		}
		DA_LOG_VERBOSE(HTTPManager,"\n-------------------------------------------------------------\n");

	}

	if (using_content_sniffing && sniffedType)
		http_msg_response_set_content_type(http_msg_response, sniffedType);

	session_table_entry
			= _pi_http_get_session_table_entry_from_message(msg);
	if (session_table_entry == -1) {
		DA_LOG_ERR(HTTPManager,"Fail to find matched session table entry..");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}

	da_event_type_data = Q_EVENT_TYPE_DATA_PACKET;

	da_queue = _pi_http_get_queue_from_session_table_entry(
			session_table_entry);

	ret = Q_make_http_data_event(da_event_type_data, &da_event);
	if (ret != DA_RESULT_OK) {
		DA_LOG_ERR(HTTPManager,"fail to make da_event");
		goto ERR;
	} else {
		Q_set_status_code_on_http_data_event(da_event, msg->status_code);
		da_event->type.q_event_data_http.http_response_msg
				= http_msg_response;

		Q_push_event(da_queue, da_event);
	}
	return;

ERR:
	if (DA_RESULT_OK != ret)
		http_msg_response_destroy(&http_msg_response);

	return;
}

void _pi_http_store_read_data_to_queue(SoupMessage *msg, const char *body_data,
		int received_body_len)
{
	da_result_t ret = DA_RESULT_OK;
	da_bool_t b_ret = DA_FALSE;

	char *body_buffer = NULL;
	queue_t *da_queue = NULL;
	q_event_t *da_event = NULL;
	q_event_type_data da_event_type_data;
	int session_table_entry = -1;
	int http_status = -1;

	http_status = msg->status_code;

	session_table_entry
			= _pi_http_get_session_table_entry_from_message(msg);
	if (session_table_entry == -1) {
		DA_LOG_ERR(HTTPManager,"Fail to find matched session table entry..");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}

	if (received_body_len == 0) {
		DA_LOG_VERBOSE(HTTPManager,"Q_EVENT_TYPE_DATA_FINAL");
		da_event_type_data = Q_EVENT_TYPE_DATA_FINAL;
	} else {
		da_event_type_data = Q_EVENT_TYPE_DATA_PACKET;
		if (received_body_len > 0) {
			body_buffer = (char*) calloc(1, received_body_len);
			DA_LOG_VERBOSE(HTTPManager,"body_buffer[%p]msg[%p]",body_buffer,msg);
			if (body_buffer == DA_NULL) {
				DA_LOG_ERR(HTTPManager,"DA_ERR_FAIL_TO_MEMALLOC");
				goto ERR;
			}
			memcpy(body_buffer, body_data, received_body_len);
		}
	}

	da_queue = _pi_http_get_queue_from_session_table_entry(
			session_table_entry);

	ret = Q_make_http_data_event(da_event_type_data, &da_event);
	if (ret != DA_RESULT_OK) {
		DA_LOG_ERR(HTTPManager,"fail to make da_event");
		goto ERR;
	} else {
		Q_set_status_code_on_http_data_event(da_event, http_status);
		Q_set_http_body_on_http_data_event(da_event, received_body_len,
				body_buffer);

		_da_thread_mutex_lock (&(da_queue->mutex_queue));
		b_ret = Q_push_event_without_lock(da_queue, da_event);
		if (b_ret == DA_FALSE) {
			DA_LOG_CRITICAL(HTTPManager,"----------------------------------------fail to push!");

			pthread_mutex_t *session_mutex = NULL;
			pthread_cond_t *session_cond = NULL;

			session_mutex
					= &(pi_session_table[session_table_entry].mutex);
			session_cond
					= &(pi_session_table[session_table_entry].cond);

			/*  MUST keep this order for these mutexes */
			_da_thread_mutex_lock (session_mutex);
			_da_thread_mutex_unlock (&(da_queue->mutex_queue));

			if (pi_session_table[session_table_entry].is_paused
					== DA_FALSE) {
				DA_LOG_CRITICAL(HTTPManager,"paused!");
				pi_session_table[session_table_entry].is_paused
						= DA_TRUE;
				_da_thread_cond_wait(session_cond, session_mutex);
			} else {
				DA_LOG_CRITICAL(HTTPManager,"NOT paused!");
			}

			_da_thread_mutex_unlock (session_mutex);

			DA_LOG_CRITICAL(HTTPManager,"wake up! push again");
			Q_push_event(da_queue, da_event);
		} else {
			_da_thread_mutex_unlock (&(da_queue->mutex_queue));
		}

	}

	return;

ERR:
	if (DA_RESULT_OK != ret) {
		if (DA_NULL != body_buffer) {
			free(body_buffer);
		}
	}

	return;
}

int _translate_error_code(int soup_error)
{
	DA_LOG_CRITICAL(HTTPManager, "soup error code[%d]", soup_error);
	switch (soup_error) {
	case SOUP_STATUS_CANT_RESOLVE:
	case SOUP_STATUS_CANT_RESOLVE_PROXY:
	case SOUP_STATUS_CANT_CONNECT:
	case SOUP_STATUS_CANT_CONNECT_PROXY:
	case SOUP_STATUS_IO_ERROR:
	case SOUP_STATUS_MALFORMED:
	case SOUP_STATUS_TRY_AGAIN:
		return DA_ERR_NETWORK_FAIL;
	case SOUP_STATUS_SSL_FAILED:
		return DA_ERR_SSL_FAIL;
	case SOUP_STATUS_REQUEST_TIMEOUT:
		return DA_ERR_HTTP_TIMEOUT;
	case SOUP_STATUS_TOO_MANY_REDIRECTS:
		return DA_ERR_TOO_MANY_REDIECTS;
	default:
		return DA_ERR_NETWORK_FAIL;
	}
}

void _pi_http_store_neterr_to_queue(SoupMessage *msg)
{
	da_result_t ret = DA_RESULT_OK;
	int error_type = -1;
	queue_t *da_queue = NULL;
	q_event_t *da_event = NULL;
	int session_table_entry = -1;

	DA_LOG_FUNC_LOGD(HTTPManager);

	error_type = _translate_error_code(msg->status_code);

	session_table_entry
			= _pi_http_get_session_table_entry_from_message(msg);
	if (session_table_entry == -1) {
		DA_LOG_ERR(HTTPManager,"Fail to find matched session table entry..");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}

	da_queue = _pi_http_get_queue_from_session_table_entry(
			session_table_entry);

	DA_LOG_CRITICAL(HTTPManager,"Q_EVENT_TYPE_DATA_ABORT");
	ret = Q_make_http_data_event(Q_EVENT_TYPE_DATA_ABORT, &da_event);
	if (ret != DA_RESULT_OK) {
		DA_LOG_ERR(HTTPManager,"fail to make da_event");
		goto ERR;
	} else {
		Q_set_error_type_on_http_data_event(da_event, error_type);

		Q_push_event(da_queue, da_event);
	}

ERR:
	return;
}

int _pi_http_get_session_table_entry_from_message(SoupMessage *msg)
{

	int out_entry = -1;
	int i;

	if (DA_NULL == msg) {
		DA_LOG_ERR(HTTPManager,"invalid message = %p", msg);
		return out_entry;
	}

	_da_thread_mutex_lock (&mutex_for_session_table);

	for (i = 0; i < MAX_SESSION_COUNT; i++) {
		if (pi_session_table[i].is_using == DA_TRUE) {
			if (pi_session_table[i].msg == msg) {
				out_entry = i;
				break;
			}
		}
	}

	_da_thread_mutex_unlock (&mutex_for_session_table);

	if (i == MAX_SESSION_COUNT) {
		DA_LOG_ERR(HTTPManager,"fail to find message = %p", msg);
	}

	return out_entry;

}

void _pi_http_finished_cb(SoupSession *session, SoupMessage *msg, gpointer data)
{
	char *url = NULL;

	DA_LOG_FUNC_LOGV(HTTPManager);

	if (!msg) {
		DA_LOG_ERR(HTTPManager, "Check NULL:msg");
		return;
	}

	url = soup_uri_to_string(soup_message_get_uri(msg), DA_FALSE);

	DA_SECURE_LOGI("status_code[%d], reason[%s], url[%s]",msg->status_code,msg->reason_phrase,url);

	if (url) {
		free(url);
		url = NULL;
	}

	if (SOUP_STATUS_IS_TRANSPORT_ERROR(msg->status_code)) {
		if (msg->status_code == SOUP_STATUS_CANCELLED) {
			_pi_http_store_read_data_to_queue(msg, DA_NULL, 0);
		} else {
			_pi_http_store_neterr_to_queue(msg);
		}
	} else {
		_pi_http_store_read_data_to_queue(msg, DA_NULL, 0);
	}

}

/* this callback is called in case of redirection */
void _pi_http_restarted_cb(SoupMessage *msg, gpointer data)
{
	DA_LOG_FUNC_LOGD(HTTPManager);
	/* Location URL is needed when extracting the file name from url.
	 * So, the response header should be handled by http mgr.*/

	if (!msg) {
		DA_LOG_ERR(HTTPManager, "Check NULL:msg");
		return;
	}
	// If there are user id and password at url, libsoup handle it automatically.
	if (msg->status_code == SOUP_STATUS_UNAUTHORIZED) {
		DA_LOG(HTTPManager,"Ignore:Unauthorized");
		return;
	}
	_pi_http_store_read_header_to_queue(msg, NULL);
}

void _pi_http_gotheaders_cb(SoupMessage *msg, gpointer data)
{
	DA_LOG_FUNC_LOGV(HTTPManager);

	if (!msg) {
		DA_LOG_ERR(HTTPManager, "Check NULL:msg");
		return;
	}

	if (SOUP_STATUS_IS_REDIRECTION(msg->status_code)) {
		DA_LOG(HTTPManager,"Redirection !!");
		if (SOUP_STATUS_NOT_MODIFIED != msg->status_code)
			return;
	}

	// If there are user id and password at url, libsoup handle it automatically.
	if (msg->status_code == SOUP_STATUS_UNAUTHORIZED) {
		DA_LOG(HTTPManager,"Ignore:Unauthorized");
		return;
	}

	soup_message_body_set_accumulate(msg->response_body, DA_FALSE);

	if (!using_content_sniffing)
		_pi_http_store_read_header_to_queue(msg, NULL);
	else
		DA_LOG_DEBUG(HTTPManager,"ignore because content sniffing is turned on");
}

void _pi_http_contentsniffed_cb(SoupMessage *msg, const char *sniffedType,
		GHashTable *params, gpointer data)
{
	DA_LOG_FUNC_LOGD(HTTPManager);

	if (!msg) {
		DA_LOG_ERR(HTTPManager, "Check NULL:msg");
		return;
	}

	if (SOUP_STATUS_IS_REDIRECTION(msg->status_code)) {
		DA_LOG(HTTPManager,"Redirection !!");
		if (SOUP_STATUS_NOT_MODIFIED != msg->status_code)
			return;
	}

	// If there are user id and password at url, libsoup handle it automatically.
	if (msg->status_code == SOUP_STATUS_UNAUTHORIZED) {
		DA_LOG(HTTPManager,"Ignore:Unauthorized");
		return;
	}

	if (using_content_sniffing)
		_pi_http_store_read_header_to_queue(msg, sniffedType);
}

void _pi_http_gotchunk_cb(SoupMessage *msg, SoupBuffer *chunk, gpointer data)
{
//	DA_LOG_FUNC_LOGV(HTTPManager);

	if (!msg) {
		DA_LOG_ERR(HTTPManager, "Check NULL:msg");
		return;
	}

	if (!chunk) {
		DA_LOG_ERR(HTTPManager, "Check NULL:chunk");
		return;
	}

	if (SOUP_STATUS_IS_REDIRECTION(msg->status_code))
		return;

	if (msg->status_code == SOUP_STATUS_UNAUTHORIZED) {
		DA_LOG(HTTPManager,"Ignore:Unauthorized");
		return;
	}

	if (chunk->data && chunk->length > 0) {
		_pi_http_store_read_data_to_queue(msg, chunk->data,
				chunk->length);
	}
}
