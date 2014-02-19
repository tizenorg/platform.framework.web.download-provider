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

#ifndef _Download_Agent_Dl_Info_Util_H
#define _Download_Agent_Dl_Info_Util_H

#include "download-agent-type.h"
#include "download-agent-http-queue.h"
#include "download-agent-utils-dl-id-history.h"

#define DA_MAX_DOWNLOAD_ID	DA_MAX_DOWNLOAD_REQ_AT_ONCE
#define DA_MAX_TYPE_COUNT	10

#define DOWNLOAD_NOTIFY_LIMIT (1024*32) //bytes
extern pthread_mutex_t mutex_download_state[];

typedef enum {
	DOWNLOAD_STATE_IDLE = 0,
	DOWNLOAD_STATE_NEW_DOWNLOAD = 10, /* stage */
	DOWNLOAD_STATE_FINISH = 50, /* stage */
	DOWNLOAD_STATE_PAUSED = 60, /* http */
	DOWNLOAD_STATE_CANCELED = 70, /* http */
	DOWNLOAD_STATE_ABORTED = 80 /* satge */
} download_state_t;

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
	HTTP_STATE_PAUSED = 10,
	HTTP_STATE_RESUMED = 11,
	HTTP_STATE_ABORTED = 12,
} http_state_t;

typedef struct _client_input_basic_t {
	char *req_url;
	char **user_request_header;
	int user_request_header_count;
} client_input_basic_t;

typedef struct _client_input_t {
	void *user_data;
	char *install_path;
	char *file_name;
	char *etag;
	char *temp_file_path;
	char *pkg_name;
	client_input_basic_t client_input_basic;
} client_input_t;

typedef struct _download_thread_input {
	int slot_id;
	client_input_t *client_input;
} download_thread_input;

typedef struct _source_info_basic_t {
	int dl_id;
	char *url;
	char **user_request_header;
	int user_request_header_count;
} source_info_basic_t;

typedef struct _source_info_t {
	union _source_info_type {
		source_info_basic_t *source_info_basic;
	} source_info_type;
} source_info_t;

#define GET_SOURCE_TYPE(SOURCE)			((SOURCE)->source_type)
#define GET_SOURCE_BASIC(SOURCE)   		((SOURCE)->source_info_type.source_info_basic)
#define GET_SOURCE_BASIC_URL(SOURCE)   		(GET_SOURCE_BASIC(SOURCE)->url)

typedef struct _req_dl_info {
	http_info_t http_info;

	/* This is just pointer assignment from stage source info. */
	char *destination_url;
	/* The location url is assigned here in case of redirection.
	 * At this time, the pointer should be freed. */
	char *location_url;
	char **user_request_header;
	int user_request_header_count;
	char *user_request_etag;
	char *user_request_temp_file_path;

	http_state_t http_state;
	pthread_mutex_t mutex_http_state;

	da_result_t result;
	/*************** will be depreciated ***********************/
	/* ToDo : previous http_info should be saved in case of pause */
	char *content_type_from_header; /* calloced in set hdr fiels on download info */
	unsigned long long content_len_from_header;
	char *etag_from_header;

	unsigned long int downloaded_data_size;

	int invloved_transaction_id;
} req_dl_info;

