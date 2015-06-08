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

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "download-agent-http-msg-handler.h"
#include "download-agent-debug.h"
#include "download-agent-encoding.h"

// '.' and ';' are request from Vodafone
#define IS_TERMINATING_CHAR(c)			( ((c) == ';') || ((c) == '\0') || ((c) == 0x0d) || ((c) == 0x0a) || ((c) == 0x20) )
#define IS_TERMINATING_CHAR_EX(c)		( ((c) == '"') || ((c) == ';') || ((c) == '\0') || ((c) == 0x0d) || ((c) == 0x0a) || ((c) == 0x20) )
#define IS_URI_TERMINATING_CHAR(c)		( ((c) == '\0') || ((c) == 0x0d) || ((c) == 0x0a) || ((c) == 0x20) )

enum parsing_type {
	WITH_PARSING_OPTION,
	WITHOUT_PARSING_OPTION
};

static da_ret_t __http_header_add_field(http_header_t **head,
	const char *field, const char *value, enum parsing_type type);
static void __http_header_destroy_all_field(http_header_t **head);
static da_bool_t __get_http_header_for_field(
	http_msg_response_t *http_msg_response, const char *in_field,
	http_header_t **out_header);
static void __exchange_header_value(http_header_t *header,
	const char *in_raw_value);
static http_header_options_t *__create_http_header_option(const char *field,
	const char *value);
static void __http_header_destroy_all_option(http_header_options_t **head);
static da_bool_t __get_http_header_option_for_field(
	http_header_options_t *header_option, const char *in_field,
	char **out_value);
static http_header_options_t *__parsing_N_create_option_str(char *org_str);
static http_header_options_t *__parsing_options(char *org_str);
static void __parsing_raw_value(http_header_t *http_header);

da_ret_t http_msg_request_create(http_msg_request_t **http_msg_request)
{
	http_msg_request_t *temp_http_msg_request = NULL;

//	DA_LOGV("");

	temp_http_msg_request = (http_msg_request_t *)calloc(1,
		sizeof(http_msg_request_t));
	if (!temp_http_msg_request) {
		*http_msg_request = NULL;
		DA_LOGE("DA_ERR_FAIL_TO_MEMALLOC");
		return DA_ERR_FAIL_TO_MEMALLOC;
	}

	temp_http_msg_request->http_method = NULL;
	temp_http_msg_request->url = NULL;
	temp_http_msg_request->head = NULL;
	temp_http_msg_request->http_body = NULL;

	*http_msg_request = temp_http_msg_request;
	DA_LOGV( "http_msg_request: %x", (unsigned int)(*http_msg_request));

	return DA_RESULT_OK;
}

void http_msg_request_destroy(http_msg_request_t **http_msg_request)
{
	http_msg_request_t *temp_http_msg_request = *http_msg_request;

	DA_LOGV("");

	if (temp_http_msg_request) {
		if (temp_http_msg_request->http_method) {
			free(temp_http_msg_request->http_method);
			temp_http_msg_request->http_method = NULL;
		}
		if (temp_http_msg_request->url) {
			free(temp_http_msg_request->url);
			temp_http_msg_request->url = NULL;
		}
		if (temp_http_msg_request->http_body) {
			free(temp_http_msg_request->http_body);
			temp_http_msg_request->http_body = NULL;
		}
		__http_header_destroy_all_field(&(temp_http_msg_request->head));
		free(temp_http_msg_request);
		*http_msg_request = NULL;
	}
}

da_ret_t http_msg_request_set_url(http_msg_request_t *http_msg_request,
	const char *url)
{
	DA_LOGV("");

	if (!http_msg_request) {
		DA_LOGE("http_msg_request is NULL; DA_ERR_INVALID_ARGUMENT");
		return DA_ERR_INVALID_ARGUMENT;
	}

	if (!url) {
		DA_LOGE("url is NULL; DA_ERR_INVALID_ARGUMENT");
		return DA_ERR_INVALID_URL;
	}

	http_msg_request->url = strdup(url);
	DA_SECURE_LOGI("http url[%s]", http_msg_request->url);
	return DA_RESULT_OK;
}

