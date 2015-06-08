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
#include "download-agent-dl-info.h"

// Reqeust Header
#define HTTP_FIELD_UAGENT			"User-Agent"
#define HTTP_FIELD_HOST				"Host"
#define HTTP_FIELD_UAPROF			"X-Wap-Profile"
#define HTTP_FIELD_IF_MATCH			"If-Match"
#define HTTP_FIELD_RANGE			"Range"
#define HTTP_FIELD_IF_RANGE			"If-Range"
#define HTTP_FIELD_ACCEPT_LANGUAGE	"Accept-Language"
#define HTTP_FIELD_ACCEPT_CHARSET	"Accept-Charset"

// Response Header
#define HTTP_FIELD_CONTENT_LENGTH	"Content-Length"
#define HTTP_FIELD_CONTENT_TYPE		"Content-Type"
#define HTTP_FIELD_CONTENT_DISPOSITION "Content-Disposition"
#define HTTP_FIELD_LOCATION "Location"
#define HTTP_FIELD_DATA "Date"
#define HTTP_FIELD_ETAG "ETag"
#ifdef _RAF_SUPPORT
#define HTTP_FIELD_RAF_MODE "x-direct-write"
#endif

#define HTTP_FIELD_END_OF_FIELD		"\r\n"

da_ret_t http_msg_request_create(http_msg_request_t **http_msg_request);
void http_msg_request_destroy(http_msg_request_t **http_msg_request);
da_ret_t http_msg_request_set_url(http_msg_request_t *http_msg_request, const char *url);
da_ret_t http_msg_request_get_url(http_msg_request_t *http_msg_request, const char **url);
da_ret_t http_msg_request_add_field(http_msg_request_t *http_msg_request, const char *field, const char *value);

da_ret_t http_msg_response_create(http_msg_response_t **http_msg_response);
void http_msg_response_destroy(http_msg_response_t **http_msg_response);
da_ret_t http_msg_response_add_field(http_msg_response_t *http_msg_response, const char *field, const char *value);
/* Caution! Caller must free memory for every "char** out_xxx" for followings */
da_bool_t http_msg_response_get_content_type(http_msg_response_t *http_msg_response, char **out_type);
void http_msg_response_set_content_type(http_msg_response_t *http_msg_response, const char *in_type);

da_bool_t http_msg_response_get_content_length(http_msg_response_t *http_msg_response, da_size_t *out_length);
da_bool_t http_msg_response_get_content_disposition(http_msg_response_t *http_msg_response, char **out_disposition, char **out_file_name);
da_bool_t http_msg_response_get_ETag(http_msg_response_t *http_msg_response, char **out_value);
da_bool_t http_msg_response_get_date(http_msg_response_t *http_msg_response, char **out_value);
da_bool_t http_msg_response_get_location(http_msg_response_t *http_msg_response, char **out_value);
#ifdef _RAF_SUPPORT
da_bool_t http_msg_response_get_RAF_mode(http_msg_response_t *http_msg_response,
	char **out_value);
#endif
da_ret_t http_msg_request_get_iter(http_msg_request_t *http_msg_request, http_msg_iter_t *http_msg_iter);
da_ret_t http_msg_response_get_iter(http_msg_response_t *http_msg_response, http_msg_iter_t *http_msg_iter);
// should remove later
da_bool_t http_msg_get_field_with_iter(http_msg_iter_t *http_msg_iter, char **field, char **value);
da_bool_t http_msg_get_header_with_iter(http_msg_iter_t *http_msg_iter, char **out_field, http_header_t **out_header);
da_bool_t extract_attribute_from_header(char *szHeadStr, const char *szFindStr, char **ppRtnValue);
da_bool_t http_msg_request_get_if_range(http_msg_request_t *http_msg_request, char **out_value);
da_bool_t http_msg_request_get_range(http_msg_request_t *http_msg_request, char **out_value);

#endif // _Download_Agent_Http_Msg_Handler_H
