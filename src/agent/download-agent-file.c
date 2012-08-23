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
 * @file		download-agent-file.c
 * @brief		functions for file operation
 * @author		Keunsoon Lee(keunsoon.lee@samsung.com)
 * @author		Jungki Kwak(jungki.kwak@samsung.com)
 ***/

#include <dirent.h>
#include <unistd.h>
#include <math.h>

#include "download-agent-client-mgr.h"
#include "download-agent-debug.h"
#include "download-agent-utils.h"
#include "download-agent-dl-mgr.h"
#include "download-agent-file.h"
#include "download-agent-installation.h"
#include "download-agent-mime-util.h"
#include "download-agent-http-mgr.h"

#define NO_NAME_TEMP_STR "No name"

static da_result_t  __set_file_size(stage_info *stage);
static da_result_t  __tmp_file_open(stage_info *stage);

static char *__derive_extension(stage_info *stage);
static da_result_t __divide_file_name_into_pure_name_N_extesion(
		const char *in_file_name,
		char **out_pure_file_name,
		char **out_extension);
static da_result_t __get_candidate_file_name(stage_info *stage,
		char **out_pure_file_name, char **out_extension);

static da_result_t __file_write_buf_make_buf(file_info *file_storage);
static da_result_t __file_write_buf_destroy_buf(file_info *file_storage);
static da_result_t __file_write_buf_flush_buf(stage_info *stage,
		file_info *file_storage);
static da_result_t __file_write_buf_copy_to_buf(file_info *file_storage,
		char *body, int body_len);
static da_result_t __file_write_buf_directly_write(stage_info *stage,
		file_info *file_storage, char *body, int body_len);


da_result_t create_temp_saved_dir(void)
{
	da_result_t ret = DA_RESULT_OK;
	char *client_dir_path = DA_NULL;

	DA_LOG_FUNC_START(FileManager);

	ret = get_client_download_path(&client_dir_path);
	if (DA_RESULT_OK != ret) {
		goto ERR;
	} else {
		if (DA_NULL == client_dir_path) {
			DA_LOG_ERR(FileManager, "client_dir_path is DA_NULL");
			ret = DA_ERR_INVALID_ARGUMENT;
			goto ERR;
		}
	}

	if (DA_FALSE == is_dir_exist(client_dir_path)) {
		ret = create_dir(client_dir_path);
	}

ERR:
	if (client_dir_path) {
		free(client_dir_path);
		client_dir_path = DA_NULL;
	}
	return ret;
}

da_result_t clean_files_from_dir(char* dir_path)
{
	da_result_t ret = DA_RESULT_OK;
	struct dirent *d = DA_NULL;
	DIR *dir;
	char file_path[DA_MAX_FULL_PATH_LEN] = { 0, };

	DA_LOG_FUNC_START(FileManager);

	if (dir_path == DA_NULL) {
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}

	if (is_dir_exist(dir_path)) {
		dir = opendir(dir_path);
		if (DA_NULL == dir) {
			DA_LOG_ERR(FileManager, "opendir() for DA_DEFAULT_TMP_FILE_DIR_PATH is failed.");
			ret = DA_ERR_FAIL_TO_ACCESS_FILE;
		} else {
			while (DA_NULL != (d = readdir(dir))) {
				DA_LOG(FileManager, "%s",d->d_name);
				if (0 == strncmp(d->d_name, ".", strlen("."))
						|| 0 == strncmp(d->d_name,
								"..",
								strlen(".."))) {
					continue;
				}

				memset(file_path, 0x00, DA_MAX_FULL_PATH_LEN);
				snprintf(file_path, DA_MAX_FULL_PATH_LEN,
						"%s/%s", dir_path, d->d_name);
				if (remove(file_path) < 0) {
					DA_LOG_ERR(FileManager, "fail to remove file");
				}
			}

			closedir(dir);
			if (remove(dir_path) < 0) {
				DA_LOG_ERR(FileManager, "fail to remove dir");
			}
		}
	}

ERR:
	return ret;
}

/* Priority to obtain MIME Type
 * 1. HTTP response header's <Content-Type> field
 * 2. from OMA descriptor file's <content-type> attribute (mandatory field)
 * 3. Otherwise, leave blank for MIME Type
 */