da_ret_t http_msg_request_get_url(http_msg_request_t *http_msg_request,
	const char **url)
{
	DA_LOGV("");

	if (!http_msg_request) {
		DA_LOGE("http_msg_request is NULL; DA_ERR_INVALID_ARGUMENT");
		return DA_ERR_INVALID_ARGUMENT;
	}

	if (http_msg_request->url) {
		*url = http_msg_request->url;
		return DA_RESULT_OK;
	} else {
		*url = DA_NULL;
		return DA_ERR_INVALID_ARGUMENT;
	}
}

da_ret_t http_msg_request_add_field(http_msg_request_t *http_msg_request,
	const char *field, const char *value)
{
	//	DA_LOGV("");

	if (!http_msg_request) {
		DA_LOGE("Check NULL! : http_msg_request");
		return DA_ERR_INVALID_ARGUMENT;
	}

	return __http_header_add_field(&(http_msg_request->head), field, value, WITHOUT_PARSING_OPTION);
}

da_ret_t http_msg_response_create(http_msg_response_t **http_msg_response)
{
	http_msg_response_t *temp_http_msg_response = NULL;

	DA_LOGV("");

	temp_http_msg_response = (http_msg_response_t *)calloc(1,
		sizeof(http_msg_response_t));
	if (!temp_http_msg_response) {
		DA_LOGE("DA_ERR_FAIL_TO_MEMALLOC");
		return DA_ERR_FAIL_TO_MEMALLOC;
	} else {
		temp_http_msg_response->status_code = 0;
		temp_http_msg_response->head = NULL;
		*http_msg_response = temp_http_msg_response;
		return DA_RESULT_OK;
	}
}

void http_msg_response_destroy(http_msg_response_t **http_msg_response)
{
	http_msg_response_t *temp_http_msg_response = *http_msg_response;

	DA_LOGV("");
	if (temp_http_msg_response) {
		__http_header_destroy_all_field(&(temp_http_msg_response->head));
		free(temp_http_msg_response);
		*http_msg_response = DA_NULL;
	}
}

da_ret_t http_msg_response_add_field(http_msg_response_t *http_msg_response,
	const char *field, const char *value)
{
	DA_LOGV("");

	if (!http_msg_response) {
		DA_LOGE("DA_ERR_INVALID_ARGUMENT");
		return DA_ERR_INVALID_ARGUMENT;
	}
	return __http_header_add_field(&(http_msg_response->head), field, value, WITH_PARSING_OPTION);
}

da_ret_t __http_header_add_field(http_header_t **head,
	const char *field, const char *value, enum parsing_type type)
{
	http_header_t *pre = NULL;
	http_header_t *cur = NULL;

	//DA_SECURE_LOGD("[%s][%s]", field, value);

	pre = cur = *head;
	while (cur) {
		pre = cur;
		/* Replace default value with user wanted value
		 * Remove the value which is stored before and add a new value.
		*/
		if (cur->field && cur->raw_value &&
				strncasecmp(cur->field, field, strlen(field)) == 0) {
			DA_SECURE_LOGD("Remove value for replacement [%s][%s]", cur->field, cur->raw_value);
			if (cur->field) {
				free(cur->field);
				cur->field = NULL;
			}
			if (cur->raw_value) {
				free(cur->raw_value);
				cur->raw_value= NULL;
			}
		}
		cur = cur->next;
	}

	cur = (http_header_t *)calloc(1, sizeof(http_header_t));
	if (cur) {
		cur->field = strdup(field);
		cur->raw_value = strdup(value);
		cur->options = NULL;
		cur->next = NULL;

		if (type == WITHOUT_PARSING_OPTION) {
			cur->value = strdup(value);
			cur->options = NULL;
		} else {
			__parsing_raw_value(cur);
		}

		if (pre)
			pre->next = cur;
		else
			*head = cur;
	} else {
		DA_LOGE("DA_ERR_FAIL_TO_MEMALLOC");
		return DA_ERR_FAIL_TO_MEMALLOC;
	}

	return DA_RESULT_OK;
}

