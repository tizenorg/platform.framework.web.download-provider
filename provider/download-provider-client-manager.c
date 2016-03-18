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
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h> 
#include <signal.h>

#include <aul.h>
#include <systemd/sd-daemon.h>
#include <glib-object.h>

#include <cynara-client.h>
#include <cynara-client-async.h>
#include <cynara-creds-socket.h>
#include <cynara-creds-dbus.h>

#include <download-provider.h>
#include <download-provider-log.h>
#include <download-provider-config.h>
#include <download-provider-pthread.h>
#include <download-provider-smack.h>
#include <download-provider-client.h>
#include <download-provider-notification.h>
#include <download-provider-notification-manager.h>
#include <download-provider-utils.h>
#include <download-provider-ipc.h>
#include <download-provider-notify.h>
#include <download-provider-db-defs.h>
#include <download-provider-db.h>
#include <download-provider-queue-manager.h>
#include <download-provider-client-manager.h>
#include <download-provider-plugin-download-agent.h>
#include <download-provider-network.h>

int g_dp_sock = -1;
dp_client_slots_fmt *g_dp_client_slots = NULL;
static void *g_db_handle = 0;
static pthread_mutex_t g_db_mutex = PTHREAD_MUTEX_INITIALIZER;

void dp_terminate(int signo)
{
	TRACE_DEBUG("Received SIGTERM:%d", signo);
	close(g_dp_sock);
	g_dp_sock = -1;
}

void dp_broadcast_signal()
{
	TRACE_INFO("broadcast");
	// announce to all thread for clients
	// signo 10 : ip changed
	if (g_dp_client_slots != NULL) {
		int i = 0;
		for (; i < DP_MAX_CLIENTS; i++) {
			if (g_dp_client_slots[i].thread > 0 &&
					pthread_kill(g_dp_client_slots[i].thread, 0) != ESRCH)
				pthread_kill(g_dp_client_slots[i].thread, SIGUSR1);
		}
	}

}

char *dp_db_get_client_smack_label(const char *pkgname)
{
	char *smack_label = NULL;
	unsigned length = 0;
	int errorcode = DP_ERROR_NONE;

	CLIENT_MUTEX_LOCK(&g_db_mutex);
	if (dp_db_get_client_property_string(g_db_handle, pkgname, DP_DB_COL_SMACK_LABEL, (unsigned char **)&smack_label, &length, &errorcode) < 0) {
		TRACE_SECURE_ERROR("failed to get smack label for %s", pkgname);
	}
	CLIENT_MUTEX_UNLOCK(&g_db_mutex);

	return smack_label;
}

static int __dp_db_open_client_manager()
{
	CLIENT_MUTEX_LOCK(&g_db_mutex);
	if (dp_db_open_client_manager(&g_db_handle) < 0) {
		TRACE_ERROR("[CRITICAL] can not open SQL");
		CLIENT_MUTEX_UNLOCK(&g_db_mutex);
		return -1;
	}
	CLIENT_MUTEX_UNLOCK(&g_db_mutex);
	return 0;
}

static void __dp_db_free_client_manager()
{
	CLIENT_MUTEX_LOCK(&g_db_mutex);
	if (g_db_handle != 0) {
		TRACE_SECURE_DEBUG("TRY to close [%s]", DP_DBFILE_CLIENTS);
		dp_db_close(g_db_handle);
		g_db_handle = 0;
	}
	CLIENT_MUTEX_UNLOCK(&g_db_mutex);
}

static int __dp_accept_socket_new()
{
	int fd_base, listen_fds = sd_listen_fds(1);
	TRACE_DEBUG("sd_listen_fds:%d", listen_fds);
	for (fd_base = 0 ; fd_base < listen_fds; fd_base++) {
		if (sd_is_socket_unix(fd_base + SD_LISTEN_FDS_START, SOCK_STREAM, 1, IPC_SOCKET, 0) >= 0) {
			TRACE_INFO("listen systemd socket:%d", fd_base + SD_LISTEN_FDS_START);
			return fd_base + SD_LISTEN_FDS_START;
		}
	}
	return -1;
}

