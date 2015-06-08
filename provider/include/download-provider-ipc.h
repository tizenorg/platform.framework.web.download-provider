
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

#ifndef DOWNLOAD_PROVIDER_IPC_H
#define DOWNLOAD_PROVIDER_IPC_H

#include "download-provider.h"

int dp_ipc_check_stderr(int basecode);
int dp_ipc_write(int sock, void *value, size_t type_size);
ssize_t dp_ipc_read(int sock, void *value, size_t type_size,
	const char *func);


dp_ipc_fmt *dp_ipc_get_fmt(int sock);
int dp_ipc_query(int sock, int download_id, short section,
	unsigned property, int error, size_t size);

int dp_ipc_socket_free(int sockfd);

#endif