void __http_header_destroy_all_field(http_header_t **head)
{
	http_header_t *pre = NULL;
	http_header_t *cur = NULL;

	cur = *head;

	while (cur) {
		if (cur->field) {
			free(cur->field);
			cur->field = DA_NULL;
		}
		if (cur->value) {
			free(cur->value);
			cur->value = DA_NULL;
		}
		if (cur->raw_value) {
			free(cur->raw_value);
			cur->raw_value = DA_NULL;
		}
		__http_header_destroy_all_option(&(cur->options));
		free(cur->options);
		cur->options = DA_NULL;
		pre = cur;
		cur = cur->next;
		free(pre);
	}
	*head = DA_NULL;
}

http_header_options_t *__create_http_header_option(const char *field,
	const char *value)
{
	http_header_options_t *option = NULL;

	option = (http_header_options_t *)calloc(1,
		sizeof(http_header_options_t));
	if (option) {
		if (field)
			option->field = strdup(field);
		if (value)
			option->value = strdup(value);
		option->next = NULL;
	}
	return option;
}

void __http_header_destroy_all_option(http_header_options_t **head)
{
	http_header_options_t *pre = NULL;
	http_header_options_t *cur = NULL;

	//	DA_LOGV("");

	cur = *head;

	while (cur) {
		if (cur->field) {
			DA_SECURE_LOGD("field= %s", cur->field);
			free(cur->field);
			cur->field = DA_NULL;
		}
		if (cur->value) {
			free(cur->value);
			cur->value = DA_NULL;
		}
		pre = cur;
		cur = cur->next;
		free(pre);
	}
	*head = DA_NULL;
}

da_ret_t http_msg_request_get_iter(http_msg_request_t *http_msg_request,
	http_msg_iter_t *http_msg_iter)
{
	DA_LOGV("");

	if (!http_msg_request) {
		DA_LOGE("DA_ERR_INVALID_ARGUMENT");
		return DA_ERR_INVALID_ARGUMENT;
	}

	*http_msg_iter = http_msg_request->head;

	return DA_RESULT_OK;
}

da_ret_t http_msg_response_get_iter(http_msg_response_t *http_msg_response,
	http_msg_iter_t *http_msg_iter)
{
	if (!http_msg_response) {
		DA_LOGE("DA_ERR_INVALID_ARGUMENT");
		return DA_ERR_INVALID_ARGUMENT;
	}

	*http_msg_iter = http_msg_response->head;
	return DA_RESULT_OK;
}

da_bool_t http_msg_get_field_with_iter(http_msg_iter_t *http_msg_iter,
	char **out_field, char **out_value)
{
	http_header_t *cur = *http_msg_iter;

	if (cur) {
		*out_field = cur->field;
		*out_value = cur->value;
		*http_msg_iter = cur->next;
		return DA_TRUE;
	} else {
		return DA_FALSE;
	}
}

da_bool_t http_msg_get_header_with_iter(http_msg_iter_t *http_msg_iter,
	char **out_field, http_header_t **out_header)
{
	http_header_t *cur = *http_msg_iter;

	if (cur) {
		*out_field = cur->field;
		*out_header = cur;
		*http_msg_iter = cur->next;
		return DA_TRUE;
	} else {
		return DA_FALSE;
	}
}

