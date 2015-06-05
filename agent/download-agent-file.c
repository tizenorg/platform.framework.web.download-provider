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
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <sys/vfs.h>
#include <math.h>
#include <errno.h>

#include "download-agent-debug.h"
#include "download-agent-file.h"
#include "download-agent-mime-util.h"
/* FIXME Later */
#include "download-agent-http-msg-handler.h"
#include "download-agent-plugin-drm.h"
#include "download-agent-plugin-conf.h"
#include <storage.h>


#define NO_NAME_TEMP_STR "No name"
#define MAX_SUFFIX_COUNT	1000000000

da_ret_t __saved_file_open(file_info_t *file_info)
{
	da_ret_t ret = DA_RESULT_OK;
	char *actual_file_path = DA_NULL;
	void *fd = DA_NULL;

	DA_LOGV("");

	actual_file_path = file_info->file_path;
	if (!actual_file_path)
		return DA_ERR_INVALID_ARGUMENT;

	fd = fopen(actual_file_path, "a+"); // for resume
	if (fd == DA_NULL) {
		DA_LOGE("File open failed");
		if (errno == ENOSPC)
			ret = DA_ERR_DISK_FULL;
		else
			ret = DA_ERR_FAIL_TO_ACCESS_FILE;
		goto ERR;
	}

	file_info->file_handle = fd;
	//DA_SECURE_LOGD("file path for saving[%s]", file_info->file_path);

ERR:
	if (DA_RESULT_OK != ret) {
		file_info->file_handle = DA_NULL;
	}
	return ret;
}