#define GET_REQUEST_HTTP_RESULT(REQUEST)  		(REQUEST->result)
#define GET_REQUEST_HTTP_TRANS_ID(REQUEST)  	(REQUEST->invloved_transaction_id)
#define GET_REQUEST_HTTP_REQ_URL(REQUEST)  		(REQUEST->destination_url)
#define GET_REQUEST_HTTP_REQ_LOCATION(REQUEST)  		(REQUEST->location_url)
#define GET_REQUEST_HTTP_USER_REQUEST_HEADER(REQUEST)  		(REQUEST->user_request_header)
#define GET_REQUEST_HTTP_USER_REQUEST_HEADER_COUNT(REQUEST)  		(REQUEST->user_request_header_count)
#define GET_REQUEST_HTTP_USER_REQUEST_ETAG(REQUEST)  		(REQUEST->user_request_etag)
#define GET_REQUEST_HTTP_USER_REQUEST_TEMP_FILE_PATH(REQUEST)  		(REQUEST->user_request_temp_file_path)
#define GET_REQUEST_HTTP_HDR_ETAG(REQUEST)  	(REQUEST->etag_from_header)
#define GET_REQUEST_HTTP_HDR_CONT_TYPE(REQUEST) (REQUEST->content_type_from_header)
#define GET_REQUEST_HTTP_HDR_CONT_LEN(REQUEST)  (REQUEST->content_len_from_header)
#define GET_REQUEST_HTTP_CONTENT_OFFSET(REQUEST)(REQUEST->downloaded_data_size)
#define GET_REQUEST_HTTP_MUTEX_HTTP_STATE(STAGE)  		(GET_STAGE_TRANSACTION_INFO(STAGE)->mutex_http_state)
#define GET_HTTP_STATE_ON_STAGE(STAGE)	(GET_STAGE_TRANSACTION_INFO(STAGE)->http_state)
#define CHANGE_HTTP_STATE(STATE,STAGE) {\
	_da_thread_mutex_lock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(STAGE)));\
	GET_HTTP_STATE_ON_STAGE(STAGE) = STATE;\
	DA_LOG_DEBUG(Default, "Changed http_state to - [%d] ", GET_HTTP_STATE_ON_STAGE(STAGE));\
	_da_thread_mutex_unlock(&(GET_REQUEST_HTTP_MUTEX_HTTP_STATE(STAGE)));\
}

typedef struct _file_info {
	void *file_handle;
	char *pure_file_name;
	char *extension;
	char *file_name_final; /* malloced in set_file_path_for_final_saving */
	char *content_type; /* malloced in make file info. */
	char *add_to_buffer;
	unsigned int file_size; /* http header's Content-Length has higher priority than DD's <size> */
	unsigned int total_bytes_written_to_file; /* current written file size */
	unsigned int bytes_written_to_file;
	unsigned int current_buffer_len;
} file_info;

#define GET_CONTENT_STORE_PURE_FILE_NAME(FILE_CNTXT) (FILE_CNTXT)->pure_file_name
#define GET_CONTENT_STORE_EXTENSION(FILE_CNTXT) (FILE_CNTXT)->extension
#define GET_CONTENT_STORE_ACTUAL_FILE_NAME(FILE_CNTXT) (FILE_CNTXT)->file_name_final
#define GET_CONTENT_STORE_FILE_HANDLE(FILE_CNTXT) (FILE_CNTXT)->file_handle
#define GET_CONTENT_STORE_FILE_SIZE(FILE_CNTXT) (FILE_CNTXT)->file_size
#define GET_CONTENT_STORE_CURRENT_FILE_SIZE(FILE_CNTXT) (FILE_CNTXT)->total_bytes_written_to_file
#define IS_CONTENT_STORE_FILE_BYTES_WRITTEN_TO_FILE(FILE_CNTXT) (FILE_CNTXT)->bytes_written_to_file
#define GET_CONTENT_STORE_FILE_BUFFER(FILE_CNTXT) (FILE_CNTXT)->add_to_buffer
#define GET_CONTENT_STORE_FILE_BUFF_LEN(FILE_CNTXT) ((FILE_CNTXT)->current_buffer_len)
#define GET_CONTENT_STORE_CONTENT_TYPE(FILE_CNTXT) (FILE_CNTXT)->content_type

typedef struct _stage_info {
	int dl_id;
	int thread_id;
	source_info_t dl_request;
	req_dl_info dl_tansaction_context;
	file_info dl_content_storage;
	struct _stage_info *next_stage_info;
} stage_info;

#define	GET_STAGE_DL_ID(STAGE) 					((STAGE)->dl_id)
#define	GET_STAGE_THREAD_ID(STAGE) 				((STAGE)->thread_id)
#define	GET_STAGE_SOURCE_INFO(STAGE) 			(&((STAGE)->dl_request))
#define	GET_STAGE_TRANSACTION_INFO(STAGE)		(&((STAGE)->dl_tansaction_context))
#define	GET_STAGE_CONTENT_STORE_INFO(STAGE)		(&((STAGE)->dl_content_storage))
#define	GET_STAGE_INSTALLATION_INFO(STAGE)		(&((STAGE)->post_dl_context))