http_header_options_t *__parsing_N_create_option_str(char *org_str)
{
	char *option_field = NULL;
	char *option_value = NULL;
	int option_field_len = 0;
	int option_value_len = 0;
	char *org_pos = NULL;
	int org_str_len = 0;
	char *working_str = NULL;
	char *working_pos = NULL;
	char *working_pos_field_start = NULL;
	char *working_pos_value_start = NULL;
	da_bool_t is_inside_quotation = DA_FALSE;
	da_bool_t is_working_for_field = DA_TRUE;
	int i = 0;
	http_header_options_t *option = NULL;

	//	DA_LOGV("");

	if (!org_str)
		return NULL;

	org_str_len = strlen(org_str);
	if (org_str_len <= 0)
		return NULL;

	working_str = (char *)calloc(1, org_str_len + 1);
	if (!working_str)
		return NULL;

	org_pos = org_str;
	working_pos_field_start = working_pos = working_str;

	for (i = 0; i < org_str_len; i++) {
		if (*org_pos == '"')
			is_inside_quotation = !is_inside_quotation;
		if (is_inside_quotation) {
			// Leave anything including blank if it is inside of double quotation mark.
			*working_pos = *org_pos;
			is_working_for_field ? option_field_len++
				: option_value_len++;
			working_pos++;
			org_pos++;
		} else {
			if (*org_pos == ' ') {
				org_pos++;
			} else if (*org_pos == '=') {
				if (is_working_for_field) {
					is_working_for_field = DA_FALSE;
					working_pos_value_start = working_pos;
				}
				org_pos++;
			} else {
				*working_pos = *org_pos;
				is_working_for_field ? option_field_len++
					: option_value_len++;
				working_pos++;
				org_pos++;
			}
		}
	}

	if (option_field_len > 0 && working_pos_field_start) {
		option_field = (char *)calloc(1, option_field_len + 1);
		if (option_field)
			strncpy(option_field, working_pos_field_start,
				option_field_len);
	}
	if (option_value_len > 0 && working_pos_value_start) {
		option_value = (char *)calloc(1, option_value_len + 1);
		if (option_value)
			strncpy(option_value, working_pos_value_start,
				option_value_len);
	}
	if (working_str) {
		free(working_str);
		working_pos = working_str = NULL;
	}

	DA_SECURE_LOGD("option_field = [%s], option_value = [%s]",
		option_field, option_value);

	if (option_field || option_value) {
		option = __create_http_header_option(
			option_field, option_value);
		if (option_field) {
			free(option_field);
			option_field = NULL;
		}
		if (option_value) {
			free(option_value);
			option_value = NULL;
		}
	}
	return option;
}

http_header_options_t *__parsing_options(char *org_str)
{
	da_ret_t ret = DA_RESULT_OK;
	http_header_options_t *head = NULL;
	http_header_options_t *pre = NULL;
	http_header_options_t *cur = NULL;

	int wanted_str_len = 0;
	char *wanted_str = NULL;
	char *wanted_str_start = NULL;
	char *wanted_str_end = NULL;
	char *cur_pos = NULL;

	DA_LOGV("");

	if (!org_str)
		return NULL;

	/* Do Not use strtok(). It's not thread safe. */
	//	DA_SECURE_LOGD("org_str = %s", org_str);

	cur_pos = org_str;

	while (cur_pos) {
		wanted_str_start = cur_pos;
		wanted_str_end = strchr(cur_pos, ';');
		if (wanted_str_end) {
			cur_pos = wanted_str_end + 1;
		} else {
			wanted_str_end = org_str + strlen(org_str);
			cur_pos = NULL;
		}
		wanted_str_len = wanted_str_end - wanted_str_start;
		wanted_str = (char *)calloc(1, wanted_str_len + 1);
		if (!wanted_str) {
			DA_LOGE("DA_ERR_FAIL_TO_MEMALLOC");
			ret = DA_ERR_FAIL_TO_MEMALLOC;
			goto ERR;
		}
		strncpy(wanted_str, wanted_str_start, wanted_str_len);

		//		DA_SECURE_LOGD("wanted_str = [%s]", wanted_str);
		cur = __parsing_N_create_option_str(wanted_str);
		if (pre) {
			pre->next = cur;
			pre = cur;
		} else {
			head = pre = cur;
		}

		free(wanted_str);
		wanted_str = NULL;
	}

ERR:
	if (ret != DA_RESULT_OK)
		__http_header_destroy_all_option(&head);
	return head;
}

