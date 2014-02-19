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

#ifndef DOWNLOAD_PROVIDER2_SOCKET_H
#define DOWNLOAD_PROVIDER2_SOCKET_H

#include <bundle.h>
#include "download-provider.h"
#include "download-provider-slots.h"

int dp_ipc_send_errorcode(int fd, dp_error_type errorcode);
int dp_ipc_send_event(int fd, int id, dp_state_type state,
	dp_error_type errorcode, unsigned long long received_size);
#if 0
int dp_ipc_send_progressinfo(int fd, int id,
	unsigned long long received_size, unsigned long long file_size,
	unsigned int chunk_size);
int dp_ipc_send_stateinfo(int fd, int id, dp_state_type state,
	dp_error_type errorcode);
#endif
char *dp_ipc_read_string(int fd);
unsigned dp_ipc_read_bundle(int fd, int *type, bundle_raw **b);
int dp_ipc_send_string(int fd, const char *str);
int dp_ipc_send_bundle(int fd, bundle_raw *b, unsigned length);
int dp_ipc_send_custom_type(int fd, void *value, size_t type_size);
int dp_ipc_read_custom_type(int fd, void *value, size_t type_size);
int dp_accept_socket_new();
int dp_socket_free(int sockfd);

#endif
