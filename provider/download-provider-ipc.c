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

#include <time.h>

#include <sys/socket.h> // shutdown

#include "download-provider-log.h"
#include "download-provider-ipc.h"

int dp_ipc_check_stderr(int basecode)
{
	int errorcode = basecode;
	if (errno == EPIPE) {
		TRACE_ERROR("[EPIPE:%d] Broken Pipe", errno);
		errorcode = DP_ERROR_IO_ERROR;
	} else if (errno == EAGAIN) {
		TRACE_ERROR("[EAGAIN:%d]", errno);
		errorcode = DP_ERROR_IO_EAGAIN;
	} else if (errno == EINTR) {
		TRACE_ERROR("[EINTR:%d]", errno);
		errorcode = DP_ERROR_IO_EINTR;
	} else if (errno == ENOENT) {
		TRACE_ERROR("[ENOENT:%d]", errno);
		errorcode = DP_ERROR_IO_ERROR;
	} else {
		TRACE_ERROR("[errno:%d]", errno);
	}
	return errorcode;
}

int dp_ipc_write(int sock, void *value, size_t type_size)
{
	if (sock < 0) {
		TRACE_ERROR("[ERROR] check sock:%d", sock);
		return -1;
	} else if (value == NULL) {
		TRACE_ERROR("[ERROR] check buffer sock:%d", sock);
		return -1;
	} else if (write(sock, value, type_size) <= 0) {
		TRACE_ERROR("[IPC.Write] exception sock:%d", sock);
		return -1;
	}
	return 0;
}

ssize_t dp_ipc_read(int sock, void *value, size_t type_size,
	const char *func)
{
	int errorcode = DP_ERROR_NONE;
	ssize_t recv_bytes = 0;

	if (sock < 0) {
		TRACE_ERROR("[ERROR] %s check sock:%d", func, sock);
		return -1;
	}
	if (value == NULL) {
		TRACE_ERROR("[ERROR] %s check buffer sock:%d", func, sock);
		return -1;
	}

	int tryagain = 3;
	do {
		errorcode = DP_ERROR_NONE;
		recv_bytes = read(sock, value, type_size);
		if (recv_bytes < 0) {
			TRACE_ERROR("[IPC.Read] %s exception sock:%d", func, sock);
			errorcode = dp_ipc_check_stderr(DP_ERROR_IO_ERROR);
		} else if (recv_bytes == 0) {
			TRACE_ERROR("[ERROR] %s closed peer sock:%d", func, sock);
			errorcode = DP_ERROR_IO_ERROR;
		}
	} while (sock >= 0 && (errorcode == DP_ERROR_IO_EAGAIN ||
		errorcode == DP_ERROR_IO_EINTR) && (--tryagain > 0));
	return recv_bytes;
}

dp_ipc_fmt *dp_ipc_get_fmt(int sock)
{
	dp_ipc_fmt *ipc_info = malloc(sizeof(dp_ipc_fmt));
	if (ipc_info == NULL) {
		TRACE_ERROR("[ERROR] Fail to malloc");
		return NULL;
	}
	memset(ipc_info, 0x00, sizeof(dp_ipc_fmt));
	ssize_t recv_size = read(sock, ipc_info, sizeof(dp_ipc_fmt));
	if (recv_size <= 0 || recv_size != sizeof(dp_ipc_fmt)) {
		TRACE_ERROR("socket read ipcinfo read size:%d", recv_size);
		free(ipc_info);
		return NULL;
	}
	return ipc_info;
}

int dp_ipc_query(int sock, int download_id, short section,
	unsigned property, int error, size_t size)
{
	dp_ipc_fmt ipc_info;
	memset(&ipc_info, 0x00, sizeof(dp_ipc_fmt));
	ipc_info.section = section;
	ipc_info.property = property;
	ipc_info.id = download_id;
	ipc_info.errorcode = error;
	ipc_info.size = size;
	if (dp_ipc_write(sock, &ipc_info, sizeof(dp_ipc_fmt)) < 0)
		return -1;
	return 0;
}

int dp_ipc_socket_free(int sockfd)
{
	if (sockfd < 0)
		return -1;
	close(sockfd);
	return 0;
}
