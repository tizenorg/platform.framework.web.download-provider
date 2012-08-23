/*
 * Download Agent
 *
 * Copyright (c) 2000 - 2012 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Jungki Kwak <jungki.kwak@samsung.com>, Keunsoon Lee <keunsoon.lee@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file		download-agent-utils.c
 * @brief		Including some utilitis
 * @author		Keunsoon Lee(keunsoon.lee@samsung.com)
 * @author		Jungki Kwak(jungki.kwak@samsung.com)
 ***/

#include <sys/vfs.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <glib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "download-agent-client-mgr.h"
#include "download-agent-debug.h"
#include "download-agent-dl-mgr.h"
#include "download-agent-file.h"
#include "download-agent-http-misc.h"
#include "download-agent-mime-util.h"
#include "download-agent-utils.h"
#include "download-agent-plugin-conf.h"
#include "download-agent-plugin-install.h"
#include "download-agent-dl-info-util.h"

#define DA_HTTP_HEADER_CONTENT_TYPE		"Content-Type"
#define DA_HTTP_HEADER_CONTENT_LENGTH	"Content-Length"
#define DA_FILE_NUMBER_LIMIT				(1024*1024)
// Addition or deletion of entries in this list should be in sync with the
// enumaration download_content_t defined in download-agent-dl-mgr.h

void get_random_number(int *out_num)
{
	int temp = DA_INVALID_ID;
	unsigned int seed = (unsigned)time(0);

	temp = (int)(rand_r(&seed) % 100 + 1.0);
	*out_num = temp;
}

da_result_t get_extension_from_mime_type(char *mime_type, char **extension)
{
	da_result_t ret = DA_RESULT_OK;
	char *ext = DA_NULL;

	DA_LOG_FUNC_START(Default);
	if (DA_NULL == mime_type || DA_NULL == extension) {
		DA_LOG_ERR(Default,"received mime_type is null");
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}
	DA_LOG(Default,"input mime type = %s", mime_type);
	if (DA_RESULT_OK != (ret = da_mime_get_ext_name(mime_type, &ext))) {
		DA_LOG_ERR(Default,"can't find proper extension!");
		goto ERR;
	}
	*extension = ext;
	DA_LOG(Default,"found extension = %s", *extension);

ERR:
	return ret;
}

int read_data_from_file(char *file, char **out_buffer)
{
	FILE *fd;
	int file_size = -1;
	char *buffer = NULL;
	int buffer_len = 0;
	int read_len = 0;

	*out_buffer = NULL;

	if (!file)
		return 0;

	/* open file with "rb", because fread() handles the file as binary mode */
	fd = fopen(file, "rb");
	if (!fd) {
		DA_LOG_ERR(FileManager,"File open err! received file path = [%s]", file);
		return 0;
	}

	get_file_size(file, &file_size);
	if (file_size <= 0) {
		DA_LOG_ERR(FileManager,"file size is [%d]", file_size);
		fclose(fd);
		return 0;
	}

	/* A guide from www.securecoding.cert.org
	 *    : FIO17-C. Do not rely on an ending null character when using fread()
	 *
	 *  buffer is initialized with null through calloc(), so, it is always null-terminated even if fread() failed.
	 *  allocate memory one more byte to ensure null-terminated even if the file is not null-terminated.
	 */
	buffer_len = sizeof(char) * file_size;
	buffer = (char *)calloc(1, buffer_len + 1);
	if (buffer) {
		read_len = fread(buffer, sizeof(char), file_size, fd);
		if (read_len == file_size) {
			*out_buffer = buffer;
		} else {
			DA_LOG_ERR(FileManager,"File Read Not Complete read length = %d", read_len);
			free(buffer);
			buffer = NULL;
			buffer_len = 0;
		}
	} else {
		buffer_len = 0;
	}

	fclose(fd);

	return buffer_len;
}

da_result_t check_enough_storage(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	unsigned long cont_size = 0;
	da_storage_size_t avail_memory = {0, };
	da_storage_type_t type = DA_STORAGE_PHONE;

	DA_LOG_FUNC_START(Default);

	/* check enough storage here because of multiple download */
	ret = get_storage_type(&type);
	if (DA_RESULT_OK != ret)
		return ret;

	cont_size
		= GET_CONTENT_STORE_CURRENT_FILE_SIZE(GET_STAGE_CONTENT_STORE_INFO(stage));

	DA_LOG(Default,"cont_size [%d]", cont_size);
	if (cont_size) {
		ret = get_available_memory(DA_STORAGE_PHONE, &avail_memory);
		if (DA_RESULT_OK == ret && (avail_memory.b_available
		        < ((cont_size + SAVE_FILE_BUFFERING_SIZE_5MB)
		                / avail_memory.b_size))) /* 5MB buffering */
		{
			DA_LOG_ERR(Default,"There is no space for content in Phone memory");
			return DA_ERR_DISK_FULL;
		} else if (DA_RESULT_OK != ret) {
			return ret;
		}
		/* If default memeory type is mmc,
		 * it need to check phone memroy for temproary folder
		 * and mmc memory for actual install folder */
		if (type == DA_STORAGE_MMC) {
			DA_LOG(Default,"Storage type == DA_STORAGE_MMC\n");
			ret = get_available_memory(type, &avail_memory);
			if (DA_RESULT_OK == ret && (avail_memory.b_available
			        < ((cont_size + SAVE_FILE_BUFFERING_SIZE_50KB)
			                / avail_memory.b_size))) /* 50KB buffering */
			{
				DA_LOG_ERR(Default,"There is no space for content in MMC memory");
				return DA_ERR_DISK_FULL;
			} else if (DA_RESULT_OK != ret) {
				return ret;
			}
		}
	}

	return ret;
}