da_ret_t __divide_file_name_into_pure_name_N_extesion(const char *in_file_name, char **out_pure_file_name, char **out_extension)
{
	char *file_name = DA_NULL;
	char *tmp_ptr = DA_NULL;
	char temp_file[DA_MAX_FILE_PATH_LEN] = {0,};
	char tmp_ext[DA_MAX_STR_LEN] = {0,};
	int len = 0;
	da_ret_t ret = DA_RESULT_OK;

	DA_LOGV("");

	if (!in_file_name)
		return DA_ERR_INVALID_ARGUMENT;

	file_name = (char *)in_file_name;
	tmp_ptr = strrchr(file_name, '.');
	if (tmp_ptr)
		tmp_ptr++;
	if (tmp_ptr && out_extension) {
		strncpy((char*) tmp_ext, tmp_ptr, sizeof(tmp_ext) - 1);
		*out_extension = strdup((const char*) tmp_ext);
		DA_SECURE_LOGD("extension [%s]", *out_extension);
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

	DA_LOGV( "pure file name [%s]", *out_pure_file_name);
	return ret;
}

da_ret_t __file_write_buf_make_buf(file_info_t *file_info)
{
	da_ret_t ret = DA_RESULT_OK;
	char *buffer = DA_NULL;

	DA_LOGV("");

	buffer = (char*) calloc(1, DA_FILE_BUF_SIZE);
	if (DA_NULL == buffer) {
		DA_LOGE("Calloc failure ");
		ret = DA_ERR_FAIL_TO_MEMALLOC;
	} else {
		file_info->buffer_len = 0;
		file_info->buffer = buffer;
	}

	return ret;
}

da_ret_t __file_write_buf_destroy_buf(file_info_t *file_info)
{
	da_ret_t ret = DA_RESULT_OK;

	DA_LOGV("");
	NULL_CHECK_RET(file_info);

	free(file_info->buffer);
	file_info->buffer = DA_NULL;
	file_info->buffer_len = 0;

	return ret;
}

da_ret_t __file_write_buf_flush_buf(file_info_t *file_info)
{
	da_ret_t ret = DA_RESULT_OK;
	char *buffer = DA_NULL;
	int buffer_size = 0;
	int write_success_len = 0;
	void *fd = DA_NULL;

	//	DA_LOGV("");

	buffer = file_info->buffer;
	buffer_size = file_info->buffer_len;

	if (buffer_size == 0) {
		DA_LOGE("no data on buffer..");
		return ret;
	}

	fd = file_info->file_handle;
	if (DA_NULL == fd) {
		DA_LOGE("There is no file handle.");

		ret = DA_ERR_FAIL_TO_ACCESS_FILE;
		goto ERR;
	}

	write_success_len = fwrite(buffer, sizeof(char), buffer_size,
			(FILE *) fd);
	/* FIXME : This can be necessary later due to progressive download.
	 * The solution for reducing fflush is needed */
	//fflush((FILE *) fd);
	if (write_success_len != buffer_size) {
		DA_LOGE("write  fails ");
		if (errno == ENOSPC)
			ret = DA_ERR_DISK_FULL;
		else
			ret = DA_ERR_FAIL_TO_ACCESS_FILE;
		goto ERR;
	}
	file_info->bytes_written_to_file += write_success_len;
	file_info->is_updated = DA_TRUE;
	file_info->buffer_len = 0;
ERR:
	return ret;
}

da_ret_t __file_write_buf_copy_to_buf(file_info_t *file_info, char *body,
		int body_len)
{
	da_ret_t ret = DA_RESULT_OK;
	char *buffer = DA_NULL;
	int buffer_size = 0;

	DA_LOGV("");

	NULL_CHECK_RET(file_info->buffer);
	buffer = file_info->buffer;
	buffer_size = file_info->buffer_len;

	memcpy(buffer + buffer_size, body, body_len);
	file_info->buffer_len += body_len;

	return ret;
}

da_ret_t __file_write_buf_directly_write(file_info_t *file_info,
		char *body, int body_len)
{
	da_ret_t ret = DA_RESULT_OK;
	size_t write_success_len = 0;
	void *fd = DA_NULL;

	//	DA_LOGV("");

	fd = file_info->file_handle;
	if (DA_NULL == fd) {
		DA_LOGE("There is no file handle.");
		ret = DA_ERR_FAIL_TO_ACCESS_FILE;
		goto ERR;
	}

	write_success_len = fwrite(body, sizeof(char), (size_t)body_len,
			(FILE *) fd);
	/* FIXME : This can be necessary later due to progressive download.
	 * The solution for reducing fflush is needed */
	//fflush((FILE *) fd);
	if (write_success_len != (size_t)body_len) {
		DA_LOGE("write  fails ");
		if (errno == ENOSPC)
			ret = DA_ERR_DISK_FULL;
		else
			ret = DA_ERR_FAIL_TO_ACCESS_FILE;
		goto ERR;
	}
	file_info->bytes_written_to_file += write_success_len;
	DA_LOGV( "write %llu bytes", write_success_len);
	file_info->is_updated = DA_TRUE;

ERR:
	return ret;
}

/* Priority to derive extension
 * 1. extension name which client set
 * 2. according to MIME-Type
 * 3. if MIME-Type is ambiguous or blank,
 *    3-1. derived from <Content-Disposition> field's "filename" attribute
 *    3-2. derived from url
 * 4. if url does not have extension, leave blank for extension
 */
char *__get_extension_name(char *mime_type,
		char *file_name_from_header, char *url)
{
	char *extension = DA_NULL;

	/* Priority 1 */
	if (mime_type && !is_ambiguous_MIME_Type(mime_type)) {
		char *extension = DA_NULL;
		da_ret_t ret = get_extension_from_mime_type(mime_type, &extension);
		if (ret == DA_RESULT_OK && extension)
			return extension;
	}
	/* Priority 2-1 */
	if (file_name_from_header) {
		char *extension = DA_NULL;
		DA_SECURE_LOGI("Content-Disposition :[%s]", file_name_from_header);
		__divide_file_name_into_pure_name_N_extesion(file_name_from_header,
				DA_NULL, &extension);
		if (extension)
			return extension;
	}
	/* Priority 2-2 */
	if (url) {
		DA_LOGV("Get extension from url");
		da_bool_t b_ret = da_get_extension_name_from_url(url, &extension);
		if (b_ret && extension)
			return extension;
	}
	return DA_NULL;
}

/** Priority for deciding file name
 * 1. file name which client set
 * 2. 'filename' option on HTTP response header's Content-Disposition field
 * 3. requesting URL
 * 4. Otherwise, define it as "No name"
 */
da_ret_t __get_candidate_file_name(char *user_file_name, char *url,
		char *file_name_from_header,
		char **out_pure_file_name, char **out_extension)
{
	da_ret_t ret = DA_RESULT_OK;

	DA_LOGV("");

	/* Priority 1 */
	if (user_file_name) {
		__divide_file_name_into_pure_name_N_extesion(
				user_file_name, out_pure_file_name, out_extension);
	}
	if (*out_pure_file_name)
		return ret;
	/* Priority 2 */
	if (file_name_from_header) {
		DA_SECURE_LOGI("Content-Disposition:[%s]", file_name_from_header);
		__divide_file_name_into_pure_name_N_extesion(file_name_from_header,
				out_pure_file_name, DA_NULL);
	}
	if (*out_pure_file_name)
		return ret ;
	/* Priority 3 */
	if (url) {
		DA_LOGV("Get file name from url");
		da_get_file_name_from_url(url, out_pure_file_name);
	}
	if (*out_pure_file_name)
		return ret ;
	/* Priority 4 */
	*out_pure_file_name = strdup(NO_NAME_TEMP_STR);
	if (*out_pure_file_name == DA_NULL)
		ret = DA_ERR_FAIL_TO_MEMALLOC;
	return ret;
}

da_ret_t __decide_file_path(da_info_t *da_info)
{
	da_ret_t ret = DA_RESULT_OK;
	char *extension = DA_NULL;
	char *file_name = DA_NULL;
	char *tmp_file_path = DA_NULL;
	char *install_dir = DA_DEFAULT_INSTALL_PATH_FOR_PHONE;
	char *user_file_name = DA_NULL;
	char *file_name_from_header = DA_NULL;
	char *url = DA_NULL;
	file_info_t *file_info = DA_NULL;
	req_info_t *req_info = DA_NULL;
	http_info_t *http_info = DA_NULL;

	DA_LOGV("");

	NULL_CHECK_RET(da_info);
	file_info = da_info->file_info;
	NULL_CHECK_RET(file_info);
	req_info = da_info->req_info;
	NULL_CHECK_RET(req_info);
	http_info = da_info->http_info;
	NULL_CHECK_RET(http_info);

	if (req_info->install_path)
		install_dir = req_info->install_path;
	user_file_name = req_info->file_name;
	/* If there is location url from response header in case of redirection,
	 * it try to parse the file name from the location url */
	if (http_info->location_url) {
		url = http_info->location_url;
		DA_LOGI("[TEST] location_url[%s][%p]",http_info->location_url, http_info->location_url);
	} else
		url = req_info->url;

	file_name_from_header = http_info->file_name_from_header;

	/* extension is extracted only if User set specific name */
	ret = __get_candidate_file_name(user_file_name, url, file_name_from_header,
			&file_name, &extension);
	if (ret != DA_RESULT_OK)
		goto ERR;

	if (file_name && strpbrk(file_name, DA_INVALID_PATH_STRING) != NULL) {
		DA_LOGI("Invalid string at file name");
		free(file_name);
		file_name = strdup(NO_NAME_TEMP_STR);
		if (!file_name) {
			ret = DA_ERR_FAIL_TO_MEMALLOC;
			goto ERR;
		}

	}

	DA_SECURE_LOGI("candidate file name [%s]", file_name);

	if (!extension) {
		extension = __get_extension_name(file_info->mime_type,
				file_name_from_header, url);
	}
	if (file_name && !file_info->pure_file_name) {
		file_info->pure_file_name = file_name;
		file_name = DA_NULL;
	}
	if (extension && !file_info->extension) {
		DA_LOGV("candidate extension [%s]", extension);
		file_info->extension = extension;
		extension = DA_NULL;
	}

	// for resume
	tmp_file_path = get_full_path_avoided_duplication(install_dir,
			file_info->pure_file_name, file_info->extension);
	if (tmp_file_path) {
		file_info->file_path = tmp_file_path;
		tmp_file_path = DA_NULL;
	} else {
		ret = DA_ERR_FAIL_TO_ACCESS_FILE;
		goto ERR;
	}

ERR:
	DA_SECURE_LOGI("decided file path [%s]", file_info->file_path);
	free(file_name);
	free(extension);
	return ret;
}

// for resume with new download request
da_ret_t __decide_file_path_for_resume(file_info_t *file_info)
{
	da_ret_t ret = DA_RESULT_OK;
	char *extension = DA_NULL;
	char *file_name = DA_NULL;
	char *file_path = DA_NULL;
	char *ptr = DA_NULL;
	char *ptr2 = DA_NULL;

	DA_LOGV("");

	NULL_CHECK_RET(file_info);

	file_path = file_info->file_path;
	ptr = strrchr(file_path, '/');
	if (ptr) {
		ptr++;
		ptr2 = strrchr(ptr, '.');
		if (ptr2) {
			int len = 0;
			len = ptr2 - ptr;
			ptr2++;
			extension = strdup(ptr2);
			file_name = calloc(1, len + 1);
			if (file_name)
				snprintf(file_name, len + 1, "%s", ptr);
		} else {
			file_name = strdup(ptr);
		}
	}

	if (file_name && !file_info->pure_file_name) {
		file_info->pure_file_name = file_name;
		file_name = DA_NULL;
	} else {
		free(file_name);
	}
	if (extension && !file_info->extension) {
		DA_LOGV( "candidate extension [%s]", extension);
		file_info->extension = extension;
		extension = DA_NULL;
	} else {
		free(extension);
	}
	return ret;
}

da_ret_t start_file_writing(da_info_t *da_info)
{
	da_ret_t ret = DA_RESULT_OK;
	file_info_t *file_info = DA_NULL;
	req_info_t *req_info = DA_NULL;

	DA_LOGV("");

	NULL_CHECK_RET(da_info);
	file_info = da_info->file_info;
	NULL_CHECK_RET(file_info);
	req_info = da_info->req_info;
	NULL_CHECK_RET(req_info);

	/* resume case */
	if (req_info->etag || req_info->temp_file_path) {
		char *file_path = DA_NULL;
		char *origin_path = DA_NULL;
		file_path = req_info->temp_file_path;
		if (!file_path)
			return DA_ERR_INVALID_ARGUMENT;
		origin_path = file_info->file_path;
		file_info->file_path = strdup(file_path);
		free(origin_path);
		ret = __decide_file_path_for_resume(file_info);
	} else {
		ret = __decide_file_path(da_info);
	}

	if (ret != DA_RESULT_OK)
		goto ERR;

	if (req_info->etag || req_info->temp_file_path) {
		da_size_t file_size = 0;
		get_file_size(req_info->temp_file_path, &file_size);
		if (file_size < 1)
			goto ERR;
#ifdef _RAF_SUPPORT
		file_info->file_size_of_temp_file = file_size;
#endif
		file_info->bytes_written_to_file = file_size;
	} else {
		file_info->bytes_written_to_file = 0;
	}
	ret = __saved_file_open(file_info);
ERR:
	return ret;
}

da_ret_t start_file_append(file_info_t *file_info)
{
	da_ret_t ret = DA_RESULT_OK;

	DA_LOGV("");

	NULL_CHECK_RET(file_info);

	ret = __saved_file_open(file_info);
	return ret;
}

da_ret_t file_write_ongoing(file_info_t *file_info, char *body, int body_len)
{
	da_ret_t ret = DA_RESULT_OK;
	int buffer_size = 0;
	char *buffer = DA_NULL;

	DA_LOGV("");

	buffer = file_info->buffer;
	buffer_size = file_info->buffer_len;

	if (DA_NULL == buffer) {
		if (body_len < DA_FILE_BUF_SIZE) {
			ret = __file_write_buf_make_buf(file_info);
			if (ret != DA_RESULT_OK)
				goto ERR;
			__file_write_buf_copy_to_buf(file_info, body, body_len);
		} else {
			ret = __file_write_buf_directly_write(file_info,
					body, body_len);
			if (ret != DA_RESULT_OK)
				goto ERR;
		}
	} else {
		if (DA_FILE_BUF_SIZE <= body_len) {
			ret = __file_write_buf_flush_buf(file_info);
			if (ret != DA_RESULT_OK)
				goto ERR;
			ret = __file_write_buf_directly_write(file_info,
					body, body_len);
			if (ret != DA_RESULT_OK)
				goto ERR;
		} else if ((DA_FILE_BUF_SIZE - buffer_size) <= body_len) {
			ret = __file_write_buf_flush_buf(file_info);
			if (ret != DA_RESULT_OK)
				goto ERR;
			__file_write_buf_copy_to_buf(file_info, body, body_len);
		} else {
			__file_write_buf_copy_to_buf(file_info, body, body_len);
		}
	}
ERR:
	if (ret != DA_RESULT_OK) {
		file_info->buffer_len = 0;
		free(file_info->buffer);
		file_info->buffer = DA_NULL;
	}
	return ret;
}

#ifdef _RAF_SUPPORT
da_ret_t file_write_complete_for_raf(file_info_t *file_info) {
	da_ret_t ret = DA_RESULT_OK;
	char *buffer = DA_NULL;
	da_size_t wrriten_size = 0;
	da_size_t file_size = 0;
	void *fd = DA_NULL;

	DA_LOGV("");
	fd = file_info->file_handle;

	wrriten_size = file_info->bytes_written_to_file;
	// test code
	get_file_size(file_info->file_path, &file_size);
	DA_LOGI("wrriten_size:%llu file_size:%llu file[%s]",
			wrriten_size, file_size, file_info->file_path);
	if (fd) {
		fclose(fd);
		fd = DA_NULL;
	}
	file_info->file_handle = DA_NULL;
	if (wrriten_size < file_size) {
		DA_LOGD("Try truncate");
		if (truncate(file_info->file_path, wrriten_size) < 0) {
			DA_LOGE("Fail to ftruncate: errno[%d,%s]", errno, strerror(errno));
		}
		DA_LOGD("Try truncate done");
	}

	ERR:
		return ret;
}
#endif

da_ret_t file_write_complete(file_info_t *file_info)
{
	da_ret_t ret = DA_RESULT_OK;
	char *buffer = DA_NULL;
	unsigned int buffer_size = 0;
	void *fd = DA_NULL;

	DA_LOGV("");

	buffer = file_info->buffer;
	buffer_size = file_info->buffer_len;

	if (DA_NULL == buffer) {
		DA_LOGE("file buffer is NULL");
	} else {
		if (buffer_size != 0) {
			ret = __file_write_buf_flush_buf(file_info);
			if (ret != DA_RESULT_OK)
				goto ERR;
		}
		__file_write_buf_destroy_buf(file_info);
	}
	fd = file_info->file_handle;

	if (fd) {
		fclose(fd);
		fd = DA_NULL;
	}
	file_info->file_handle = DA_NULL;
ERR:
	return ret;
}

da_ret_t discard_download(file_info_t *file_info)
{
	da_ret_t ret = DA_RESULT_OK;
	FILE *f_handle = DA_NULL;

	DA_LOGV("");

	f_handle = file_info->file_handle;
	if (f_handle) {
		fclose(f_handle);
		file_info->file_handle = DA_NULL;
	}
	return ret;
}

void clean_paused_file(file_info_t *file_info)
{
	char *paused_file_path = DA_NULL;
	FILE *fd = DA_NULL;

	DA_LOGV("");

	fd = file_info->file_handle;
	if (fd) {
		fclose(fd);
		file_info->file_handle = DA_NULL;
	}

	paused_file_path = file_info->file_path;
	file_info->bytes_written_to_file = 0; // Ignore resume flow after failed or cancled.
	remove_file((const char*) paused_file_path);

	return;
}

da_bool_t is_file_exist(const char *file_path)
{
	struct stat dir_state;
	int stat_ret;

	if (file_path == DA_NULL) {
		DA_LOGE("file path is DA_NULL");
		return DA_FALSE;
	}
	stat_ret = stat(file_path, &dir_state);
	if (stat_ret == 0) {
		if (dir_state.st_mode & S_IFREG) {
			//DA_SECURE_LOGD("Exist! %s is a regular file & its size = %lu", file_path, dir_state.st_size);
			return DA_TRUE;
		}

		return DA_FALSE;
	}
	return DA_FALSE;

}

void get_file_size(char *file_path, da_size_t *out_file_size)
{
	struct stat dir_state;
	int stat_ret;

	*out_file_size = -1;

	if (file_path == DA_NULL) {
		DA_LOGE("file path is DA_NULL");
		return;
	}
	/* Please do not use ftell() to obtain file size, use stat instead.
	 *  This is a guide from www.securecoding.cert.org
	 *    : FIO19-C. Do not use fseek() and ftell() to compute the size of a file
	 */
	stat_ret = stat(file_path, &dir_state);
	if (stat_ret == 0) {
		if (dir_state.st_mode & S_IFREG) {
			DA_LOGV( "size = %lu", dir_state.st_size);
			*out_file_size = dir_state.st_size;
		}
	}
	return;
}

char *get_full_path_avoided_duplication(char *in_dir,
		char *in_candidate_file_name, char *in_extension)
{
	char *dir = in_dir;
	char *file_name = in_candidate_file_name;
	char *extension = in_extension;
	char *final_path = DA_NULL;

	int dir_path_len = 0;
	int final_path_len = 0;
	int extension_len = 0;

	int suffix_count = 0;	/* means suffix on file name. up to "_99" */
	int suffix_len = (int)log10(MAX_SUFFIX_COUNT + 1) + 1;	/* 1 means "_" */

	if (!in_dir || !in_candidate_file_name)
		return DA_NULL;

	//DA_SECURE_LOGI("in_candidate_file_name=[%s],in_extension=[%s]",
			//in_candidate_file_name, in_extension);

	if (extension)
		extension_len = strlen(extension);

	// to remove trailing slash from dir path
	dir_path_len = strlen(dir);
	if (dir[dir_path_len - 1] == '/') {
		dir[dir_path_len - 1] = '\0';
		--dir_path_len;
	}

	/* first 1 for "/", second 1 for ".", last 1 for DA_NULL */
	final_path_len = dir_path_len + 1 + strlen(file_name) + 1
			+ suffix_len + extension_len + 1;

	final_path = (char*)calloc(1, final_path_len);
	if (!final_path) {
		DA_LOGE("DA_ERR_FAIL_TO_MEMALLOC");
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
			if (suffix_count > MAX_SUFFIX_COUNT) {
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

	//DA_SECURE_LOGD("decided path = [%s]", final_path);
	return final_path;
}


da_ret_t check_drm_convert(file_info_t *file_info)
{
	da_ret_t ret = DA_RESULT_OK;
	da_bool_t ret_b = DA_TRUE;

	DA_LOGD("");

	NULL_CHECK_RET(file_info);

#ifdef _ENABLE_OMA_DRM

	/* In case of OMA DRM 1.0 SD, it is not necessary to call DRM convert API.
	 * Because it is already converted itself.
	 * And, the case will return fail because The SD is not supported now.
	 */
	if (is_content_drm_dcf(file_info->mime_type)) {
		DA_LOGI("DRM SD case");
		unlink(file_info->file_path);
		free(file_info->file_path);
		file_info->file_path = DA_NULL;
		return DA_ERR_DRM_FAIL;
	}
	if (is_content_drm_dm(file_info->mime_type)) {
		char *actual_file_path = DA_NULL;
		char *out_file_path = DA_NULL;

		actual_file_path = file_info->file_path;
		DA_SECURE_LOGD("actual_file_path = %s", actual_file_path);
		if (!actual_file_path)
			return DA_ERR_INVALID_ARGUMENT;
		ret_b = EDRM_convert(actual_file_path, &out_file_path);
		unlink(actual_file_path);
		free(actual_file_path);
		if (!ret_b)
			ret = DA_ERR_DRM_FAIL;
		file_info->file_path = out_file_path;
	} else {
		return ret;
	}
#endif

	return ret;
}

void remove_file(const char *file_path)
{
	DA_LOGV("");

	if (file_path && is_file_exist(file_path)) {
		DA_SECURE_LOGD("remove file [%s]", file_path);
		if (unlink(file_path) < 0) {
			DA_LOGE("file removing failed.");
		}
	}
}

da_ret_t get_available_memory(char *dir_path, da_size_t len)
{
	da_ret_t ret = DA_RESULT_OK;
	int fs_ret = 0;
	//struct statfs filesys_info = {0, };
	struct statvfs filesys_info;

	DA_LOGV("");

	if (!dir_path)
		return DA_ERR_INVALID_INSTALL_PATH;

	//fs_ret = statfs(dir_path, &filesys_info);
	// Using this as it considers FOTA memory while returning available memory
	fs_ret = storage_get_internal_memory_size(&filesys_info);

	if (fs_ret != 0) {
	//	DA_LOGE("statfs error[%s]", strerror(errno));
		return DA_ERR_INVALID_ARGUMENT;
	//	return DA_ERR_INVALID_INSTALL_PATH;
	}

	double available_size =  (double)filesys_info.f_bsize * filesys_info.f_bavail;
	double total_size = (double)filesys_info.f_frsize * filesys_info.f_blocks;
	DA_SECURE_LOGI(" total = %lf ", total_size);
	DA_SECURE_LOGI(" available = %lf ",available_size);

	DA_LOGV("Available Memory(f_bavail) : %lu", filesys_info.f_bavail);
	DA_LOGV("Available Memory(f_bsize) : %d", filesys_info.f_bsize);
	DA_LOGD("Available Memory(kbytes) : %d", (filesys_info.f_bavail/1024)*filesys_info.f_bsize);
	DA_LOGV("Content: %llu", len);
	if (available_size < (len
			+ SAVE_FILE_BUFFERING_SIZE_50KB)) /* 50KB buffering */
		ret = DA_ERR_DISK_FULL;

	return ret;
}
