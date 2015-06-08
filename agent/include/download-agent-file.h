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

#ifndef _DOWNLOAD_AGENT_FILE_H
#define _DOWNLOAD_AGENT_FILE_H

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "download-agent-type.h"
#include "download-agent-dl-info.h"

#define DA_FILE_BUF_SIZE (1024*32) //bytes

da_ret_t check_drm_convert(file_info_t *file_info);
da_bool_t is_file_exist(const char *file_path);
void get_file_size(char *file_path, da_size_t *out_file_size);
da_ret_t file_write_ongoing(file_info_t *file_info, char *body, int body_len);
da_ret_t file_write_complete(file_info_t *file_info);
#ifdef _RAF_SUPPORT
da_ret_t file_write_complete_for_raf(file_info_t *file_info);
#endif
da_ret_t start_file_writing(da_info_t *da_info);
da_ret_t start_file_append(file_info_t *file_info);
da_ret_t  discard_download(file_info_t *file_info) ;
void clean_paused_file(file_info_t *file_info);
char *get_full_path_avoided_duplication(char *in_dir,
		char *in_candidate_file_name, char *in_extension);
void remove_file(const char *file_path);
da_ret_t get_available_memory(char *dir_path, da_size_t len);
#endif
