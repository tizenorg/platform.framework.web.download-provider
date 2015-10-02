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

#ifndef _DOWNLOAD_AGENT_INFO_H
#define _DOWNLOAD_AGENT_INFO_H

#include "download-agent-type.h"
#include "download-agent-interface.h"
#include "download-agent-pthread.h"

#include "curl/curl.h"

typedef struct {
	CURL *curl;
	pthread_mutex_t mutex;
	da_bool_t is_paused;
	da_bool_t is_cancel_reqeusted;
} http_msg_t;

typedef enum {
	HTTP_STATE_READY_TO_DOWNLOAD = 0,
	HTTP_STATE_REDIRECTED = 1,
	HTTP_STATE_DOWNLOAD_REQUESTED = 2,
	HTTP_STATE_DOWNLOAD_STARTED = 3,
	HTTP_STATE_DOWNLOADING = 4,
	HTTP_STATE_DOWNLOAD_FINISH = 5,
	HTTP_STATE_REQUEST_CANCEL = 6,
	HTTP_STATE_REQUEST_PAUSE = 7,
	HTTP_STATE_REQUEST_RESUME = 8,
	HTTP_STATE_CANCELED = 9,
	HTTP_STATE_FAILED = 10,
	HTTP_STATE_PAUSED = 11,
	HTTP_STATE_RESUMED = 12,
	HTTP_STATE_ABORTED = 13,
	HTTP_STATE_WAIT_FOR_NET_ERR = 14,
} http_state_t;

typedef enum {
	HTTP_METHOD_GET = 1,
	HTTP_METHOD_HEAD,
	HTTP_METHOD_POST
} http_method_t;

typedef struct {
	char *url;
	char **req_header;
	int req_header_count;
	char *install_path;
	char *file_name;
	char *etag;
	char *temp_file_path;
	char *pkg_name;
	int network_bonding;
	void *user_req_data;
	void *user_client_data;
} req_info_t;

typedef enum {
	HTTP_EVENT_GOT_HEADER = 0,
	HTTP_EVENT_GOT_PACKET,
	HTTP_EVENT_FINAL,
//	HTTP_EVENT_ABORT
} http_event_type_t;

typedef struct _http_header_options_t http_header_options_t;
struct _http_header_options_t{
	char *field;
	char *value;
	http_header_options_t *next;
};

typedef struct _http_header_t http_header_t;
typedef http_header_t *http_msg_iter_t;
struct _http_header_t{
	char *field;
	char *value;
	http_header_options_t *options;
	char *raw_value; // raw string including options
	http_header_t *next;
};

typedef struct{
	char *http_method;
	char *url;
	http_header_t *head;
	char *http_body;
} http_msg_request_t;


typedef struct{
	int status_code;
	http_header_t *head;
} http_msg_response_t;

typedef struct {
	http_event_type_t type;
	char *body;
	int body_len;
#ifdef _RAF_SUPPORT
	da_size_t received_len;
#endif
	int status_code;
	int error;
} http_raw_data_t;

typedef void (*http_update_cb) (http_raw_data_t *data, void *user_param);

typedef struct {
	char *location_url;
	http_state_t state;
	pthread_mutex_t mutex_state;
	pthread_mutex_t mutex_http;
	pthread_cond_t cond_http;
	http_msg_request_t *http_msg_request;
	http_msg_response_t *http_msg_response;
	http_method_t http_method;
	http_msg_t *http_msg;
	char *proxy_addr;
	char *content_type_from_header;
	char *file_name_from_header;
	da_size_t content_len_from_header;
	char *etag_from_header;
	int error_code; // for error value for http abort.
	da_size_t total_size;
#ifdef _RAF_SUPPORT
	da_bool_t is_raf_mode_confirmed;
#endif
	http_update_cb update_cb;
} http_info_t;

typedef struct {
	void *file_handle;
	char *pure_file_name;
	char *extension;
	char *file_path; /* malloced in set_file_path_for_final_saving */
	char *mime_type;// For drm converting
	char *buffer;
	da_size_t buffer_len;
	da_size_t file_size; /* http header's Content-Length has higher priority than DD's <size> */
	da_size_t bytes_written_to_file; /* The file size to be written at actual file */
#ifdef _RAF_SUPPORT
	da_size_t file_size_of_temp_file; /* If the temporary file is existed, the file size of it */
#endif
	da_bool_t is_updated; /* The flag for updating progress event only if the data is wrriten to file not buffer */
} file_info_t;

typedef struct {
	int da_id;
	int tid;
	pthread_t thread_id;
	http_info_t *http_info;
	file_info_t *file_info;
	req_info_t *req_info;
	da_cb_t cb_info;
	da_bool_t is_cb_update;
	int update_time;
} da_info_t;

pthread_mutex_t *g_openssl_locks_list;
da_info_t *da_info_list[DA_MAX_ID];

#define GET_STATE_MUTEX(INFO) (INFO->mutex_state)
#define GET_STATE(INFO) (INFO->state)
#define CHANGE_STATE(STATE,INFO) {\
	DA_MUTEX_LOCK (&GET_STATE_MUTEX(INFO));\
	GET_STATE(INFO) = STATE;\
	DA_LOGV("Changed state[%d]", GET_STATE(INFO));\
	DA_MUTEX_UNLOCK (&GET_STATE_MUTEX(INFO));\
	}

da_ret_t init_openssl_locks(void);
da_ret_t deinit_openssl_locks(void);
da_ret_t get_available_da_id(int *available_id);
da_ret_t copy_user_input_data(da_info_t *da_info, const char *url,
		req_data_t *ext_data, da_cb_t *da_cb_data);
da_bool_t is_valid_download_id(int id);
void destroy_da_info(int id);
void destroy_da_info_list(void);
da_ret_t get_da_info_with_da_id(int id, da_info_t **out_info);
da_ret_t init_http_msg_t(http_msg_t **http_msg);
void destroy_http_msg_t(http_msg_t *http_msg);
void reset_http_info(http_info_t *http_info);
void reset_http_info_for_resume(http_info_t *http_info);
void destroy_http_info(http_info_t *http_info);
void destroy_file_info(file_info_t *file_info);

#endif	/* _DOWNLOAD_AGENT_INFO_H */
