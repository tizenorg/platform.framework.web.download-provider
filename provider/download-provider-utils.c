/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd All Rights Reserved
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
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <sys/time.h>
#include <sys/statfs.h>
#include <unistd.h>

#include "download-provider-log.h"

char *dp_strdup(char *src)
{
	char *dest = NULL;
	size_t src_len = 0;

	if (src == NULL) {
		TRACE_ERROR("[CHECK PARAM]");
		return NULL;
	}

	src_len = strlen(src);
	if (src_len <= 0) {
		TRACE_ERROR("[CHECK PARAM] len[%d]", src_len);
		return NULL;
	}

	dest = (char *)calloc(src_len + 1, sizeof(char));
	if (dest == NULL) {
		TRACE_ERROR("[CHECK] allocation");
		return NULL;
	}
	memcpy(dest, src, src_len * sizeof(char));
	dest[src_len] = '\0';

	return dest;
}

int dp_is_file_exist(const char *file_path)
{
	struct stat file_state;
	int stat_ret;

	if (file_path == NULL) {
		TRACE_ERROR("[NULL-CHECK] file path is NULL");
		return -1;
	}

	stat_ret = stat(file_path, &file_state);

	if (stat_ret == 0)
		if (file_state.st_mode & S_IFREG)
			return 0;

	return -1;
}

long dp_get_file_modified_time(const char *file_path)
{
	struct stat file_state;
	int stat_ret;

	if (file_path == NULL) {
		TRACE_ERROR("[NULL-CHECK] file path is NULL");
		return -1;
	}

	stat_ret = stat(file_path, &file_state);
	if (stat_ret == 0)
		return file_state.st_mtime;
	return -1;
}

int dp_remove_file(const char *file_path)
{
	if ((file_path != NULL && strlen(file_path) > 0) &&
			dp_is_file_exist(file_path) == 0) {
		if (unlink(file_path) != 0) {
			TRACE_ERROR("failed to remove file");
			return -1;
		}
		return 0;
	}
	return -1;
}
