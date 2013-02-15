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

#include <time.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

#include <signal.h>

#include <app_manager.h>

#include "download-provider.h"
#include "download-provider-log.h"
#include "download-provider-config.h"
#include "download-provider-slots.h"
#include "download-provider-socket.h"
#include "download-provider-pthread.h"
#include "download-provider-db.h"
#include "download-provider-queue.h"
#include "download-provider-request.h"
#include "download-provider-da-interface.h"

void dp_terminate(int signo);

static char *__print_command(dp_command_type cmd)
{
	switch(cmd)
	{
		case DP_CMD_CREATE :
			return "CREATE";
		case DP_CMD_START :
			return "START";
		case DP_CMD_PAUSE :
			return "PAUSE";
		case DP_CMD_CANCEL :
			return "CANCEL";
		case DP_CMD_DESTROY :
			return "DESTROY";
		case DP_CMD_FREE :
			return "FREE";
		case DP_CMD_ECHO :
			return "ECHO";
		case DP_CMD_SET_URL :
			return "SET_URL";
		case DP_CMD_SET_DESTINATION :
			return "SET_DESTINATION";
		case DP_CMD_SET_FILENAME :
			return "SET_FILENAME";
		case DP_CMD_SET_NOTIFICATION :
			return "SET_NOTIFICATION";
		case DP_CMD_SET_STATE_CALLBACK :
			return "SET_STATE_CALLBACK";
		case DP_CMD_SET_PROGRESS_CALLBACK :
			return "SET_PROGRESS_CALLBACK";
		case DP_CMD_SET_AUTO_DOWNLOAD :
			return "SET_AUTO_DOWNLOAD";
		case DP_CMD_SET_NETWORK_TYPE :
			return "SET_NETWORK_TYPE";
		case DP_CMD_SET_HTTP_HEADER :
			return "SET_HTTP_HEADER";
		case DP_CMD_SET_EXTRA_PARAM :
			return "SET_EXTRA_PARAM";
		case DP_CMD_DEL_HTTP_HEADER :
			return "DEL_HTTP_HEADER";
		case DP_CMD_GET_HTTP_HEADER :
			return "GET_HTTP_HEADER";
		case DP_CMD_GET_URL :
			return "GET_URL";
		case DP_CMD_GET_DESTINATION :
			return "GET_DESTINATION";
		case DP_CMD_GET_FILENAME :
			return "GET_FILENAME";
		case DP_CMD_GET_NOTIFICATION :
			return "GET_NOTIFICATION";
		case DP_CMD_GET_STATE_CALLBACK :
			return "GET_STATE_CALLBACK";
		case DP_CMD_GET_PROGRESS_CALLBACK :
			return "GET_PROGRESS_CALLBACK";
		case DP_CMD_GET_HTTP_HEADERS :
			return "GET_HTTP_HEADERS";
		case DP_CMD_GET_EXTRA_PARAM :
			return "GET_EXTRA_PARAM";
		case DP_CMD_GET_AUTO_DOWNLOAD :
			return "GET_AUTO_DOWNLOAD";
		case DP_CMD_GET_NETWORK_TYPE :
			return "GET_NETWORK_TYPE";
		case DP_CMD_GET_SAVED_PATH :
			return "GET_SAVED_PATH";
		case DP_CMD_GET_TEMP_SAVED_PATH :
			return "GET_TEMP_SAVED_PATH";
		case DP_CMD_GET_MIME_TYPE :
			return "GET_MIME_TYPE";
		case DP_CMD_GET_RECEIVED_SIZE :
			return "GET_RECEIVED_SIZE";
		case DP_CMD_GET_TOTAL_FILE_SIZE :
			return "GET_TOTAL_FILE_SIZE";
		case DP_CMD_GET_CONTENT_NAME :
			return "GET_CONTENT_NAME";
		case DP_CMD_GET_HTTP_STATUS :
			return "GET_HTTP_STATUS";
		case DP_CMD_GET_ETAG :
			return "DP_CMD_GET_ETAG";
		case DP_CMD_GET_STATE :
			return "GET_STATE";
		case DP_CMD_GET_ERROR :
			return "ERROR";
		case DP_CMD_SET_COMMAND_SOCKET :
			return "SET_COMMAND_SOCKET";
		case DP_CMD_SET_EVENT_SOCKET :
			return "SET_EVENT_SOCKET";
		default :
			break;
	}
	return "UNKNOWN COMMAND";
}

