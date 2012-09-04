#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>

#include <glib.h>

#include "download-provider-config.h"
#include "download-provider-log.h"
#include "download-provider-pthread.h"
#include "download-provider-notification.h"
#include "download-provider-ipc.h"
#include "download-provider-db.h"
#include "download-provider-utils.h"

#include "download-agent-defs.h"
#include "download-agent-interface.h"

int _init_agent(void);
void _deinit_agent(void);
static void __downloading_info_cb(user_downloading_info_t *download_info,
					void *user_data);
static void __download_info_cb(user_download_info_t *download_info,
					void *user_data);
static void __notify_cb(user_notify_info_t *notify_info, void *user_data);
static int __change_error(int err);
static int __change_state(da_state state);

void TerminateDaemon(int signo);

pthread_attr_t g_download_provider_thread_attr;
fd_set g_download_provider_socket_readset;
fd_set g_download_provider_socket_exceptset;

int _change_pended_download(download_clientinfo *clientinfo)
{
	if (!clientinfo)
		return -1;
	clientinfo->state = DOWNLOAD_STATE_PENDED;
	clientinfo->err = DOWNLOAD_ERROR_TOO_MANY_DOWNLOADS;
	download_provider_db_requestinfo_update_column(clientinfo, DOWNLOAD_DB_STATE);
	ipc_send_request_stateinfo(clientinfo);
	return 0;
}

void *_start_download(void *args)
{
	int da_ret = -1;
	int req_dl_id = -1;

	download_clientinfo_slot *clientinfoslot =
		(download_clientinfo_slot *) args;
	if (!clientinfoslot) {
		TRACE_DEBUG_MSG("[NULL-CHECK] download_clientinfo_slot");
		return 0;
	}
	download_clientinfo *clientinfo =
		(download_clientinfo *) clientinfoslot->clientinfo;
	if (!clientinfo) {
		TRACE_DEBUG_MSG("[NULL-CHECK] download_clientinfo");
		return 0;
	}

	CLIENT_MUTEX_LOCK(&(clientinfo->client_mutex));
	clientinfo->state = DOWNLOAD_STATE_READY;
	clientinfo->err = DOWNLOAD_ERROR_NONE;
	CLIENT_MUTEX_UNLOCK(&(clientinfo->client_mutex));

	// call start_download() of download-agent
	if (clientinfo->requestinfo->headers.rows > 0) {
		int len = 0;
		int i = 0;
		char **req_header = NULL;
		len = clientinfo->requestinfo->headers.rows;
		req_header = calloc(len, sizeof(char *));
		if (!req_header) {
			TRACE_DEBUG_MSG("fail to calloc");
			return 0;
		}
		for (i = 0; i < len; i++)
			req_header[i] =
				strdup(clientinfo->requestinfo->headers.str[i].str);
		if (clientinfo->requestinfo->install_path.length > 1) {
			if (clientinfo->requestinfo->filename.length > 1)
				da_ret =
					da_start_download_with_extension(clientinfo->requestinfo->
							url.str, &req_dl_id,
							DA_FEATURE_REQUEST_HEADER,
							req_header, &len,
							DA_FEATURE_INSTALL_PATH,
							clientinfo->requestinfo->install_path.str,
							DA_FEATURE_FILE_NAME,
							clientinfo->requestinfo->filename.str,
							DA_FEATURE_USER_DATA,
							(void *)clientinfoslot,
							NULL);
			else
				da_ret =
					da_start_download_with_extension(clientinfo->requestinfo->
							url.str, &req_dl_id,
							DA_FEATURE_REQUEST_HEADER,
							req_header, &len,
							DA_FEATURE_INSTALL_PATH,
							clientinfo->requestinfo->install_path.str,
							DA_FEATURE_USER_DATA,
							(void *)clientinfoslot,
							NULL);
		} else {
			if (clientinfo->requestinfo->filename.length > 1)
				da_ret =
					da_start_download_with_extension(clientinfo->requestinfo->
							url.str, &req_dl_id,
							DA_FEATURE_REQUEST_HEADER,
							req_header, &len,
							DA_FEATURE_FILE_NAME,
							clientinfo->requestinfo->filename.str,
							DA_FEATURE_USER_DATA,
							(void *)clientinfoslot,
							NULL);
			else
				da_ret =
					da_start_download_with_extension(clientinfo->requestinfo->
							url.str, &req_dl_id,
							DA_FEATURE_REQUEST_HEADER,
							req_header, &len,
							DA_FEATURE_USER_DATA,
							(void *)clientinfoslot,
							NULL);
		}
		for (i = 0; i < len; i++) {
			if (req_header[i])
				free(req_header[i]);
		}
	} else {
		if (clientinfo->requestinfo->install_path.length > 1) {
			if (clientinfo->requestinfo->filename.length > 1)
				da_ret =
					da_start_download_with_extension(clientinfo->requestinfo->
							url.str, &req_dl_id,
							DA_FEATURE_INSTALL_PATH,
							clientinfo->requestinfo->install_path.str,
							DA_FEATURE_FILE_NAME,
							clientinfo->requestinfo->filename.str,
							DA_FEATURE_USER_DATA,
							(void *)clientinfoslot,
							NULL);
			else
				da_ret =
					da_start_download_with_extension(clientinfo->requestinfo->
							url.str, &req_dl_id,
							DA_FEATURE_INSTALL_PATH,
							clientinfo->requestinfo->install_path.str,
							DA_FEATURE_USER_DATA,
							(void *)clientinfoslot,
							NULL);
		} else {
			if (clientinfo->requestinfo->filename.length > 1)
				da_ret =
					da_start_download_with_extension(clientinfo->requestinfo->
							url.str, &req_dl_id,
							DA_FEATURE_FILE_NAME,
							clientinfo->requestinfo->filename.str,
							DA_FEATURE_USER_DATA,
							(void *)clientinfoslot,
							NULL);
			else
				da_ret =
					da_start_download_with_extension(clientinfo->requestinfo->
							url.str, &req_dl_id,
							DA_FEATURE_USER_DATA,
							(void *)clientinfoslot,
							NULL);
		}
	}

	// if start_download() return error cause of maximun download limitation,
	// set state to DOWNLOAD_STATE_PENDED.
	if (da_ret == DA_ERR_ALREADY_MAX_DOWNLOAD) {
		TRACE_DEBUG_INFO_MSG("change to pended request [%d]", da_ret);
		CLIENT_MUTEX_LOCK(&(clientinfo->client_mutex));
		_change_pended_download(clientinfo);
		CLIENT_MUTEX_UNLOCK(&(clientinfo->client_mutex));
		return 0;
	} else if (da_ret != DA_RESULT_OK) {
		TRACE_DEBUG_INFO_MSG("Fail to request start [%d]", da_ret);
		/* FIXME : need to seperate in detail according to error return values */
		CLIENT_MUTEX_LOCK(&(clientinfo->client_mutex));
		clientinfo->state = DOWNLOAD_STATE_FAILED;
		clientinfo->err = DOWNLOAD_ERROR_INVALID_PARAMETER;
		download_provider_db_requestinfo_remove(clientinfo->
							requestinfo->requestid);
		ipc_send_request_stateinfo(clientinfo);
		CLIENT_MUTEX_UNLOCK(&(clientinfo->client_mutex));
		return 0;
	}

	CLIENT_MUTEX_LOCK(&(clientinfo->client_mutex));

	TRACE_DEBUG_INFO_MSG("started download [%d]", da_ret);

	clientinfo->req_id = req_dl_id;
	clientinfo->state = DOWNLOAD_STATE_DOWNLOADING;
	clientinfo->err = DOWNLOAD_ERROR_NONE;

	download_provider_db_requestinfo_update_column(clientinfo,
								DOWNLOAD_DB_STATE);

	// sync return  // client should be alive till this line at least.
	ipc_send_request_stateinfo(clientinfo);

	CLIENT_MUTEX_UNLOCK(&(clientinfo->client_mutex));
	return 0;
}