da_result_t get_mime_type(stage_info *stage, char **out_mime_type)
{
	char *mime_type = DA_NULL;

	if (!GET_STAGE_SOURCE_INFO(stage))
		return DA_ERR_INVALID_ARGUMENT;

	/* Priority 1 */
	if (GET_REQUEST_HTTP_HDR_CONT_TYPE(GET_STAGE_TRANSACTION_INFO(stage))) {
		mime_type = GET_REQUEST_HTTP_HDR_CONT_TYPE(GET_STAGE_TRANSACTION_INFO(stage));
		 DA_LOG(FileManager, "content type from HTTP response header [%s]", mime_type);
	}

	if (!mime_type) {
		DA_LOG(FileManager, "no content type derived");
		return DA_RESULT_OK;
	}

	/* FIXME really need memory allocation? */
	*out_mime_type = (char *) calloc(1, strlen(mime_type) + 1);
	if (*out_mime_type) {
		snprintf(*out_mime_type, strlen(mime_type) + 1, mime_type);
		DA_LOG_VERBOSE(FileManager, "out_mime_type str[%s] ptr[%p] len[%d]",
				*out_mime_type,*out_mime_type,strlen(*out_mime_type));
	} else {
		DA_LOG_ERR(FileManager, "fail to allocate memory");
		return DA_ERR_FAIL_TO_MEMALLOC;
	}

	DA_LOG(FileManager, "mime type = %s", *out_mime_type);
	return DA_RESULT_OK;
}

da_bool_t is_file_exist(const char *file_path)
{
	struct stat dir_state;
	int stat_ret;

	if (file_path == DA_NULL) {
		DA_LOG_ERR(FileManager, "file path is DA_NULL");
		return DA_FALSE;
	}

	stat_ret = stat(file_path, &dir_state);

	if (stat_ret == 0) {
		if (dir_state.st_mode & S_IFREG) {
			DA_LOG(FileManager, "Exist! %s is a regular file & its size = %lu", file_path, dir_state.st_size);
			return DA_TRUE;
		}

		return DA_FALSE;
	}
	return DA_FALSE;

}

da_bool_t is_dir_exist(char *file_path)
{
	struct stat dir_state;
	int stat_ret;

	if (file_path == DA_NULL) {
		DA_LOG_ERR(FileManager, "file path is DA_NULL");
		return DA_FALSE;
	}

	stat_ret = stat(file_path, &dir_state);

	if (stat_ret == 0) {
		if (dir_state.st_mode & S_IFDIR) {
			DA_LOG(FileManager, "Exist! %s is a directory.", file_path);
			return DA_TRUE;
		}

		return DA_FALSE;
	}
	return DA_FALSE;
}

void get_file_size(char *file_path, int *out_file_size)
{
	struct stat dir_state;
	int stat_ret;

	*out_file_size = -1;

	if (file_path == DA_NULL) {
		DA_LOG_ERR(FileManager, "file path is DA_NULL");
		return;
	}

	/* Please do not use ftell() to obtain file size, use stat instead.
	 *  This is a guide from www.securecoding.cert.org
	 *    : FIO19-C. Do not use fseek() and ftell() to compute the size of a file
	 */
	stat_ret = stat(file_path, &dir_state);
	if (stat_ret == 0) {
		if (dir_state.st_mode & S_IFREG) {
			DA_LOG(FileManager, "size = %lu", dir_state.st_size);
			*out_file_size = dir_state.st_size;
		}
	}
	return;
}

da_result_t __tmp_file_open(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;

	file_info *file_storage = DA_NULL;
	char *actual_file_path = DA_NULL;
	char *tmp_file_path = DA_NULL;
	void *fd = DA_NULL;

	DA_LOG_FUNC_START(FileManager);

	file_storage = GET_STAGE_CONTENT_STORE_INFO(stage);
	if (!file_storage)
		return DA_ERR_INVALID_ARGUMENT;

	actual_file_path = GET_CONTENT_STORE_ACTUAL_FILE_NAME(file_storage);
	DA_LOG(FileManager, "actual_file_path = %s", actual_file_path);
	if (!actual_file_path)
		return DA_ERR_INVALID_ARGUMENT;

	tmp_file_path = (char *) calloc(1, (strlen(actual_file_path) + 1));
	if (!tmp_file_path) {
		ret = DA_ERR_FAIL_TO_MEMALLOC;
		goto ERR;
	}
	snprintf(tmp_file_path, strlen(actual_file_path) + 1, "%s", actual_file_path);

	GET_CONTENT_STORE_TMP_FILE_NAME(file_storage) = tmp_file_path;
	DA_LOG(FileManager, "GET_CONTENT_STORE_TMP_FILE_NAME = %s ",GET_CONTENT_STORE_TMP_FILE_NAME(file_storage));

	fd = fopen(tmp_file_path, "a"); // for resume
	if (fd == DA_NULL) {
		DA_LOG_ERR(FileManager, "File open failed");
		ret = DA_ERR_FAIL_TO_ACCESS_FILE;
		goto ERR;
	}
	GET_CONTENT_STORE_FILE_HANDLE(file_storage) = fd;

	DA_LOG(FileManager, "file path for tmp saving = %s", GET_CONTENT_STORE_TMP_FILE_NAME(file_storage));

ERR:
	if (DA_RESULT_OK != ret) {
		if (tmp_file_path) {
			free(tmp_file_path);
		}
		if (file_storage != DA_NULL) {
			GET_CONTENT_STORE_TMP_FILE_NAME(file_storage) = DA_NULL;
			GET_CONTENT_STORE_FILE_HANDLE(file_storage) = DA_NULL;
		}
	}
	return ret;
}

