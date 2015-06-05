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
#include "glib.h"

#include "download-agent-debug.h"
#include "download-agent-encoding.h"

da_ret_t _parsing_base64_encoded_str(const char *in_encoded_str,
	char **out_charset_type,
	char *out_encoding_type,
	char **out_raw_encoded_str);

da_bool_t is_base64_encoded_word(const char *in_str)
{
	const char *haystack = DA_NULL;
	char first_needle[8] = {0,};
	char second_needle[8] = {0,};
	char *found_str = DA_NULL;

	if (!in_str) {
		DA_LOGE("input string is NULL");
		return DA_FALSE;
	}

	haystack = in_str;
	if (haystack[0] == '"') {
		snprintf(first_needle, sizeof(first_needle), "%s", "\"=?");	// "=?
		snprintf(second_needle, sizeof(second_needle), "%s", "?=\"");	// ?="
	} else {
		snprintf(first_needle, sizeof(first_needle), "%s", "=?");	// =?
		snprintf(second_needle, sizeof(second_needle), "%s", "?=");	// ?=
	}

	found_str = strstr(haystack, first_needle);
	if (found_str) {
		if (found_str == haystack) {
			haystack = haystack + strlen(haystack) - strlen(second_needle);
			if(!strcmp(haystack, second_needle))
				return DA_TRUE;
		}
	}
	return DA_FALSE;
}

da_ret_t decode_base64_encoded_str(const char *in_encoded_str,
	char **out_decoded_ascii_str)
{
	da_ret_t ret = DA_RESULT_OK;
	const char *org_str = DA_NULL;
	char *charset_type = NULL;
	char encoding_type = '\0';
	char *raw_encoded_str = NULL;
	char *decoded_str = NULL;
	const gchar *g_encoded_text = NULL;
	guchar *g_decoded_text = NULL;
	gsize g_decoded_text_len = 0;

	DA_SECURE_LOGD("input str = [%s]", in_encoded_str);

	org_str = in_encoded_str;
	if(!org_str) {
		DA_LOGE("Input string is NULL");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}

	ret = _parsing_base64_encoded_str(org_str, &charset_type,
		&encoding_type, &raw_encoded_str);
	if(ret != DA_RESULT_OK) {
		goto ERR;
	}

	if(encoding_type != 'B') {
		DA_LOGE("Encoded Word is not encoded with Base64, but %c. We can only handle Base64.", encoding_type);
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}

	/*
	 * on glib/gtype.h
	 * typedef char   gchar;
	 * typedef unsigned char   guchar;
	 *
	 */
	g_encoded_text = (const gchar*)raw_encoded_str;
	g_decoded_text = g_base64_decode(g_encoded_text, &g_decoded_text_len);

	if(g_decoded_text) {
		DA_SECURE_LOGD("g_decoded_text = [%s]", g_decoded_text);
		decoded_str = (char*)calloc(1, g_decoded_text_len+1);
		if(!decoded_str) {
			DA_LOGE("DA_ERR_FAIL_TO_MEMALLOC");
			ret = DA_ERR_FAIL_TO_MEMALLOC;
			goto ERR;
		} else {
			memcpy(decoded_str, g_decoded_text, g_decoded_text_len);
		}
	}
	DA_SECURE_LOGD("decoded_str = [%s]", decoded_str);

ERR:
	*out_decoded_ascii_str = decoded_str;
	if(charset_type) {
		free(charset_type);
		charset_type = NULL;
	}
	if(raw_encoded_str) {
		free(raw_encoded_str);
		raw_encoded_str = NULL;
	}
	if(g_decoded_text) {
		g_free(g_decoded_text);
	}
	return ret;
}