da_result_t get_available_memory(
        da_storage_type_t storage_type,
        da_storage_size_t *avail_memory)
{
	int fs_ret = 0;
	struct statfs filesys_info = {0, };

	DA_LOG_FUNC_START(Default);

	if (!avail_memory)
		return DA_ERR_INVALID_ARGUMENT;

	if (storage_type == DA_STORAGE_PHONE) {
		fs_ret = statfs(DA_DEFAULT_TMP_FILE_DIR_PATH, &filesys_info);
	} else if (storage_type == DA_STORAGE_MMC) {
		char *default_install_dir = NULL;
		default_install_dir = PI_get_default_install_dir();
		if (default_install_dir) {
			fs_ret = statfs(default_install_dir, &filesys_info);
		} else {
			return DA_ERR_FAIL_TO_ACCESS_STORAGE;
		}
	} else {
		DA_LOG_ERR(Default,"Invalid storage type");
		return DA_ERR_INVALID_ARGUMENT;
	}

	if (fs_ret != 0) {
		DA_LOG_ERR(Default,"Phone file path :statfs error - [%d]", errno);
		return DA_ERR_FAIL_TO_ACCESS_FILE;
	}

	avail_memory->b_available = filesys_info.f_bavail;
	avail_memory->b_size = filesys_info.f_bsize;

	DA_LOG(Default, "Memory type : %d", storage_type);
	DA_LOG(Default, "Available Memory(f_bavail) : %lu", filesys_info.f_bavail);
	DA_LOG(Default, "Available Memory(f_bsize) : %d", filesys_info.f_bsize);
	DA_LOG(Default, "Available Memory(kbytes) : %lu", (filesys_info.f_bavail/1024)*filesys_info.f_bsize);

	return DA_RESULT_OK;
}

da_bool_t is_valid_url(const char* url, da_result_t *err_code)
{
	da_result_t ret = DA_RESULT_OK;
	da_bool_t b_ret = DA_FALSE;

	int wanted_str_len = 0;
	char *wanted_str = NULL;
	char *wanted_str_start = NULL;
	char *wanted_str_end = NULL;

	if ((DA_NULL == url) || (1 > strlen(url))) {
		ret = DA_ERR_INVALID_URL;
		goto ERR;
	}

	wanted_str_start = (char*)url;
	wanted_str_end = strstr(url, "://");
	if (!wanted_str_end) {
		DA_LOG_ERR(Default,"No protocol on this url");
		ret = DA_ERR_INVALID_URL;
		goto ERR;
	}

	wanted_str_len = wanted_str_end - wanted_str_start;
	wanted_str = (char*)calloc(1, wanted_str_len + 1);
	if (!wanted_str) {
		DA_LOG_ERR(Default,"DA_ERR_FAIL_TO_MEMALLOC");
		ret = DA_ERR_FAIL_TO_MEMALLOC;
		goto ERR;
	}
	strncpy(wanted_str, wanted_str_start, wanted_str_len);

	b_ret = is_supporting_protocol(wanted_str);
	if (!b_ret) {
		ret = DA_ERR_UNSUPPORTED_PROTOCAL;
		goto ERR;
	}

ERR:
	if (wanted_str) {
		free(wanted_str);
		wanted_str = NULL;
	}

	if (err_code)
		*err_code = ret;

	return b_ret;
}

da_result_t move_file(const char *from_path, const char *to_path)
{
	da_result_t ret = DA_RESULT_OK;

	if (!from_path || !to_path)
		return DA_ERR_INVALID_ARGUMENT;

	if (rename(from_path, to_path) != 0) {
		DA_LOG_CRITICAL(FileManager,"rename failed : syserr[%d]",errno);
		if (errno == EXDEV) {
			DA_LOG_CRITICAL(FileManager,"File system is diffrent. Try to copy a file");
			ret = copy_file(from_path, to_path);
			if (ret == DA_RESULT_OK) {
				remove_file(from_path);
			} else {
				if (is_file_exist(to_path))
					remove_file(to_path);
				ret = DA_ERR_FAIL_TO_INSTALL_FILE;
			}
		} else {
			ret = DA_ERR_FAIL_TO_INSTALL_FILE;
		}
	}
	return ret;
}

void remove_file(const char *file_path)
{
	DA_LOG_FUNC_START(FileManager);

	if (file_path) {
		DA_LOG(FileManager,"remove file [%s]", file_path);
		if (unlink(file_path) < 0) {
			DA_LOG_ERR(FileManager,"file removing failed.");
		}
	}
}

char *print_dl_state(da_state state)
{
	switch (state) {
	case DA_STATE_WAITING:
		return "DA_STATE_WAITING";
	case DA_STATE_DOWNLOAD_STARTED:
		return "DA_STATE_DOWNLOAD_STARTED";
	case DA_STATE_DOWNLOADING:
		return "DA_STATE_DOWNLOADING";
	case DA_STATE_DOWNLOAD_COMPLETE:
		return "DA_STATE_DOWNLOAD_COMPLETE";
	case DA_STATE_FINISHED:
		return "DA_STATE_FINISHED";
	case DA_STATE_CANCELED:
		return "DA_STATE_CANCELED";
	case DA_STATE_CANCELED_ALL:
		return "DA_STATE_CANCELED_ALL";
	case DA_STATE_SUSPENDED:
		return "DA_STATE_SUSPENDED";
	case DA_STATE_SUSPENDED_ALL:
		return "DA_STATE_SUSPENDED_ALL";
	case DA_STATE_RESUMED:
		return "DA_STATE_RESUMED";
	case DA_STATE_FAILED:
		return "DA_STATE_FAILED";
	default:
		return "STATE ERROR";

	}
}