da_result_t __set_file_size(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	req_dl_info *stage_req_info = DA_NULL;
	file_info *file_storage = DA_NULL;

	DA_LOG_FUNC_START(FileManager);

	if (!stage) {
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}

	stage_req_info = GET_STAGE_TRANSACTION_INFO(stage);

	file_storage = GET_STAGE_CONTENT_STORE_INFO(stage);
	if (!file_storage)
		goto ERR;

	if (GET_REQUEST_HTTP_HDR_CONT_LEN(stage_req_info) != DA_NULL) {
		GET_CONTENT_STORE_FILE_SIZE(file_storage)
				= GET_REQUEST_HTTP_HDR_CONT_LEN(stage_req_info);
	} else {
		GET_CONTENT_STORE_FILE_SIZE(file_storage) = 0;
	}
	DA_LOG(FileManager, "file size = %d", GET_CONTENT_STORE_FILE_SIZE(file_storage));
ERR:
	return ret;

}

/* Priority to derive extension
 * 1. according to MIME-Type
 * 2. if MIME-Type is ambiguous or blank,
 *    2-1. derived from <Content-Disposition> field's "filename" attribute
 *    2-2. derived from url
 * 3. if url does not have extension, leave blank for extension
 */
char *__derive_extension(stage_info *stage)
{
	if (!stage)
		return DA_NULL;

	source_info_t *source_info = GET_STAGE_SOURCE_INFO(stage);
	req_dl_info *request_info = GET_STAGE_TRANSACTION_INFO(stage);
	file_info *file_info_data = GET_STAGE_CONTENT_STORE_INFO(stage);
	char *extension = DA_NULL;
	char *url = DA_NULL;

	/* Priority 1 */
	char *mime_type = DA_NULL;
	mime_type = GET_CONTENT_STORE_CONTENT_TYPE(file_info_data);
	if (mime_type && !is_ambiguous_MIME_Type(mime_type)) {
		char *extension = DA_NULL;
		da_result_t ret = get_extension_from_mime_type(mime_type, &extension);
		if (ret == DA_RESULT_OK && extension)
			return extension;
	}

	/* Priority 2-1 */
	http_msg_response_t *http_msg_response = DA_NULL;
	http_msg_response = request_info->http_info.http_msg_response;
	if (http_msg_response) {
		char *file_name = DA_NULL;
		da_bool_t b_ret = http_msg_response_get_content_disposition(http_msg_response,
				DA_NULL, &file_name);
		if (b_ret && file_name) {
			char *extension = DA_NULL;
			DA_LOG(FileManager, "Name from Content-Disposition :[%s]", file_name);
			__divide_file_name_into_pure_name_N_extesion(file_name, DA_NULL, &extension);
				if (file_name) {
				free(file_name);
				file_name = DA_NULL;
			}
			if (extension)
				return extension;
		}
	}
	/* Priority 2-2 */
	/* If there is location url from response header in case of redirection,
	 * it try to parse the extention name from the location url */
	if (GET_REQUEST_HTTP_REQ_LOCATION(request_info))
		url = GET_REQUEST_HTTP_REQ_LOCATION(request_info);
	else
		url = GET_SOURCE_BASIC_URL(source_info);
	if (url) {
		DA_LOG(FileManager, "url:[%s]", url);
		da_bool_t b_ret = da_get_extension_name_from_url(url, &extension);
		if (b_ret && extension)
			return extension;
	}

	return DA_NULL;
}

/** Priority for deciding file name
 * 1. what client wants, which is conveyed by DA_FEATURE_FILE_NAME
 * 2. 'filename' option on HTTP response header's Content-Disposition field
 * 3. requesting URL
 * 4. Otherwise, define it as "No name"
 */
