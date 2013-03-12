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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <pthread.h>

#include <time.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

#include <signal.h>

#include "download-provider.h"
#include "download-provider-log.h"
#include "download-provider-socket.h"

//////////////////////////////////////////////////////////////////////////
/// @brief write the error to socket
/// @return if success, return 0
int dp_ipc_send_errorcode(int fd, dp_error_type errorcode)
{
	if (fd < 0) {
		TRACE_ERROR("[ERROR] CHECK FD[%d]", fd);
		return -1;
	}

	if (fd >= 0 && write(fd, &errorcode, sizeof(dp_error_type)) <= 0) {
		TRACE_STRERROR("[ERROR] write FD[%d]", fd);
		return -1;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
/// @brief write the progressinfo to socket
/// @return if success, return 0
int dp_ipc_send_event(int fd, int id, dp_state_type state,
	dp_error_type errorcode, unsigned long long received_size)
{
	if (fd < 0) {
		TRACE_ERROR("[ERROR][%d] CHECK FD[%d]", id, fd);
		return -1;
	}

	dp_event_info eventinfo;
	eventinfo.id = id;
	eventinfo.state = state;
	eventinfo.err = errorcode;
	eventinfo.received_size = received_size;

	// write
	if (fd >= 0 && write(fd, &eventinfo, sizeof(dp_event_info)) <= 0) {
		TRACE_STRERROR("[ERROR][%d] write FD[%d]", id, fd);
		return -1;
	}
	return 0;
}

// keep the order/ unsigned , str
char *dp_ipc_read_string(int fd)
{
	unsigned length = 0;
	char *str = NULL;

	if (fd < 0) {
		TRACE_ERROR("[ERROR] CHECK FD[%d]", fd);
		return NULL;
	}

	// read flexible URL from client.
	if (read(fd, &length, sizeof(unsigned)) < 0) {
		TRACE_STRERROR("[ERROR] read FD[%d] length[%d]", fd, length);
		return NULL;
	}
	if (length < 1 || length > DP_MAX_URL_LEN) {
		TRACE_ERROR("[STRING LEGNTH] [%d]", length);
		return NULL;
	}
	str = (char *)calloc((length + 1), sizeof(char));
	if (read(fd, str, length * sizeof(char)) < 0) {
		TRACE_STRERROR("[ERROR] read FD[%d]", fd);
		free(str);
		str = NULL;
		return NULL;
	}
	str[length] = '\0';
	return str;
}

// keep the order/ unsigned , str
int dp_ipc_send_string(int fd, const char *str)
{
	unsigned length = 0;

	if (fd < 0) {
		TRACE_ERROR("[ERROR] CHECK FD[%d]", fd);
		return -1;
	}
	if (!str) {
		TRACE_ERROR("[ERROR] CHECK STRING FD[%d]", fd);
		return -1;
	}

	length = strlen(str);
	if (length < 1) {
		TRACE_ERROR("[ERROR] CHECK LENGTH FD[%d]", fd);
		return -1;
	}
	if (fd >= 0 && write(fd, &length, sizeof(unsigned)) <= 0) {
		TRACE_STRERROR("[ERROR] read FD[%d] length[%d]", fd, length);
		return -1;
	}
	if (fd >= 0 && write(fd, str, length * sizeof(char)) <= 0) {
		TRACE_STRERROR("[ERROR] write FD[%d]", fd);
		return -1;
	}
	return 0;
}

int dp_ipc_send_custom_type(int fd, void *value, size_t type_size)
{
	if (fd < 0) {
		TRACE_ERROR("[ERROR] CHECK FD[%d]", fd);
		return -1;
	}
	if (!value) {
		TRACE_ERROR("[ERROR] CHECK VALUE FD[%d]", fd);
		return -1;
	}
	if (fd >= 0 && write(fd, value, type_size) <= 0) {
		TRACE_STRERROR("[ERROR] write FD[%d]", fd);
		return -1;
	}
	return 0;
}

int dp_ipc_read_custom_type(int fd, void *value, size_t type_size)
{
	if (fd < 0) {
		TRACE_ERROR("[ERROR] CHECK FD[%d]", fd);
		return -1;
	}

	if (read(fd, value, type_size) < 0) {
		TRACE_STRERROR("[ERROR] read FD[%d]", fd);
		return -1;
	}
	return 0;
}

int dp_accept_socket_new()
{
	int sockfd = -1;
	struct sockaddr_un listenaddr;

	if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		TRACE_STRERROR("failed to create socket");
		return -1;
	}

	bzero(&listenaddr, sizeof(listenaddr));
	listenaddr.sun_family = AF_UNIX;
	strcpy(listenaddr.sun_path, DP_IPC);

	if (bind(sockfd, (struct sockaddr *)&listenaddr, sizeof listenaddr) !=
		0) {
		TRACE_STRERROR("[CRITICAL] bind");
		close(sockfd);
		return -1;
	}

	if (chmod(listenaddr.sun_path, 0777) < 0) {
		TRACE_STRERROR("[CRITICAL] chmod");
		close(sockfd);
		return -1;
	}

	// need 3 socket per a group
	if (listen(sockfd, DP_MAX_GROUP * 3) != 0) {
		TRACE_STRERROR("[CRITICAL] listen");
		close(sockfd);
		return -1;
	}
	return sockfd;
}

int dp_socket_free(int sockfd)
{
	if (sockfd < 0)
		return -1;
	shutdown(sockfd, 0);
	close(sockfd);
	return 0;
}