int _create_download_thread(download_clientinfo_slot *clientinfo_slot)
{
	if (!clientinfo_slot)
		return -1;

	// create thread for receiving the reqeust info from client.
	// and if possible, it will create the thread for listening the event.
	clientinfo_slot->clientinfo->state = DOWNLOAD_STATE_READY;
	clientinfo_slot->clientinfo->err = DOWNLOAD_ERROR_NONE;
	if (pthread_create
		(&clientinfo_slot->clientinfo->thread_pid,
		&g_download_provider_thread_attr, _start_download,
		clientinfo_slot) != 0) {
		TRACE_DEBUG_INFO_MSG("failed to call pthread_create for client");
		TRACE_DEBUG_INFO_MSG("Change to pended job");
		_change_pended_download(clientinfo_slot->clientinfo);
		return -1;
	}
	return 0;
}

int _handle_new_connection(download_clientinfo_slot *clientinfo_list, download_clientinfo *request_clientinfo)
{
	int searchslot = 0;
	unsigned active_count = 0;

	// NULL - checking
	if (!clientinfo_list || !request_clientinfo ) {
		TRACE_DEBUG_MSG("NULL-CHECK");
		return -1;
	}

	CLIENT_MUTEX_INIT(&(request_clientinfo->client_mutex), NULL);

#ifdef SO_PEERCRED
	socklen_t cr_len =
		sizeof(request_clientinfo->credentials);
	if (getsockopt
		(request_clientinfo->clientfd, SOL_SOCKET,
		SO_PEERCRED, &request_clientinfo->credentials,
		&cr_len) == 0) {
		TRACE_DEBUG_INFO_MSG
			("Client Info : pid=%d, uid=%d, gid=%d\n",
			request_clientinfo->credentials.pid,
			request_clientinfo->credentials.uid,
			request_clientinfo->credentials.gid);
	}
#endif

	download_controls type =
		ipc_receive_header(request_clientinfo->clientfd);
	TRACE_DEBUG_INFO_MSG("[ACCEPT] HEADER : [%d] ", type);
	// first of all, receive requestinfo .
	if (type <= 0 || ipc_receive_request_msg(request_clientinfo) < 0) {
		TRACE_DEBUG_MSG("Ignore this connection, Invalid command");
		clear_clientinfo(request_clientinfo);
		return -1;
	}

	if (type == DOWNLOAD_CONTROL_STOP
		|| type == DOWNLOAD_CONTROL_GET_STATE_INFO
		|| type == DOWNLOAD_CONTROL_RESUME
		|| type == DOWNLOAD_CONTROL_PAUSE) {
		// get requestid from socket.
		if (request_clientinfo->requestinfo
			&& request_clientinfo->requestinfo->requestid > 0) {
			// search requestid in slots.
			int searchindex = get_same_request_slot_index
							(clientinfo_list,
							request_clientinfo->requestinfo->requestid);
			if (type == DOWNLOAD_CONTROL_STOP) {
				TRACE_DEBUG_INFO_MSG("Request : DOWNLOAD_CONTROL_STOP");
				if (searchindex >= 0) {
					if (da_cancel_download
						(clientinfo_list[searchindex].clientinfo->req_id)
						== DA_RESULT_OK) {
						request_clientinfo->state = DOWNLOAD_STATE_STOPPED;
						request_clientinfo->err = DOWNLOAD_ERROR_NONE;
						if (clientinfo_list[searchindex].clientinfo->requestinfo
							&& clientinfo_list[searchindex].clientinfo->requestinfo->notification)
							set_downloadedinfo_appfw_notification(clientinfo_list[searchindex].clientinfo);
						download_provider_db_requestinfo_remove(request_clientinfo->requestinfo->requestid);
						download_provider_db_history_new(clientinfo_list[searchindex].clientinfo);
					} else {
						request_clientinfo->state = DOWNLOAD_STATE_FAILED;
						request_clientinfo->err = DOWNLOAD_ERROR_INVALID_PARAMETER;
					}
				} else { // no found
					request_clientinfo->state = DOWNLOAD_STATE_NONE;
					request_clientinfo->err = DOWNLOAD_ERROR_NONE;
				}
				ipc_send_stateinfo(request_clientinfo);
			} else if (type == DOWNLOAD_CONTROL_GET_STATE_INFO) {
				// search slots/downloading db/history db
				if (searchindex > 0) { // exist in slots (memory)
					request_clientinfo->state =
						clientinfo_list[searchindex].clientinfo->state;
					request_clientinfo->err =
						clientinfo_list[searchindex].clientinfo->err;
				} else {  //search downloading db or history db
					download_dbinfo* dbinfo =
						download_provider_db_get_info(
							request_clientinfo->requestinfo->requestid);
					if (dbinfo) { // found in downloading db..it means crashed job
						request_clientinfo->state = DOWNLOAD_STATE_PENDED;
						request_clientinfo->err = DOWNLOAD_ERROR_TOO_MANY_DOWNLOADS;
					} else { // no exist in downloading db
						dbinfo = download_provider_db_history_get_info(
							request_clientinfo->requestinfo->requestid);
						if (!dbinfo) { // no info anywhere
							request_clientinfo->state = DOWNLOAD_STATE_NONE;
							request_clientinfo->err = DOWNLOAD_ERROR_NONE;
						} else { //history info
							request_clientinfo->state = dbinfo->state;
							request_clientinfo->err = DOWNLOAD_ERROR_NONE;
						}
					}
					download_provider_db_info_free(dbinfo);
					free(dbinfo);
				}
				ipc_send_stateinfo(request_clientinfo);
				// estabilish the spec of return value.
			} else if (type == DOWNLOAD_CONTROL_PAUSE) {
				if (searchindex >= 0) {
					if (da_suspend_download
						(clientinfo_list[searchindex].clientinfo->req_id)
						== DA_RESULT_OK) {
						request_clientinfo->state = DOWNLOAD_STATE_PAUSE_REQUESTED;
						request_clientinfo->err = DOWNLOAD_ERROR_NONE;
					} else {
						request_clientinfo->state = DOWNLOAD_STATE_FAILED;
						request_clientinfo->err = DOWNLOAD_ERROR_INVALID_PARAMETER;
					}
				} else { // no found
					request_clientinfo->state = DOWNLOAD_STATE_NONE;
					request_clientinfo->err = DOWNLOAD_ERROR_NONE;
				}
				ipc_send_stateinfo(request_clientinfo);
			} else if (type == DOWNLOAD_CONTROL_RESUME) {
				if (searchindex >= 0) {
					if (da_resume_download
						(clientinfo_list[searchindex].clientinfo->req_id)
						== DA_RESULT_OK) {
						request_clientinfo->state = DOWNLOAD_STATE_DOWNLOADING;
						request_clientinfo->err = DOWNLOAD_ERROR_NONE;
					} else {
						request_clientinfo->state = DOWNLOAD_STATE_FAILED;
						request_clientinfo->err = DOWNLOAD_ERROR_INVALID_PARAMETER;
					}
				} else { // no found
					request_clientinfo->state = DOWNLOAD_STATE_NONE;
					request_clientinfo->err = DOWNLOAD_ERROR_NONE;
				}
				ipc_send_stateinfo(request_clientinfo);
			}
			ipc_receive_header(request_clientinfo->clientfd);
		}
		clear_clientinfo(request_clientinfo);
		return 0;
	}

	if (type != DOWNLOAD_CONTROL_START) {
		TRACE_DEBUG_MSG
			("Now, DOWNLOAD_CONTROL_START is only supported");
		clear_clientinfo(request_clientinfo);
		return -1;
	}

	// check whether requestinfo has requestid or not.
	if (request_clientinfo->requestinfo
		&& request_clientinfo->requestinfo->requestid > 0) {
		// search same request id.
		int searchindex = get_same_request_slot_index(clientinfo_list,
						request_clientinfo->requestinfo->requestid);
		if (searchindex < 0) {
			TRACE_DEBUG_INFO_MSG("Not Found Same Request ID");
			// Invalid id
			request_clientinfo->state = DOWNLOAD_STATE_FAILED;
			request_clientinfo->err = DOWNLOAD_ERROR_INVALID_PARAMETER;
			ipc_send_request_stateinfo(request_clientinfo);
			clear_clientinfo(request_clientinfo);
			return 0;
		} else {	// found request id. // how to deal etag ?
			// connect to slot.
			TRACE_DEBUG_INFO_MSG("Found Same Request ID slot[%d]", searchindex);
			CLIENT_MUTEX_LOCK(&(request_clientinfo->client_mutex));
			// close previous socket.
			if (clientinfo_list[searchindex].clientinfo->clientfd > 0)
				close(clientinfo_list[searchindex].clientinfo->clientfd);
			// change to new socket.
			clientinfo_list[searchindex].clientinfo->clientfd =
				request_clientinfo->clientfd;
			ipc_send_request_stateinfo(clientinfo_list[searchindex].clientinfo);
			// update some info.
			clientinfo_list[searchindex].clientinfo->requestinfo->callbackinfo =
				request_clientinfo->requestinfo->callbackinfo;
			clientinfo_list[searchindex].clientinfo->requestinfo->notification =
				request_clientinfo->requestinfo->notification;
			request_clientinfo->clientfd = 0;	// prevent to not be disconnected.
			CLIENT_MUTEX_UNLOCK(&(request_clientinfo->client_mutex));
			clear_clientinfo(request_clientinfo);
			return 0;
		}
	}

	// new request.
	searchslot = get_empty_slot_index(clientinfo_list);
	if (searchslot < 0) {
		TRACE_DEBUG_MSG("download-provider is busy, try later");
		clear_clientinfo(request_clientinfo);
		sleep(5);	// provider need the time of refresh.
		return -1;
	}
	// create new unique id, and insert info to DB.
	if (request_clientinfo->requestinfo
		&& request_clientinfo->requestinfo->requestid <= 0) {
		request_clientinfo->requestinfo->requestid =
			get_download_request_id();
		if (download_provider_db_requestinfo_new
			(request_clientinfo) < 0) {
			clear_clientinfo(request_clientinfo);
			sleep(5);	// provider need the time of refresh.
			return -1;
		}
	}

	clientinfo_list[searchslot].clientinfo = request_clientinfo;

	active_count = get_downloading_count(clientinfo_list);

	TRACE_DEBUG_INFO_MSG("New Connection slot [%d/%d] active [%d/%d]",
											searchslot,
											MAX_CLIENT,
											active_count,
											DA_MAX_DOWNLOAD_REQ_AT_ONCE);

	if (active_count >= DA_MAX_DOWNLOAD_REQ_AT_ONCE) {
		// deal as pended job.
		_change_pended_download(clientinfo_list[searchslot].clientinfo);
		TRACE_DEBUG_INFO_MSG ("Pended Request is saved to [%d/%d]",
			searchslot, MAX_CLIENT);
	} else {
		// Pending First
		unsigned free_slot_count
			= DA_MAX_DOWNLOAD_REQ_AT_ONCE - active_count;
		int pendedslot = get_pended_slot_index(clientinfo_list);
		if(pendedslot >= 0) {
			TRACE_DEBUG_INFO_MSG ("Free Space [%d]", free_slot_count);
			for (;free_slot_count > 0 && pendedslot >= 0; free_slot_count--) {
				TRACE_DEBUG_INFO_MSG ("Start pended job [%d]", pendedslot);
				_create_download_thread(&clientinfo_list[pendedslot]);
				pendedslot = get_pended_slot_index(clientinfo_list);
			}
		}

		if (free_slot_count <= 0) { // change to PENDED
			// start pended job, deal this job to pended
			_change_pended_download(clientinfo_list[searchslot].clientinfo);
			TRACE_DEBUG_INFO_MSG ("Pended Request is saved to [%d/%d]",
				searchslot, MAX_CLIENT);
		} else
			_create_download_thread(&clientinfo_list[searchslot]);
	}
	return 0;
}

