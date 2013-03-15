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

#ifndef _Download_Agent_Http_Msg_Handler_H
#define _Download_Agent_Http_Msg_Handler_H

#include "download-agent-type.h"

#define HTTP_FIELD_UAGENT			"User-Agent"
#define HTTP_FIELD_HOST				"Host"
#define HTTP_FIELD_UAPROF			"X-Wap-Profile"
#define HTTP_FIELD_CONTENT_LENGTH	"Content-Length"
#define HTTP_FIELD_CONTENT_TYPE		"Content-Type"
#define HTTP_FIELD_IF_MATCH			"If-Match"
#define HTTP_FIELD_RANGE			"Range"
#define HTTP_FIELD_IF_RANGE			"If-Range"
#define HTTP_FIELD_ACCEPT_LANGUAGE	"Accept-Language"
#define HTTP_FIELD_ACCEPT_CHARSET	"Accept-Charset"

typedef struct _http_header_options_t http_header_options_t;
struct _http_header_options_t{
	char *field;
	char *value;

	http_header_options_t *next;
};

typedef struct _http_header_t http_header_t;
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
}http_msg_request_t;


typedef struct{
	int status_code;
	http_header_t *head;
}http_msg_response_t;

typedef http_header_t *http_msg_iter_t;


typedef struct{
	http_msg_request_t *http_msg_request;
	http_msg_response_t *http_msg_response;
}http_info_t;


da_result_t http_msg_request_create(http_msg_request_t **http_msg_request);
void http_msg_request_destroy(http_msg_request_t **http_msg_request);

da_result_t http_msg_request_set_method(http_msg_request_t *http_msg_request, const char *method);
da_result_t http_msg_request_get_method(http_msg_request_t *http_msg_request, const char **method);

da_result_t http_msg_request_set_url(http_msg_request_t *http_msg_request, const char *url);
da_result_t http_msg_request_get_url(http_msg_request_t *http_msg_request, const char **url);

da_result_t http_msg_request_set_body(http_msg_request_t *http_msg_request, const char *body);
da_result_t http_msg_request_get_body(http_msg_request_t *http_msg_request, const char **body);

da_result_t http_msg_request_add_field(http_msg_request_t *http_msg_request, const char *field, const char *value);


da_result_t http_msg_response_create(http_msg_response_t **http_msg_response);
void http_msg_response_destroy(http_msg_response_t **http_msg_response);

da_result_t http_msg_response_set_status_code(http_msg_response_t *http_msg_response, int status_code);
da_result_t http_msg_response_get_status_code(http_msg_response_t *http_msg_response, int *status_code);

da_result_t http_msg_response_add_field(http_msg_response_t *http_msg_response, const char *field, const char *value);

/* Caution! Caller must free memory for every "char** out_xxx" for followings */
da_bool_t http_msg_response_get_content_type(http_msg_response_t *http_msg_response, char **out_type);
void http_msg_response_set_content_type(http_msg_response_t *http_msg_response, const char *in_type);

da_bool_t http_msg_response_get_content_length(http_msg_response_t *http_msg_response, unsigned long long *out_length);
da_bool_t http_msg_response_get_content_disposition(http_msg_response_t *http_msg_response, char **out_disposition, char **out_file_name);
da_bool_t http_msg_response_get_ETag(http_msg_response_t *http_msg_response, char **out_value);
da_bool_t http_msg_response_get_date(http_msg_response_t *http_msg_response, char **out_value);
da_bool_t http_msg_response_get_location(http_msg_response_t *http_msg_response, char **out_value);
// should be refactored later
da_result_t http_msg_response_get_boundary(http_msg_response_t *http_msg_response, char **out_val);


da_result_t http_msg_request_get_iter(http_msg_request_t *http_msg_request, http_msg_iter_t *http_msg_iter);
da_result_t http_msg_response_get_iter(http_msg_response_t *http_msg_response, http_msg_iter_t *http_msg_iter);

// should remove later
da_bool_t http_msg_get_field_with_iter(http_msg_iter_t *http_msg_iter, char **field, char **value);
da_bool_t http_msg_get_header_with_iter(http_msg_iter_t *http_msg_iter, char **out_field, http_header_t **out_header);

char *get_http_response_header_raw(http_msg_response_t *http_msg_response);

da_bool_t extract_attribute_from_header(char *szHeadStr, const char *szFindStr, char **ppRtnValue);
#endif // _Download_Agent_Http_Msg_Handler_H
