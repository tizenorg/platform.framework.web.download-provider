#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <net_connection.h>

#include "download-provider-config.h"
#include "download-provider-notification.h"
#include "download-provider-pthread.h"
#include "download-provider-log.h"

extern fd_set g_download_provider_socket_readset;
extern fd_set g_download_provider_socket_exceptset;

int get_download_request_id(void)
{
	int uniquetime = 0;
	struct timeval tval;
	static int last_uniquetime = 0;

	do {
		uniquetime = (int)time(NULL);
		gettimeofday(&tval, NULL);
		if (tval.tv_usec == 0)
			uniquetime = uniquetime + (tval.tv_usec + 1) % 0xfffff;
		else
			uniquetime = uniquetime + tval.tv_usec;
		TRACE_DEBUG_INFO_MSG("ID : %d", uniquetime);
	} while (last_uniquetime == uniquetime);
	last_uniquetime = uniquetime;	// store
	return uniquetime;
}

void clear_socket(download_clientinfo *clientinfo)
{
	if (!clientinfo)
		return;
	if (clientinfo->clientfd) {
		FD_CLR(clientinfo->clientfd, &g_download_provider_socket_readset);
		FD_CLR(clientinfo->clientfd, &g_download_provider_socket_exceptset);
		shutdown(clientinfo->clientfd, 0);
		fdatasync(clientinfo->clientfd);
		close(clientinfo->clientfd);
		clientinfo->clientfd = 0;
	}
}

void clear_clientinfo(download_clientinfo *clientinfo)
{
	TRACE_DEBUG_INFO_MSG("[TEST] clear [%p]",clientinfo);
	// clear this slot
	if (!clientinfo)
		return;

	clear_socket(clientinfo);

	CLIENT_MUTEX_LOCK(&(clientinfo->client_mutex));
	if (clientinfo->requestinfo) {
		clientinfo->requestinfo->requestid = 0;
		if (clientinfo->requestinfo->client_packagename.length > 0
			&& clientinfo->requestinfo->client_packagename.str)
			free(clientinfo->requestinfo->client_packagename.str);
		clientinfo->requestinfo->client_packagename.str = NULL;
		if (clientinfo->requestinfo->url.str)
			free(clientinfo->requestinfo->url.str);
		clientinfo->requestinfo->url.str = NULL;
		if (clientinfo->requestinfo->install_path.str)
			free(clientinfo->requestinfo->install_path.str);
		clientinfo->requestinfo->install_path.str = NULL;
		if (clientinfo->requestinfo->filename.str)
			free(clientinfo->requestinfo->filename.str);
		clientinfo->requestinfo->filename.str = NULL;
		if (clientinfo->requestinfo->service_data.str)
			free(clientinfo->requestinfo->service_data.str);
		clientinfo->requestinfo->service_data.str = NULL;
		if (clientinfo->requestinfo->headers.rows) {
			int i = 0;
			for (i = 0; i < clientinfo->requestinfo->headers.rows;
				i++) {
				if (clientinfo->requestinfo->headers.str[i].str)
					free(clientinfo->requestinfo->headers.
						str[i].str);
				clientinfo->requestinfo->headers.str[i].str =
					NULL;
			}
			free(clientinfo->requestinfo->headers.str);
			clientinfo->requestinfo->headers.str = NULL;
		}
		free(clientinfo->requestinfo);
		clientinfo->requestinfo = NULL;
	}
	if (clientinfo->downloadinginfo)
		free(clientinfo->downloadinginfo);
	clientinfo->downloadinginfo = NULL;
	if (clientinfo->downloadinfo)
		free(clientinfo->downloadinfo);
	clientinfo->downloadinfo = NULL;
	if (clientinfo->tmp_saved_path)
		free(clientinfo->tmp_saved_path);
	clientinfo->tmp_saved_path = NULL;
	if (clientinfo->ui_notification_handle || clientinfo->service_handle)
		destroy_appfw_notification(clientinfo);
	CLIENT_MUTEX_UNLOCK(&(clientinfo->client_mutex));
	CLIENT_MUTEX_DESTROY(&(clientinfo->client_mutex));
	memset(clientinfo, 0x00, sizeof(download_clientinfo));
	free(clientinfo);
	clientinfo = NULL;
}