da_result_t __get_candidate_file_name(stage_info *stage, char **out_pure_file_name, char **out_extension)
{
	da_result_t ret = DA_RESULT_OK;
	source_info_t *source_info = DA_NULL;
	char *pure_file_name = DA_NULL;
	char *extension = DA_NULL;

	DA_LOG_FUNC_START(FileManager);

	if (!stage || !out_pure_file_name)
		return DA_ERR_INVALID_ARGUMENT;

	source_info = GET_STAGE_SOURCE_INFO(stage);
	if (!source_info)
		return DA_ERR_INVALID_ARGUMENT;

	/* Priority 1 */
	if (!pure_file_name && GET_DL_USER_FILE_NAME(GET_STAGE_DL_ID(stage))) {
		__divide_file_name_into_pure_name_N_extesion(
			GET_DL_USER_FILE_NAME(GET_STAGE_DL_ID(stage)),
			&pure_file_name, &extension);
	}

	/* Priority 2 */
	if (!pure_file_name) {
		req_dl_info *request_info = GET_STAGE_TRANSACTION_INFO(stage);
		http_msg_response_t *http_msg_response = DA_NULL;
		http_msg_response = request_info->http_info.http_msg_response;
		if (http_msg_response) {
			char *file_name = DA_NULL;
			da_bool_t b_ret = http_msg_response_get_content_disposition(http_msg_response,
					DA_NULL, &file_name);
			if (b_ret && file_name) {
				DA_LOG(FileManager, "Name from Content-Disposition :[%s]", file_name);
				__divide_file_name_into_pure_name_N_extesion(file_name, &pure_file_name, DA_NULL);
				if (file_name) {
					free(file_name);
					file_name = DA_NULL;
				}
			}
		}
	}

	/* Priority 3 */
	if (!pure_file_name) {
		char *url = DA_NULL;
		req_dl_info *request_info = GET_STAGE_TRANSACTION_INFO(stage);
		/* If there is location url from response header in case of redirection,
		 * it try to parse the file name from the location url */
		if (GET_REQUEST_HTTP_REQ_LOCATION(request_info))
			url = GET_REQUEST_HTTP_REQ_LOCATION(request_info);
		else
			url = GET_SOURCE_BASIC_URL(source_info);
		if (url) {
			DA_LOG(FileManager, "url: [%s]", url);
			da_get_file_name_from_url(url, &pure_file_name);
		}
	}

	/* Priority 4 */
	if (!pure_file_name) {
		pure_file_name = strdup(NO_NAME_TEMP_STR);
		if (!pure_file_name) {
			ret = DA_ERR_FAIL_TO_MEMALLOC;
			goto ERR;
		}
	}

	*out_pure_file_name = pure_file_name;
	pure_file_name = DA_NULL;
	DA_LOG(FileManager, "candidate file name [%s]", *out_pure_file_name);

	if (out_extension) {
		if (extension) {
			*out_extension = extension;
			extension = DA_NULL;
		} else {
			*out_extension = __derive_extension(stage);
			DA_LOG(FileManager, "candidate extension [%s]", *out_extension);
		}
	}

	if (extension)
		free(extension);

	return DA_RESULT_OK;

ERR:
	if (pure_file_name)
		free(pure_file_name);

	if (extension)
		free(extension);

	return ret;
}


da_result_t decide_final_file_path(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	char *default_dir = DA_NULL;
	char *extension = DA_NULL;
	char *tmp_file_path = DA_NULL;
	char *file_name_without_extension = DA_NULL;
	file_info *file_info_data = DA_NULL;

	DA_LOG_FUNC_START(FileManager);

	file_info_data = GET_STAGE_CONTENT_STORE_INFO(stage);
	if (!file_info_data)
		return DA_ERR_INVALID_ARGUMENT;

	ret = __get_candidate_file_name(stage, &file_name_without_extension, &extension);
	if (ret != DA_RESULT_OK)
		goto ERR;

	ret = get_client_download_path(&default_dir);
	if (DA_RESULT_OK != ret || DA_NULL == default_dir) {
		goto ERR;
	}

	tmp_file_path = get_full_path_avoided_duplication(default_dir, file_name_without_extension, extension);
	if (tmp_file_path) {
		GET_CONTENT_STORE_ACTUAL_FILE_NAME(GET_STAGE_CONTENT_STORE_INFO(stage))
				= tmp_file_path;
		tmp_file_path = DA_NULL;
	} else {
		ret = DA_ERR_FAIL_TO_ACCESS_FILE;
		goto ERR;
	}

	if (file_name_without_extension && !GET_CONTENT_STORE_PURE_FILE_NAME(file_info_data)) {
		GET_CONTENT_STORE_PURE_FILE_NAME(file_info_data) = file_name_without_extension;
		file_name_without_extension = DA_NULL;
	}

	if (extension && !GET_CONTENT_STORE_EXTENSION(file_info_data)) {
		GET_CONTENT_STORE_EXTENSION(file_info_data) = extension;
		extension = DA_NULL;
	}

ERR:
	DA_LOG(FileManager, "decided file path = %s", GET_CONTENT_STORE_ACTUAL_FILE_NAME(file_info_data));
	if (default_dir) {
		free(default_dir);
		default_dir = DA_NULL;
	}

	if (file_name_without_extension) {
		free(file_name_without_extension);
		file_name_without_extension = DA_NULL;
	}

	if (extension) {
		free(extension);
		extension = DA_NULL;
	}

	return ret;
}

