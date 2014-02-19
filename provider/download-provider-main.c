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
#include <glib.h>
#include <glib-object.h>
#include <pthread.h>
#include <locale.h>
#include <libintl.h>
#include <systemd/sd-daemon.h>

#include "vconf.h"

#include "download-provider-config.h"
#include "download-provider-log.h"
#include "download-provider-socket.h"
#include "download-provider-pthread.h"
#include "download-provider-slots.h"
#include "download-provider-db.h"
#include "download-provider-network.h"
#include "download-provider-queue.h"
#include "download-provider-notification.h"
#include "download-provider-da-interface.h"

// declare functions
int dp_lock_pid(char *path);
void *dp_thread_requests_manager(void *arg);
static pthread_t g_dp_thread_queue_manager_pid = 0;

// declare global variables
// need for libsoup, decided the life-time by mainloop.
GMainLoop *g_main_loop_handle = 0;

void dp_terminate(int signo)
{
	TRACE_DEBUG("Received SIGTERM:%d", signo);
	if (g_main_loop_is_running(g_main_loop_handle))
		g_main_loop_quit(g_main_loop_handle);
}

static gboolean __dp_idle_start_service(void *data)
{
	// declare all resources
	pthread_t thread_pid;
	pthread_attr_t thread_attr;

	// initialize
	if (pthread_attr_init(&thread_attr) != 0) {
		TRACE_STRERROR("failed to init pthread attr");
		dp_terminate(SIGTERM);
		return FALSE;
	}
	if (pthread_attr_setdetachstate(&thread_attr,
									PTHREAD_CREATE_DETACHED) != 0) {
		TRACE_STRERROR("failed to set detach option");
		dp_terminate(SIGTERM);
		return FALSE;
	}

	// create thread for managing QUEUEs
	if (pthread_create
		(&g_dp_thread_queue_manager_pid, NULL, dp_thread_queue_manager,
		data) != 0) {
		TRACE_STRERROR
			("failed to create pthread for run_manage_download_server");
		dp_terminate(SIGTERM);
	}

	// start service, accept url-download ( client package )
	if (pthread_create
		(&thread_pid, &thread_attr, dp_thread_requests_manager,
		data) != 0) {
		TRACE_STRERROR
			("failed to create pthread for run_manage_download_server");
		dp_terminate(SIGTERM);
	}
	return FALSE;
}

void __set_locale()
{
	char *str = NULL;
	str = vconf_get_str(VCONFKEY_LANGSET);
	if (str != NULL) {
		setlocale(LC_ALL, str);
		bindtextdomain(PKG_NAME, LOCALE_DIR);
		textdomain(PKG_NAME);
	}
	free(str);
}
void __lang_changed_cb(keynode_t *key, void* data)
{
	__set_locale();
}