int dp_client_slot_free(dp_client_slots_fmt *slot)
{
	if (slot->client.channel >= 0) {
		close(slot->client.channel);
		slot->client.channel = -1;
	}
	if (slot->client.dbhandle != 0) {
		dp_db_close(slot->client.dbhandle);
		slot->client.dbhandle = 0;
	}
	// free all requests
	// remove notify fifo
	if (slot->client.notify >= 0) {
		close(slot->client.notify);
		slot->client.notify = -1;
	}
	dp_notify_deinit(slot->credential.pid);
	// kill thread
	if (slot->thread != 0)
		pthread_cancel(slot->thread);
	slot->thread = 0;
	if (slot->pkgname != NULL) {
		TRACE_SECURE_DEBUG("TRY to close [%s]", slot->pkgname);
		free(slot->pkgname);
		slot->pkgname = NULL;
	}
	return 0;
}

// precondition : all slots are empty
static int __dp_manage_client_requests(dp_client_slots_fmt *clients)
{
	int errorcode = DP_ERROR_NONE;
	int i = 0;
	int slot_index = 0;

	dp_notification_manager_kill();
	dp_queue_manager_kill();

	// get all clients info from clients database.

	int *ids = (int *)calloc(DP_MAX_CLIENTS, sizeof(int));
	if (ids == NULL) {
		TRACE_ERROR("failed to allocate the clients");
		return -1;
	}
	// getting ids of clients
	int rows_count = dp_db_get_ids(g_db_handle, DP_TABLE_CLIENTS, NULL, ids, NULL, DP_MAX_CLIENTS, DP_DB_COL_ACCESS_TIME, "ASC", &errorcode);
	for (; i < rows_count; i++) {
		char *pkgname = NULL;
		unsigned length = 0;
		errorcode = DP_ERROR_NONE;
		if (dp_db_get_property_string(g_db_handle, ids[i], DP_TABLE_CLIENTS, DP_DB_COL_PACKAGE, (unsigned char **)&pkgname, &length, &errorcode) < 0) {
			TRACE_ERROR("failed to get pkgname for id:%d", ids[i]);
			continue;
		}

		if (pkgname != NULL) {
			if (dp_db_remove_database(pkgname, time(NULL), DP_CARE_CLIENT_INFO_PERIOD * 3600) == 0) { // old database
				// remove info from client database;
				if (dp_db_delete(g_db_handle, ids[i], DP_TABLE_CLIENTS, &errorcode) == 0) {
					TRACE_SECURE_ERROR("clear info for %s", pkgname);
					// remove database file
				}
				TRACE_SECURE_INFO("remove database for %s", pkgname);
				free(pkgname);
				continue;
			}

			dp_credential credential;
			credential.pid = 0;
			if (dp_db_get_property_int(g_db_handle, ids[i], DP_TABLE_CLIENTS, DP_DB_COL_UID, &credential.uid, &errorcode) < 0 ||
					dp_db_get_property_int(g_db_handle, ids[i], DP_TABLE_CLIENTS, DP_DB_COL_GID, &credential.gid, &errorcode) < 0) {
				TRACE_SECURE_ERROR("failed to get credential for %s", pkgname);
				free(pkgname);
				continue;
			}
			if (dp_mutex_init(&clients[slot_index].mutex, NULL) != 0) {
				TRACE_SECURE_ERROR("failed to initialize mutex for %s", pkgname);
				free(pkgname);
				continue;
			}
			// open database of a clients
			if (dp_db_open_client_v2(&clients[slot_index].client.dbhandle, pkgname) < 0) {
				TRACE_SECURE_ERROR("failed to open database for %s", pkgname);
				// remove this client from clients database
				if (dp_db_delete(g_db_handle, ids[i], DP_TABLE_CLIENTS, &errorcode) == 0) {
					TRACE_SECURE_ERROR("clear info for %s", pkgname);
					// remove database file
					if (dp_db_remove_database(pkgname, time(NULL), 0) == 0) {
						TRACE_SECURE_INFO("remove database for %s", pkgname);
					} else {
						TRACE_SECURE_ERROR("failed to remove database for %s", pkgname);
					}
				}
				free(pkgname);
				continue;
			}

			// get ids if state is QUEUED, CONNECTING or DOWNLOADING with auto_download
			int *request_ids = (int *)calloc(DP_MAX_REQUEST, sizeof(int));
			if (request_ids == NULL) {
				TRACE_SECURE_ERROR("failed to allocate the requests for %s", pkgname);
				free(pkgname);
				continue;
			}
			int request_count = dp_db_get_crashed_ids(clients[slot_index].client.dbhandle, DP_TABLE_LOGGING, request_ids, DP_MAX_REQUEST, &errorcode);
			TRACE_DEBUG("client: %s requests:%d", pkgname, request_count);
			int ids_i = 0;
			if (request_count > 0) {
				clients[slot_index].pkgname = pkgname;
				clients[slot_index].client.channel = -1;
				clients[slot_index].client.notify = -1;
				clients[slot_index].credential.pid = credential.pid;
				clients[slot_index].credential.uid = credential.uid;
				clients[slot_index].credential.gid = credential.gid;
				for (ids_i = 0; ids_i < request_count; ids_i++) {
					// loading requests from client's database... attach to client.requests
					dp_request_fmt *request = (dp_request_fmt *) calloc(1, sizeof(dp_request_fmt));
					if (request == NULL) {
						TRACE_ERROR("check memory download-id:%d", request_ids[ids_i]);
						break;
					}
					request->id = request_ids[ids_i];
					request->agent_id = -1;
					request->state = DP_STATE_QUEUED;
					request->error = DP_ERROR_NONE;
					if (dp_db_get_property_int(clients[slot_index].client.dbhandle, request->id, DP_TABLE_REQUEST, DP_DB_COL_NETWORK_TYPE, &request->network_type, &errorcode) < 0) {
						TRACE_ERROR("failed to get network type for id:%d", request->id);
						request->network_type = DP_NETWORK_WIFI;
					}
					request->access_time = (int)time(NULL);
					request->state_cb = 0;
					request->progress_cb = 0;
					if (dp_db_get_property_int(clients[slot_index].client.dbhandle, request->id, DP_TABLE_LOGGING, DP_DB_COL_STARTCOUNT, &request->startcount, &errorcode) < 0) {
						TRACE_ERROR("failed to get start count for id:%d", request->id);
						request->startcount = 0;
					}
					request->startcount++;
					request->noti_type = DP_NOTIFICATION_TYPE_NONE;
					if (dp_db_get_property_int(clients[slot_index].client.dbhandle, request->id, DP_TABLE_NOTIFICATION, DP_DB_COL_NOTI_TYPE, &request->noti_type, &errorcode) < 0) {
						TRACE_ERROR("failed to get notification type for id:%d", request->id);
					}
					if (request->noti_type == DP_NOTIFICATION_TYPE_NONE) {
						TRACE_INFO("enable notification for id:%d", request->id);
						request->noti_type = DP_NOTIFICATION_TYPE_COMPLETE_ONLY;
					}
					request->progress_lasttime = 0;
					request->received_size = 0; // ?
					request->content_type = DP_CONTENT_UNKNOWN;
					request->file_size = 0; // ?
					if (dp_db_get_property_int(clients[slot_index].client.dbhandle, request->id, DP_TABLE_NOTIFICATION, DP_DB_COL_NOTI_PRIV_ID, &request->noti_priv_id, &errorcode) < 0) {
						TRACE_ERROR("failed to get notification noti_priv_id for id:%d", request->id);
						request->noti_priv_id = -1;
					}

					dp_request_create(&clients[slot_index].client, request);

					if (dp_db_update_logging(clients[slot_index].client.dbhandle, request->id, DP_STATE_QUEUED, DP_ERROR_NONE, &errorcode) < 0) {
						TRACE_ERROR("update log download-id:%d", request->id);
						errorcode = DP_ERROR_DISK_BUSY;
						break;
					}
					if (dp_queue_manager_push_queue((void *)&clients[slot_index], (void *)request) < 0) {
						errorcode = DP_ERROR_QUEUE_FULL;
						TRACE_INFO("failed to push to queue for id:%d", request->id);
						dp_request_destroy(&(clients[slot_index].client), NULL, request);
						break;
					}
					// notification
					if (dp_notification_manager_push_notification((void *)&clients[slot_index], (void *)request, DP_NOTIFICATION_ONGOING) < 0) {
						TRACE_ERROR("failed to register notification for id:%d", request->id);
					}

				}

				slot_index++;

			} else {
				free(pkgname);
			}
			free(request_ids);
		}
	}
	free(ids);
	TRACE_DEBUG("slot_index:%d", slot_index);
	if (slot_index > 0)
		dp_queue_manager_wake_up();
	return slot_index;
}