char *get_full_path_avoided_duplication(char *in_dir, char *in_candidate_file_name, char * in_extension)
{
	char *dir = in_dir;
	char *file_name = in_candidate_file_name;
	char *extension = in_extension;
	char *final_path = DA_NULL;

	int final_path_len = 0;
	int extension_len = 0;

	int suffix_count = 0;	/* means suffix on file name. up to "_99" */
	const int max_suffix_count = 99;
	int suffix_len = (int)log10(max_suffix_count+1) + 1;	/* 1 means "_" */

	if (!in_dir || !in_candidate_file_name)
		return DA_NULL;

//	DA_LOG_FUNC_START(FileManager);
	DA_LOG(FileManager, "in_candidate_file_name=[%s], in_extension=[%s]", in_candidate_file_name, in_extension);

	if (extension)
		extension_len = strlen(extension);

	/* first 1 for "/", second 1 for ".", last 1 for DA_NULL */
	final_path_len = strlen(dir) + 1 + strlen(file_name) + 1
			+ suffix_len + extension_len + 1;

	final_path = (char*)calloc(1, final_path_len);
	if (!final_path) {
		DA_LOG_ERR(FileManager, "DA_ERR_FAIL_TO_MEMALLOC");
		return DA_NULL;
	}

	do {
		/* e.g) /tmp/abc.jpg
		 * if there is no extension name, just make a file name without extension */
		if (0 == extension_len) {
			if (suffix_count == 0) {
				snprintf(final_path, final_path_len,
						"%s/%s", dir, file_name);
			} else {
				snprintf(final_path, final_path_len,
						"%s/%s_%d", dir, file_name, suffix_count);
			}
		} else {
			if (suffix_count == 0) {
				snprintf(final_path, final_path_len,
						"%s/%s.%s", dir, file_name, extension);
			} else {
				snprintf(final_path, final_path_len,
						"%s/%s_%d.%s",
						dir, file_name, suffix_count, extension);
			}
		}

		if (is_file_exist(final_path)) {
			suffix_count++;
			if (suffix_count > max_suffix_count) {
				free(final_path);
				final_path = DA_NULL;
				break;
			} else {
				memset(final_path, 0x00, final_path_len);
				continue;
			}
		}

		break;
	} while (1);

	DA_LOG(FileManager, "decided path = [%s]", final_path);
	return final_path;
}

da_result_t __divide_file_name_into_pure_name_N_extesion(const char *in_file_name, char **out_pure_file_name, char **out_extension)
{
	char *file_name = DA_NULL;
	char *tmp_ptr = DA_NULL;
	char temp_file[DA_MAX_FILE_PATH_LEN] = {0,};
	char tmp_ext[DA_MAX_STR_LEN] = {0,};
	int len = 0;
	da_result_t ret = DA_RESULT_OK;

	DA_LOG_FUNC_START(FileManager);

	if (!in_file_name)
		return DA_ERR_INVALID_ARGUMENT;

	file_name = (char *)in_file_name;
	tmp_ptr = strrchr(file_name, '.');
	if (tmp_ptr)
		tmp_ptr++;
	if (tmp_ptr && out_extension) {
		strncpy((char*) tmp_ext, tmp_ptr, sizeof(tmp_ext) - 1);
		*out_extension = strdup((const char*) tmp_ext);
		DA_LOG(FileManager, "extension [%s]", *out_extension);
	}

	if (!out_pure_file_name)
		return ret;

	if (tmp_ptr)
		len = tmp_ptr - file_name - 1;
	else
		len = strlen(file_name);

	if (len >= DA_MAX_FILE_PATH_LEN) {
		strncpy((char*) temp_file, file_name,
				DA_MAX_FILE_PATH_LEN - 1);
	} else {
		strncpy((char*) temp_file, file_name, len);
	}

	delete_prohibited_char((char*) temp_file,
			strlen((char*) temp_file));
	if (strlen(temp_file) < 1) {
		*out_pure_file_name = strdup(NO_NAME_TEMP_STR);
	} else {
		*out_pure_file_name = strdup(
				(const char*) temp_file);
	}

	DA_LOG(FileManager, "pure file name [%s]", *out_pure_file_name);
	return ret;
}

da_result_t __file_write_buf_make_buf(file_info *file_storage)
{
	da_result_t ret = DA_RESULT_OK;
	char *buffer = DA_NULL;

	DA_LOG_FUNC_START(FileManager);

	buffer = (char*) calloc(DOWNLOAD_NOTIFY_LIMIT, 1);
	if (DA_NULL == buffer) {
		DA_LOG_ERR(FileManager, "Calloc failure ");
		ret = DA_ERR_FAIL_TO_MEMALLOC;
	} else {
		GET_CONTENT_STORE_FILE_BUFF_LEN(file_storage) = 0;
		GET_CONTENT_STORE_FILE_BUFFER(file_storage) = buffer;
	}

	return ret;
}

