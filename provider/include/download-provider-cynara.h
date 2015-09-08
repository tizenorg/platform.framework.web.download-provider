/*
 *  Copyright (c) 2015 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License
 */
/**
 * @file        download-provider-cynara.h
 * @author      Lukasz Wojciechowski <l.wojciechow@partner.samsung.com>
 * @author      Oskar Świtalski <o.switalski@samsung.com>
 * @version     1.0
 * @brief       Cynara based access control functions definitions
 */

#ifndef DOWNLOAD_PROVIDER2_CYNARA_H
#define DOWNLOAD_PROVIDER2_CYNARA_H

#include <cynara-client.h>

#define SECURITY_PRIVILEGE_DOWNLOAD "http://tizen.org/privilege/download"

int dp_cynara_new();
void dp_cynara_free();
int dp_cynara_check(int fd, const char *priv);

const char *dp_cynara_user_new(uid_t uid);
const char *dp_cynara_session_new(pid_t pid);

#endif