void clear_clientinfoslot(download_clientinfo_slot *clientinfoslot)
{
	// clear this slot
	if (!clientinfoslot)
		return;
	download_clientinfo *clientinfo =
		(download_clientinfo *) clientinfoslot->clientinfo;
	clear_clientinfo(clientinfo);
	clientinfoslot->clientinfo = NULL;
}

int get_downloading_count(download_clientinfo_slot *clientinfo_list)
{
	int count = 0;
	int i = 0;
	for (i = 0; i < MAX_CLIENT; i++) {
		if (clientinfo_list[i].clientinfo) {
			if (clientinfo_list[i].clientinfo->state ==
				DOWNLOAD_STATE_DOWNLOADING
				|| clientinfo_list[i].clientinfo->state ==
				DOWNLOAD_STATE_INSTALLING
				|| clientinfo_list[i].clientinfo->state ==
				DOWNLOAD_STATE_READY)
				count++;
		}
	}
	return count;
}

int get_same_request_slot_index(download_clientinfo_slot *clientinfo_list,
				int requestid)
{
	int i = 0;

	if (!clientinfo_list || !requestid)
		return -1;

	for (i = 0; i < MAX_CLIENT; i++) {
		if (clientinfo_list[i].clientinfo
			&& clientinfo_list[i].clientinfo->requestinfo) {
			if (clientinfo_list[i].clientinfo->requestinfo->
				requestid == requestid) {
				return i;
			}
		}
	}
	return -1;
}

int get_empty_slot_index(download_clientinfo_slot *clientinfo_list)
{
	int i = 0;

	if (!clientinfo_list)
		return -1;

	for (i = 0; i < MAX_CLIENT; i++)
		if (!clientinfo_list[i].clientinfo)
			return i;
	return -1;
}

int get_pended_slot_index(download_clientinfo_slot *clientinfo_list)
{
	int i = 0;

	if (!clientinfo_list)
		return -1;

	for (i = 0; i < MAX_CLIENT; i++)
		if (clientinfo_list[i].clientinfo
			&& clientinfo_list[i].clientinfo->state
				== DOWNLOAD_STATE_PENDED)
			return i;
	return -1;
}

int get_network_status()
{
	connection_h network_handle = NULL;
	if (connection_create(&network_handle) < 0) {
		TRACE_DEBUG_MSG("Failed connection_create");
		return -1;
	}

	connection_ethernet_state_e system_network_state
		= CONNECTION_ETHERNET_STATE_DISCONNECTED;
	if (connection_get_ethernet_state(network_handle,
		&system_network_state) != CONNECTION_ERROR_NONE)
		TRACE_DEBUG_MSG("Failed connection_get_ethernet_state");
	TRACE_DEBUG_INFO_MSG
		("ethernet check result : [%d]", (int)system_network_state);

	connection_cellular_state_e system_cellular_state
		= CONNECTION_CELLULAR_STATE_OUT_OF_SERVICE;
	if (connection_get_cellular_state(network_handle,
		&system_cellular_state) != CONNECTION_ERROR_NONE)
		TRACE_DEBUG_MSG("Failed connection_get_ethernet_state");
	TRACE_DEBUG_INFO_MSG
		("cellula check result : [%d]", (int)system_cellular_state);

	connection_wifi_state_e system_wifi_state
		= CONNECTION_WIFI_STATE_DEACTIVATED;
	if (connection_get_wifi_state(network_handle,
		&system_wifi_state) != CONNECTION_ERROR_NONE)
		TRACE_DEBUG_MSG("Failed connection_get_ethernet_state");
	TRACE_DEBUG_INFO_MSG
		("wifi check result : [%d]", (int)system_wifi_state);

	if (connection_destroy(network_handle) != CONNECTION_ERROR_NONE)
		TRACE_DEBUG_MSG("Failed connection_destroy");

	if (!(system_network_state == CONNECTION_ETHERNET_STATE_CONNECTED
		|| system_cellular_state == CONNECTION_CELLULAR_STATE_AVAILABLE
		|| system_wifi_state == CONNECTION_WIFI_STATE_CONNECTED))
		return -1;
	return 0;
	}