void __parsing_raw_value(http_header_t *http_header_field)
{
	char *raw_value = NULL;
	char *option_str_start = NULL;
	char *trimed_value = NULL;
	int trimed_value_len = 0;
	char *trimed_value_start = NULL;
	char *trimed_value_end = NULL;

	raw_value = http_header_field->raw_value;
	//	DA_SECURE_LOGD("raw_value = [%s]", raw_value);

	if (!raw_value)
		return;

	trimed_value_start = raw_value;
	trimed_value_end = strchr(raw_value, ';');
	if (!trimed_value_end) {
		// No options
		http_header_field->value = strdup(raw_value);
		http_header_field->options = NULL;

		return;
	}

	// for trimed value
	trimed_value_len = trimed_value_end - trimed_value_start;
	trimed_value = (char *)calloc(1, trimed_value_len + 1);
	if (!trimed_value) {
		DA_LOGE("DA_ERR_FAIL_TO_MEMALLOC");
		return;
	}
	strncpy(trimed_value, trimed_value_start, trimed_value_len);
	http_header_field->value = trimed_value;

	// for option parsing
	option_str_start = trimed_value_end + 1;
	http_header_field->options = __parsing_options(option_str_start);

	/////////////// show
	http_header_options_t *cur = NULL;
	cur = http_header_field->options;
	while (cur) {
//		DA_SECURE_LOGD("field = [%s], value = [%s]", cur->field, cur->value);
		cur = cur->next;
	}
}

da_bool_t __get_http_header_option_for_field(
	http_header_options_t *header_option, const char *in_field,
	char **out_value)
{
	http_header_options_t *cur = NULL;

	//	DA_LOGV("");

	if (!header_option) {
		DA_LOGE("input header_option is NULL.");
		return DA_FALSE;
	}

	cur = header_option;
	while (cur) {
		if (cur->field) {
			if (!strncasecmp(cur->field, in_field, strlen(cur->field)) &&
					cur->value) {
				DA_SECURE_LOGD("[%s][%s]", cur->field, cur->value);
				*out_value = cur->value;
				return DA_TRUE;
			}

		}
		cur = cur->next;
	}
	return DA_FALSE;
}

da_bool_t __get_http_header_for_field(http_msg_response_t *http_msg_response,
	const char *in_field, http_header_t **out_header)
{
	http_msg_iter_t http_msg_iter;
	http_header_t *header = NULL;
	char *field = NULL;

	//DA_LOGV("");

	http_msg_response_get_iter(http_msg_response, &http_msg_iter);
	while (http_msg_get_header_with_iter(&http_msg_iter, &field, &header)) {
		if (field && header && !strncasecmp(field, in_field, strlen(field))) {
			//DA_SECURE_LOGD("[%s][%s]", field, header->value);
			*out_header = header;
			return DA_TRUE;
		}
	}

	return DA_FALSE;
}

da_bool_t __get_http_req_header_for_field(http_msg_request_t *http_msg_request,
	const char *in_field, http_header_t **out_header)
{
	http_msg_iter_t http_msg_iter;
	http_header_t *header = NULL;
	char *field = NULL;

	//DA_LOGV("");

	http_msg_request_get_iter(http_msg_request, &http_msg_iter);
	while (http_msg_get_header_with_iter(&http_msg_iter, &field, &header)) {
		if (field && header && !strncasecmp(field, in_field, strlen(field))) {
			//DA_SECURE_LOGD("[%s][%s]", field, header->value);
			*out_header = header;
			return DA_TRUE;
		}
	}

	return DA_FALSE;
}

void __exchange_header_value(http_header_t *header, const char *in_raw_value)
{
	DA_LOGV("");

	if (!header || !in_raw_value)
		return;

	__http_header_destroy_all_option(&(header->options));

	if (header->value) {
		free(header->value);
		header->value = DA_NULL;
	}
	if (header->raw_value)
		free(header->raw_value);
	header->raw_value = strdup(in_raw_value);

	__parsing_raw_value(header);
}

da_bool_t http_msg_response_get_content_type(
	http_msg_response_t *http_msg_response, char **out_type)
{
	da_bool_t b_ret = DA_FALSE;
	http_header_t *header = NULL;

	DA_LOGV("");

	b_ret = __get_http_header_for_field(http_msg_response,
			HTTP_FIELD_CONTENT_TYPE, &header);
	if (!b_ret) {
		DA_LOGV("no Content-Type");
		return DA_FALSE;
	}
	if (out_type)
		*out_type = strdup(header->value);

	return DA_TRUE;
}

