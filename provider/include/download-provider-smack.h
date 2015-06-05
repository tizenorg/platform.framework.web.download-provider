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

#ifndef DOWNLOAD_PROVIDER_SMACK_H
#define DOWNLOAD_PROVIDER_SMACK_H

#ifdef SUPPORT_SECURITY_PRIVILEGE
#include <security-server.h>
#define SECURITY_PRIVILEGE_INTERNET "system::use_internet"
#endif

int dp_smack_is_mounted();
int dp_smack_set_label(char *label, char *source, char *target);
char *dp_smack_get_label_from_socket(int sock);
int dp_smack_is_valid_dir(int uid, int gid, char *smack_label, char *dir);
void dp_rebuild_dir(const char *dirpath, mode_t mode);
int dp_is_valid_dir(const char *dirpath);


#endif