static int __dp_client_run(int clientfd, dp_client_slots_fmt *slot,
	dp_credential credential)
{
	int errorcode = DP_ERROR_NONE;
	// make notify fifo
	slot->client.notify = dp_notify_init(credential.pid);
	if (slot->client.notify < 0) {
		TRACE_STRERROR("failed to open fifo slot:%d", clientfd);
		errorcode = DP_ERROR_IO_ERROR;
	} else {
		char *smack_label = NULL;
		if (dp_smack_is_mounted() == 1) {
			smack_label = dp_smack_get_label_from_socket(clientfd);
			if (smack_label == NULL) {
				TRACE_SECURE_ERROR("smack_new_label_from_socket");
			}
		}
		// save client info to database
		CLIENT_MUTEX_LOCK(&g_db_mutex);
		if (dp_db_update_client_info(g_db_handle,
				slot->pkgname, smack_label,
				credential.uid, credential.gid, &errorcode) < 0) {
			TRACE_ERROR("check error:%s", dp_print_errorcode(errorcode));
		}
		CLIENT_MUTEX_UNLOCK(&g_db_mutex);
		free(smack_label);
	}
	if (errorcode == DP_ERROR_NONE) {

		// create a thread for client
		if (pthread_create(&slot->thread, NULL,
				dp_client_request_thread, (void *)slot) != 0) {
			TRACE_ERROR("failed to create client thread slot:%d", clientfd);
			errorcode = DP_ERROR_OUT_OF_MEMORY;
			slot->thread = 0;
			dp_client_slot_free(slot); // => make pkgname as NULL
		} else {
			pthread_detach(slot->thread);
			TRACE_SECURE_INFO("accept client[%s] pid:%d sock:%d",
				slot->pkgname, credential.pid, clientfd);
			slot->client.channel = clientfd;
			slot->credential.pid = credential.pid;
			slot->credential.uid = credential.uid;
			slot->credential.gid = credential.gid;
		}
	}
	return errorcode;
}