int main(int argc, char **argv)
{
	dp_privates *privates = NULL;
	int lock_fd = -1;

	if (chdir("/") < 0) {
		TRACE_STRERROR("failed to call setsid or chdir");
		exit(EXIT_FAILURE);
	}

#if 0
	// close all console I/O
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
#endif

	if (signal(SIGTERM, dp_terminate) == SIG_ERR) {
		TRACE_ERROR("failed to register signal callback");
		exit(EXIT_FAILURE);
	}
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		TRACE_ERROR("failed to register signal callback");
		exit(EXIT_FAILURE);
	}
	if (signal(SIGINT, dp_terminate) == SIG_ERR) {
		TRACE_ERROR("failed to register signal callback");
		exit(EXIT_FAILURE);
	}
	// write IPC_FD_PATH. and lock
	if ((lock_fd = dp_lock_pid(DP_LOCK_PID)) < 0) {
		TRACE_ERROR
			("It need to check download-provider is already alive");
		TRACE_ERROR("Or fail to create pid file in (%s)",
				DP_LOCK_PID);
		exit(EXIT_FAILURE);
	}

	g_type_init();

	// locale
	__set_locale();
	if (vconf_notify_key_changed(VCONFKEY_LANGSET, __lang_changed_cb, NULL) != 0)
		TRACE_ERROR("Fail to set language changed vconf callback");

	privates = (dp_privates *) calloc(1, sizeof(dp_privates));
	if (!privates) {
		TRACE_ERROR("[CRITICAL] failed to alloc for private info");
		goto DOWNLOAD_EXIT;
	}
	privates->groups = dp_client_group_slots_new(DP_MAX_GROUP);
	if (privates->groups == NULL) {
		TRACE_ERROR("[CRITICAL]  failed to alloc for groups");
		goto DOWNLOAD_EXIT;
	}
	privates->requests = dp_request_slots_new(DP_MAX_REQUEST);
	if (privates->requests == NULL) {
		TRACE_ERROR("[CRITICAL]  failed to alloc for requests");
		goto DOWNLOAD_EXIT;
	}

	// ready socket ( listen )
	privates->listen_fd = dp_accept_socket_new();
	if (privates->listen_fd < 0) {
		TRACE_ERROR("[CRITICAL] failed to bind SOCKET");
		goto DOWNLOAD_EXIT;
	}

	dp_db_open();

	// convert to request type, insert all to privates->requests
	// timeout of request thread will start these jobs by queue thread
	// load all list from (queue table)
	if (dp_db_crashed_list(privates->requests, DP_MAX_REQUEST) > 0) {
		int i = 0;
		for (i = 0; i < DP_MAX_REQUEST; i++) {
			if (!privates->requests[i].request)
				continue;
			dp_request *request = privates->requests[i].request;
			TRACE_DEBUG
				("ID [%d] state[%d]", request->id, request->state);

			// load to memory, Can be started automatically.
			if (request->state == DP_STATE_DOWNLOADING ||
				request->state == DP_STATE_CONNECTING) {
				request->state = DP_STATE_QUEUED;
				if (dp_db_set_column
						(request->id, DP_DB_TABLE_LOG, DP_DB_COL_STATE,
						DP_DB_COL_TYPE_INT, &request->state) < 0) {
					TRACE_ERROR("[CHECK SQL]");
				}
			}

			if (request->state == DP_STATE_QUEUED) {
				int auto_download = dp_db_get_int_column(request->id,
									DP_DB_TABLE_REQUEST_INFO,
									DP_DB_COL_AUTO_DOWNLOAD);
				if (auto_download == 1) {
					// auto retry... defaultly, show notification
					request->auto_notification = 1;
					request->start_time = (int)time(NULL);
					continue;
				}
				// do not retry this request
				request->state = DP_STATE_FAILED;
				request->error = DP_ERROR_SYSTEM_DOWN;
				if (dp_db_set_column
						(request->id, DP_DB_TABLE_LOG, DP_DB_COL_STATE,
						DP_DB_COL_TYPE_INT, &request->state) < 0) {
					TRACE_ERROR("[CHECK SQL]");
				}
				if (dp_db_set_column
						(request->id, DP_DB_TABLE_LOG,
						DP_DB_COL_ERRORCODE, DP_DB_COL_TYPE_INT,
						&request->error) < 0) {
					TRACE_ERROR("[CHECK SQL]");
				}
			}

			// if wanna restart, call continue before this line.
			// default. update state/error. move to history. unload memory
			// remove from memory
			dp_request_free(request);
			privates->requests[i].request = NULL;
		}
	} // query crashed_list

#if 0
	if (argc != 2 || memcmp(argv[1], "service", 7) != 0) {
		// in first launch in booting time, not request. terminate by self
		if (dp_get_request_count(privates->requests) <= 0) {
			TRACE_DEBUG("First Boot, No Request");
			goto DOWNLOAD_EXIT;
		}
	}
#endif

	dp_clear_downloadinginfo_notification();

	if (dp_init_agent() != DP_ERROR_NONE) {
		TRACE_ERROR("[CRITICAL] failed to init agent");
		goto DOWNLOAD_EXIT;
	}

	privates->connection = 0;
	privates->network_status = DP_NETWORK_TYPE_OFF;
	if (dp_network_connection_init(privates) < 0) {
		TRACE_DEBUG("use instant network check");
		privates->connection = 0;
	}

	// libsoup need mainloop.
	g_main_loop_handle = g_main_loop_new(NULL, 0);

	g_idle_add(__dp_idle_start_service, privates);

	sd_notify(0, "READY=1");

	g_main_loop_run(g_main_loop_handle);

DOWNLOAD_EXIT :

	TRACE_DEBUG("Download-Provider will be terminated.");
	if (vconf_ignore_key_changed(VCONFKEY_LANGSET, __lang_changed_cb) != 0)
		TRACE_ERROR("Fail to unset language changed vconf callback");

	dp_deinit_agent();

	if (privates != NULL) {

		if (privates->connection)
			dp_network_connection_destroy(privates->connection);

		if (privates->listen_fd >= 0)
			privates->listen_fd = -1;

		if (g_dp_thread_queue_manager_pid > 0 &&
				pthread_kill(g_dp_thread_queue_manager_pid, 0) != ESRCH) {
			//pthread_cancel(g_dp_thread_queue_manager_pid);
			//send signal to queue thread
			dp_thread_queue_manager_wake_up();
			int status;
			pthread_join(g_dp_thread_queue_manager_pid, (void **)&status);
			g_dp_thread_queue_manager_pid = 0;
		}
		dp_request_slots_free(privates->requests, DP_MAX_REQUEST);
		privates->requests = NULL;
		dp_client_group_slots_free(privates->groups, DP_MAX_GROUP);
		privates->groups = NULL;
		free(privates);
		privates = NULL;
	}
	dp_db_close();

	// delete pid file
	if (access(DP_LOCK_PID, F_OK) == 0) {
		close(lock_fd);
		unlink(DP_LOCK_PID);
	}
	exit(EXIT_SUCCESS);
}
