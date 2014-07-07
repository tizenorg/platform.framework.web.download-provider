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

#ifndef _Download_Agent_Utils_H
#define _Download_Agent_Utils_H

#include <time.h>
#include "download-agent-defs.h"
#include "download-agent-interface.h"
#include "download-agent-dl-mgr.h"

#define SAVE_FILE_BUFFERING_SIZE_50KB (50*1024)
#define SAVE_FILE_BUFFERING_SIZE_5MB (5*1024*1024)

#define DA_SLEEP(x) \
	do \
	{ \
		struct timespec interval,remainder; \
		interval.tv_sec = (unsigned int)((x)/1000); \
		interval.tv_nsec = (((x)-(interval.tv_sec*1000))*1000000); \
		nanosleep(&interval,&remainder); \
	} while(0)

typedef enum {
	DA_MIME_TYPE_NONE,
	DA_MIME_TYPE_DRM1_MESSATE,
	DA_MIME_TYPE_END
} da_mime_type_id_t;

void get_random_number(int *out_num);
da_result_t  get_available_dd_id(int *available_id);
da_result_t  get_extension_from_mime_type(char *mime_type, char **extension);
da_mime_type_id_t get_mime_type_id(char *content_type);
da_bool_t is_valid_url(const char *url, da_result_t *err_code);

int read_data_from_file(char *file, char**out_buffer);
da_result_t move_file(const char *from_path, const char *to_path);
void remove_file(const char *file_path);
char *_stristr(const char *long_str, const char *find_str);

#endif