static int __dp_client_new(int clientfd, dp_client_slots_fmt *clients,
	dp_credential credential)
{
	// search in clients list.
	// if same pkgname. update it.
	// search same pkg or pid in clients
	int errorcode = DP_ERROR_NONE;
	int i = 0;
	int pkg_len = 0;
	char *pkgname[256] = { 0 };

	// getting the package name via pid
	if (aul_app_get_appid_bypid_for_uid(credential.pid, pkgname, 256, credential.uid) != AUL_R_OK)
		TRACE_ERROR("[CRITICAL] aul_app_get_appid_bypid_for_uid");
/*
	//// TEST CODE ... to allow sample client ( no package name ).
	if (pkgname == NULL) {
		//pkgname = dp_strdup("unknown_app");
		char *temp_pkgname = (char *)calloc(41, sizeof(char));
		if (temp_pkgname == NULL ||
				snprintf(temp_pkgname, 41,"unknown_app_%d", credential.pid) < 0) {
			pkgname = dp_strdup("unknown_app");
		} else {
			pkgname = temp_pkgname;
		}
	}

	if (pkgname == NULL) {
		TRACE_ERROR("[CRITICAL] app_manager_get_app_id");
		return DP_ERROR_INVALID_PARAMETER;
	}
*/
	if ((pkg_len = strlen(pkgname)) <= 0) {
		TRACE_ERROR("[CRITICAL] pkgname:%s", pkgname);
		free(pkgname);
		return DP_ERROR_INVALID_PARAMETER;
	}

#ifdef SUPPORT_SECURITY_PRIVILEGE_OLD
	TRACE_DEBUG("SUPPORT_SECURITY_PRIVILEGE_OLD");
	int result = security_server_check_privilege_by_sockfd(clientfd, SECURITY_PRIVILEGE_INTERNET, "w");
	if (result != SECURITY_SERVER_API_SUCCESS) {
		TRACE_ERROR("check privilege permission:%d", result);
		return DP_ERROR_PERMISSION_DENIED;
	}
#endif

#if 1
	TRACE_DEBUG("SUPPORT_SECURITY_PRIVILEGE");
	// Cynara structure init
	int ret;
	cynara *p_cynara;
	//cynara_configuration conf;
	ret = cynara_initialize(&p_cynara, NULL);
	if(ret != CYNARA_API_SUCCESS) { /* error */ }

	// Get client peer credential
	char *clientSmack;
	ret = cynara_creds_socket_get_client(clientfd, CLIENT_METHOD_SMACK, &clientSmack);
	// In case of D-bus peer credential??
	// ret = cynara_creds_dbus_get_client(DBusConnection *connection, const char *uniqueName,CLIENT_METHOD_SMACK, &clientSmack);
	if(ret != CYNARA_API_SUCCESS) { /* error */ }

	char *uid;
	ret = cynara_creds_socket_get_user(clientfd, USER_METHOD_UID, &uid);
	// In case of D-bus peer credential??
	// ret = cynara_creds_dbus_get_client(DBusConnection *connection, const char *uniqueName,CLIENT_METHOD_SMACK, &clientSmack);
	if (ret != CYNARA_API_SUCCESS) { /* error */ }

	/* Concept of session is service-specific.
	  * Might be empty string if service does not have such concept
	  */
	char *client_session="";

	// Cynara check

	ret = cynara_check(p_cynara, clientSmack, client_session, uid, "http://tizen.org/privilege/download");

	if(ret == CYNARA_API_ACCESS_ALLOWED) {
		TRACE_DEBUG("CYNARA_API_ACCESS_ALLOWED");
	} else {
		TRACE_DEBUG("DP_ERROR_PERMISSION_DENIED");
		return DP_ERROR_PERMISSION_DENIED;
	}

	// Cleanup of cynara structure
	if(clientSmack) {
		//free(clientSmack);
	}

	if(client_session) {
		//free(client_session);
	}

	if(uid) {
		//free(uid);
	}

	cynara_finish(p_cynara);

#endif

	// EINVAL: empty slot
	// EBUSY : occupied slot
	// locked & thread == 0 : downloading without client <= check target
	// thread == 0, requests == NULL : clear target

	// Have this client ever been connected before ?
	for (i = 0; i < DP_MAX_CLIENTS; i++) {

		int locked = CLIENT_MUTEX_TRYLOCK(&clients[i].mutex);
		if (locked != 0) { // empty or used by other thread. it would be same client, but it's busy
			continue;
		}
		TRACE_DEBUG("locked slot:%d", i);
		if (locked == 0 && clients[i].thread == 0) { // this slot has run without the client
			if (clients[i].pkgname != NULL) {
				// check package name.
				TRACE_DEBUG("check client[%s] slot:%d", clients[i].pkgname, i);
				int cname_len = strlen(clients[i].pkgname);
				if (pkg_len == cname_len &&
						strncmp(clients[i].pkgname, pkgname, pkg_len) == 0) {
					TRACE_SECURE_INFO("update client[%s] slot:%d pid:%d sock:%d",
						pkgname, i, credential.pid, clientfd);
					if (clients[i].client.channel >= 0 &&
							clients[i].client.channel != clientfd) {
						dp_ipc_socket_free(clients[i].client.channel);
						if (clients[i].client.notify >= 0)
							close(clients[i].client.notify);
						dp_notify_deinit(clients[i].credential.pid);
					}
					errorcode = __dp_client_run(clientfd, &clients[i], credential);
					CLIENT_MUTEX_UNLOCK(&clients[i].mutex);
					if (errorcode != DP_ERROR_NONE)
						dp_mutex_destroy(&clients[i].mutex);
					free(pkgname);
					return errorcode;
				}
			}
			if (clients[i].client.requests == NULL) { // clear
				dp_client_slot_free(&clients[i]);
				dp_mutex_destroy(&clients[i].mutex);
				continue;
			}
		}
		CLIENT_MUTEX_UNLOCK(&clients[i].mutex);
	}

	TRACE_DEBUG("search empty client[%s] slot:%d", pkgname, i);
	// search empty slot
	for (i = 0; i < DP_MAX_CLIENTS; i++) {
		int locked = CLIENT_MUTEX_TRYLOCK(&clients[i].mutex);
		if (locked == EINVAL) {
			if (dp_mutex_init(&clients[i].mutex, NULL) == 0) {
				CLIENT_MUTEX_LOCK(&clients[i].mutex);
				TRACE_DEBUG("found empty client[%s] slot:%d", pkgname, i);
				clients[i].pkgname = pkgname;
				clients[i].client.dbhandle = 0;
				clients[i].client.requests = NULL;
				errorcode = __dp_client_run(clientfd, &clients[i], credential);
				CLIENT_MUTEX_UNLOCK(&clients[i].mutex);
				if (errorcode != DP_ERROR_NONE)
					dp_mutex_destroy(&clients[i].mutex);
				return errorcode;
			}
		}
		if (locked == 0)
			CLIENT_MUTEX_UNLOCK(&clients[i].mutex);
	}

	TRACE_SECURE_INFO("busy client[%s] pid:%d sock:%d", pkgname,
		credential.pid, clientfd);
	free(pkgname);
	return DP_ERROR_TOO_MANY_DOWNLOADS;
}