da_result_t __file_write_buf_destroy_buf(file_info *file_storage)
{
	da_result_t ret = DA_RESULT_OK;

	DA_LOG_FUNC_START(FileManager);

	if (GET_CONTENT_STORE_FILE_BUFFER(file_storage))
		free(GET_CONTENT_STORE_FILE_BUFFER(file_storage));

	GET_CONTENT_STORE_FILE_BUFFER(file_storage) = DA_NULL;
	GET_CONTENT_STORE_FILE_BUFF_LEN(file_storage) = 0;

	return ret;
}

da_result_t __file_write_buf_flush_buf(stage_info *stage, file_info *file_storage)
{
	da_result_t ret = DA_RESULT_OK;
	char *buffer = DA_NULL;
	int buffer_size = 0;
	int write_success_len = 0;
	void *fd = DA_NULL;

	//	DA_LOG_FUNC_START(FileManager);

	buffer = GET_CONTENT_STORE_FILE_BUFFER(file_storage);
	buffer_size = GET_CONTENT_STORE_FILE_BUFF_LEN(file_storage);

	if (buffer_size == 0) {
		DA_LOG_ERR(FileManager, "no data on buffer..");
		return ret;
	}

	fd = GET_CONTENT_STORE_FILE_HANDLE(file_storage);
	if (DA_NULL == fd) {
		DA_LOG_ERR(FileManager, "There is no file handle.");

		ret = DA_ERR_FAIL_TO_ACCESS_FILE;
		goto ERR;
	}

	write_success_len = fwrite(buffer, sizeof(char), buffer_size,
			(FILE *) fd);
	fflush((FILE *) fd);
	if (write_success_len != buffer_size) {
		DA_LOG_ERR(FileManager, "write  fails ");
		ret = DA_ERR_FAIL_TO_ACCESS_FILE;
		goto ERR;
	}
	GET_CONTENT_STORE_CURRENT_FILE_SIZE(GET_STAGE_CONTENT_STORE_INFO(stage))
			+= write_success_len;
	DA_LOG(FileManager, "write %d bytes", write_success_len);

	IS_CONTENT_STORE_FILE_BYTES_WRITTEN_TO_FILE(file_storage) = DA_TRUE;
	GET_CONTENT_STORE_FILE_BUFF_LEN(file_storage) = 0;

ERR:
	return ret;
}

da_result_t __file_write_buf_copy_to_buf(file_info *file_storage, char *body,
		int body_len)
{
	da_result_t ret = DA_RESULT_OK;
	char *buffer = DA_NULL;
	int buffer_size = 0;

	//	DA_LOG_FUNC_START(FileManager);

	buffer = GET_CONTENT_STORE_FILE_BUFFER(file_storage);
	buffer_size = GET_CONTENT_STORE_FILE_BUFF_LEN(file_storage);

	memcpy(buffer + buffer_size, body, body_len);
	GET_CONTENT_STORE_FILE_BUFF_LEN(file_storage) += body_len;

	return ret;
}

da_result_t __file_write_buf_directly_write(stage_info *stage,
		file_info *file_storage, char *body, int body_len)
{
	da_result_t ret = DA_RESULT_OK;
	int write_success_len = 0;
	void *fd = DA_NULL;

	//	DA_LOG_FUNC_START(FileManager);

	fd = GET_CONTENT_STORE_FILE_HANDLE(file_storage);
	if (DA_NULL == fd) {
		DA_LOG_ERR(FileManager, "There is no file handle.");

		ret = DA_ERR_FAIL_TO_ACCESS_FILE;
		goto ERR;
	}

	write_success_len = fwrite(body, sizeof(char), body_len,
			(FILE *) fd);
	fflush((FILE *) fd);
	if (write_success_len != body_len) {
		DA_LOG_ERR(FileManager, "write  fails ");
		ret = DA_ERR_FAIL_TO_ACCESS_FILE;
		goto ERR;
	}
	GET_CONTENT_STORE_CURRENT_FILE_SIZE(GET_STAGE_CONTENT_STORE_INFO(stage))
			+= write_success_len;
	DA_LOG(FileManager, "write %d bytes", write_success_len);
	IS_CONTENT_STORE_FILE_BYTES_WRITTEN_TO_FILE(file_storage) = DA_TRUE;

ERR:
	return ret;
}