da_ret_t _parsing_base64_encoded_str(const char *in_encoded_str,
	char **out_charset_type,
	char *out_encoding_type,
	char **out_raw_encoded_str)
{
	da_ret_t ret = DA_RESULT_OK;
	const char *org_str = DA_NULL;	// e.g. =?UTF-8?B?7Jew7JWE7JmA7IKs7J6QLmpwZw==?=
	char *charset_type = NULL;		// e.g. UTF-8
	char encoding_type = '\0';		// e.g. B (means Base64)
	char *raw_encoded_str = NULL;	// e.g. 7Jew7JWE7JmA7IKs7J6QLmpwZw==
	char *haystack = DA_NULL;
	char needle[8] = {0,};
	char *wanted_str = DA_NULL;
	int wanted_str_len = 0;
	char *wanted_str_start = DA_NULL;
	char *wanted_str_end = DA_NULL;

	org_str = in_encoded_str;
	if (!org_str) {
		DA_LOGE("Input string is NULL");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}

	// strip "=?"
	haystack = (char*)org_str;
	snprintf(needle, sizeof(needle), "=?");
	wanted_str_end = strstr(haystack, needle);
	if (!wanted_str_end) {
		DA_LOGE("DA_ERR_INVALID_ARGUMENT");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	} else {
		wanted_str = wanted_str_end + strlen(needle);
	}

	// for charset
	haystack = wanted_str_start = wanted_str;
	needle[0] = '?';
	wanted_str_end = strchr(haystack, needle[0]);
	if (!wanted_str_end) {
		DA_LOGE("DA_ERR_INVALID_ARGUMENT");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	} else {
		wanted_str_len = wanted_str_end - wanted_str_start + 1;
		wanted_str = (char*)calloc(1, wanted_str_len+1);
		if (!wanted_str) {
			DA_LOGE("DA_ERR_FAIL_TO_MEMALLOC");
			ret = DA_ERR_FAIL_TO_MEMALLOC;
			goto ERR;
		} else {
			snprintf(wanted_str, wanted_str_len+1, "%s", wanted_str_start);
			charset_type = wanted_str;
			wanted_str = DA_NULL;
		}

		DA_LOGV("charset [%s]", charset_type);
	}

	// for encoding
	encoding_type = *(++wanted_str_end);
	DA_LOGV("encoding [%c]", encoding_type);

	// for raw encoded str
	haystack = wanted_str_start = wanted_str_end + 1;
	snprintf(needle, sizeof(needle), "?=");
	wanted_str_end = strstr(haystack, needle);
	if (!wanted_str_end) {
		DA_LOGE("DA_ERR_INVALID_ARGUMENT");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	} else {
		wanted_str_len = wanted_str_end - wanted_str_start + 1;
		wanted_str = (char*)calloc(1, wanted_str_len+1);
		if (!wanted_str) {
			DA_LOGE("DA_ERR_FAIL_TO_MEMALLOC");
			ret = DA_ERR_FAIL_TO_MEMALLOC;
			goto ERR;
		} else {
			snprintf(wanted_str, wanted_str_len+1, "%s", wanted_str_start);
			raw_encoded_str = wanted_str;
			wanted_str = NULL;
		}

		DA_SECURE_LOGD("raw encoded str [%s]", raw_encoded_str);
	}
ERR:
	if (ret != DA_RESULT_OK) {
		if (charset_type) {
			free(charset_type);
			charset_type = NULL;
		}
	}
	*out_charset_type = charset_type;
	*out_encoding_type = encoding_type;
	*out_raw_encoded_str = raw_encoded_str;
	return ret;
}

void decode_url_encoded_str(const char *in_encoded_str, char **out_str)
{
	char *in = NULL;
	char *out = NULL;
	*out_str = calloc(1, strlen(in_encoded_str) + 1);
	if (*out_str == NULL)
		return;
    out = *out_str;
    in = (char *)in_encoded_str;
	while (*in)
	{
		if (*in == '%') {
			int hex = 0;
			in++;
			if (sscanf(in, "%2x", &hex) <= 0) {
				return;
			} else {
				*out = hex;
				in++;
			}
		} else if (*in == '+') {
			*out = ' ';
		} else {
			*out = *in;
		}
		in++;
		out++;
	}
}