void http_msg_response_set_content_type(http_msg_response_t *http_msg_response,
	const char *in_type)
{
	da_bool_t b_ret = DA_FALSE;
	http_header_t *header = NULL;

	DA_LOGV("");

	if (!http_msg_response || !in_type)
		return;

	b_ret = __get_http_header_for_field(http_msg_response,
			HTTP_FIELD_CONTENT_TYPE, &header);
	if (b_ret) {
		if (header->raw_value && (!strncmp(header->raw_value, in_type,
			strlen(header->raw_value))))
			return;

		DA_SECURE_LOGD("exchange Content-Type to [%s] from [%s]", in_type, header->value);
		__exchange_header_value(header, in_type);
	} else {
		__http_header_add_field(&(http_msg_response->head),
				HTTP_FIELD_CONTENT_TYPE, in_type, WITH_PARSING_OPTION);
	}
}

da_bool_t http_msg_response_get_content_length(
	http_msg_response_t *http_msg_response, da_size_t *out_length)
{
	da_bool_t b_ret = DA_FALSE;
	http_header_t *header = NULL;

	DA_LOGV("");

	b_ret = __get_http_header_for_field(http_msg_response,
			HTTP_FIELD_CONTENT_LENGTH, &header);
	if (!b_ret) {
		DA_LOGV( "no Content-Length");
		return DA_FALSE;
	}

	if (out_length)
		*out_length = atoll(header->value);

	return DA_TRUE;
}

da_bool_t http_msg_response_get_content_disposition(
	http_msg_response_t *http_msg_response, char **out_disposition,
	char **out_file_name)
{
	da_bool_t b_ret = DA_FALSE;
	http_header_t *header = NULL;
	char *file_name = NULL;
	char *wanted_str = NULL;
	char *wanted_str_start = NULL;
	char *wanted_str_end = NULL;
	char *decoded_str = NULL;
	int wanted_str_len = 0;

	DA_LOGV("");

	b_ret = __get_http_header_for_field(http_msg_response,
			HTTP_FIELD_CONTENT_DISPOSITION, &header);
	if (!b_ret) {
		DA_LOGV( "no Content-Disposition");
		return DA_FALSE;
	}
	if (out_disposition)
		*out_disposition = strdup(header->value);
	if (!out_file_name)
		return DA_FALSE;

	b_ret = __get_http_header_option_for_field(header->options, "filename",
		&file_name);
	if (!b_ret) {
		DA_LOGV( "no option");
		return DA_FALSE;
	}

	// eliminate double quotation mark if it exists on derived value
	wanted_str_start = strchr(file_name, '"');
	if (!wanted_str_start) {
		*out_file_name = strdup(file_name);
		return DA_TRUE;
	} else {
		//		DA_SECURE_LOGD("wanted_str_start = [%s]", wanted_str_start);
		wanted_str_start++;
		wanted_str_end = strchr(wanted_str_start, '"');
		if (wanted_str_end) {
			wanted_str_len = wanted_str_end - wanted_str_start;
			wanted_str = (char*)calloc(1, wanted_str_len + 1);
			if (!wanted_str) {
				DA_LOGE("DA_ERR_FAIL_TO_MEMALLOC");
				return DA_FALSE;
			}
			strncpy(wanted_str, wanted_str_start, wanted_str_len);

			b_ret = is_base64_encoded_word(wanted_str);
			if (b_ret) {
				DA_LOGV("It's base64 encoded-word string");
				if (DA_RESULT_OK == decode_base64_encoded_str(
					wanted_str, &decoded_str)) {
					DA_SECURE_LOGD("base64 decoded str = [%s]", decoded_str);
					free(wanted_str);
					wanted_str = decoded_str;
					decoded_str = NULL;
				} else {
					DA_LOGV("Fail to base64 decode. Just use un-decoded string.");
				}
			} else {
				DA_LOGV("It's NOT base64 encoded-word string");
			}
			decode_url_encoded_str(wanted_str, &decoded_str);
			/* If it is url encoded string */
			if (decoded_str) {
				DA_SECURE_LOGD("Url decoded str = [%s]", decoded_str);
				free(wanted_str);
				wanted_str = decoded_str;
				decoded_str = NULL;
			}
			*out_file_name = wanted_str;
			DA_SECURE_LOGI("out_file_name = [%s]", *out_file_name);
			return DA_TRUE;
		} else {
			DA_LOGE("Not matched \" !");
			return DA_FALSE;
		}
	}
}