int _handle_client_request(download_clientinfo* clientinfo)
{
	int da_ret = 0;
	int msgType = 0;

	// NULL - checking
	if (!clientinfo) {
		TRACE_DEBUG_MSG("NULL-CHECK");
		return -1;
	}

	switch (msgType = ipc_receive_header(clientinfo->clientfd)) {
	case DOWNLOAD_CONTROL_STOP:
		if (clientinfo->state >= DOWNLOAD_STATE_FINISHED) {
			// clear slot requested by client after finished download
			TRACE_DEBUG_INFO_MSG("request Free slot to main thread");
			clear_socket(clientinfo);
			break;
		}
		TRACE_DEBUG_INFO_MSG("DOWNLOAD_CONTROL_STOP");
		da_ret = da_cancel_download(clientinfo->req_id);
		CLIENT_MUTEX_LOCK(&(clientinfo->client_mutex));
		if (da_ret != DA_RESULT_OK) {
			/* FIXME : need to seperate in detail according to error return values */
			clientinfo->state = DOWNLOAD_STATE_FAILED;
			clientinfo->err = DOWNLOAD_ERROR_INVALID_PARAMETER;
			TRACE_DEBUG_MSG("Fail to request cancel [%d]", da_ret);
		} else {
			clientinfo->state = DOWNLOAD_STATE_STOPPED;
			clientinfo->err = DOWNLOAD_ERROR_NONE;
			if (clientinfo->requestinfo) {
				if (clientinfo->requestinfo->notification)
					set_downloadedinfo_appfw_notification(clientinfo);
				download_provider_db_requestinfo_remove(clientinfo->
					requestinfo->requestid);
			}
			download_provider_db_history_new(clientinfo);
		}
		ipc_send_stateinfo(clientinfo);
		CLIENT_MUTEX_UNLOCK(&(clientinfo->client_mutex));
		break;
	case DOWNLOAD_CONTROL_PAUSE:
		TRACE_DEBUG_INFO_MSG("DOWNLOAD_CONTROL_PAUSE");
		da_ret = da_suspend_download(clientinfo->req_id);
		CLIENT_MUTEX_LOCK(&(clientinfo->client_mutex));
		if (da_ret != DA_RESULT_OK) {
			/* FIXME : need to seperate in detail according to error return values */
			clientinfo->state = DOWNLOAD_STATE_FAILED;
			clientinfo->err = DOWNLOAD_ERROR_INVALID_PARAMETER;
			TRACE_DEBUG_MSG("Fail to request suspend [%d]", da_ret);
		} else {
			clientinfo->state = DOWNLOAD_STATE_PAUSE_REQUESTED;
			clientinfo->err = DOWNLOAD_ERROR_NONE;
		}
		ipc_send_stateinfo(clientinfo);
		CLIENT_MUTEX_UNLOCK(&(clientinfo->client_mutex));
		break;
	case DOWNLOAD_CONTROL_RESUME:
		TRACE_DEBUG_INFO_MSG("DOWNLOAD_CONTROL_RESUME");
		da_ret = da_resume_download(clientinfo->req_id);
		CLIENT_MUTEX_LOCK(&(clientinfo->client_mutex));
		if (da_ret != DA_RESULT_OK) {
			/* FIXME : need to seperate in detail according to error return values */
			clientinfo->state = DOWNLOAD_STATE_FAILED;
			clientinfo->err = DOWNLOAD_ERROR_INVALID_PARAMETER;
			TRACE_DEBUG_MSG("Fail to request resume [%d]", da_ret);
		} else {
			clientinfo->state = DOWNLOAD_STATE_DOWNLOADING;
			clientinfo->err = DOWNLOAD_ERROR_NONE;
		}
		ipc_send_stateinfo(clientinfo);
		CLIENT_MUTEX_UNLOCK(&(clientinfo->client_mutex));
		break;
	case DOWNLOAD_CONTROL_GET_STATE_INFO:	// sync call
		TRACE_DEBUG_INFO_MSG("DOWNLOAD_CONTROL_GET_STATE_INFO");
		CLIENT_MUTEX_LOCK(&(clientinfo->client_mutex));
		ipc_send_stateinfo(clientinfo);
		CLIENT_MUTEX_UNLOCK(&(clientinfo->client_mutex));
		break;
	case DOWNLOAD_CONTROL_GET_DOWNLOAD_INFO:	// sync call
		TRACE_DEBUG_INFO_MSG("DOWNLOAD_CONTROL_GET_DOWNLOAD_INFO");
		CLIENT_MUTEX_LOCK(&(clientinfo->client_mutex));
		ipc_send_downloadinfo(clientinfo);
		CLIENT_MUTEX_UNLOCK(&(clientinfo->client_mutex));
		break;
	case -1:
	case 0:
		TRACE_DEBUG_MSG("(Closed Socket ) terminate event thread (%d)",
			msgType);
		// bloken socket... it seems the client is dead or closed by agent thread.
		// close socket, this will break the loop. and terminate this thread.
		clear_socket(clientinfo);
		return -1;
	default:
		TRACE_DEBUG_MSG("Unknow message [%d]", msgType);
		return -1;
	}
	return 0;
}

