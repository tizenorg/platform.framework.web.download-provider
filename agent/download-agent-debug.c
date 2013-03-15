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

#include <stdlib.h>
#include <string.h>

#include "download-agent-debug.h"
#include "download-agent-utils.h"

#define STRING_IT(x) #x
#define TURN_ON_LOG(channel) (DALogBitMap |= (0x1<<(channel)))

int DALogBitMap;

char *__get_log_env(void);
char **__parsing_log_env(char *in_log_env);
char *__copying_str(char *source, int length);
char *__get_channel_name_from_enum(da_log_channel channel_enum);

da_result_t init_log_mgr(void) {
	da_result_t ret = DA_RESULT_OK;
	static da_bool_t did_log_mgr_init = DA_FALSE;
	char *log_env = DA_NULL;
	char **parsed_log_env = DA_NULL;
	char **cur_parsed_log_env = DA_NULL;
	int i = 0;

	if (did_log_mgr_init)
		return ret;

	did_log_mgr_init = DA_TRUE;

	log_env = __get_log_env();
	if (!log_env) {
		/* If no environment values are found, do behave like all logs are turned on except for Soup log */
		DALogBitMap = ~(0x1 << Soup);
		return ret;
	}

	TURN_ON_LOG(Default);

	parsed_log_env = __parsing_log_env(log_env);
	if (parsed_log_env) {
		char *channel_keyward = DA_NULL;
		for (cur_parsed_log_env = parsed_log_env; *cur_parsed_log_env; cur_parsed_log_env++) {
			if (!*cur_parsed_log_env)
				break;
			for (i = 0; i < DA_LOG_CHANNEL_MAX; i++) {
				channel_keyward = __get_channel_name_from_enum(i);
				if (channel_keyward && !strcmp(*cur_parsed_log_env,
						channel_keyward)) {
					TURN_ON_LOG(i);
					break;
				}
			}
			free(*cur_parsed_log_env);
		}
		free(parsed_log_env);
	}

	if (log_env)
		free(log_env);

	return ret;
}

char *__get_log_env(void) {
	char *log_env = DA_NULL;

	/* environment value has higher priority than configure file */
	log_env = getenv(DA_DEBUG_ENV_KEY);
	if (log_env && strlen(log_env))
		return strdup(log_env);

	if (read_data_from_file(DA_DEBUG_CONFIG_FILE_PATH, &log_env))
		return log_env;

	return DA_NULL;
}

char **__parsing_log_env(char *in_log_env) {
	char **out_parsed_result = DA_NULL;

	char **temp_result_array = DA_NULL;
	char **cur_temp_result_array = DA_NULL;
	int how_many_item = 0;
	int how_many_delimeter = 0;

	char delimiter = ',';

	char *org_str = in_log_env;
	char *cur_char = org_str;
	char *start = org_str;
	char *end = org_str;
	int target_len = 0;

	if (!org_str)
		return DA_NULL;

	/* counting delimiter to know how many items should be memory allocated.
	 *  This causes two round of loop (counting delimiter and real operation).
	 *  But I think it is tolerable, because input parameter is from console.
	 *  And it is also a reason why we should not use fixed length array.
	 *  Users are hard to input very long environment, but it is possible. */
	for (cur_char = org_str; *cur_char; cur_char++) {
		if (*cur_char == delimiter)
			how_many_delimeter++;
	}
	how_many_item = how_many_delimeter + 1;
	temp_result_array = (char**) calloc(1, how_many_item + 1);
	if (!(temp_result_array))
		goto ERR;

	cur_temp_result_array = temp_result_array;
	cur_char = org_str;
	while (1) {
		if (*cur_char == delimiter) {
			end = cur_char;
			target_len = (int) (end - start);
			*cur_temp_result_array++ = __copying_str(start,
					target_len);
			start = ++cur_char;
			continue;
		} else if (!(*cur_char)) {
			end = cur_char;
			target_len = (int) (end - start);
			*cur_temp_result_array++ = __copying_str(start,
					target_len);
			*cur_temp_result_array = DA_NULL;
			break;
		} else {
			cur_char++;
		}
	}
	out_parsed_result = temp_result_array;
ERR:
	return out_parsed_result;
}

char *__copying_str(char *source, int length) {
	char *copied_str = DA_NULL;
	char *cur_pos = DA_NULL;
	char white_space = ' ';
	char end_of_line = 10; /* ASCII for LF */
	int i = 0;

	if (!source || !(length > 0))
		return DA_NULL;

	copied_str = (char*) calloc(1, length + 1);
	if (copied_str) {
		cur_pos = copied_str;
		for (i = 0; i < length; i++) {
			if ((source[i] != white_space) && (source[i]
					!= end_of_line))
				*cur_pos++ = source[i];
		}
	}

	return copied_str;
}

char *__get_channel_name_from_enum(da_log_channel channel_enum) {
	switch (channel_enum) {
	case Soup:
		return STRING_IT(Soup);
	case HTTPManager:
		return STRING_IT(HTTPManager);
	case FileManager:
		return STRING_IT(FileManager);
	case DRMManager:
		return STRING_IT(DRMManager);
	case DownloadManager:
		return STRING_IT(DownloadManager);
	case ClientNoti:
		return STRING_IT(ClientNoti);
	case HTTPMessageHandler:
		return STRING_IT(HTTPMessageHandler);
	case Encoding:
		return STRING_IT(Encoding);
	case QueueManager:
		return STRING_IT(QueueManager);
	case Parsing:
		return STRING_IT(Parsing);
	case Thread:
		return STRING_IT(Thread);
	case Default:
		return STRING_IT(Default);
	default:
		return DA_NULL;
	}
}