da_result_t file_write_ongoing(stage_info *stage, char *body, int body_len)
{
	da_result_t ret = DA_RESULT_OK;
	file_info *file_storage = DA_NULL;
	int buffer_size = 0;
	char *buffer = DA_NULL;

	//	DA_LOG_FUNC_START(FileManager);

	if (DA_TRUE == is_this_client_manual_download_type()) {
		return ret;
	}
	file_storage = GET_STAGE_CONTENT_STORE_INFO(stage);
	if (!file_storage) {
		DA_LOG_ERR(FileManager, "file_info is empty.");
		ret = DA_ERR_FAIL_TO_ACCESS_FILE;
		goto ERR;
	}

	buffer = GET_CONTENT_STORE_FILE_BUFFER(file_storage);
	buffer_size = GET_CONTENT_STORE_FILE_BUFF_LEN(file_storage);
	IS_CONTENT_STORE_FILE_BYTES_WRITTEN_TO_FILE(file_storage) = DA_FALSE;

	if (DA_NULL == buffer) {
		if (body_len < DOWNLOAD_NOTIFY_LIMIT) {
			ret = __file_write_buf_make_buf(file_storage);
			if (ret != DA_RESULT_OK)
				goto ERR;

			__file_write_buf_copy_to_buf(file_storage, body, body_len);
		} else {
			ret = __file_write_buf_directly_write(stage,
					file_storage, body, body_len);
			if (ret != DA_RESULT_OK)
				goto ERR;
		}
	} else {
		if (DOWNLOAD_NOTIFY_LIMIT <= body_len) {
			ret = __file_write_buf_flush_buf(stage, file_storage);
			if (ret != DA_RESULT_OK)
				goto ERR;

			ret = __file_write_buf_directly_write(stage,
					file_storage, body, body_len);
			if (ret != DA_RESULT_OK)
				goto ERR;

		} else if ((DOWNLOAD_NOTIFY_LIMIT - buffer_size) <= body_len) {
			ret = __file_write_buf_flush_buf(stage, file_storage);
			if (ret != DA_RESULT_OK)
				goto ERR;

			__file_write_buf_copy_to_buf(file_storage, body, body_len);
		} else {
			__file_write_buf_copy_to_buf(file_storage, body, body_len);
		}
	}

ERR:
	if (ret != DA_RESULT_OK) {
		if (file_storage) {
			GET_CONTENT_STORE_FILE_BUFF_LEN(file_storage) = 0;
			if (GET_CONTENT_STORE_FILE_BUFFER(file_storage)) {
				free(
						GET_CONTENT_STORE_FILE_BUFFER(file_storage));
				GET_CONTENT_STORE_FILE_BUFFER(file_storage)
						= DA_NULL;
			}
		}
	}
	return ret;
}

da_result_t file_write_complete(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	file_info*file_storage = DA_NULL;
	char *buffer = DA_NULL;
	unsigned int buffer_size = 0;
	void *fd = DA_NULL;

	DA_LOG_FUNC_START(FileManager);

	if (DA_TRUE == is_this_client_manual_download_type()) {
		return ret;
	}
	file_storage = GET_STAGE_CONTENT_STORE_INFO(stage);
	if (!file_storage) {
		DA_LOG_ERR(FileManager, "file_info is DA_NULL.");
		ret = DA_ERR_FAIL_TO_ACCESS_FILE;
		goto ERR;
	}

	buffer = GET_CONTENT_STORE_FILE_BUFFER(file_storage);
	buffer_size = GET_CONTENT_STORE_FILE_BUFF_LEN(file_storage);

	if (DA_NULL == buffer) {
		DA_LOG_ERR(FileManager, "file buffer is DA_NULL");
	} else {
		if (buffer_size != 0) {
			ret = __file_write_buf_flush_buf(stage, file_storage);
			if (ret != DA_RESULT_OK)
				goto ERR;
		}
		__file_write_buf_destroy_buf(file_storage);
	}
	fd = GET_CONTENT_STORE_FILE_HANDLE(file_storage);

	if (fd) {
		// call sync
		fsync(fileno(fd));
		fclose(fd);
		fd = DA_NULL;
	}
	GET_CONTENT_STORE_FILE_HANDLE(file_storage) = DA_NULL;
ERR:
	return ret;
}

da_result_t start_file_writing(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	file_info *file_info_data = DA_NULL;

	DA_LOG_FUNC_START(FileManager);

	if (DA_TRUE == is_this_client_manual_download_type()) {
		return ret;
	}
	file_info_data = GET_STAGE_CONTENT_STORE_INFO(stage);
	ret = get_mime_type(stage,
			&GET_CONTENT_STORE_CONTENT_TYPE(file_info_data));
	if (ret != DA_RESULT_OK)
		goto ERR;

	ret = decide_final_file_path(stage);
	if (ret != DA_RESULT_OK)
		goto ERR;

	ret = __set_file_size(stage);
	if (DA_RESULT_OK != ret)
		goto ERR;

	GET_CONTENT_STORE_CURRENT_FILE_SIZE(GET_STAGE_CONTENT_STORE_INFO(stage))
			= 0;

	ret = __tmp_file_open(stage);

ERR:
	return ret;
}

da_result_t start_file_writing_append(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;

	DA_LOG_FUNC_START(FileManager);
	if (DA_TRUE == is_this_client_manual_download_type()) {
		return ret;
	}

	ret = __tmp_file_open(stage);

	return ret;
}

