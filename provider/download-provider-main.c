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

#include <systemd/sd-daemon.h>
#include <glib-object.h>

#include "download-provider-config.h"
#include "download-provider-log.h"
#include "download-provider-client-manager.h"
#include "download-provider-network.h"

void *dp_client_manager(void *arg);

int main(int argc, char **argv)
{
	GMainLoop *event_loop;
	pthread_t tid;
	TRACE_INFO("download-provider's working is started");

	g_type_init();

	event_loop = g_main_loop_new(NULL, FALSE);
	if (!event_loop) {
		TRACE_ERROR("Failed to create g main loop handle");
		return 0;
	}
	// check network status
	if (dp_network_connection_init() < 0) {
		TRACE_ERROR("failed to init network-manager");
		return 0;
	}
	// create a thread for main thread
	if (pthread_create(&tid, NULL, dp_client_manager, (void *)event_loop) != 0) {
		TRACE_ERROR("failed to create main thread");
		return 0;
	} else {
		pthread_detach(tid);
		TRACE_INFO("download main thread is created[%lu]", tid);
	}

	TRACE_INFO("g main loop is started");
	g_main_loop_run(event_loop);
	dp_network_connection_destroy();
	g_main_loop_unref(event_loop);

	TRACE_INFO("download-provider's working is done");
	return 0;
}