/* compare two string */
static int __cmp_string(char *s1, char *s2)
{
	size_t s1_len = 0;
	size_t s2_len = 0;

	if (!s1 || !s2) {
		TRACE_ERROR("[CHECK PARAM]");
		return -1;
	}

	s1_len = strlen(s1);
	if (s1_len <= 0) {
		TRACE_ERROR("[CHECK PARAM] len[%d]", s1_len);
		return -1;
	}

	s2_len = strlen(s2);
	if (s2_len <= 0) {
		TRACE_ERROR("[CHECK PARAM] len[%d]", s2_len);
		return -1;
	}

	if (s1_len != s2_len) {
		TRACE_ERROR("[DIFF] len[%d:%d]", s1_len, s2_len);
		return -1;
	}

	if (strncmp(s1, s2, s1_len) != 0) {
		TRACE_ERROR("[DIFF] cmp[%s:%s]", s1, s2);
		return -1;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
/// @brief	return index of empty slot
static int __get_empty_request_index(dp_request_slots *slots)
{
	int i = 0;

	if (!slots)
		return -1;

	for (i = 0; i < DP_MAX_REQUEST; i++)
		if (!slots[i].request)
			return i;
	return -1;
}

//////////////////////////////////////////////////////////////////////////
/// @brief	return index of slot having same ID
/// @param	ID want to search
static int __get_same_request_index(dp_request_slots *slots, int id)
{
	int i = 0;

	if (!slots || id < 0)
		return -1;

	for (i = 0; i < DP_MAX_REQUEST; i++) {
		if (slots[i].request) {
			if (slots[i].request->id == id) {
				return i;
			}
		}
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////
/// @brief	return string via IPC
static void __send_return_string(int fd, dp_error_type errcode, char* str)
{
	if (fd < 0 || !str)
		return ;
	dp_ipc_send_errorcode(fd, errcode);
	dp_ipc_send_string(fd, str);
}

//////////////////////////////////////////////////////////////////////////
/// @brief	return custom value via IPC
static void __send_return_custom_type(int fd, dp_error_type errcode, void *value, size_t type_size)
{
	dp_ipc_send_errorcode(fd, errcode);
	dp_ipc_send_custom_type(fd, value, type_size);
}

// in url-download, make 3 connection before send CREATE command.
// after accepting, fill info to pacakgelist.
// 3 socket per 1 package ( info/request/progress )
void *dp_thread_requests_manager(void *arg)
{
	fd_set rset, eset, listen_fdset, except_fdset;
	struct timeval timeout; // for timeout of select
	long flexible_timeout = DP_CARE_CLIENT_MAX_INTERVAL;
	int listenfd, clientfd, maxfd;
	socklen_t clientlen;
	struct sockaddr_un clientaddr;
	dp_credential credential;
	unsigned i, is_timeout;
	int prev_timeout = 0;
	dp_error_type errorcode = DP_ERROR_NONE;

	dp_privates *privates = (dp_privates*)arg;
	if (!privates || !privates->groups) {
		TRACE_ERROR("[CRITICAL] Invalid Address");
		dp_terminate(SIGTERM);
		pthread_exit(NULL);
		return 0;
	}

	listenfd = privates->listen_fd;
	maxfd = listenfd;

	TRACE_INFO("Ready to listen [%d][%s]", listenfd, DP_IPC);

	FD_ZERO(&listen_fdset);
	FD_ZERO(&except_fdset);
	FD_SET(listenfd, &listen_fdset);
	FD_SET(listenfd, &except_fdset);

	while (privates && privates->listen_fd) {

		// select with timeout
		// initialize timeout structure for calling timeout exactly
		memset(&timeout, 0x00, sizeof(struct timeval));
		timeout.tv_sec = flexible_timeout;
		clientfd = -1;
		credential.pid = -1;
		credential.uid = -1;
		credential.gid = -1;
		is_timeout = 1;

		rset = listen_fdset;
		eset = except_fdset;

		if (select((maxfd + 1), &rset, 0, &eset, &timeout) < 0) {
			TRACE_STRERROR("[CRITICAL] select");
			break;
		}

		if (!privates) {
			TRACE_INFO("Terminate Thread");
			break;
		}

		if (FD_ISSET(listenfd, &eset) > 0) {
			TRACE_ERROR("[CRITICAL] listenfd Exception of socket");
			break;
		} else if (FD_ISSET(listenfd, &rset) > 0) {
			// new client(package) called url_download_create API.
			// update g_dp_request_max_fd & g_dp_info_max_fd
			// add to socket to g_dp_request_rset & g_dp_info_rset

			is_timeout = 0;

			// Anyway accept client.
			clientlen = sizeof(clientaddr);
			clientfd = accept(listenfd,
							(struct sockaddr*)&clientaddr,
							&clientlen);

			TRACE_INFO("[New Connection]");

			if (clientfd < 0) {
				TRACE_ERROR("[CRITICAL] accept provider was crashed ?");
				// provider need the time of refresh.
				break;
			}

			dp_command_type connect_cmd = DP_CMD_NONE;
			if (dp_ipc_read_custom_type(clientfd,
					&connect_cmd, sizeof(dp_command_type)) < 0) {
				TRACE_ERROR("[CRITICAL] CAPI not support CONNECT CMD");
				close(clientfd);
				continue;
			}
			if (connect_cmd <= 0) {
				TRACE_ERROR("[CRITICAL] peer terminate ?");
				close(clientfd);
				continue;
			}
			if (connect_cmd != DP_CMD_SET_COMMAND_SOCKET
				&& connect_cmd != DP_CMD_SET_EVENT_SOCKET) {
				TRACE_ERROR("[CRITICAL] Bad access, ignore this client");
				close(clientfd);
				continue;
			}

			#ifdef SO_PEERCRED
			// getting the info of client
			socklen_t cr_len = sizeof(credential);
			if (getsockopt(clientfd, SOL_SOCKET, SO_PEERCRED,
				&credential, &cr_len) == 0) {
				TRACE_INFO
					("credential : pid=%d, uid=%d, gid=%d",
					credential.pid, credential.uid, credential.gid);
			}
			#else // In case of not supported SO_PEERCRED
			int client_pid = 0;
			if (dp_ipc_read_custom_type(clientfd,
					&client_pid, sizeof(int)) < 0) {
				TRACE_ERROR("[CRITICAL] not support SO_PEERCRED");
				close(clientfd);
				continue;
			}
			if (client_pid <= 0) {
				TRACE_ERROR("[CRITICAL] not support SO_PEERCRED");
				close(clientfd);
				continue;
			}
			credential.pid = client_pid;
			credential.uid = 5000;
			credential.gid = 5000;
			#endif

			struct timeval tv_timeo; // 2.5 sec
			tv_timeo.tv_sec = 2;
			tv_timeo.tv_usec = 500000;
			if (setsockopt(clientfd, SOL_SOCKET, SO_SNDTIMEO, &tv_timeo,
				sizeof( tv_timeo ) ) < 0) {
				TRACE_STRERROR("[CRITICAL] setsockopt SO_SNDTIMEO");
				close(clientfd);
				continue;
			}
			if (setsockopt(clientfd, SOL_SOCKET, SO_RCVTIMEO, &tv_timeo,
				sizeof( tv_timeo ) ) < 0) {
				TRACE_STRERROR("[CRITICAL] setsockopt SO_SNDTIMEO");
				close(clientfd);
				continue;
			}

			if (connect_cmd == DP_CMD_SET_COMMAND_SOCKET) {

				// search in groups.
				// if same group. update it.
				// search same pkg or pid in groups
				int group_index = -1;
				int pkgname_length = 0;
				char *client_pkgname = NULL;

				// getting the package name via pid
				int errcode =
				app_manager_get_package(credential.pid, &client_pkgname);
				if (errcode == APP_MANAGER_ERROR_NONE && client_pkgname
					&& strlen(client_pkgname) < DP_MAX_STR_LEN) {
					TRACE_INFO("package : %s", client_pkgname);
				} else
					TRACE_ERROR("[CRITICAL] app_manager_get_package");

				if (client_pkgname
					&& (pkgname_length = strlen(client_pkgname)) > 1) {
					for (i = 0; i < DP_MAX_GROUP; i++) {
						if (privates->groups[i].group) {
							if (privates->groups[i].group->cmd_socket
								<= 0
								|| !privates->groups[i].group->pkgname) {
								dp_client_group_free
									(privates->groups[i].group);
								privates->groups[i].group = NULL;
								continue;
							}
							if (strlen
									(privates->groups[i].group->pkgname)
									== pkgname_length
								&& strncmp
									(privates->groups[i].group->pkgname,
									client_pkgname, pkgname_length)
									== 0 ) {
								// Found Same Group
								TRACE_INFO
								("UPDATE Group : I %d [%s] PID [%d] cmd_socket[%d]",
								i, client_pkgname, credential.pid, clientfd);
								if (privates->groups[i].group->cmd_socket
									> 0
									&& privates->groups[i].group->cmd_socket
									!= clientfd) {
									FD_CLR
									(privates->groups[i].group->cmd_socket,
									&listen_fdset);
									dp_socket_free
									(privates->groups[i].group->cmd_socket);
								}
								privates->groups[i].group->cmd_socket = clientfd;
								FD_SET(privates->groups[i].group->cmd_socket, &listen_fdset);
								if (privates->groups[i].group->cmd_socket > maxfd)
								maxfd = privates->groups[i].group->cmd_socket;
								group_index = i;
								break;
							}
						}
					}
				}
				if (group_index == -1) { // search emtpy slot in groups
					// search empty slot in groups
					for (i = 0; i < DP_MAX_GROUP; i++)
						if (!privates->groups[i].group)
							break;
					if (i >= DP_MAX_GROUP) {
						TRACE_ERROR("[CRITICAL] No space in groups");
						close(clientfd); // how to deal in url-download ?
						if (client_pkgname)
							free(client_pkgname);
						continue;
					}

					//// TEST CODE ... to allow sample client ( no package name ).
					if (!client_pkgname) {
						client_pkgname = dp_strdup("unknown_app");
						TRACE_INFO("package : %s", client_pkgname);
					}

					TRACE_INFO
					("New Group : GI %d [%s] PID [%d] cmd_socket[%d]",
					i, client_pkgname, credential.pid, clientfd);

					// allocation
					privates->groups[i].group =
						(dp_client_group *) calloc(1,
							sizeof(dp_client_group));
					if (!privates->groups[i].group) {
						TRACE_ERROR
							("[CRITICAL] calloc, ignore this client");
						close(clientfd);
						if (client_pkgname)
							free(client_pkgname);
						continue;
					}
					// fill info
					privates->groups[i].group->cmd_socket = clientfd;
					privates->groups[i].group->event_socket = -1;
					privates->groups[i].group->queued_count = 0;
					privates->groups[i].group->pkgname =
						dp_strdup(client_pkgname);
					privates->groups[i].group->credential.pid =
						credential.pid;
					privates->groups[i].group->credential.uid =
						credential.uid;
					privates->groups[i].group->credential.gid =
						credential.gid;
					FD_SET(privates->groups[i].group->cmd_socket,
						&listen_fdset);
					if (privates->groups[i].group->cmd_socket > maxfd)
						maxfd = privates->groups[i].group->cmd_socket;
					TRACE_INFO
						("Group : GI [%d] [%s] S [%d] Max [%d]",
						i, client_pkgname,
						privates->groups[i].group->cmd_socket, maxfd);
				}
				if (client_pkgname)
					free(client_pkgname);

			} else if (connect_cmd == DP_CMD_SET_EVENT_SOCKET) {

				// search same pid in groups. have same life-cycle with cmd_socket
				TRACE_INFO
					("Group event IPC: PID [%d] cmd_socket[%d]",
					credential.pid, clientfd);
				// search same pkg or pid in groups
				for (i = 0; i < DP_MAX_GROUP; i++) {
					if (privates->groups[i].group) {
						dp_client_group *group =
							privates->groups[i].group;
						if (group->credential.pid == credential.pid) {
							TRACE_INFO
							("Found Group : I %d [%s] PID [%d] \
							socket (%d/%d)", i,
							group->pkgname, credential.pid,
							group->cmd_socket, group->event_socket);
							if (group->event_socket > 0
								&& group->event_socket != clientfd)
								dp_socket_free(group->event_socket);
							group->event_socket = clientfd;
							TRACE_INFO
							("Found Group : I %d PID [%d] \
							event_socket[%d]",
							i, credential.pid, clientfd);
							break;
						}
					}
				}
				if (i >= DP_MAX_GROUP) {
					TRACE_ERROR
						("[CRITICAL] Not found group for PID [%d]",
						credential.pid);
					close(clientfd);
					continue;
				}
			}
		} // New Connection

		// listen cmd_socket of all group
		for (i = 0; i < DP_MAX_GROUP; i++) {
			if (!privates->groups[i].group)
				continue;
			if (privates->groups[i].group->cmd_socket < 0) {
				continue;
			}
			if (FD_ISSET(privates->groups[i].group->cmd_socket, &rset)
				> 0) {
				dp_client_group *group = privates->groups[i].group;
				dp_command client_cmd;
				int index = -1;

				is_timeout = 0;
				client_cmd.cmd = DP_CMD_NONE;
				client_cmd.id = -1;

				if (dp_ipc_read_custom_type(group->cmd_socket,
							&client_cmd, sizeof(dp_command)) < 0) {
					TRACE_STRERROR("failed to read cmd");
					//Resource temporarily unavailable
					continue;
				}

				// print detail info
				TRACE_INFO
					("[%s][%d] FD[%d] CLIENT[%s] PID[%d] GINDEX[%d]",
					__print_command(client_cmd.cmd), client_cmd.id,
					group->cmd_socket, group->pkgname,
					group->credential.pid, i);

				if (client_cmd.cmd == 0) { // Client meet some Error.
					TRACE_INFO
					("[Closed Peer] group[%d][%s] socket[%d]",
					i, group->pkgname, group->cmd_socket);
					// check all request included to this group
					for (index = 0; index < DP_MAX_REQUEST; index++) {
						if (!privates->requests[index].request)
							continue;
						if (!privates->requests[index].request->group)
							continue;
						if (privates->requests[index].request->id <= 0)
							continue;
						dp_request *request =
							privates->requests[index].request;

						CLIENT_MUTEX_LOCK(&request->mutex);
						if (request->group != group ||
							request->group->cmd_socket !=
							group->cmd_socket) {
							CLIENT_MUTEX_UNLOCK(&request->mutex);
							continue;
						}
						int auto_download =
							dp_db_get_int_column(request->id,
									DP_DB_TABLE_REQUEST_INFO,
									DP_DB_COL_AUTO_DOWNLOAD);
						if (auto_download <= 0) {
							int agentid = request->agent_id;
							int requestid = request->id;
							int state = request->state;
							TRACE_INFO("[CANCEL][%d] [%s] fd[%d]",
								requestid, request->group->pkgname,
								request->group->cmd_socket);

							if ((state == DP_STATE_READY ||
								state == DP_STATE_QUEUED ||
								state == DP_STATE_CONNECTING ||
								state == DP_STATE_DOWNLOADING ||
								state == DP_STATE_PAUSE_REQUESTED ||
								state == DP_STATE_PAUSED)) {
								request->state = DP_STATE_FAILED;
								request->error = DP_ERROR_CLIENT_DOWN;
								if (dp_db_set_column
										(request->id, DP_DB_TABLE_LOG,
										DP_DB_COL_STATE,
										DP_DB_COL_TYPE_INT,
										&request->state) < 0) {
									TRACE_ERROR("[ERROR][%d][SQL]",
										requestid);
									dp_db_remove_all(request->id);
								} else {
									if (dp_db_set_column
										(request->id, DP_DB_TABLE_LOG,
										DP_DB_COL_ERRORCODE,
										DP_DB_COL_TYPE_INT,
										&request->error) < 0) {
									TRACE_ERROR("[ERROR][%d][SQL]",
										requestid);
									}
								}
							}
							CLIENT_MUTEX_UNLOCK(&request->mutex);
							dp_request_free(request);
							privates->requests[index].request = NULL;

							// call cancel_agent after free.
							if (agentid > 0 &&
								dp_is_alive_download(agentid)) {
								TRACE_INFO
									("[CANCEL-AGENT][%d] state [%s]",
									requestid, dp_print_state(state));
								if (dp_cancel_agent_download(agentid) <
									0)
									TRACE_INFO("[CANCEL FAILURE]");
							}

							continue;
						}


						TRACE_INFO("[DISCONNECT][%d] [%s] fd[%d]",
							request->id, request->group->pkgname,
							request->group->cmd_socket);

						request->group = NULL;
						request->state_cb = 0;
						request->progress_cb = 0;

						CLIENT_MUTEX_UNLOCK(&request->mutex);
						// yield to agent thread before free
						CLIENT_MUTEX_LOCK(&request->mutex);
						// free finished slot without client process
						if (request->state == DP_STATE_COMPLETED
							|| request->state == DP_STATE_FAILED
							|| request->state == DP_STATE_CANCELED) {
							TRACE_INFO
								("[FREE][%d] state[%s]", request->id,
								dp_print_state(request->state));
							CLIENT_MUTEX_UNLOCK(&request->mutex);
							dp_request_free(request);
							privates->requests[index].request = NULL;
							continue;
						}
						CLIENT_MUTEX_UNLOCK(&(request->mutex));
					}
					// clear this group
					FD_CLR(group->cmd_socket, &listen_fdset);
					dp_client_group_free(group);
					privates->groups[i].group = NULL;
					continue;
				}

				// Echo .client can check whether provider is busy
				if (client_cmd.cmd == DP_CMD_ECHO) {
					// provider can clear read buffer here
					TRACE_INFO("[ECHO] fd[%d]", group->cmd_socket);
					dp_ipc_send_errorcode
						(group->cmd_socket, DP_ERROR_NONE);
					continue;
				}

				if (client_cmd.cmd == DP_CMD_CREATE) {
					// search empty slot in privates->requests
					index = __get_empty_request_index(privates->requests);
					if (index < 0) {
						TRACE_ERROR("[CHECK] [DP_ERROR_QUEUE_FULL]");
						// Busy, No Space in slot
						dp_ipc_send_errorcode(group->cmd_socket,
							DP_ERROR_QUEUE_FULL);
					} else {
						dp_error_type ret = DP_ERROR_NONE;
						ret = dp_request_create(client_cmd.id, group,
							&privates->requests[index].request);
						if (ret == DP_ERROR_NONE) {
							TRACE_INFO
								("[CREATE] GOOD ID[%d] SLOT[%d]",
								privates->requests[index].request->id,
								index);
							__send_return_custom_type(group->cmd_socket,
								DP_ERROR_NONE,
								&privates->requests[index].request->id,
								sizeof(int));
						} else {
							TRACE_ERROR
								("[ERROR][%s]", dp_print_errorcode(ret));
							dp_ipc_send_errorcode
								(group->cmd_socket, ret);
						}
					}
					continue;
				}

				// below commands must need ID
				// check exception before searching slots.
				if (client_cmd.id < 0) {
					TRACE_ERROR("[CHECK PROTOCOL] ID should not be -1");
					dp_ipc_send_errorcode(group->cmd_socket,
						DP_ERROR_INVALID_PARAMETER);
					// disconnect this group, bad client
					FD_CLR(group->cmd_socket, &listen_fdset);
					dp_client_group_free(group);
					privates->groups[i].group = NULL;
					continue;
				}

				// search id in requests slot
				index = __get_same_request_index
							(privates->requests, client_cmd.id);

				dp_request *request = NULL;
				if (index >= 0)
					request = privates->requests[index].request;

				char *auth_pkgname = NULL;
				if (request != NULL) {
					auth_pkgname = dp_strdup(request->packagename);
				} else {
					auth_pkgname = dp_db_get_text_column(client_cmd.id,
							DP_DB_TABLE_LOG, DP_DB_COL_PACKAGENAME);
					if (auth_pkgname == NULL) {
						TRACE_ERROR
							("[ERROR][%d] [DP_ERROR_ID_NOT_FOUND]",
							client_cmd.id);
						dp_ipc_send_errorcode
						(group->cmd_socket, DP_ERROR_ID_NOT_FOUND);
						continue;
					}
				}

				// auth by pkgname
				if (__cmp_string
					(group->pkgname, auth_pkgname) < 0) {
					TRACE_ERROR
						("[ERROR][%d] Auth [%s]/[%s]", client_cmd.id,
						group->pkgname, auth_pkgname);
					dp_ipc_send_errorcode
					(group->cmd_socket, DP_ERROR_INVALID_PARAMETER);
					free(auth_pkgname);
					continue;
				}
				if (auth_pkgname != NULL)
					free(auth_pkgname);

				if (request != NULL && request->group == NULL) {
					// if no group, update group.
					request->group = group;
				}

				if (client_cmd.cmd == DP_CMD_DESTROY) {
					if (request != NULL) {
						CLIENT_MUTEX_LOCK(&(request->mutex));
						// call download_cancel API
						if (request->agent_id > 0 &&
							dp_is_alive_download(request->agent_id)) {
							TRACE_INFO("[CANCEL-AGENT][%d] agent_id[%d]",
								client_cmd.id, request->agent_id);
							if (dp_cancel_agent_download
									(request->agent_id) < 0)
								TRACE_INFO("[CANCEL FAILURE][%d]",
									client_cmd.id);
							request->state = DP_STATE_CANCELED;
							if (dp_db_set_column
									(request->id, DP_DB_TABLE_LOG,
									DP_DB_COL_STATE, DP_DB_COL_TYPE_INT,
									&request->state) < 0) {
								TRACE_ERROR("[ERROR][%d][SQL] try [%s]",
									client_cmd.id,
									dp_print_state(request->state));
								dp_db_remove_all(request->id);
							}
						}
						CLIENT_MUTEX_UNLOCK(&(request->mutex));
					}
					// Always return GOOD
					dp_ipc_send_errorcode
						(group->cmd_socket, DP_ERROR_NONE);
					#if 0
					// remove tmp file
					char *tmp_path =
						dp_request_get_tmpsavedpath
							(client_cmd.id, &errorcode);
					if ((tmp_path != NULL && strlen(tmp_path) > 0) &&
						dp_is_file_exist(tmp_path) == 0) {
						TRACE_INFO("[REMOVE][%d] TEMP FILE [%s]",
							client_cmd.id, tmp_path);
						if (unlink(tmp_path) != 0)
							TRACE_STRERROR("[ERROR][%d] remove file",
								client_cmd.id);
						free(tmp_path);
					}
					// in DESTROY, clear all info
					dp_db_remove_all(client_cmd.id);
					#else
					// in DESTROY, maintain DB logging
					if (request != NULL) {
						CLIENT_MUTEX_LOCK(&(request->mutex));
						// change state to CANCELED.
						if (request->state == DP_STATE_NONE ||
							request->state == DP_STATE_READY ||
							request->state == DP_STATE_QUEUED ||
							request->state == DP_STATE_CONNECTING ||
							request->state == DP_STATE_DOWNLOADING ||
							request->state == DP_STATE_PAUSE_REQUESTED ||
							request->state == DP_STATE_PAUSED) {
							request->state = DP_STATE_CANCELED;
							if (dp_db_set_column
									(request->id, DP_DB_TABLE_LOG,
									DP_DB_COL_STATE, DP_DB_COL_TYPE_INT,
									&request->state) < 0) {
								TRACE_ERROR("[ERROR][%d][SQL] try [%s]",
									client_cmd.id,
									dp_print_state(request->state));
							}
						}
						CLIENT_MUTEX_UNLOCK(&(request->mutex));
					}
					#endif
				} else if (client_cmd.cmd == DP_CMD_FREE) {
					// [destory]-[return]-[free]
					// No return errorcode
					if (request != NULL) {
						dp_request_free(request);
						privates->requests[index].request = NULL;
					}
				} else if (client_cmd.cmd == DP_CMD_START) {
					if (request == NULL) { // Support Re-download
						index =
							__get_empty_request_index(privates->requests);
						if (index < 0) {
							TRACE_ERROR
								("[ERROR][%d] DP_ERROR_QUEUE_FULL",
								client_cmd.id);
							// Busy, No Space in slot
							dp_ipc_send_errorcode(group->cmd_socket,
								DP_ERROR_QUEUE_FULL);
							continue;
						}
						request = dp_request_load_from_log
									(client_cmd.id, &errorcode);
						if (request == NULL) {
							TRACE_ERROR("[ERROR][%d] [%s]",
								client_cmd.id,
								dp_print_errorcode(errorcode));
							dp_ipc_send_errorcode
								(group->cmd_socket, errorcode);
							continue;
						}
						// restore callback info
						request->state_cb = 
							dp_db_get_int_column(client_cmd.id,
									DP_DB_TABLE_REQUEST_INFO,
									DP_DB_COL_STATE_EVENT);
						request->progress_cb = 
							dp_db_get_int_column(client_cmd.id,
									DP_DB_TABLE_REQUEST_INFO,
									DP_DB_COL_PROGRESS_EVENT);
						privates->requests[index].request = request;
					}
					// Check URL
					char *url = dp_request_get_url
							(client_cmd.id, request, &errorcode);
					if (url == NULL) {
						TRACE_ERROR("[ERROR][%d] DP_ERROR_INVALID_URL",
							client_cmd.id);
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_INVALID_URL);
						continue;
					} else {
						free(url);
					}
					if (request->state == DP_STATE_QUEUED ||
						request->state == DP_STATE_CONNECTING ||
						request->state == DP_STATE_DOWNLOADING ||
						request->state == DP_STATE_COMPLETED) {
						TRACE_ERROR
							("[ERROR][%d] now [%s]", client_cmd.id,
							dp_print_state(request->state));
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_INVALID_STATE);
						continue;
					}
					dp_state_type state = DP_STATE_QUEUED;
					if (dp_db_set_column
							(request->id, DP_DB_TABLE_LOG,
							DP_DB_COL_STATE, DP_DB_COL_TYPE_INT,
							&state) < 0) {
						TRACE_ERROR
							("[ERROR][%d][SQL] try [%s]->[%s]",
							client_cmd.id,
							dp_print_state(request->state),
							dp_print_state(state));
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_IO_ERROR);
						continue;
					}
					group->queued_count++;
					if (dp_db_update_date(request->id, DP_DB_TABLE_LOG,
							DP_DB_COL_ACCESS_TIME) < 0)
						TRACE_ERROR("[ERROR][%d][SQL]", client_cmd.id);
					request->start_time = (int)time(NULL);
					request->pause_time = 0;
					request->stop_time = 0;
					request->state = state;
					request->error = DP_ERROR_NONE;
					// need to check how long take to come here.
					dp_ipc_send_errorcode
						(group->cmd_socket, DP_ERROR_NONE);
					//send signal to queue thread
					dp_thread_queue_manager_wake_up();
				} else if (client_cmd.cmd == DP_CMD_PAUSE) {
					if (request == NULL) {
						TRACE_ERROR
							("[ERROR][%d] [DP_ERROR_ID_NOT_FOUND]",
							client_cmd.id);
						dp_ipc_send_errorcode(group->cmd_socket,
							DP_ERROR_ID_NOT_FOUND);
						continue;
					}
					if (dp_db_update_date(request->id, DP_DB_TABLE_LOG,
							DP_DB_COL_ACCESS_TIME) < 0)
						TRACE_ERROR("[ERROR][%d][SQL]", client_cmd.id);

					CLIENT_MUTEX_LOCK(&(request->mutex));
					if (request->state > DP_STATE_DOWNLOADING) {
						TRACE_ERROR
							("[ERROR][%d] now [%s]", client_cmd.id,
							dp_print_state(request->state));
						CLIENT_MUTEX_UNLOCK(&(request->mutex));
						dp_ipc_send_errorcode(group->cmd_socket,
							DP_ERROR_INVALID_STATE);
						continue;
					}
					if (request->state <= DP_STATE_QUEUED) {
						request->state = DP_STATE_PAUSED;
						request->error = DP_ERROR_NONE;
						if (dp_db_set_column
								(request->id, DP_DB_TABLE_LOG,
								DP_DB_COL_STATE, DP_DB_COL_TYPE_INT,
								&request->state) < 0) {
							TRACE_ERROR("[ERROR][%d][SQL] try [%s]",
								client_cmd.id,
								dp_print_state(request->state));
							dp_ipc_send_errorcode
								(group->cmd_socket, DP_ERROR_IO_ERROR);
						} else {
							dp_ipc_send_errorcode
								(group->cmd_socket, DP_ERROR_NONE);
						}
						CLIENT_MUTEX_UNLOCK(&(request->mutex));
						continue;
					}
					if (dp_pause_agent_download(request->agent_id)
						< 0) {
						TRACE_ERROR
							("[ERROR][%d][AGENT][ID:%d] now [%s]",
							client_cmd.id, request->agent_id,
							dp_print_state(request->state));
						CLIENT_MUTEX_UNLOCK(&(request->mutex));
						dp_ipc_send_errorcode(group->cmd_socket,
							DP_ERROR_INVALID_STATE);
						continue;
					}
					request->pause_time = (int)time(NULL);
					request->state = DP_STATE_PAUSE_REQUESTED;
					request->error = DP_ERROR_NONE;
					if (dp_db_set_column
							(request->id, DP_DB_TABLE_LOG,
							DP_DB_COL_STATE, DP_DB_COL_TYPE_INT,
							&request->state) < 0) {
						TRACE_ERROR("[ERROR][%d][SQL] try [%s]",
								client_cmd.id,
								dp_print_state(request->state));
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_IO_ERROR);
					} else {
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_NONE);
					}
					CLIENT_MUTEX_UNLOCK(&(request->mutex));
				} else if (client_cmd.cmd == DP_CMD_CANCEL) {
					if (request == NULL) {
						// find id in DB.
						dp_state_type state =
							dp_db_get_int_column(client_cmd.id,
								DP_DB_TABLE_LOG, DP_DB_COL_STATE);
						if (state > DP_STATE_NONE) {
							// if exist & it is paused, change to canceled.
							if (state == DP_STATE_PAUSED ||
								state == DP_STATE_PAUSE_REQUESTED) {
								// change state to canceled.
								state = DP_STATE_CANCELED;
								if (dp_db_set_column
										(client_cmd.id,
										DP_DB_TABLE_LOG,
										DP_DB_COL_STATE,
										DP_DB_COL_TYPE_INT,
										&state) < 0) {
									TRACE_ERROR
										("[ERROR][%d][SQL] try [%s]",
											client_cmd.id,
											dp_print_state(state));
									dp_ipc_send_errorcode
										(group->cmd_socket,
										DP_ERROR_IO_ERROR);
								} else {
									dp_ipc_send_errorcode
										(group->cmd_socket,
										DP_ERROR_NONE);
								}
							} else {
								TRACE_ERROR("[ERROR][%d] now [%s]",
										client_cmd.id,
										dp_print_state(state));
								dp_ipc_send_errorcode(group->cmd_socket,
									DP_ERROR_INVALID_STATE);
							}
							continue;
						}
						// if not match these conditions, invalied param
						TRACE_ERROR
							("[ERROR][%d] [DP_ERROR_ID_NOT_FOUND]",
							client_cmd.id);
						dp_ipc_send_errorcode(group->cmd_socket,
							DP_ERROR_ID_NOT_FOUND);
						continue;
					}
					CLIENT_MUTEX_LOCK(&(request->mutex));
					if (request->state >= DP_STATE_COMPLETED) {
						// already finished.
						TRACE_ERROR("[ERROR][%d] now [%s]",
								client_cmd.id,
								dp_print_state(request->state));
						CLIENT_MUTEX_UNLOCK(&(request->mutex));
						dp_ipc_send_errorcode(group->cmd_socket,
							DP_ERROR_INVALID_STATE);
						continue;
					}
					if (request->state <= DP_STATE_QUEUED) {
						request->state = DP_STATE_CANCELED;
						request->error = DP_ERROR_NONE;
						if (dp_db_set_column
								(request->id, DP_DB_TABLE_LOG,
								DP_DB_COL_STATE, DP_DB_COL_TYPE_INT,
								&request->state) < 0) {
							TRACE_ERROR("[ERROR][%d][SQL] try [%s]",
								client_cmd.id,
								dp_print_state(request->state));
							dp_ipc_send_errorcode
								(group->cmd_socket, DP_ERROR_IO_ERROR);
						} else {
							dp_ipc_send_errorcode
								(group->cmd_socket, DP_ERROR_NONE);
						}
						CLIENT_MUTEX_UNLOCK(&(request->mutex));
						continue;
					}
					if (dp_db_update_date(request->id, DP_DB_TABLE_LOG,
							DP_DB_COL_ACCESS_TIME) < 0)
						TRACE_ERROR("[ERROR][%d][SQL]", client_cmd.id);

					if (dp_cancel_agent_download(request->agent_id)
						< 0) {
						TRACE_ERROR("[ERROR][%d][AGENT][ID:%d] now [%s]",
								client_cmd.id,
								request->agent_id,
								dp_print_state(request->state));
						CLIENT_MUTEX_UNLOCK(&(request->mutex));
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_INVALID_STATE);
						continue;
					}
					// need to check how long take to come here.
					if (dp_db_set_column
							(request->id, DP_DB_TABLE_LOG,
							DP_DB_COL_STATE, DP_DB_COL_TYPE_INT,
							&request->state) < 0) {
						TRACE_ERROR("[ERROR][%d][SQL] try [%s]",
								client_cmd.id,
								dp_print_state(request->state));
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_IO_ERROR);
					} else {
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_NONE);
					}
					CLIENT_MUTEX_UNLOCK(&(request->mutex));
				} else if (client_cmd.cmd == DP_CMD_SET_URL) {
					char *url = dp_ipc_read_string(group->cmd_socket);
					if (url == NULL) {
						TRACE_ERROR("[ERROR][%d] DP_ERROR_INVALID_URL",
							client_cmd.id);
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_INVALID_URL);
						continue;
					}
					dp_error_type ret =
						dp_request_set_url(client_cmd.id, request, url);
					TRACE_INFO("[SET_URL][%d][%s]", client_cmd.id, url);
					dp_ipc_send_errorcode(group->cmd_socket, ret);
					free(url);
					if (ret != DP_ERROR_NONE)
						TRACE_ERROR("[ERROR][%d][%s]",
							client_cmd.id, dp_print_errorcode(ret));
				} else if (client_cmd.cmd == DP_CMD_SET_DESTINATION) {
					char *dest = dp_ipc_read_string(group->cmd_socket);
					if (dest == NULL) {
						TRACE_ERROR
							("[ERROR][%d] DP_ERROR_INVALID_DESTINATION",
							client_cmd.id);
						dp_ipc_send_errorcode(group->cmd_socket,
							DP_ERROR_INVALID_DESTINATION);
						continue;
					}
					dp_error_type ret =
						dp_request_set_destination
							(client_cmd.id, request, dest);
					dp_ipc_send_errorcode(group->cmd_socket, ret);
					TRACE_INFO("[SET_DEST][%d][%s]", client_cmd.id, dest);
					free(dest);
					if (ret != DP_ERROR_NONE)
						TRACE_ERROR("[ERROR][%d][%s]",
							client_cmd.id, dp_print_errorcode(ret));
				} else if (client_cmd.cmd == DP_CMD_SET_FILENAME) {
					char *fname = dp_ipc_read_string(group->cmd_socket);
					if (fname == NULL) {
						TRACE_ERROR
							("[ERROR][%d] DP_ERROR_INVALID_PARAMETER",
							client_cmd.id);
						dp_ipc_send_errorcode(group->cmd_socket,
							DP_ERROR_INVALID_PARAMETER);
						continue;
					}
					dp_error_type ret =
						dp_request_set_filename
							(client_cmd.id, request, fname);
					dp_ipc_send_errorcode(group->cmd_socket, ret);
					TRACE_INFO
						("[SET_FILE][%d][%s]", client_cmd.id, fname);
					free(fname);
					if (ret != DP_ERROR_NONE)
						TRACE_ERROR("[ERROR][%d][%s]",
							client_cmd.id, dp_print_errorcode(ret));
				} else if (client_cmd.cmd == DP_CMD_SET_NOTIFICATION) {
					int value = 0;
					if (dp_ipc_read_custom_type(group->cmd_socket,
						&value, sizeof(int)) < 0) {
						TRACE_ERROR("[ERROR][%d] [DP_ERROR_IO_ERROR]",
						client_cmd.id);
						dp_ipc_send_errorcode(group->cmd_socket,
							DP_ERROR_IO_ERROR);
						continue;
					}
					dp_error_type ret =
						dp_request_set_notification
							(client_cmd.id, request, value);
					dp_ipc_send_errorcode(group->cmd_socket, ret);
					TRACE_INFO
						("[SET_NOTI][%d] [%d]", client_cmd.id, value);
					if (ret != DP_ERROR_NONE)
						TRACE_ERROR("[ERROR][%d][%s]",
							client_cmd.id, dp_print_errorcode(ret));
				} else if (client_cmd.cmd == DP_CMD_SET_STATE_CALLBACK) {
					int value = 0;
					if (dp_ipc_read_custom_type(group->cmd_socket,
						&value, sizeof(int)) < 0) {
						TRACE_ERROR("[ERROR][%d] [DP_ERROR_IO_ERROR]",
							client_cmd.id);
						dp_ipc_send_errorcode(group->cmd_socket,
							DP_ERROR_IO_ERROR);
						continue;
					}
					dp_error_type ret =
						dp_request_set_state_event
							(client_cmd.id, request, value);
					dp_ipc_send_errorcode(group->cmd_socket, ret);
					TRACE_INFO
						("[STATE-EVENT][%d][%d]", client_cmd.id, value);
					if (ret != DP_ERROR_NONE)
						TRACE_ERROR("[ERROR][%d][%s]",
							client_cmd.id, dp_print_errorcode(ret));
				} else if (client_cmd.cmd == DP_CMD_SET_PROGRESS_CALLBACK) {
					int value = 0;
					if (dp_ipc_read_custom_type(group->cmd_socket,
						&value, sizeof(int)) < 0) {
						TRACE_ERROR("[ERROR][%d] [DP_ERROR_IO_ERROR]",
							client_cmd.id);
						dp_ipc_send_errorcode(group->cmd_socket,
							DP_ERROR_IO_ERROR);
						continue;
					}
					dp_error_type ret =
						dp_request_set_progress_event
							(client_cmd.id, request, value);
					dp_ipc_send_errorcode(group->cmd_socket, ret);
					TRACE_INFO
						("[PROG-EVENT][%d][%d]", client_cmd.id, value);
					if (ret != DP_ERROR_NONE)
						TRACE_ERROR("[ERROR][%d][%s]",
							client_cmd.id, dp_print_errorcode(ret));
				} else if (client_cmd.cmd == DP_CMD_SET_AUTO_DOWNLOAD) {
					int value = 0;
					if (dp_ipc_read_custom_type(group->cmd_socket,
						&value, sizeof(int)) < 0) {
						TRACE_ERROR("[ERROR][%d] [DP_ERROR_IO_ERROR]",
							client_cmd.id);
						dp_ipc_send_errorcode(group->cmd_socket,
							DP_ERROR_IO_ERROR);
						continue;
					}
					dp_error_type ret =
						dp_request_set_auto_download
							(client_cmd.id, request, value);
					dp_ipc_send_errorcode(group->cmd_socket, ret);
					TRACE_INFO
						("[SET_AUTO][%d][%d]", client_cmd.id, value);
					if (ret != DP_ERROR_NONE)
						TRACE_ERROR("[ERROR][%d][%s]",
							client_cmd.id, dp_print_errorcode(ret));
				} else if (client_cmd.cmd == DP_CMD_SET_NETWORK_TYPE) {
					int value = 0;
					if (dp_ipc_read_custom_type(group->cmd_socket,
						&value, sizeof(int)) < 0) {
						TRACE_ERROR("[ERROR][%d] [DP_ERROR_IO_ERROR]",
							client_cmd.id);
						dp_ipc_send_errorcode(group->cmd_socket,
							DP_ERROR_IO_ERROR);
						continue;
					}
					dp_error_type ret =
						dp_request_set_network_type
							(client_cmd.id, request, value);
					dp_ipc_send_errorcode(group->cmd_socket, ret);
					TRACE_INFO
						("[SET_NETTYPE][%d][%d]", client_cmd.id, value);
					if (ret != DP_ERROR_NONE)
						TRACE_ERROR("[ERROR][%d][%s]",
							client_cmd.id, dp_print_errorcode(ret));
				} else if (client_cmd.cmd == DP_CMD_SET_EXTRA_PARAM) {
					char *key = dp_ipc_read_string(group->cmd_socket);
					if (key == NULL) {
						TRACE_ERROR("[ERROR][%d] [DP_ERROR_IO_ERROR]",
							client_cmd.id);
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_IO_ERROR);
						continue;
					}
					char *value = dp_ipc_read_string(group->cmd_socket);
					if (value == NULL) {
						TRACE_ERROR("[ERROR][%d] [DP_ERROR_IO_ERROR]",
							client_cmd.id);
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_IO_ERROR);
						free(key);
						continue;
					}

					TRACE_INFO("[EXTRA-PARAM][%d][%s][%s]",
						client_cmd.id, key, value);

					if (dp_db_replace_column
							(client_cmd.id, DP_DB_TABLE_NOTIFICATION,
							DP_DB_COL_EXTRA_KEY, DP_DB_COL_TYPE_TEXT,
							key) < 0) {
						TRACE_ERROR("[ERROR][%d][SQL]", client_cmd.id);
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_IO_ERROR);
						free(key);
						free(value);
						continue;
					}
					free(key);
					if (dp_db_set_column
							(client_cmd.id, DP_DB_TABLE_NOTIFICATION,
							DP_DB_COL_EXTRA_VALUE, DP_DB_COL_TYPE_TEXT,
							value) < 0) {
						TRACE_ERROR("[ERROR][%d][SQL]", client_cmd.id);
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_IO_ERROR);
					} else {
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_NONE);
					}
					free(value);
				} else if (client_cmd.cmd == DP_CMD_SET_HTTP_HEADER) {
					char *field = dp_ipc_read_string(group->cmd_socket);
					if (field == NULL) {
						TRACE_ERROR("[ERROR][%d] [DP_ERROR_IO_ERROR]",
							client_cmd.id);
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_IO_ERROR);
						continue;
					}
					char *value = dp_ipc_read_string(group->cmd_socket);
					if (value == NULL) {
						TRACE_ERROR("[ERROR][%d] [DP_ERROR_IO_ERROR]",
							client_cmd.id);
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_IO_ERROR);
						free(field);
						continue;
					}
					char *check_field = dp_db_cond_get_text_column
						(client_cmd.id, DP_DB_TABLE_HTTP_HEADERS,
						DP_DB_COL_HEADER_FIELD, DP_DB_COL_HEADER_FIELD,
						DP_DB_COL_TYPE_TEXT, field);
					if (check_field == NULL) {
						// INSERT New Field
						if (dp_db_insert_column
								(client_cmd.id,
								DP_DB_TABLE_HTTP_HEADERS,
								DP_DB_COL_HEADER_FIELD,
								DP_DB_COL_TYPE_TEXT, field) < 0) {
							TRACE_ERROR
								("[ERROR][%d][SQL]", client_cmd.id);
							free(field);
							free(value);
							dp_ipc_send_errorcode
								(group->cmd_socket, DP_ERROR_IO_ERROR);
							continue;
						}
					} else {
						free(check_field);
					}
					// UPDATE Value for Field
					if (dp_db_cond_set_column(client_cmd.id,
							DP_DB_TABLE_HTTP_HEADERS,
							DP_DB_COL_HEADER_DATA,
							DP_DB_COL_TYPE_TEXT, value,
							DP_DB_COL_HEADER_FIELD, DP_DB_COL_TYPE_TEXT,
							field) < 0) {
						TRACE_ERROR("[ERROR][%d][SQL]", client_cmd.id);
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_IO_ERROR);
					} else {
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_NONE);
					}
					free(field);
					free(value);
				} else if (client_cmd.cmd == DP_CMD_DEL_HTTP_HEADER) {
					char *field = dp_ipc_read_string(group->cmd_socket);
					if (field == NULL) {
						TRACE_ERROR("[ERROR][%d] [DP_ERROR_IO_ERROR]",
							client_cmd.id);
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_IO_ERROR);
						continue;
					}
					char *check_field = dp_db_cond_get_text_column
						(client_cmd.id, DP_DB_TABLE_HTTP_HEADERS,
						DP_DB_COL_HEADER_FIELD, DP_DB_COL_HEADER_FIELD,
						DP_DB_COL_TYPE_TEXT, field);
					if (check_field == NULL) {
						TRACE_ERROR
							("[ERROR][%d] [DP_ERROR_NO_DATA]",
							client_cmd.id);
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_NO_DATA);
						free(field);
						continue;
					}
					free(check_field);
					if (dp_db_cond_remove(client_cmd.id,
							DP_DB_TABLE_HTTP_HEADERS,
							DP_DB_COL_HEADER_FIELD, DP_DB_COL_TYPE_TEXT,
							field) < 0) {
						TRACE_ERROR("[ERROR][%d][SQL]", client_cmd.id);
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_IO_ERROR);
					} else {
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_NONE);
					}
					free(field);
				} else if (client_cmd.cmd == DP_CMD_GET_HTTP_HEADER) {
					char *field = dp_ipc_read_string(group->cmd_socket);
					if (field == NULL) {
						TRACE_ERROR("[ERROR][%d] [DP_ERROR_IO_ERROR]",
							client_cmd.id);
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_IO_ERROR);
						continue;
					}
					char *value = dp_db_cond_get_text_column
						(client_cmd.id, DP_DB_TABLE_HTTP_HEADERS,
						DP_DB_COL_HEADER_DATA, DP_DB_COL_HEADER_FIELD,
						DP_DB_COL_TYPE_TEXT, field);
					free(field);
					if (value == NULL) {
						TRACE_ERROR
							("[ERROR][%d] [DP_ERROR_NO_DATA]",
							client_cmd.id);
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_NO_DATA);
					} else {
						__send_return_string
							(group->cmd_socket, DP_ERROR_NONE, value);
						free(value);
					}
				} else if (client_cmd.cmd == DP_CMD_GET_EXTRA_PARAM) {
					char *key = dp_db_get_text_column
							(client_cmd.id, DP_DB_TABLE_NOTIFICATION,
							DP_DB_COL_EXTRA_KEY);
					if (key == NULL) {
						TRACE_ERROR
							("[ERROR][%d] [DP_ERROR_NO_DATA]",
							client_cmd.id);
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_NO_DATA);
						continue;
					}
					char *value = dp_db_get_text_column
							(client_cmd.id, DP_DB_TABLE_NOTIFICATION,
							DP_DB_COL_EXTRA_VALUE);
					if (value == NULL) {
						TRACE_ERROR
							("[ERROR][%d] [DP_ERROR_NO_DATA]",
							client_cmd.id);
						dp_ipc_send_errorcode
							(group->cmd_socket, DP_ERROR_NO_DATA);
					} else {
						__send_return_string
							(group->cmd_socket, DP_ERROR_NONE, key);
						dp_ipc_send_string(group->cmd_socket, value);
						free(value);
					}
					free(key);
				} else if (client_cmd.cmd == DP_CMD_GET_URL) {
					char *url = NULL;
					errorcode = DP_ERROR_NONE;
					url = dp_request_get_url
							(client_cmd.id, request, &errorcode);
					if (url == NULL) {
						TRACE_ERROR("[ERROR][%d] [%s]", client_cmd.id,
							dp_print_errorcode(errorcode));
						dp_ipc_send_errorcode
							(group->cmd_socket, errorcode);
					} else {
						__send_return_string
							(group->cmd_socket, DP_ERROR_NONE, url);
						free(url);
					}
				} else if (client_cmd.cmd == DP_CMD_GET_DESTINATION) {
					char *dest = NULL;
					errorcode = DP_ERROR_NONE;
					dest = dp_request_get_destination
							(client_cmd.id, request, &errorcode);
					if (dest == NULL) {
						TRACE_ERROR("[ERROR][%d] [%s]", client_cmd.id,
							dp_print_errorcode(errorcode));
						dp_ipc_send_errorcode
							(group->cmd_socket, errorcode);
					} else {
						__send_return_string
							(group->cmd_socket, DP_ERROR_NONE, dest);
						free(dest);
					}
				} else if (client_cmd.cmd == DP_CMD_GET_FILENAME) {
					char *filename = NULL;
					errorcode = DP_ERROR_NONE;
					filename = dp_request_get_filename
							(client_cmd.id, request, &errorcode);
					if (filename == NULL) {
						TRACE_ERROR("[ERROR][%d] [%s]", client_cmd.id,
							dp_print_errorcode(errorcode));
						dp_ipc_send_errorcode
							(group->cmd_socket, errorcode);
					} else {
						__send_return_string
							(group->cmd_socket, DP_ERROR_NONE, filename);
						free(filename);
					}
				} else if (client_cmd.cmd == DP_CMD_GET_NOTIFICATION) {
					int enable = 0;
					enable = dp_db_get_int_column(client_cmd.id,
									DP_DB_TABLE_NOTIFICATION,
									DP_DB_COL_NOTIFICATION_ENABLE);
					if (enable < 0) {
						TRACE_ERROR
							("[ERROR][%d] [DP_ERROR_NO_DATA]",
							client_cmd.id);
						dp_ipc_send_errorcode(group->cmd_socket,
							DP_ERROR_NO_DATA);
					} else {
						__send_return_custom_type
							(group->cmd_socket, DP_ERROR_NONE,
							&enable, sizeof(int));
					}
				} else if (client_cmd.cmd == DP_CMD_GET_AUTO_DOWNLOAD) {
					int enable = 0;
					enable = dp_db_get_int_column(client_cmd.id,
									DP_DB_TABLE_REQUEST_INFO,
									DP_DB_COL_AUTO_DOWNLOAD);
					if (enable < 0) {
						TRACE_ERROR
							("[ERROR][%d] [DP_ERROR_NO_DATA]",
							client_cmd.id);
						dp_ipc_send_errorcode(group->cmd_socket,
							DP_ERROR_NO_DATA);
					} else {
						__send_return_custom_type
							(group->cmd_socket, DP_ERROR_NONE,
							&enable, sizeof(int));
					}
				} else if (client_cmd.cmd == DP_CMD_GET_NETWORK_TYPE) {
					int type = 0;
					type = dp_db_get_int_column(client_cmd.id,
									DP_DB_TABLE_REQUEST_INFO,
									DP_DB_COL_NETWORK_TYPE);
					if (type < 0) {
						TRACE_ERROR
							("[ERROR][%d] [DP_ERROR_NO_DATA]",
							client_cmd.id);
						dp_ipc_send_errorcode(group->cmd_socket,
							DP_ERROR_NO_DATA);
					} else {
						__send_return_custom_type
							(group->cmd_socket, DP_ERROR_NONE,
							&type, sizeof(dp_network_type));
					}
				} else if (client_cmd.cmd == DP_CMD_GET_SAVED_PATH) {
					char *savedpath = NULL;
					errorcode = DP_ERROR_NONE;
					savedpath = dp_request_get_savedpath
							(client_cmd.id, request, &errorcode);
					if (savedpath == NULL) {
						TRACE_ERROR("[ERROR][%d] [%s]", client_cmd.id,
							dp_print_errorcode(errorcode));
						dp_ipc_send_errorcode
							(group->cmd_socket, errorcode);
					} else {
						__send_return_string
							(group->cmd_socket, DP_ERROR_NONE, savedpath);
						free(savedpath);
					}
				} else if (client_cmd.cmd == DP_CMD_GET_TEMP_SAVED_PATH) {
					char *tmppath = NULL;
					errorcode = DP_ERROR_NONE;
					tmppath = dp_request_get_tmpsavedpath
							(client_cmd.id, request, &errorcode);
					if (tmppath == NULL) {
						TRACE_ERROR("[ERROR][%d] [%s]", client_cmd.id,
							dp_print_errorcode(errorcode));
						dp_ipc_send_errorcode
							(group->cmd_socket, errorcode);
					} else {
						__send_return_string
							(group->cmd_socket, DP_ERROR_NONE, tmppath);
						free(tmppath);
					}
				} else if (client_cmd.cmd == DP_CMD_GET_MIME_TYPE) {
					char *mimetype = NULL;
					errorcode = DP_ERROR_NONE;
					mimetype = dp_request_get_mimetype
							(client_cmd.id, request, &errorcode);
					if (mimetype == NULL) {
						TRACE_ERROR("[ERROR][%d] [%s]", client_cmd.id,
							dp_print_errorcode(errorcode));
						dp_ipc_send_errorcode
							(group->cmd_socket, errorcode);
					} else {
						__send_return_string
							(group->cmd_socket, DP_ERROR_NONE, mimetype);
						free(mimetype);
					}
				} else if (client_cmd.cmd == DP_CMD_GET_RECEIVED_SIZE) {
					if (request == NULL) {
						TRACE_ERROR
							("[ERROR][%d] [DP_ERROR_ID_NOT_FOUND]",
							client_cmd.id);
						dp_ipc_send_errorcode(group->cmd_socket,
							DP_ERROR_ID_NOT_FOUND);
					} else {
						__send_return_custom_type
							(group->cmd_socket, DP_ERROR_NONE,
							&request->received_size,
							sizeof(unsigned long long));
					}
				} else if (client_cmd.cmd == DP_CMD_GET_TOTAL_FILE_SIZE) {
					if (request != NULL) {
						__send_return_custom_type(group->cmd_socket,
							DP_ERROR_NONE, &request->file_size,
							sizeof(unsigned long long));
						continue;
					}
					long long file_size =
						dp_db_get_int64_column(client_cmd.id,
									DP_DB_TABLE_DOWNLOAD_INFO,
									DP_DB_COL_CONTENT_SIZE);
					if (file_size < 0) {
						TRACE_ERROR
							("[ERROR][%d] [DP_ERROR_NO_DATA]",
							client_cmd.id);
						dp_ipc_send_errorcode(group->cmd_socket,
							DP_ERROR_NO_DATA);
					} else {
						// casting
						unsigned long long total_file_size = file_size;
						__send_return_custom_type
							(group->cmd_socket, DP_ERROR_NONE,
							&total_file_size, sizeof(total_file_size));
					}
				} else if (client_cmd.cmd == DP_CMD_GET_CONTENT_NAME) {
					char *content = NULL;
					errorcode = DP_ERROR_NONE;
					content = dp_request_get_contentname
							(client_cmd.id, request, &errorcode);
					if (content == NULL) {
						TRACE_ERROR("[ERROR][%d] [%s]", client_cmd.id,
							dp_print_errorcode(errorcode));
						dp_ipc_send_errorcode
							(group->cmd_socket, errorcode);
					} else {
						__send_return_string
							(group->cmd_socket, DP_ERROR_NONE, content);
						free(content);
					}
				} else if (client_cmd.cmd == DP_CMD_GET_HTTP_STATUS) {
					int http_status = 0;
					http_status = dp_db_get_int_column(client_cmd.id,
									DP_DB_TABLE_DOWNLOAD_INFO,
									DP_DB_COL_HTTP_STATUS);
					if (http_status < 0) {
						TRACE_ERROR
							("[ERROR][%d] [DP_ERROR_NO_DATA]",
							client_cmd.id);
						dp_ipc_send_errorcode(group->cmd_socket,
							DP_ERROR_NO_DATA);
					} else {
						__send_return_custom_type
							(group->cmd_socket, DP_ERROR_NONE,
							&http_status, sizeof(int));
					}
				} else if (client_cmd.cmd == DP_CMD_GET_ETAG) {
					char *etag = NULL;
					errorcode = DP_ERROR_NONE;
					etag = dp_request_get_etag
							(client_cmd.id, request, &errorcode);
					if (etag == NULL) {
						TRACE_ERROR("[ERROR][%d] [%s]", client_cmd.id,
							dp_print_errorcode(errorcode));
						dp_ipc_send_errorcode
							(group->cmd_socket, errorcode);
					} else {
						__send_return_string
							(group->cmd_socket, DP_ERROR_NONE, etag);
						free(etag);
					}
				} else if (client_cmd.cmd == DP_CMD_GET_STATE) {
					dp_state_type download_state = DP_STATE_NONE;
					if (request == NULL) {
						download_state =
							dp_db_get_int_column(client_cmd.id,
										DP_DB_TABLE_LOG,
										DP_DB_COL_STATE);
						if (download_state < 0) {
							TRACE_ERROR
							("[ERROR][%d] [DP_ERROR_ID_NOT_FOUND]",
							client_cmd.id);
							dp_ipc_send_errorcode(group->cmd_socket,
								DP_ERROR_ID_NOT_FOUND);
							continue;
						}
					} else {
						CLIENT_MUTEX_LOCK(&(request->mutex));
						download_state = request->state;
						CLIENT_MUTEX_UNLOCK(&(request->mutex));
					}
					__send_return_custom_type
						(group->cmd_socket, DP_ERROR_NONE,
						&download_state, sizeof(dp_state_type));
				} else if (client_cmd.cmd == DP_CMD_GET_ERROR) {
					errorcode = DP_ERROR_NONE;
					if (request == NULL) {
						errorcode =
							dp_db_get_int_column(client_cmd.id,
										DP_DB_TABLE_LOG,
										DP_DB_COL_ERRORCODE);
						if (errorcode < 0) {
							TRACE_ERROR
							("[ERROR][%d] [DP_ERROR_ID_NOT_FOUND]",
							client_cmd.id);
							dp_ipc_send_errorcode(group->cmd_socket,
								DP_ERROR_ID_NOT_FOUND);
							continue;
						}
					} else {
						CLIENT_MUTEX_LOCK(&(request->mutex));
						errorcode = request->error;
						CLIENT_MUTEX_UNLOCK(&(request->mutex));
					}
					dp_ipc_send_errorcode
						(group->cmd_socket, DP_ERROR_NONE);
					dp_ipc_send_errorcode
						(group->cmd_socket, errorcode);
				} else {
					TRACE_INFO
						("[UNKNOWN] I [%d] PID [%d]", i,
						privates->groups[i].group->credential.pid);
					dp_ipc_send_errorcode(group->cmd_socket,
						DP_ERROR_INVALID_PARAMETER);
					// disconnect this group, bad client
					FD_CLR(group->cmd_socket, &listen_fdset);
					dp_client_group_free(group);
					privates->groups[i].group = NULL;
				}
			} // FD_ISSET
		} // DP_MAX_GROUP

		// timeout
		if (is_timeout == 1) {
			int now_timeout = (int)time(NULL);
			TRACE_INFO("[TIMEOUT] prev %ld, now %ld, setted %ld sec",
				prev_timeout, now_timeout, flexible_timeout);
			if (prev_timeout == 0) {
				prev_timeout = now_timeout;
			} else {
				if ((now_timeout - prev_timeout) <
					DP_CARE_CLIENT_MIN_INTERVAL) {
					// this is error.
					// terminate Process
					TRACE_STRERROR
					("[CRITICAL] Sock exception prev[%ld]now[%ld][%ld]",
					prev_timeout, now_timeout, flexible_timeout);
					break;
				}
			}

			// get 48hour old request from log DB
			// clear old request
			dp_db_limit_rows(DP_LOG_DB_LIMIT_ROWS);
			int old_request_count = dp_db_get_count_by_limit_time();
			if (old_request_count > 0) {
				if (old_request_count > DP_LOG_DB_CLEAR_LIMIT_ONE_TIME)
					old_request_count = DP_LOG_DB_CLEAR_LIMIT_ONE_TIME;

				TRACE_INFO
					("[CLEAR] [%d] old reqeusts", old_request_count);

				dp_request_slots *old_requests =
					dp_request_slots_new(old_request_count);
				if (old_requests) {
					int list_count = dp_db_get_list_by_limit_time
								(old_requests, old_request_count);
					if (list_count > 0) {
						for (i = 0; i < list_count; i++) {
							// search on slots by ID.
							int index =
								__get_same_request_index
									(privates->requests,
									old_requests[i].request->id);
							if (index >= 0) {
								dp_request *request =
									privates->requests[index].request;
								// if downloading..remain it.
								CLIENT_MUTEX_LOCK(&(request->mutex));
								if (request->state ==
									DP_STATE_CONNECTING ||
									request->state ==
									DP_STATE_DOWNLOADING) {
									CLIENT_MUTEX_UNLOCK
										(&(request->mutex));
									continue;
								}
								CLIENT_MUTEX_UNLOCK(&(request->mutex));
								// unload from slots ( memory )
								dp_request_free(request);
								privates->requests[index].request = NULL;
							}
							// remove tmp file
							char *tmp_path =
								dp_request_get_tmpsavedpath
									(old_requests[i].request->id,
									old_requests[i].request,
									&errorcode);
							if ((tmp_path && strlen(tmp_path) > 0) &&
								dp_is_file_exist(tmp_path) == 0) {
								TRACE_INFO
									("[REMOVE][%d] TEMP FILE [%s]",
									old_requests[i].request->id,
									tmp_path);
								if (unlink(tmp_path) != 0)
									TRACE_STRERROR
										("[ERROR][%d] remove file",
										old_requests[i].request->id);
								free(tmp_path);
							}
							// remove from DB
							dp_db_remove_all(old_requests[i].request->id);
						}
					}
					dp_request_slots_free(old_requests, old_request_count);
				}
			}

			// clean slots
			int ready_requests = 0;
			for (i = 0; i < DP_MAX_REQUEST; i++) {
				if (!privates->requests[i].request)
					continue;
				dp_request *request = privates->requests[i].request;

				CLIENT_MUTEX_LOCK(&(request->mutex));

				// If downloading is too slow ? how to deal this request?
				// can limit too slot download using starttime.(48 hours)
				if (request->state == DP_STATE_CONNECTING ||
					request->state == DP_STATE_DOWNLOADING) {
					CLIENT_MUTEX_UNLOCK(&(request->mutex));
					continue;
				}

				// paused & agent_id not exist.... unload from memory.
				if (request->state == DP_STATE_PAUSED &&
					dp_is_alive_download(request->agent_id) == 0) {
					CLIENT_MUTEX_UNLOCK(&(request->mutex));
					TRACE_INFO
					("[FREE] [%d] unavailable agent ID [%d]",
						request->id, request->agent_id);
					dp_request_free(request);
					privates->requests[i].request = NULL;
					continue;
				}

				// client should call START command in 60 sec
				// unload from memory
				if (request->start_time <= 0 &&
					(now_timeout - request->create_time) >
						DP_CARE_CLIENT_MAX_INTERVAL) {
					int download_id = request->id;
					dp_state_type state = DP_STATE_FAILED;
					errorcode = DP_ERROR_RESPONSE_TIMEOUT;
					if (request->group
						&& request->group->event_socket >= 0
						&& request->state_cb)
						dp_ipc_send_event
							(request->group->event_socket,
							download_id, state, errorcode, 0);
					CLIENT_MUTEX_UNLOCK(&(request->mutex));
					TRACE_INFO
					("[FREE] no response ID[%d] last access [%ld]",
						request->id, request->start_time);
					dp_request_free(request);
					privates->requests[i].request = NULL;
					// no problem although updating is failed.
					if (dp_db_set_column
							(download_id, DP_DB_TABLE_LOG,
							DP_DB_COL_STATE, DP_DB_COL_TYPE_INT,
							&state) < 0) {
						TRACE_ERROR("[ERROR][%d][SQL]", request->id);
					}
					if (dp_db_set_column
							(download_id, DP_DB_TABLE_LOG,
							DP_DB_COL_ERRORCODE, DP_DB_COL_TYPE_INT,
							&errorcode) < 0) {
						TRACE_ERROR("[ERROR][%d][SQL]", request->id);
					}
					continue;
				}

				// client should call DESTROY command in 60 sec after finished
				if (request->stop_time > 0 &&
					(now_timeout - request->stop_time) >
					DP_CARE_CLIENT_MAX_INTERVAL) {
					// check state again. stop_time means it's stopped
					if (request->state == DP_STATE_COMPLETED ||
						request->state == DP_STATE_FAILED ||
						request->state == DP_STATE_CANCELED) {
						CLIENT_MUTEX_UNLOCK(&(request->mutex));
						TRACE_INFO("[FREE] by timeout cleaner ID[%d]",
							request->id);
						dp_request_free(request);
						privates->requests[i].request = NULL;
						continue;
					}
				}

				// check after clear
				if (request->state == DP_STATE_QUEUED)
					ready_requests++;

				CLIENT_MUTEX_UNLOCK(&(request->mutex));
			}

			if (ready_requests > 0) {
				//send signal to queue thread will check queue.
				dp_thread_queue_manager_wake_up();
			} else {
#ifdef DP_SUPPORT_DBUS_ACTIVATION
				// if no request & timeout is bigger than 60 sec
				// terminate by self.
				if ((now_timeout - prev_timeout) >= flexible_timeout &&
					dp_get_request_count(privates->requests) <= 0) {
					TRACE_INFO("No Request. Terminate Daemon");
					break;
				}
#endif
			}
			prev_timeout = now_timeout;
		} // timeout
	}
	TRACE_INFO("terminate main thread ...");
	dp_terminate(SIGTERM);
	pthread_exit(NULL);
	return 0;
}