da_bool_t http_msg_response_get_ETag(http_msg_response_t *http_msg_response,
	char **out_value)
{
	da_bool_t b_ret = DA_FALSE;
	http_header_t *header = NULL;

	DA_LOGV("");

	b_ret = __get_http_header_for_field(http_msg_response, HTTP_FIELD_ETAG,
			&header);
	if (!b_ret) {
		DA_LOGV( "no ETag");
		return DA_FALSE;
	}
	if (out_value)
		*out_value = strdup(header->value);

	return DA_TRUE;
}

#ifdef _RAF_SUPPORT
da_bool_t http_msg_response_get_RAF_mode(http_msg_response_t *http_msg_response,
	char **out_value)
{
	da_bool_t b_ret = DA_FALSE;
	http_header_t *header = NULL;

	DA_LOGV("");

	b_ret = __get_http_header_for_field(http_msg_response, HTTP_FIELD_RAF_MODE,
			&header);
	if (!b_ret) {
		DA_LOGV( "no RAF mode");
		return DA_FALSE;
	}
	if (out_value)
		*out_value = strdup(header->value);

	return DA_TRUE;
}
#endif

da_bool_t http_msg_response_get_date(http_msg_response_t *http_msg_response,
	char **out_value)
{
	da_bool_t b_ret = DA_FALSE;
	http_header_t *header = NULL;

	DA_LOGV("");

	b_ret = __get_http_header_for_field(http_msg_response,
			HTTP_FIELD_DATA, &header);
	if (!b_ret) {
		DA_LOGV( "no Date");
		return DA_FALSE;
	}
	if (out_value)
		*out_value = strdup(header->value);

	return DA_TRUE;
}

da_bool_t http_msg_response_get_location(http_msg_response_t *http_msg_response,
	char **out_value)
{
	da_bool_t b_ret = DA_FALSE;
	http_header_t *header = NULL;

	DA_LOGV("");

	b_ret = __get_http_header_for_field(http_msg_response,
			HTTP_FIELD_LOCATION, &header);
	if (!b_ret) {
		DA_LOGV( "no Location");
		return DA_FALSE;
	}
	if (out_value)
		*out_value = strdup(header->value);

	return DA_TRUE;
}

char *__stristr(const char *long_str, const char *find_str)
{
	int i = 0;
	int length_long = 0;
	int length_find = 0;
	char *ret_ptr = NULL;
	char *org_ptr = NULL;
	char *look_ptr = NULL;

	if (long_str == NULL || find_str == NULL) {
		DA_LOGE("INVALID ARGUMENT");
		return NULL;
	}

	length_long = strlen(long_str);
	length_find = strlen(find_str);

	org_ptr = (char*)calloc(1, length_long + 1);

	if (org_ptr == NULL) {
		DA_LOGE("INVALID ARGUMENT");
		return NULL;
	}

	look_ptr = (char*)calloc(1, length_find + 1);

	if (look_ptr == NULL) {
		DA_LOGE("INVALID ARGUMENT");
		free(org_ptr);
		return NULL;
	}

	while (i < length_long) {
		if (isalpha(long_str[i]) != 0) {
			if (isupper(long_str[i]) != 0) {
				org_ptr[i] = long_str[i];
			} else {
				org_ptr[i] = toupper(long_str[i]);
			}
		} else {
			org_ptr[i] = long_str[i];
		}
		i++;
	}

	i = 0;

	while (i < length_find) {
		if (isalpha(find_str[i]) != 0) {
			if (isupper(find_str[i]) != 0) {
				look_ptr[i] = find_str[i];
			} else {
				look_ptr[i] = toupper(find_str[i]);
			}
		} else {
			look_ptr[i] = find_str[i];
		}
		i++;
	}

	ret_ptr = strstr(org_ptr, look_ptr);

	if (ret_ptr == 0) {
		free(org_ptr);
		free(look_ptr);
		return NULL;
	} else {
		i = ret_ptr - org_ptr;
	}

	free(org_ptr);
	free(look_ptr);

	return (char*)(long_str + i);
}