typedef struct {
	da_bool_t is_using;
	int slot_id;
	int dl_id;
	pthread_t active_dl_thread_id;
	download_state_t state;
	stage_info *download_stage_data;
	queue_t queue;
	int http_status;
	da_bool_t enable_pause_update;
	// FIXME have client_input itself, not to have each of them
	char *user_install_path;
	char *user_file_name;
	char *user_etag;
	char *user_temp_file_path;
	void *user_data;
} dl_info_t;

#define GET_DL_THREAD_ID(ID)		(download_mgr.dl_info[ID].active_dl_thread_id)
#define GET_DL_STATE_ON_ID(ID)		(download_mgr.dl_info[ID].state)
#define GET_DL_STATE_ON_STAGE(STAGE)	(GET_DL_STATE_ON_ID(GET_STAGE_DL_ID(STAGE)))
#define GET_DL_CURRENT_STAGE(ID)	(download_mgr.dl_info[ID].download_stage_data)
#define GET_DL_ID(ID)		(download_mgr.dl_info[ID].dl_id)
#define GET_DL_QUEUE(ID)		&(download_mgr.dl_info[ID].queue)
#define GET_DL_ENABLE_PAUSE_UPDATE(ID)		(download_mgr.dl_info[ID].enable_pause_update)
#define GET_DL_USER_INSTALL_PATH(ID)		(download_mgr.dl_info[ID].user_install_path)
#define GET_DL_USER_FILE_NAME(ID)		(download_mgr.dl_info[ID].user_file_name)
#define GET_DL_USER_ETAG(ID)		(download_mgr.dl_info[ID].user_etag)
#define GET_DL_USER_TEMP_FILE_PATH(ID)		(download_mgr.dl_info[ID].user_temp_file_path)
#define GET_DL_USER_DATA(ID)		(download_mgr.dl_info[ID].user_data)
#define IS_THIS_DL_ID_USING(ID)	(download_mgr.dl_info[ID].is_using)

#define CHANGE_DOWNLOAD_STATE(STATE,STAGE) {\
	_da_thread_mutex_lock (&mutex_download_state[GET_STAGE_DL_ID(STAGE)]);\
	GET_DL_STATE_ON_STAGE(STAGE) = STATE;\
	DA_LOG_DEBUG(Default, "Changed download_state to - [%d] ", GET_DL_STATE_ON_STAGE(STAGE));\
	_da_thread_mutex_unlock (&mutex_download_state[GET_STAGE_DL_ID(STAGE)]);\
	}

typedef struct _download_mgr_t {
	da_bool_t is_init;
	dl_info_t dl_info[DA_MAX_DOWNLOAD_ID];
	dl_id_history_t dl_id_history;
	/* FIXME: This is temporary solution to prevent crash on following case;
	 *    1) OMA download(that is, DA's libsoup is using) is on progressing on Browser
	 *    2) User push END hard key
	 *    3) da_deinit() is called. - on UI thread
	 *    4) cancel_download(all) is called.
	 *    5) plugin-libsoup.c calls soup_session_cancel_message().
	 *    6) da_deinit() is finished and process is over.
	 *    7) soup's callback for soup_session_cancel_message() is trying to be called - on UI thread
	 *    8) Browser crashed because the callback address is no longer exist.
	 *
	 * Here is a temporary solution;
	 *    If cancel is from da_deinit(), plugin-libsoup.c will not call soup_session_cancel_message().
	 *    So, append following variable to recognize this.
	 **/
	//da_bool_t is_progressing_deinit;
} download_mgr_t;

extern download_mgr_t download_mgr;

da_result_t init_download_mgr();
da_result_t deinit_download_mgr(void);
void init_download_info(int slot_id);
void destroy_download_info(int slot_id);
void *Add_new_download_stage(int slot_id);
void remove_download_stage(int slot_id, stage_info *in_stage);
void empty_stage_info(stage_info *in_stage);
void clean_up_client_input_info(client_input_t *client_input);
da_result_t  get_available_slot_id(int *available_id);
da_result_t  get_slot_id_for_dl_id(int dl_id , int* slot_id);
da_bool_t is_valid_slot_id(int slot_id);
void store_http_status(int dl_id, int status);
int get_http_status(int dl_id);
#endif	/* _Download_Agent_Dl_Info_Util_H */
