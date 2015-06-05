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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <download-provider-log.h>
#include <download-provider-client.h>
#include <download-provider-client-manager.h>
#include <download-provider-ipc.h>

static char *__dp_notify_get_path(pid_t pid)
{
	size_t path_size = sizeof(NOTIFY_DIR) + 21;
	char *notify_fifo = (char *)calloc(path_size, sizeof(char));
	if (notify_fifo == NULL) {
		TRACE_STRERROR("failed to alocalte fifo path pid:%d", (int)pid);
		return NULL;
	}
	if (snprintf(notify_fifo, path_size,"%s/%d", NOTIFY_DIR, pid) < 0) {
		TRACE_STRERROR("failed to make fifo path pid:%d", (int)pid);
		free(notify_fifo);
		return NULL;
	}
	return notify_fifo;
}

int dp_notify_init(pid_t pid)
{
	char *notify_fifo = __dp_notify_get_path(pid);
	if (notify_fifo == NULL)
		return -1;
	int notify_fd = -1;
	struct stat fifo_state;
	if (stat(notify_fifo, &fifo_state) == 0) // found
		unlink(notify_fifo);
	if (mkfifo(notify_fifo, 0644/*-rwrr*/) < 0) {
		TRACE_STRERROR("failed to make fifo %s", notify_fifo);
	} else {
		notify_fd = open(notify_fifo, O_RDWR | O_NONBLOCK, 0644);
	}
	free(notify_fifo);
	return notify_fd;
}

void dp_notify_deinit(pid_t pid)
{
	char *notify_fifo = __dp_notify_get_path(pid);
	if (notify_fifo == NULL)
		return ;
	struct stat fifo_state;
	if (stat(notify_fifo, &fifo_state) == 0) // found
		unlink(notify_fifo);
	free(notify_fifo);
}

static int __dp_notify_feedback(int sock, int id, int state, int errorcode, unsigned long long received_size)
{
	dp_ipc_event_fmt eventinfo;
	memset(&eventinfo, 0x00, sizeof(dp_ipc_event_fmt));
	eventinfo.id = id;
	eventinfo.state = state;
	eventinfo.errorcode = errorcode;
	eventinfo.received_size = received_size;
	if (dp_ipc_write(sock, &eventinfo, sizeof(dp_ipc_event_fmt)) < 0) {
		// failed to read from socket // ignore this status
		return -1;
	}
	return 0;
}

int dp_notify_feedback(int sock, void *slot, int id, int state, int errorcode, unsigned long long received_size)
{
	if (__dp_notify_feedback(sock, id, state, errorcode, received_size) < 0) {
		TRACE_ERROR("notify failure by IO_ERROR");
		if (slot != NULL) {
			dp_client_slots_fmt *base_slot = slot;
			if (base_slot->client.notify >= 0)
				close(base_slot->client.notify);
			base_slot->client.notify = -1;
			dp_notify_deinit(base_slot->credential.pid);
			TRACE_ERROR("disable notify channel by IO_ERROR");
		}
		return -1;
	}
	return 0;
}
