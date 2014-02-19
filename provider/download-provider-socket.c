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
#include <systemd/sd-daemon.h>
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
	size_t recv_size = 0;
	unsigned remain_size = 0;
	size_t buffer_size = 0;
	char *str = NULL;

	if (fd < 0) {
		TRACE_ERROR("[ERROR] CHECK FD[%d]", fd);
		return NULL;
	}

	// read flexible URL from client.
	ssize_t recv_bytes = read(fd, &length, sizeof(unsigned));
	if (recv_bytes < 0) {
		TRACE_STRERROR("[ERROR] read FD[%d] length[%d]", fd, length);
		return NULL;
	}
	if (length < 1 || length > DP_MAX_URL_LEN) {
		TRACE_ERROR("[STRING LEGNTH] [%d]", length);
		return NULL;
	}
	str = (char *)calloc((length + 1), sizeof(char));
	if (str == NULL) {
		TRACE_STRERROR("[ERROR] calloc length:%d FD[%d]", length, fd);
		return NULL;
	}
	remain_size = length;
	do {
		buffer_size = 0;
		if (remain_size > DP_DEFAULT_BUFFER_SIZE)
			buffer_size = DP_DEFAULT_BUFFER_SIZE;
		else
			buffer_size = remain_size;
		recv_size = (size_t)read(fd, str + (int)(length - remain_size),
				buffer_size * sizeof(char));
		if (recv_size > DP_DEFAULT_BUFFER_SIZE) {
			recv_size = -1;
			break;
		}
		if (recv_size > 0)
			remain_size = remain_size - (unsigned)recv_size;
	} while (recv_size > 0 && remain_size > 0);

	if (recv_size == 0) {
		TRACE_STRERROR("[ERROR] closed peer:%d", fd);
		free(str);
		return NULL;
	}
	str[length] = '\0';
	return str;
}


// 0 : Socket Error
// -1 : Invalid type
unsigned dp_ipc_read_bundle(int fd, int *type, bundle_raw **b)
{
	unsigned length = 0;
	size_t recv_size = 0;
	unsigned remain_size = 0;
	size_t buffer_size = 0;
	bundle_raw *b_raw = NULL;

	if (fd < 0) {
		TRACE_ERROR("[ERROR] CHECK FD[%d]", fd);
		return 0;
	}

	// read flexible URL from client.
	ssize_t recv_bytes = read(fd, type, sizeof(int));
	if (recv_bytes < 0) {
		TRACE_STRERROR("[ERROR] read FD[%d] type[%d]", fd, type);
		return 0;
	}
	if ((*type) != DP_NOTIFICATION_BUNDLE_TYPE_ONGOING &&
			(*type) !=  DP_NOTIFICATION_BUNDLE_TYPE_COMPLETE &&
			(*type) !=  DP_NOTIFICATION_BUNDLE_TYPE_FAILED) {
		TRACE_ERROR("[NOTI TYPE] [%d]", *type);
		return -1;
	}
	// read flexible URL from client.
	recv_bytes = read(fd, &length, sizeof(unsigned));
	if (recv_bytes < 0) {
		TRACE_STRERROR("[ERROR] read FD[%d] length[%d]", fd, length);
		return 0;
	}
	if (length < 1 || length > DP_MAX_URL_LEN) {
		TRACE_ERROR("[STRING LEGNTH] [%d]", length);
		return 0;
	}
	b_raw = (bundle_raw *)calloc(length, 1);
	if (b_raw == NULL) {
		TRACE_STRERROR("[ERROR] calloc length:%d FD[%d]", length, fd);
		return 0;
	}
	remain_size = length;
	do {
		buffer_size = 0;
		if (remain_size > DP_DEFAULT_BUFFER_SIZE)
			buffer_size = DP_DEFAULT_BUFFER_SIZE;
		else
			buffer_size = remain_size;
		recv_size = (size_t)read(fd, b_raw + (int)(length - remain_size),
				buffer_size * sizeof(char));
		if (recv_size > DP_DEFAULT_BUFFER_SIZE) {
			recv_size = -1;
			break;
		}
		if (recv_size > 0)
			remain_size = remain_size - (unsigned)recv_size;
	} while (recv_size > 0 && remain_size > 0);

	if (recv_size <= 0) {
		TRACE_STRERROR("[ERROR] closed peer:%d", fd);
		bundle_free_encoded_rawdata(&b_raw);
		return 0;
	}
	*b = b_raw;
	return length;
}

// keep the order/ unsigned , str
int dp_ipc_send_string(int fd, const char *str)
{
	unsigned length = 0;

	if (fd < 0) {
		TRACE_ERROR("[ERROR] CHECK FD[%d]", fd);
		return -1;
	}
	if (str == NULL) {
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

int dp_ipc_send_bundle(int fd, bundle_raw *b, unsigned length)
{
	if (fd < 0) {
		TRACE_ERROR("[ERROR] CHECK FD[%d]", fd);
		return -1;
	}
	if (b == NULL) {
		TRACE_ERROR("[ERROR] CHECK STRING FD[%d]", fd);
		return -1;
	}

	if (length < 1) {
		TRACE_ERROR("[ERROR] CHECK LENGTH FD[%d]", fd);
		return -1;
	}
	if (fd >= 0 && write(fd, &length, sizeof(unsigned)) <= 0) {
		TRACE_STRERROR("[ERROR] read FD[%d] length[%d]", fd, length);
		return -1;
	}
	if (fd >= 0 && write(fd, b, length) <= 0) {
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
	if (value == NULL) {
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
	if (value == NULL) {
		TRACE_ERROR("[ERROR] CHECK VALUE FD[%d]", fd);
		return -1;
	}

	ssize_t recv_bytes = read(fd, value, type_size);
	if (recv_bytes < 0) {
		TRACE_STRERROR("[ERROR] read FD[%d]", fd);
		return -1;
	}
	return 0;
}

int dp_accept_socket_new()
{
	int sockfd = -1;
	struct sockaddr_un listenaddr;

	int n = sd_listen_fds(1);
	if (n > 1) {
		TRACE_STRERROR("too many file descriptors received");
		return -1;
	} else if (n == 1) {
		int r;
		if ((r = sd_is_socket_unix(SD_LISTEN_FDS_START, SOCK_STREAM, 1, DP_IPC, 0)) <= 0) {
			TRACE_STRERROR("passed systemd file descriptor is of wrong type");
			return -1;
		}
		sockfd = SD_LISTEN_FDS_START + 0;
	} else {
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
	}
	return sockfd;
}

int dp_socket_free(int sockfd)
{
	TRACE_DEBUG("[%d]", sockfd);
	if (sockfd < 0)
		return -1;
	shutdown(sockfd, 0);
	close(sockfd);
	return 0;
}