da_result_t discard_download(stage_info *stage)
{
	da_result_t ret = DA_RESULT_OK;
	file_info *file_storage = DA_NULL;
	char *temp_file_path = DA_NULL;
	FILE *f_handle = DA_NULL;

	DA_LOG_FUNC_START(FileManager);

	file_storage = GET_STAGE_CONTENT_STORE_INFO(stage);

	f_handle = GET_CONTENT_STORE_FILE_HANDLE(file_storage);
	if (f_handle) {
		fclose(f_handle);
		GET_CONTENT_STORE_FILE_HANDLE(file_storage) = DA_NULL;
	}
	temp_file_path = GET_CONTENT_STORE_ACTUAL_FILE_NAME(file_storage);
	if (temp_file_path) {
		remove_file((const char*) temp_file_path);
		free(temp_file_path);
		GET_CONTENT_STORE_ACTUAL_FILE_NAME(file_storage) = DA_NULL;
	}

	return ret;
}

void clean_paused_file(stage_info *stage)
{
	file_info *file_info_data = DA_NULL;
	char *paused_file_path = DA_NULL;
	FILE *fd = DA_NULL;

	DA_LOG_FUNC_START(FileManager);

	file_info_data = GET_STAGE_CONTENT_STORE_INFO(stage);

	fd = GET_CONTENT_STORE_FILE_HANDLE(file_info_data);
	if (fd) {
		fclose(fd);
		GET_CONTENT_STORE_FILE_HANDLE(file_info_data) = DA_NULL;
	}

	paused_file_path = GET_CONTENT_STORE_ACTUAL_FILE_NAME(file_info_data);
	remove_file((const char*) paused_file_path);

	return;
}

da_result_t replace_content_file_in_stage(stage_info *stage,
		const char *dest_dd_file_path)
{
	da_result_t ret = DA_RESULT_OK;
	char *dd_file_path = DA_NULL;
	int len;

	DA_LOG_FUNC_START(FileManager);

	if (!dest_dd_file_path
			&& (DA_FALSE == is_file_exist(dest_dd_file_path))) {
		ret = DA_ERR_INVALID_ARGUMENT;
		goto ERR;
	}

	dd_file_path
			=GET_CONTENT_STORE_ACTUAL_FILE_NAME(GET_STAGE_CONTENT_STORE_INFO(stage));

	if (DA_NULL != dd_file_path) {
		remove_file((const char*) dd_file_path);
		free(dd_file_path);
	}
	len = strlen(dest_dd_file_path);
	dd_file_path = calloc(1, len + 1);
	if (!dd_file_path) {
		ret = DA_ERR_FAIL_TO_MEMALLOC;
		goto ERR;
	}
	strncpy(dd_file_path, dest_dd_file_path, len);
	GET_CONTENT_STORE_ACTUAL_FILE_NAME(GET_STAGE_CONTENT_STORE_INFO(stage))
			= dd_file_path;

ERR:
	return ret;

}

da_result_t copy_file(const char *src, const char *dest)
{
	FILE *fs = DA_NULL;
	FILE *fd = DA_NULL;
	int freadnum = 0;
	int fwritenum = 0;
	char buff[4096] = { 0, };

	DA_LOG_FUNC_START(FileManager);

	/* open files to copy */
	fs = fopen(src, "rb");
	if (!fs) {
		DA_LOG_ERR(FileManager, "Fail to open src file");
		return DA_ERR_FAIL_TO_ACCESS_FILE;
	}

	fd = fopen(dest, "wb");
	if (!fd) {
		DA_LOG_ERR(FileManager, "Fail to open dest file");

		fclose(fs);
		return DA_ERR_FAIL_TO_ACCESS_FILE;
	}

	/* actual copy */
	while (!feof(fs)) {
		memset(buff, 0x00, 4096);
		freadnum = fread(buff, sizeof(char), sizeof(buff), fs);
		if (freadnum > 0) {
			fwritenum = fwrite(buff, sizeof(char), freadnum, fd);
			if (fwritenum <= 0) {
				DA_LOG(FileManager, "written = %d",fwritenum);
				break;
			}
		} else {
			DA_LOG(FileManager, "read = %d",freadnum);
			break;
		}
	}

	fflush(fd);
	fsync(fileno(fd));
	fclose(fd);
	fclose(fs);

	return DA_RESULT_OK;
}

da_result_t create_dir(const char *install_dir)
{
	da_result_t ret = DA_RESULT_OK;
		/* read/write/search permissions for owner and group,
		 * and with read/search permissions for others. */
	if (mkdir(install_dir, S_IRWXU | S_IRWXG | S_IRWXO)) {
		DA_LOG_ERR(FileManager, "Fail to creaate directory [%s]", install_dir);
		ret = DA_ERR_FAIL_TO_ACCESS_STORAGE;
	} else {
		DA_LOG(FileManager, "[%s] is created!", install_dir);
	}
	return ret;
}