void *dp_client_manager(void *arg)
{
	fd_set rset, eset, listen_fdset, except_fdset;
	struct timeval timeout; // for timeout of select
	socklen_t clientlen;
	struct sockaddr_un clientaddr;
	dp_credential credential;
	unsigned i;
	int errorcode = DP_ERROR_NONE;
	GMainLoop *event_loop = (GMainLoop *)arg;

	g_dp_sock = __dp_accept_socket_new();
	if (g_dp_sock < 0) {
		TRACE_STRERROR("failed to open listen socket");
		g_main_loop_quit(event_loop);
		return 0;
	}

	if (signal(SIGTERM, dp_terminate) == SIG_ERR ||
			signal(SIGPIPE, SIG_IGN) == SIG_ERR ||
			signal(SIGINT, dp_terminate) == SIG_ERR) {
		TRACE_ERROR("failed to register signal callback");
		g_main_loop_quit(event_loop);
		return 0;
	}

	dp_notification_clear_ongoings();

#ifdef PROVIDER_DIR
	dp_rebuild_dir(PROVIDER_DIR, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
#endif
#ifdef DATABASE_DIR
	dp_rebuild_dir(DATABASE_DIR, S_IRWXU);
#endif
#ifdef DATABASE_CLIENT_DIR
	dp_rebuild_dir(DATABASE_CLIENT_DIR, S_IRWXU);
#endif
#ifdef NOTIFY_DIR
	dp_rebuild_dir(NOTIFY_DIR, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
#endif

	if (__dp_db_open_client_manager() < 0) {
		TRACE_STRERROR("failed to open database for client-manager");
		g_main_loop_quit(event_loop);
		return 0;
	}

	dp_client_slots_fmt *clients =
		(dp_client_slots_fmt *)calloc(DP_MAX_CLIENTS,
			sizeof(dp_client_slots_fmt));
	if (clients == NULL) {
		TRACE_ERROR("failed to allocate client slots");
		g_main_loop_quit(event_loop);
		return 0;
	}
	g_dp_client_slots = clients;
	for (i = 0; i < DP_MAX_CLIENTS; i++) {
		dp_mutex_destroy(&clients[i].mutex); // clear mutex init
	}

	int maxfd = g_dp_sock;
	FD_ZERO(&listen_fdset);
	FD_ZERO(&except_fdset);
	FD_SET(g_dp_sock, &listen_fdset);
	FD_SET(g_dp_sock, &except_fdset);

	while (g_dp_sock >= 0) {

		int clientfd = -1;

		// initialize timeout structure for calling timeout exactly
		memset(&timeout, 0x00, sizeof(struct timeval));
		timeout.tv_sec = DP_CARE_CLIENT_MANAGER_INTERVAL;
		credential.pid = -1;
		credential.uid = -1;
		credential.gid = -1;

		rset = listen_fdset;
		eset = except_fdset;

		if (select((maxfd + 1), &rset, 0, &eset, &timeout) < 0) {
			TRACE_STRERROR("interrupted by terminating");
			break;
		}

		if (g_dp_sock < 0) {
			TRACE_DEBUG("queue-manager is closed by other thread");
			break;
		}

		if (FD_ISSET(g_dp_sock, &eset) > 0) {
			TRACE_STRERROR("exception of socket");
			break;
		} else if (FD_ISSET(g_dp_sock, &rset) > 0) {

			// Anyway accept client.
			clientlen = sizeof(clientaddr);
			clientfd = accept(g_dp_sock, (struct sockaddr *)&clientaddr,
					&clientlen);
			if (clientfd < 0) {
				TRACE_STRERROR("too many client ? accept failure");
				// provider need the time of refresh.
				break;
			}

			// blocking & timeout to prevent the lockup by client.
			struct timeval tv_timeo = {1, 500000}; // 1.5 sec
			if (setsockopt(clientfd, SOL_SOCKET, SO_RCVTIMEO, &tv_timeo,
					sizeof(tv_timeo)) < 0) {
				TRACE_ERROR("failed to set timeout in blocking socket");
				close(clientfd);
				continue;
			}

			dp_ipc_fmt ipc_info;
			memset(&ipc_info, 0x00, sizeof(dp_ipc_fmt));
			if (read(clientfd, &ipc_info, sizeof(dp_ipc_fmt)) <= 0 || 
					ipc_info.section == DP_SEC_NONE ||
					ipc_info.property != DP_PROP_NONE ||
					ipc_info.id != -1 ||
					ipc_info.size != 0) {
				TRACE_ERROR("peer terminate ? ignore this connection");
				close(clientfd);
				continue;
			}

#ifdef SO_PEERCRED // getting the info of client
			socklen_t cr_len = sizeof(credential);
			if (getsockopt(clientfd, SOL_SOCKET, SO_PEERCRED,
					&credential, &cr_len) < 0) {
				TRACE_ERROR("failed to cred from sock:%d", clientfd);
				close(clientfd);
				continue;
			}
#else // In case of not supported SO_PEERCRED
			if (read(clientfd, &credential, sizeof(dp_credential)) <= 0) {
				TRACE_ERROR("failed to cred from client:%d", clientfd);
				close(clientfd);
				continue;
			}
#endif

			CLIENT_MUTEX_LOCK(&g_db_mutex);
			if (dp_db_check_connection(g_db_handle) < 0) {
				TRACE_ERROR("check database, provider can't work anymore");
				CLIENT_MUTEX_UNLOCK(&g_db_mutex);
				close(clientfd);
				break;
			}
			CLIENT_MUTEX_UNLOCK(&g_db_mutex);

			if (ipc_info.section == DP_SEC_INIT) {

				// new client
				errorcode = __dp_client_new(clientfd, clients, credential);

			} else {
				errorcode = DP_ERROR_INVALID_PARAMETER;
			}
			if (dp_ipc_query(clientfd, -1, DP_SEC_INIT, DP_PROP_NONE, errorcode, 0) < 0) {
				TRACE_ERROR("check ipc sock:%d", clientfd);
			}
			if (errorcode == DP_ERROR_NONE) {
				// write client info into database
				
			} else {
				TRACE_ERROR("sock:%d id:%d section:%s property:%s errorcode:%s size:%d",
					clientfd, ipc_info.id,
					dp_print_section(ipc_info.section),
					dp_print_property(ipc_info.property),
					dp_print_errorcode(ipc_info.errorcode),
					ipc_info.size);
				close(clientfd); // ban this client
			}

		} else {

			// take care zombie client, slots
			unsigned connected_clients = 0;
			int i = 0;
			for (; i < DP_MAX_CLIENTS; i++) {

				int locked = CLIENT_MUTEX_TRYLOCK(&clients[i].mutex);
				if (locked == EINVAL) { // not initialized
					continue;
				} else if (locked == EBUSY) { // already locked
					connected_clients++;
					continue;
				}

				if (locked == 0) { // locked

					// if no client thread, requests should be checked here
					// if no queued, connecting or downloading, close the slot
					if (clients[i].pkgname != NULL) {
						if (clients[i].thread == 0) {
							dp_client_clear_requests(&clients[i]);
							if (clients[i].client.requests == NULL) {
								dp_client_slot_free(&clients[i]);
								CLIENT_MUTEX_UNLOCK(&clients[i].mutex);
								dp_mutex_destroy(&clients[i].mutex);
								continue;
							}
						}
						connected_clients++;
					}
					CLIENT_MUTEX_UNLOCK(&clients[i].mutex);
				}
			}
			TRACE_DEBUG("%d clients are active now", connected_clients);
			// terminating download-provider if no clients.
			if (connected_clients == 0) {
				if (__dp_manage_client_requests(clients) <= 0) // if no crashed job
					break;
			} else {
				dp_queue_manager_wake_up();
				dp_notification_manager_wake_up();
			}
		}

	}
	if (g_dp_sock >= 0)
		close(g_dp_sock);
	g_dp_sock = -1;

	dp_queue_manager_kill();
	dp_notification_clear_ongoings();
	dp_notification_manager_kill();

	__dp_db_free_client_manager();

	// kill other clients
	TRACE_DEBUG("try to deallocate the resources for all clients");
	for (i = 0; i < DP_MAX_CLIENTS; i++) {
		int locked = CLIENT_MUTEX_TRYLOCK(&clients[i].mutex);
		if (locked == EBUSY) { // already locked
			CLIENT_MUTEX_LOCK(&clients[i].mutex);
		} else if (locked == EINVAL) { // not initialized, empty slot
			continue;
		}
		dp_client_slot_free(&clients[i]);
		CLIENT_MUTEX_UNLOCK(&clients[i].mutex);
		dp_mutex_destroy(&clients[i].mutex);
	}
	free(clients);
	// free all resources

	TRACE_INFO("client-manager's working is done");
	g_main_loop_quit(event_loop);
	return 0;
}