void *run_manage_download_server(void *args)
{
	int listenfd = 0;	// main socket to be albe to listen the new connection
	int maxfd;
	int ret = 0;
	fd_set rset, exceptset;
	struct timeval timeout;
	long flexible_timeout;
	download_clientinfo_slot *clientinfo_list;
	int searchslot = 0;
	unsigned active_count = 0;
	download_clientinfo *request_clientinfo;
	int check_retry = 1;
	int i = 0;
	int is_timeout = 0;

	socklen_t clientlen;
	struct sockaddr_un listenaddr, clientaddr;

	GMainLoop *mainloop = (GMainLoop *) args;

	ret = _init_agent();
	if (ret != DOWNLOAD_ERROR_NONE) {
		TRACE_DEBUG_MSG("failed to init agent");
		TerminateDaemon(SIGTERM);
		return 0;
	}
	clear_downloadinginfo_appfw_notification();

	if ((listenfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		TRACE_DEBUG_MSG("failed to create socket");
		TerminateDaemon(SIGTERM);
		return 0;
	}

	bzero(&listenaddr, sizeof(listenaddr));
	listenaddr.sun_family = AF_UNIX;
	strcpy(listenaddr.sun_path, DOWNLOAD_PROVIDER_IPC);

	if (bind(listenfd, (struct sockaddr *)&listenaddr, sizeof listenaddr) !=
		0) {
		TRACE_DEBUG_MSG("failed to call bind");
		TerminateDaemon(SIGTERM);
		return 0;
	}

	if (chmod(listenaddr.sun_path, 0777) < 0) {
		TRACE_DEBUG_MSG
			("failed to change the permission of socket file");
		TerminateDaemon(SIGTERM);
		return 0;
	}

	if (listen(listenfd, MAX_CLIENT) != 0) {
		TRACE_DEBUG_MSG("failed to call listen");
		TerminateDaemon(SIGTERM);
		return 0;
	}

	maxfd = listenfd;
	TRACE_DEBUG_INFO_MSG("Ready to listen IPC [%d][%s]", listenfd,
			DOWNLOAD_PROVIDER_IPC);

	// allocation the array structure for managing the clients.
	clientinfo_list =
		(download_clientinfo_slot *) calloc(MAX_CLIENT,
							sizeof(download_clientinfo_slot));
	if (clientinfo_list == NULL) {
		TRACE_DEBUG_MSG("failed to allocate the memory for client list");
		TerminateDaemon(SIGTERM);
		return 0;
	}

	if (pthread_attr_init(&g_download_provider_thread_attr) != 0) {
		TRACE_DEBUG_MSG("failed to call pthread_attr_init for client");
		TerminateDaemon(SIGTERM);
		return 0;
	}
	if (pthread_attr_setdetachstate(&g_download_provider_thread_attr,
									PTHREAD_CREATE_DETACHED) != 0) {
		TRACE_DEBUG_MSG("failed to set detach option");
		TerminateDaemon(SIGTERM);
		return 0;
	}

	flexible_timeout = DOWNLOAD_PROVIDER_CARE_CLIENT_MIN_INTERVAL;

	FD_ZERO(&g_download_provider_socket_readset);
	FD_ZERO(&g_download_provider_socket_exceptset);
	FD_SET(listenfd, &g_download_provider_socket_readset);
	FD_SET(listenfd, &g_download_provider_socket_exceptset);

	while (g_main_loop_is_running(mainloop)) {

		// clean slots
		for (i = 0; i < MAX_CLIENT; i++) {
			if (!clientinfo_list[i].clientinfo)
				continue;
			// clear slot.
			if (clientinfo_list[i].clientinfo->state >= DOWNLOAD_STATE_FINISHED) {
				if (clientinfo_list[i].clientinfo->clientfd <= 0)
					clear_clientinfoslot(&clientinfo_list[i]);
				continue;
			}
		}

		is_timeout = 1;
		rset = g_download_provider_socket_readset;
		exceptset = g_download_provider_socket_exceptset;

		memset(&timeout, 0x00, sizeof(struct timeval));
		timeout.tv_sec = flexible_timeout;

		if (select((maxfd + 1), &rset, 0, &exceptset, &timeout) < 0) {
			TRACE_DEBUG_MSG
				("select error, provider can't receive any request from client.");
			TerminateDaemon(SIGTERM);
			break;
		}

		for (i = 0; i < MAX_CLIENT; i++) {  // find the socket received the packet.
			if (!clientinfo_list[i].clientinfo)
				continue;
			//Even if no socket, downloading should be progressed.
			if (clientinfo_list[i].clientinfo->clientfd <= 0)
				continue;
			if (FD_ISSET(clientinfo_list[i].clientinfo->clientfd, &rset) > 0) {
				// ignore it is not started yet.
				if (clientinfo_list[i].clientinfo->state <= DOWNLOAD_STATE_READY)
					continue;
				TRACE_DEBUG_INFO_MSG("FD_ISSET [%d] readset slot[%d]",
					clientinfo_list[i].clientinfo->clientfd, i);
				_handle_client_request(clientinfo_list[i].clientinfo);
				if (is_timeout)
					is_timeout = 0;
			} else if (FD_ISSET(clientinfo_list[i].clientinfo->clientfd, &exceptset) > 0) {
				TRACE_DEBUG_MSG("FD_ISSET [%d] exceptset slot[%d]", clientinfo_list[i].clientinfo->clientfd, i);
				clear_clientinfoslot(&clientinfo_list[i]);
			}
		} // MAX_CLIENT

		if (FD_ISSET(listenfd, &exceptset) > 0) {
			TRACE_DEBUG_MSG("meet listenfd Exception of socket");
			TerminateDaemon(SIGTERM);
			break;
		} else if (FD_ISSET(listenfd, &rset) > 0) { // new connection
			TRACE_DEBUG_INFO_MSG("FD_ISSET listenfd rset");
			if (is_timeout)
				is_timeout = 0;
			// reset timeout.
			flexible_timeout =
				DOWNLOAD_PROVIDER_CARE_CLIENT_MIN_INTERVAL;
			// ready the buffer.
			request_clientinfo =
				(download_clientinfo *) calloc(1,
							sizeof(download_clientinfo));
			if (!request_clientinfo) {
				TRACE_DEBUG_MSG
					("download-provider can't allocate the memory, try later");
				clientlen = sizeof(clientaddr);
				int clientfd = accept(listenfd,
					(struct sockaddr *)&clientaddr, &clientlen);
				close(clientfd);	// disconnect.
				sleep(5);	// provider need the time of refresh.
				continue;
			}
			// accept client.
			clientlen = sizeof(clientaddr);
			request_clientinfo->clientfd = accept(listenfd,
								(struct sockaddr*)&clientaddr,
								&clientlen);
			if (request_clientinfo->clientfd < 0) {
				clear_clientinfo(request_clientinfo);
				sleep(5);	// provider need the time of refresh.
				continue;
			}
			if (_handle_new_connection(clientinfo_list, request_clientinfo) < 0) {
				sleep(1);
				continue;
			}
			// after starting the download by DA, event thread will start to get the event from client.
			if (request_clientinfo && request_clientinfo->clientfd > 0) {
				FD_SET(request_clientinfo->clientfd, &g_download_provider_socket_readset);	// add new descriptor to set
				FD_SET(request_clientinfo->clientfd, &g_download_provider_socket_exceptset);
				if (request_clientinfo->clientfd > maxfd )
					maxfd = request_clientinfo->clientfd;	/* for select */
			}
		}

		if (is_timeout) { // timeout
			// If there is better solution to be able to know
			// the number of downloading threads, replace below rough codes.
			active_count = get_downloading_count(clientinfo_list);
			// check whether the number of downloading is already maximum.
			if (active_count >= DA_MAX_DOWNLOAD_REQ_AT_ONCE)
				continue;

			unsigned free_slot_count
					= DA_MAX_DOWNLOAD_REQ_AT_ONCE - active_count;
			int pendedslot = get_pended_slot_index(clientinfo_list);
			if(pendedslot >= 0) {
				TRACE_DEBUG_INFO_MSG ("Free Space [%d]", free_slot_count);
				for (;free_slot_count > 0 && pendedslot >= 0; free_slot_count--) {
					TRACE_DEBUG_INFO_MSG ("start pended job [%d]", pendedslot);
					if (_create_download_thread(&clientinfo_list[pendedslot]) > 0)
						active_count++;
					pendedslot = get_pended_slot_index(clientinfo_list);
				}
			}

			if (check_retry && free_slot_count > 0) {
				// Auto re-download feature. 
				// ethernet may be connected with other downloading items.
				if (get_network_status() < 0)
					continue;

				// check auto-retrying list regardless state. pended state is also included to checking list.
				int i = 0;
				download_dbinfo_list *db_list =
					download_provider_db_get_list(DOWNLOAD_STATE_NONE);
				if (!db_list || db_list->count <= 0) {
					TRACE_DEBUG_INFO_MSG
						("provider does not need to check DB anymore. in this life.");
					// provider does not need to check DB anymore. in this life.
					check_retry = 0;
					if (db_list)
						download_provider_db_list_free(db_list);
					continue;
				}
				for (i = 0;
					active_count <
					DA_MAX_DOWNLOAD_REQ_AT_ONCE
					&& i < db_list->count; i++) {
					if (db_list->item[i].requestid <= 0)
						continue;
					if (get_same_request_slot_index
						(clientinfo_list,db_list->item[i].requestid) < 0) {
						// not found requestid in memory
						TRACE_DEBUG_INFO_MSG
							("Retry download [%d]",
							db_list->item[i].requestid);
						//search empty slot. copy db info to slot.
						searchslot =
							get_empty_slot_index(clientinfo_list);
						if (searchslot < 0) {
							TRACE_DEBUG_INFO_MSG
								("download-provider is busy, try later");
							flexible_timeout =
								flexible_timeout * 2;
							break;
						}
						// allocte requestinfo to empty slot.
						request_clientinfo =
							(download_clientinfo *)
							calloc(1, sizeof(download_clientinfo));
						if (!request_clientinfo)
							continue;
						request_clientinfo->requestinfo
							=
							download_provider_db_get_requestinfo
							(&db_list->item[i]);
						if (!request_clientinfo->requestinfo) {
							free(request_clientinfo);
							request_clientinfo = NULL;
							continue;
						}

						CLIENT_MUTEX_INIT(&(request_clientinfo->client_mutex), NULL);
						request_clientinfo->state = DOWNLOAD_STATE_READY;
						clientinfo_list[searchslot].clientinfo =
							request_clientinfo;

						TRACE_DEBUG_INFO_MSG
							("Retry download [%d/%d][%d/%d]",
							searchslot, MAX_CLIENT,
							active_count,
							DA_MAX_DOWNLOAD_REQ_AT_ONCE);
						if (_create_download_thread(&clientinfo_list[searchslot]) > 0)
							active_count++;
					}
				}
				if (i >= db_list->count)	// do not search anymore.
					check_retry = 0;
				download_provider_db_list_free(db_list);
			}

			// save system resource (CPU)
			if (check_retry == 0 && active_count == 0
				&& flexible_timeout <
				DOWNLOAD_PROVIDER_CARE_CLIENT_MAX_INTERVAL)
				flexible_timeout = flexible_timeout * 2;
			if (flexible_timeout >
				DOWNLOAD_PROVIDER_CARE_CLIENT_MAX_INTERVAL)
				flexible_timeout =
					DOWNLOAD_PROVIDER_CARE_CLIENT_MAX_INTERVAL;
			TRACE_DEBUG_INFO_MSG("Next Timeout after [%ld] sec",
					flexible_timeout);

		} // if (i >= MAX_CLIENT) { // timeout
	}

	FD_CLR(listenfd, &rset);
	FD_CLR(listenfd, &exceptset);
	FD_CLR(listenfd, &g_download_provider_socket_readset);
	FD_CLR(listenfd, &g_download_provider_socket_exceptset);

	// close accept socket.
	if (listenfd)
		close(listenfd);

	_deinit_agent();

	// close all sockets for client. .. 
	// client thread will terminate by itself through catching this closing.
	for (searchslot = 0; searchslot < MAX_CLIENT; searchslot++)
		if (clientinfo_list[searchslot].clientinfo)
			clear_clientinfoslot(&clientinfo_list[searchslot]);

	if (clientinfo_list)
		free(clientinfo_list);

	pthread_exit(NULL);
	return 0;
}

void __download_info_cb(user_download_info_t *download_info, void *user_data)
{
	int len = 0;
	if (!user_data) {
		TRACE_DEBUG_MSG("[CRITICAL] clientinfoslot is NULL");
		return;
	}
	download_clientinfo_slot *clientinfoslot =
		(download_clientinfo_slot *) user_data;
	download_clientinfo *clientinfo =
		(download_clientinfo *) clientinfoslot->clientinfo;
	if (!clientinfo) {
		TRACE_DEBUG_MSG("[CRITICAL] clientinfo is NULL");
		return;
	}
	TRACE_DEBUG_INFO_MSG("id[%d],size[%lu]",
			download_info->da_dl_req_id, download_info->file_size);

	if (clientinfo->req_id != download_info->da_dl_req_id) {
		TRACE_DEBUG_MSG("[CRITICAL] req_id[%d] da_dl_req_id[%d}",
				clientinfo->req_id,
				download_info->da_dl_req_id);
		return;
	}
	CLIENT_MUTEX_LOCK(&(clientinfo->client_mutex));
	if (!clientinfo->downloadinfo)
		clientinfo->downloadinfo =
			(download_content_info *) calloc(1, sizeof(download_content_info));
	if (clientinfo->downloadinfo)
		clientinfo->downloadinfo->file_size = download_info->file_size;
	if (download_info->file_type) {
		TRACE_DEBUG_INFO_MSG("mime[%s]", download_info->file_type);

		len = strlen(download_info->file_type);
		if (len > (DP_MAX_STR_LEN - 1))
			len = DP_MAX_STR_LEN - 1;
		if (clientinfo->downloadinfo) {
			strncpy(clientinfo->downloadinfo->mime_type,
				download_info->file_type, len);
			download_provider_db_requestinfo_update_column
				(clientinfo, DOWNLOAD_DB_MIMETYPE);
		}
	}
	if (download_info->tmp_saved_path) {
		char *str = NULL;
		TRACE_DEBUG_INFO_MSG("tmp path[%s]", download_info->tmp_saved_path);
		clientinfo->tmp_saved_path =
			strdup(download_info->tmp_saved_path);
		download_provider_db_requestinfo_update_column(clientinfo,
									DOWNLOAD_DB_SAVEDPATH);
		str = strrchr(download_info->tmp_saved_path, '/');
		if (str) {
			str++;
			len = strlen(str);
			if (len > (DP_MAX_STR_LEN - 1))
				len = DP_MAX_STR_LEN - 1;
			if (clientinfo->downloadinfo) {
				strncpy(clientinfo->downloadinfo->content_name,
					str, len);
				download_provider_db_requestinfo_update_column
					(clientinfo, DOWNLOAD_DB_FILENAME);
				TRACE_DEBUG_INFO_MSG("content_name[%s]",
						clientinfo->downloadinfo->
						content_name);
			}
		}
	}

	if (clientinfo->requestinfo->callbackinfo.started)
		ipc_send_downloadinfo(clientinfo);

	CLIENT_MUTEX_UNLOCK(&(clientinfo->client_mutex));
}

void __downloading_info_cb(user_downloading_info_t *download_info,
			   void *user_data)
{
	int len = 0;
	if (!user_data) {
		TRACE_DEBUG_MSG("[CRITICAL] clientinfoslot is NULL");
		return;
	}
	download_clientinfo_slot *clientinfoslot =
		(download_clientinfo_slot *) user_data;
	download_clientinfo *clientinfo =
		(download_clientinfo *) clientinfoslot->clientinfo;
	if (!clientinfo) {
		TRACE_DEBUG_MSG("[CRITICAL] clientinfo is NULL");
		return;
	}
	CLIENT_MUTEX_LOCK(&(clientinfo->client_mutex));
	if (clientinfo->req_id != download_info->da_dl_req_id) {
		TRACE_DEBUG_MSG("[CRITICAL] req_id[%d] da_dl_req_id[%d}",
				clientinfo->req_id,
				download_info->da_dl_req_id);
		CLIENT_MUTEX_UNLOCK(&(clientinfo->client_mutex));
		return;
	}
	if (!clientinfo->downloadinginfo)
		clientinfo->downloadinginfo = (downloading_state_info *) calloc(1,
									sizeof(downloading_state_info));
	if (clientinfo->downloadinginfo)
		clientinfo->downloadinginfo->received_size =
			download_info->total_received_size;
	if (download_info->saved_path) {
		TRACE_DEBUG_INFO_MSG("tmp path[%s]", download_info->saved_path);
		len = strlen(download_info->saved_path);
		if (len > (DP_MAX_PATH_LEN - 1))
			len = DP_MAX_PATH_LEN - 1;
		if (clientinfo->downloadinginfo)
			strncpy(clientinfo->downloadinginfo->saved_path,
				download_info->saved_path, len);
		/* FIXME : This should be reviewd again after smack rules is applied */
		if (chown(clientinfo->downloadinginfo->saved_path,
				clientinfo->credentials.uid,
				clientinfo->credentials.gid) < 0)
			TRACE_DEBUG_INFO_MSG("Fail to chown [%s]", strerror(errno));
	}

	static size_t updated_second;
	time_t tt = time(NULL);
	struct tm *localTime = localtime(&tt);

	if (updated_second != localTime->tm_sec || download_info->saved_path) {	// every 1 second.
		if (clientinfo->requestinfo
			&& clientinfo->requestinfo->notification)
			set_downloadinginfo_appfw_notification(clientinfo);
		if (clientinfo->requestinfo->callbackinfo.progress)
			ipc_send_downloadinginfo(clientinfo);
		updated_second = localTime->tm_sec;
	}
	CLIENT_MUTEX_UNLOCK(&(clientinfo->client_mutex));
}

void __notify_cb(user_notify_info_t *notify_info, void *user_data)
{
	if (!user_data) {
		TRACE_DEBUG_MSG("[CRITICAL] clientinfoslot is NULL");
		return;
	}
	download_clientinfo_slot *clientinfoslot =
		(download_clientinfo_slot *) user_data;
	download_clientinfo *clientinfo =
		(download_clientinfo *) clientinfoslot->clientinfo;
	if (!clientinfo) {
		TRACE_DEBUG_MSG("[CRITICAL] clientinfo is NULL");
		return;
	}

	TRACE_DEBUG_INFO_MSG("id[%d],state[%d],err[%d]",
			notify_info->da_dl_req_id,
			notify_info->state, notify_info->err);
	if (clientinfo->req_id != notify_info->da_dl_req_id) {
		TRACE_DEBUG_MSG("[CRITICAL] req_id[%d] da_dl_req_id[%d}",
				clientinfo->req_id, notify_info->da_dl_req_id);
		return;
	}

	CLIENT_MUTEX_LOCK(&(clientinfo->client_mutex));

	clientinfo->state = __change_state(notify_info->state);
	clientinfo->err = __change_error(notify_info->err);
	if (clientinfo->state == DOWNLOAD_STATE_FINISHED ||
			clientinfo->state == DOWNLOAD_STATE_FAILED) {
		if (clientinfo->requestinfo) {
			if (clientinfo->requestinfo->notification)
				set_downloadedinfo_appfw_notification(clientinfo);
			download_provider_db_requestinfo_remove(clientinfo->
				requestinfo->requestid);
		}
		download_provider_db_history_new(clientinfo);
		TRACE_DEBUG_INFO_MSG("[TEST]Finish clientinfo[%p], fd[%d]",
			clientinfo, clientinfo->clientfd);
	}

	TRACE_DEBUG_INFO_MSG("state[%d]", clientinfo->state);
	ipc_send_stateinfo(clientinfo);

	CLIENT_MUTEX_UNLOCK(&(clientinfo->client_mutex));
}

int __change_state(da_state state)
{
	int ret = DOWNLOAD_STATE_NONE;
	switch (state) {
	case DA_STATE_WAITING:
	case DA_STATE_DOWNLOAD_STARTED:
		TRACE_DEBUG_INFO_MSG("DA_STATE_DOWNLOAD_STARTED");
		ret = DOWNLOAD_STATE_READY;
		break;
	case DA_STATE_DOWNLOADING:
		TRACE_DEBUG_INFO_MSG("DA_STATE_DOWNLOADING");
		ret = DOWNLOAD_STATE_DOWNLOADING;
		break;
	case DA_STATE_DOWNLOAD_COMPLETE:
		TRACE_DEBUG_INFO_MSG("DA_STATE_COMPLETE");
		ret = DOWNLOAD_STATE_INSTALLING;
		break;
	case DA_STATE_CANCELED:
		TRACE_DEBUG_INFO_MSG("DA_STATE_CANCELED");
		ret = DOWNLOAD_STATE_STOPPED;
		break;
	case DA_STATE_CANCELED_ALL:
		TRACE_DEBUG_INFO_MSG("DA_STATE_CANCELED_ALL");
		break;
	case DA_STATE_SUSPENDED:
		TRACE_DEBUG_INFO_MSG("DA_STATE_SUSPENDED");
		ret = DOWNLOAD_STATE_PAUSED;
		break;
	case DA_STATE_SUSPENDED_ALL:
		TRACE_DEBUG_INFO_MSG("DA_STATE_SUSPENDED_ALL");
		break;
	case DA_STATE_RESUMED:
		TRACE_DEBUG_INFO_MSG("DA_STATE_RESUMED");
		ret = DOWNLOAD_STATE_DOWNLOADING;
		break;
	case DA_STATE_RESUMED_ALL:
		TRACE_DEBUG_INFO_MSG("DA_STATE_RESUMED_ALL");
		break;
	case DA_STATE_FINISHED:
		TRACE_DEBUG_INFO_MSG("DA_STATE_FINISHED");
		ret = DOWNLOAD_STATE_FINISHED;
		break;
	case DA_STATE_FAILED:
		TRACE_DEBUG_INFO_MSG("DA_STATE_FAILED");
		ret = DOWNLOAD_STATE_FAILED;
		break;
	default:
		break;
	}
	return ret;
}

int __change_error(int err)
{
	int ret = DOWNLOAD_ERROR_UNKOWN;
	switch (err) {
	case DA_RESULT_OK:
		ret = DOWNLOAD_ERROR_NONE;
		break;
	case DA_ERR_INVALID_ARGUMENT:
		ret = DOWNLOAD_ERROR_INVALID_PARAMETER;
		break;
	case DA_ERR_FAIL_TO_MEMALLOC:
		ret = DOWNLOAD_ERROR_OUT_OF_MEMORY;
		break;
	case DA_ERR_UNREACHABLE_SERVER:
		ret = DOWNLOAD_ERROR_NETWORK_UNREACHABLE;
		break;
	case DA_ERR_HTTP_TIMEOUT:
		ret = DOWNLOAD_ERROR_CONNECTION_TIMED_OUT;
		break;
	case DA_ERR_DISK_FULL:
		ret = DOWNLOAD_ERROR_NO_SPACE;
		break;
	case DA_ERR_INVALID_STATE:
		ret = DOWNLOAD_ERROR_INVALID_STATE;
		break;
	case DA_ERR_NETWORK_FAIL:
		ret = DOWNLOAD_ERROR_CONNECTION_FAILED;
		break;
	case DA_ERR_INVALID_URL:
		ret = DOWNLOAD_ERROR_INVALID_URL;
		break;
	case DA_ERR_INVALID_INSTALL_PATH:
		ret = DOWNLOAD_ERROR_INVALID_DESTINATION;
		break;
	case DA_ERR_ALREADY_MAX_DOWNLOAD:
		ret = DOWNLOAD_ERROR_TOO_MANY_DOWNLOADS;
		break;
	case DA_ERR_FAIL_TO_INSTALL_FILE:
		ret = DOWNLOAD_ERROR_INSTALL_FAIL;
		break;
	case DA_ERR_FAIL_TO_CREATE_THREAD:
	case DA_ERR_FAIL_TO_OBTAIN_MUTEX:
	case DA_ERR_FAIL_TO_ACCESS_FILE:
	case DA_ERR_FAIL_TO_GET_CONF_VALUE:
	case DA_ERR_FAIL_TO_ACCESS_STORAGE:
	case DA_ERR_DLOPEN_FAIL:
		ret = DOWNLOAD_ERROR_IO_ERROR;
		break;
	}
	return ret;
}

int _init_agent()
{
	int da_ret = 0;
	da_client_cb_t da_cb = {
		__notify_cb,
		__download_info_cb,
		__downloading_info_cb
	};
	da_ret = da_init(&da_cb, DA_DOWNLOAD_MANAGING_METHOD_AUTO);
	if (da_ret != DA_RESULT_OK) {
		return DOWNLOAD_ERROR_FAIL_INIT_AGENT;
	}
	return DOWNLOAD_ERROR_NONE;
}

void _deinit_agent()
{
	da_deinit();
}
