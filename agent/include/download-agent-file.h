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

#ifndef _Download_Agent_File_H
#define _Download_Agent_File_H

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "download-agent-type.h"
#include "download-agent-dl-mgr.h"

#define DA_DEFAULT_INSTALL_PATH_FOR_PHONE "/opt/usr/media/Downloads"

da_bool_t is_file_exist(const char *file_path);
da_bool_t is_dir_exist(const char *dir_path);

void get_file_size(char *file_path, unsigned long long *out_file_size);

da_result_t clean_files_from_dir(char *dir_path);
da_result_t file_write_ongoing(stage_info *stage, char *body, int body_len);
da_result_t file_write_complete(stage_info *stage);
da_result_t start_file_writing(stage_info *stage);
da_result_t start_file_writing_append(stage_info *stage);
da_result_t start_file_writing_append_with_new_download(stage_info *stage);

da_result_t  get_mime_type(stage_info *stage, char **out_mime_type);
da_result_t  discard_download(stage_info *stage) ;
void clean_paused_file(stage_info *stage);
da_result_t  replace_content_file_in_stage(stage_info *stage, const char *dest_dd_file_path);
da_result_t  decide_tmp_file_path(stage_info *stage);
char *get_full_path_avoided_duplication(char *in_dir, char *in_candidate_file_name, char *in_extension);

da_result_t copy_file(const char *src, const char *dest);
da_result_t create_dir(const char *install_dir);
da_result_t get_default_install_dir(char **out_path);

#endif