/* This is not used. But it can be needed if there is no http header parser at http library.*/
da_bool_t extract_attribute_from_header(
        char *szHeadStr,
        const char *szFindStr,
        char **ppRtnValue)
{
	char *pValuePos = NULL;
	int index = 0;
	int startPos = 0;
	int strLen = 0;
	int need_to_end_quataion_mark = 0;

	if (szHeadStr == DA_NULL || szFindStr == DA_NULL) {
		DA_LOGE("INVALID ARGUMENT");
		return DA_FALSE;
	}
	if (strlen(szHeadStr) <= 0 || strlen(szFindStr) <= 0) {
		DA_LOGE("INVALID ARGUMENT");;

		return DA_FALSE;
	}
	if (ppRtnValue == NULL) {
		return DA_FALSE;
	}

	pValuePos = __stristr(szHeadStr, (char*)szFindStr);
	if (pValuePos == NULL) {
		*ppRtnValue = NULL;
		goto ERR;
	}

	index = strlen(szFindStr);

	while (pValuePos[index] != ':' && pValuePos[index] != '=') {
		index++;

		if (pValuePos[index] == '\0') {
			return DA_FALSE;
		}
	}

	index++;

	/* jump space */
	while (pValuePos[index] == ' ') {
		index++;
	}

	/* jump quatation mark */
	while (pValuePos[index] == '"') {
		need_to_end_quataion_mark = 1;
		index++;
	}

	startPos = index;

	/* Find the end of data. */
	if (0 == strncasecmp(szFindStr, HTTP_FIELD_LOCATION,
			strlen(HTTP_FIELD_LOCATION)))//terminate character list does not contain ';' in case of URI
	{
		while (DA_FALSE == IS_URI_TERMINATING_CHAR(pValuePos[index])) {
			index++;
		}
	} else if (need_to_end_quataion_mark) {
		while (DA_FALSE == IS_TERMINATING_CHAR_EX(pValuePos[index])) {
			index++;
		}
	} else {
		while (DA_FALSE == IS_TERMINATING_CHAR(pValuePos[index])) {
			index++;
		}
	}

	strLen = index - startPos;

	if (strLen < 1) {
		DA_LOGE(" strLen is < 1");
		goto ERR;
	}

	*ppRtnValue = (char*)calloc(1, sizeof(char) * (strLen + 1));

	if (*ppRtnValue == NULL) {
		DA_LOGE(" *ppRtnValue is NULL");
		goto ERR;
	}

	strncpy(*ppRtnValue, pValuePos + startPos, strLen);
	*(*ppRtnValue + strLen) = '\0';

	return DA_TRUE;
ERR:
	if (*ppRtnValue) {
		free(*ppRtnValue);
		*ppRtnValue = NULL;
	}
	return DA_FALSE;
}

da_bool_t http_msg_request_get_if_range(http_msg_request_t *http_msg_request,
	char **out_value)
{
	da_bool_t b_ret = DA_FALSE;
	http_header_t *header = NULL;

	DA_LOGV("");

	b_ret = __get_http_req_header_for_field(http_msg_request, HTTP_FIELD_IF_RANGE,
			&header);
	if (!b_ret) {
		DA_LOGV( "no If Range");
		return DA_FALSE;
	}
	if (out_value)
		*out_value = strdup(header->value);

	return DA_TRUE;
}

da_bool_t http_msg_request_get_range(http_msg_request_t *http_msg_request,
	char **out_value)
{
	da_bool_t b_ret = DA_FALSE;
	http_header_t *header = NULL;

	DA_LOGV("");

	b_ret = __get_http_req_header_for_field(http_msg_request, HTTP_FIELD_RANGE,
			&header);
	if (!b_ret) {
		DA_LOGV( "no Range");
		return DA_FALSE;
	}
	if (out_value)
		*out_value = strdup(header->value);

	return DA_TRUE;
}
