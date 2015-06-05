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
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <signal.h>

#include <download-provider-log.h>
#include <download-provider-config.h>
#include <download-provider-ipc.h>
#include <download-provider-network.h>
#include <download-provider-client.h>
#include <download-provider-pthread.h>
#include <download-provider-notify.h>
#include <download-provider-notification-manager.h>
#include <download-provider-queue-manager.h>
#include <download-provider-client-manager.h>
#include <download-provider-db-defs.h>
#include <download-provider-db.h>
#include <download-provider-plugin-download-agent.h>
#include <download-provider-smack.h>

char *dp_print_state(int state)
{
	switch(state)
	{
		case DP_STATE_NONE :
			return "NONE";
		case DP_STATE_READY :
			return "READY";
		case DP_STATE_QUEUED :
			return "QUEUED";
		case DP_STATE_CONNECTING :
			return "CONNECTING";
		case DP_STATE_DOWNLOADING :
			return "DOWNLOADING";
		case DP_STATE_PAUSED :
			return "PAUSED";
		case DP_STATE_COMPLETED :
			return "COMPLETED";
		case DP_STATE_CANCELED :
			return "CANCELED";
		case DP_STATE_FAILED :
			return "FAILED";
		default :
			break;
	}
	return "UNKNOWN";
}

char *dp_print_errorcode(int errorcode)
{
	switch(errorcode)
	{
		case DP_ERROR_NONE :
			return "NONE";
		case DP_ERROR_INVALID_PARAMETER :
			return "INVALID_PARAMETER";
		case DP_ERROR_OUT_OF_MEMORY :
			return "OUT_OF_MEMORY";
		case DP_ERROR_IO_ERROR :
			return "IO_ERROR";
		case DP_ERROR_NETWORK_UNREACHABLE :
			return "NETWORK_UNREACHABLE";
		case DP_ERROR_CONNECTION_TIMED_OUT :
			return "CONNECTION_TIMED_OUT";
		case DP_ERROR_NO_SPACE :
			return "NO_SPACE";
		case DP_ERROR_FIELD_NOT_FOUND :
			return "FIELD_NOT_FOUND";
		case DP_ERROR_INVALID_STATE :
			return "INVALID_STATE";
		case DP_ERROR_CONNECTION_FAILED :
			return "CONNECTION_FAILED";
		case DP_ERROR_INVALID_URL :
			return "INVALID_URL";
		case DP_ERROR_INVALID_DESTINATION :
			return "INVALID_DESTINATION";
		case DP_ERROR_QUEUE_FULL :
			return "QUEUE_FULL";
		case DP_ERROR_ALREADY_COMPLETED :
			return "ALREADY_COMPLETED";
		case DP_ERROR_FILE_ALREADY_EXISTS :
			return "FILE_ALREADY_EXISTS";
		case DP_ERROR_TOO_MANY_DOWNLOADS :
			return "TOO_MANY_DOWNLOADS";
		case DP_ERROR_NO_DATA :
			return "NO_DATA";
		case DP_ERROR_UNHANDLED_HTTP_CODE :
			return "UNHANDLED_HTTP_CODE";
		case DP_ERROR_CANNOT_RESUME :
			return "CANNOT_RESUME";
		case DP_ERROR_PERMISSION_DENIED :
			return "PERMISSION_DENIED";
		case DP_ERROR_RESPONSE_TIMEOUT :
			return "RESPONSE_TIMEOUT";
		case DP_ERROR_REQUEST_TIMEOUT :
			return "REQUEST_TIMEOUT";
		case DP_ERROR_SYSTEM_DOWN :
			return "SYSTEM_DOWN";
		case DP_ERROR_CLIENT_DOWN :
			return "CLIENT_DOWN";
		case DP_ERROR_DISK_BUSY:
			return "DISK_BUSY";
		case DP_ERROR_ID_NOT_FOUND :
			return "ID_NOT_FOUND";
		default :
			break;
	}
	return "UNKNOWN";
}

char *dp_print_section(short section)
{
	switch (section) {
	case DP_SEC_NONE:
		return "NONE";
	case DP_SEC_INIT:
		return "INIT";
	case DP_SEC_DEINIT:
		return "DEINIT";
	case DP_SEC_CONTROL:
		return "CONTROL";
	case DP_SEC_GET:
		return "GET";
	case DP_SEC_SET:
		return "SET";
	case DP_SEC_UNSET:
		return "UNSET";
	default:
		break;
	}
	return "UNKNOWN";
}

char *dp_print_property(unsigned property)
{
	switch (property) {
	case DP_PROP_NONE:
		return "NONE";
	case DP_PROP_CREATE:
		return "CREATE";
	case DP_PROP_START:
		return "START";
	case DP_PROP_PAUSE:
		return "PAUSE";
	case DP_PROP_CANCEL:
		return "CANCEL";
	case DP_PROP_DESTROY:
		return "DESTROY";
	case DP_PROP_URL:
		return "URL";
	case DP_PROP_DESTINATION:
		return "DESTINATION";
	case DP_PROP_FILENAME:
		return "FILENAME";
	case DP_PROP_STATE_CALLBACK:
		return "STATE_CB";
	case DP_PROP_PROGRESS_CALLBACK:
		return "PROGRESS_CB";
	case DP_PROP_AUTO_DOWNLOAD:
		return "AUTO_DOWNLOAD";
	case DP_PROP_NETWORK_TYPE:
		return "NETWORK_TYPE";
	case DP_PROP_NETWORK_BONDING:
		return "NETWORK_BONDING";
	case DP_PROP_SAVED_PATH:
		return "SAVED_PATH";
	case DP_PROP_TEMP_SAVED_PATH:
		return "TEMP_SAVED_PATH";
	case DP_PROP_MIME_TYPE:
		return "MIME_TYPE";
	case DP_PROP_RECEIVED_SIZE:
		return "RECEIVED_SIZE";
	case DP_PROP_TOTAL_FILE_SIZE:
		return "TOTAL_FILE_SIZE";
	case DP_PROP_CONTENT_NAME:
		return "CONTENT_NAME";
	case DP_PROP_HTTP_STATUS:
		return "HTTP_STATUS";
	case DP_PROP_ETAG:
		return "ETAG";
	case DP_PROP_STATE:
		return "STATE";
	case DP_PROP_ERROR:
		return "ERROR";
	case DP_PROP_NOTIFICATION_RAW:
		return "NOTIFICATION_RAW";
	case DP_PROP_NOTIFICATION_SUBJECT:
		return "NOTIFICATION_SUBJECT";
	case DP_PROP_NOTIFICATION_DESCRIPTION:
		return "NOTIFICATION_DESCRIPTION";
	case DP_PROP_NOTIFICATION_TYPE:
		return "NOTIFICATION_TYPE";
	case DP_PROP_HTTP_HEADERS:
		return "HTTP_HEADERS";
	case DP_PROP_HTTP_HEADER:
		return "HTTP_HEADER";
	default:
		break;
	}
	return "UNKNOWN";
}

static int __dp_get_download_id(dp_client_fmt *client)
{
	int download_id = -1;
	static int last_download_id = 0;
	int check_duplicate = 0;
	int errorcode = DP_ERROR_NONE;

	do {
		do {
			struct timeval tval;
			int cipher = 1;
			int c = 0;

			download_id = -1;
			gettimeofday(&tval, NULL);

			int usec = tval.tv_usec;
			for (c = 0; ; c++, cipher++) {
				if ((usec /= 10) <= 0)
					break;
			}
			if (tval.tv_usec == 0)
				tval.tv_usec = (tval.tv_sec & 0x0fff);
			int disit_unit = 10;
			for (c = 0; c < cipher - 3; c++)
				disit_unit = disit_unit * 10;
			download_id = tval.tv_sec + ((tval.tv_usec << 2) * 100) +
					((tval.tv_usec >> (cipher - 1)) * disit_unit) +
					((tval.tv_usec + (tval.tv_usec % 10)) & 0x0fff);
		} while (last_download_id == download_id);
		last_download_id = download_id;
		check_duplicate = dp_db_check_duplicated_int(client->dbhandle,
				DP_TABLE_LOGGING, DP_DB_COL_ID, download_id, &errorcode);
		if (errorcode != DP_ERROR_NONE) {
			TRACE_ERROR("ERROR [%d]", errorcode);
		}
	} while (check_duplicate != 0); // means duplicated id
	return download_id;
}

void dp_request_create(dp_client_fmt *client, dp_request_fmt *request)
{
	// new request
	// find the tail of linked list & check limitation
	int i = 0;
	dp_request_fmt *tailp = client->requests;
	dp_request_fmt *prevp = NULL;
	for (; i < MAX_DOWNLOAD_HANDLE; i++) {
		if (tailp == NULL)
			break;
		TRACE_DEBUG("link %d info: id:%d access-time:%d", i, tailp->id, tailp->access_time);
		prevp = tailp;
		tailp = tailp->next;
	}
	request->next = NULL;
	if (prevp == NULL) // it's first link
		client->requests = request;
	else
		prevp->next = request; // attach at the tail

	TRACE_DEBUG("sock:%d download-id:%d", client->channel, request->id);
}

static int __dp_request_create(dp_client_fmt *client, dp_ipc_fmt *ipc_info)
{
	int errorcode = DP_ERROR_NONE;

	int download_id = __dp_get_download_id(client);

	// allocation new request.
	dp_request_fmt *request = (dp_request_fmt *) calloc(1, sizeof(dp_request_fmt));
	if (request == NULL) {
		TRACE_ERROR("check memory sock:%d download-id:%d", client->channel, download_id);
		return DP_ERROR_OUT_OF_MEMORY;
	}

	if (dp_db_new_logging(client->dbhandle, download_id, DP_STATE_READY, DP_ERROR_NONE, &errorcode) < 0) {
		TRACE_ERROR("new log sock:%d download-id:%d", client->channel, download_id);
		free(request);
		return DP_ERROR_DISK_BUSY;
	}

	request->id = download_id;
	request->agent_id = -1;
	request->state = DP_STATE_READY;
	request->error = DP_ERROR_NONE;
	request->network_type = DP_NETWORK_ALL;
	request->access_time = (int)time(NULL);
	request->state_cb = 0;
	request->progress_cb = 0;
	request->startcount = 0;
	request->noti_type = DP_NOTIFICATION_TYPE_NONE;
	request->progress_lasttime = 0;
	request->received_size = 0;
	request->content_type = DP_CONTENT_UNKNOWN;
	request->file_size = 0;
	request->noti_priv_id = -1;

	dp_request_create(client, request);
	ipc_info->id = download_id;
	return errorcode;
}

void dp_request_free(dp_request_fmt *request)
{
	// free notification handle here
	TRACE_DEBUG("destory id:%d", request->id);
	free(request);
}

void dp_client_clear_requests(void *slotp)
{
	dp_client_slots_fmt *slot = (dp_client_slots_fmt *)slotp;
	if (slot == NULL) {
		TRACE_ERROR("check slot memory");
		return ;
	}
	dp_client_fmt *client = &slot->client;
	if (client == NULL) {
		TRACE_ERROR("check client memory");
		return ;
	}

	int now_time = (int)time(NULL);
	int i = 0;
	unsigned queued_count = 0;
	unsigned ongoing_count = 0;
	dp_request_fmt *tailp = client->requests;
	dp_request_fmt *prevp = NULL;
	for (; tailp != NULL; i++) {

		unsigned can_unload = 0;
		if (tailp->id <= 0 || tailp->state == DP_STATE_NONE) {
			TRACE_ERROR("id:%d unexpected request (%ld/%ld)", tailp->id, tailp->access_time, now_time);
			can_unload = 1;
		} else if (tailp->access_time > 0 &&
				(now_time - tailp->access_time) > DP_CARE_CLIENT_CLEAR_INTERVAL) {
			// check accesstime. if difference is bigger than DP_CARE_CLIENT_CLEAR_INTERVAL, clear.

			if (tailp->state == DP_STATE_READY ||
					tailp->state == DP_STATE_COMPLETED ||
					tailp->state == DP_STATE_CANCELED ||
					tailp->state == DP_STATE_FAILED) {
				can_unload = 1;
			} else if (tailp->state == DP_STATE_CONNECTING) { // it take 120 sec over to connect. it means zombie.
				TRACE_ERROR("id:%d connection timeout (%ld/%ld)", tailp->id, tailp->access_time, now_time);
				if (dp_cancel_agent_download_without_update(tailp->agent_id) < 0) {
					TRACE_ERROR("failed to cancel download(%d) id:%d", tailp->agent_id, tailp->id);
				}
				tailp->state = DP_STATE_FAILED;
				tailp->error = DP_ERROR_CONNECTION_TIMED_OUT;
				if (tailp->noti_type == DP_NOTIFICATION_TYPE_COMPLETE_ONLY ||
						tailp->noti_type == DP_NOTIFICATION_TYPE_ALL) {
					if (dp_notification_manager_push_notification(slot, tailp, DP_NOTIFICATION) < 0) {
						TRACE_ERROR("failed to register notification for id:%d", tailp->id);
					}
				}
			}
		} else if (tailp->state == DP_STATE_PAUSED &&
				dp_is_alive_download(tailp->agent_id) == 0) {
			// paused & agent_id not exist.... unload from memory.
			TRACE_ERROR("id:%d hanged as paused (%ld/%ld)", tailp->id, tailp->access_time, now_time);
			can_unload = 1;
		}

		if (can_unload == 1) {
			dp_request_fmt *removep = tailp;
			if (prevp == NULL) // first request.
				client->requests = tailp->next;
			else
				prevp->next = tailp->next;
			tailp = tailp->next;
			TRACE_DEBUG("request %d clear: id:%d state:%s", i, removep->id, dp_print_state(removep->state));
			dp_request_free(removep);
			continue;
		} else {
			ongoing_count++;
		}

		if (tailp->state == DP_STATE_QUEUED)
			queued_count++;

		prevp = tailp;
		tailp = tailp->next;
	}
	TRACE_DEBUG("info requests:%d queued:%d", ongoing_count, queued_count);
	if (queued_count > 0)
		dp_queue_manager_wake_up();
}

int dp_request_destroy(dp_client_fmt *client, dp_ipc_fmt *ipc_info, dp_request_fmt *requestp)
{
	int errorcode = DP_ERROR_NONE;

	if (requestp != NULL && client->requests != NULL) {
		if (requestp == client->requests) {
			// cancel downloading ... after checking status
			client->requests = requestp->next;
			dp_request_free(requestp);
		} else {
			int i = 1;
			dp_request_fmt *prevp = client->requests;
			dp_request_fmt *removep = client->requests->next;
			for (; i < MAX_DOWNLOAD_HANDLE; i++) {
				if (removep == NULL) {
					errorcode = DP_ERROR_ID_NOT_FOUND;
					break;
				}
				if (removep == requestp) {
					// cancel downloading ... after checking status
					prevp->next = removep->next;
					dp_request_free(removep);
					break;
				}
				prevp = removep;
				removep = removep->next;
			}
		}
	}

	TRACE_DEBUG("sock:%d id:%d errorcode:%s", client->channel,
		(ipc_info) ? ipc_info->id : -1, dp_print_errorcode(errorcode));

	return errorcode;
}

static int __dp_request_read_int(int sock, dp_ipc_fmt *ipc_info, int *value)
{
	int errorcode = DP_ERROR_NONE;
	if (ipc_info->size == sizeof(int)) {
		if (dp_ipc_read(sock, value, ipc_info->size, __FUNCTION__) < 0) {
			TRACE_ERROR("sock:%d check ipc length:%d", sock, ipc_info->size);
			errorcode = DP_ERROR_IO_ERROR;
		}
	} else {
		errorcode = DP_ERROR_IO_ERROR;
	}
	return errorcode;
}

static int __dp_request_feedback_string(int sock, dp_ipc_fmt *ipc_info, void *string, size_t length, int errorvalue)
{
	int errorcode = DP_ERROR_NONE;

	if (length == 0 && errorvalue == DP_ERROR_NONE)
		errorvalue = DP_ERROR_NO_DATA;

	if (dp_ipc_query(sock, ipc_info->id, ipc_info->section, ipc_info->property, errorvalue, length * sizeof(char)) < 0) {
		errorcode = DP_ERROR_IO_ERROR;
		TRACE_ERROR("sock:%d check ipc length:%d", sock, length);
	}
	if (errorvalue == DP_ERROR_NONE && errorcode == DP_ERROR_NONE) {
		if (dp_ipc_write(sock, string, sizeof(char) * length) < 0) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("sock:%d check ipc length:%d", sock, length);
		}
	}
	return errorcode;
}

static int __dp_request_read_string(int sock, dp_ipc_fmt *ipc_info, char **string)
{
	int errorcode = DP_ERROR_NONE;
	if (ipc_info->size > 0) {
		char *recv_str = (char *)calloc((ipc_info->size + (size_t)1), sizeof(char));
		if (recv_str == NULL) {
			TRACE_STRERROR("sock:%d check memory length:%d", sock, ipc_info->size);
			errorcode = DP_ERROR_OUT_OF_MEMORY;
		} else {
			if (dp_ipc_read(sock, recv_str, ipc_info->size, __FUNCTION__) <= 0) {
				TRACE_ERROR("sock:%d check ipc length:%d", sock, ipc_info->size);
				errorcode = DP_ERROR_IO_ERROR;
				free(recv_str);
			} else {
				recv_str[ipc_info->size] = '\0';
				TRACE_DEBUG("sock:%d length:%d string:%s", sock, ipc_info->size, recv_str);
				*string = recv_str;
			}
		}
	} else {
		errorcode = DP_ERROR_IO_ERROR;
	}
	return errorcode;
}

static int __dp_request_feedback_int(int sock, dp_ipc_fmt *ipc_info, void *value, int errorvalue, size_t extra_size)
{
	int errorcode = DP_ERROR_NONE;
	if (errorvalue != DP_ERROR_NONE)
		extra_size = 0;
	if (dp_ipc_query(sock, ipc_info->id, ipc_info->section, ipc_info->property, errorvalue, extra_size) < 0) {
		errorcode = DP_ERROR_IO_ERROR;
		TRACE_ERROR("sock:%d check ipc length:%d", sock, extra_size);
	}
	if (errorvalue == DP_ERROR_NONE && errorcode == DP_ERROR_NONE) {
		if (dp_ipc_write(sock, value, extra_size) < 0) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("sock:%d check ipc length:%d", sock, extra_size);
		}
	}
	return errorcode;
}

static int __dp_request_get_info(dp_client_fmt *client, dp_ipc_fmt *ipc_info, dp_request_fmt *requestp)
{
	int errorcode = DP_ERROR_NONE;
	switch (ipc_info->property) {
	case DP_PROP_URL:
	{
		char *string = NULL;
		unsigned length = 0;
		if (dp_db_get_property_string(client->dbhandle, ipc_info->id, DP_TABLE_REQUEST, DP_DB_COL_URL, (unsigned char **)&string, &length, &errorcode) < 0) {
			TRACE_ERROR("failed to get %d", dp_print_property(ipc_info->property));
			errorcode = DP_ERROR_NO_DATA;
		}
		int result = __dp_request_feedback_string(client->channel, ipc_info, string, length, errorcode);
		if (result == DP_ERROR_IO_ERROR) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", client->channel);
		}
		free(string);
		break;
	}
	case DP_PROP_DESTINATION:
	{
		char *string = NULL;
		unsigned length = 0;
		if (dp_db_get_property_string(client->dbhandle, ipc_info->id, DP_TABLE_REQUEST, DP_DB_COL_DESTINATION, (unsigned char **)&string, &length, &errorcode) < 0) {
			TRACE_ERROR("failed to get %d", dp_print_property(ipc_info->property));
			errorcode = DP_ERROR_NO_DATA;
		}
		int result = __dp_request_feedback_string(client->channel, ipc_info, string, length, errorcode);
		if (result == DP_ERROR_IO_ERROR) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", client->channel);
		}
		free(string);
		break;
	}
	case DP_PROP_FILENAME:
	{
		char *string = NULL;
		unsigned length = 0;
		if (dp_db_get_property_string(client->dbhandle, ipc_info->id, DP_TABLE_REQUEST, DP_DB_COL_FILENAME, (unsigned char **)&string, &length, &errorcode) < 0) {
			TRACE_ERROR("failed to get %d", dp_print_property(ipc_info->property));
			errorcode = DP_ERROR_NO_DATA;
		}
		int result = __dp_request_feedback_string(client->channel, ipc_info, string, length, errorcode);
		if (result == DP_ERROR_IO_ERROR) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", client->channel);
		}
		free(string);
		break;
	}
	case DP_PROP_STATE_CALLBACK:
	{
		int callback = 0;
		if (requestp != NULL) {
			callback = requestp->state_cb;
		} else {
			if (dp_db_get_property_int(client->dbhandle, ipc_info->id, DP_TABLE_REQUEST, DP_DB_COL_STATE_EVENT, &callback, &errorcode) < 0) {
				TRACE_ERROR("failed to get %d", dp_print_property(ipc_info->property));
				errorcode = DP_ERROR_NO_DATA;
			}
		}
		int result = __dp_request_feedback_int(client->channel, ipc_info, (void *)&callback, errorcode, sizeof(int));
		if (result == DP_ERROR_IO_ERROR) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", client->channel);
		}
		break;
	}
	case DP_PROP_PROGRESS_CALLBACK:
	{
		int callback = 0;
		if (requestp != NULL) {
			callback = requestp->progress_cb;
		} else {
			if (dp_db_get_property_int(client->dbhandle, ipc_info->id, DP_TABLE_REQUEST, DP_DB_COL_PROGRESS_EVENT, &callback, &errorcode) < 0) {
				TRACE_ERROR("failed to get %d", dp_print_property(ipc_info->property));
				errorcode = DP_ERROR_NO_DATA;
			}
		}
		int result = __dp_request_feedback_int(client->channel, ipc_info, (void *)&callback, errorcode, sizeof(int));
		if (result == DP_ERROR_IO_ERROR) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", client->channel);
		}
		break;
	}
	case DP_PROP_AUTO_DOWNLOAD:
	{
		int autodownload = 0;
		if (dp_db_get_property_int(client->dbhandle, ipc_info->id, DP_TABLE_LOGGING, DP_DB_COL_AUTO_DOWNLOAD, &autodownload, &errorcode) < 0) {
			TRACE_ERROR("failed to get %d", dp_print_property(ipc_info->property));
			errorcode = DP_ERROR_NO_DATA;
		}
		int result = __dp_request_feedback_int(client->channel, ipc_info, (void *)&autodownload, errorcode, sizeof(int));
		if (result == DP_ERROR_IO_ERROR) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", client->channel);
		}
		break;
	}
	case DP_PROP_NETWORK_TYPE:
	{
		int network = 0;
		if (requestp != NULL) {
			network = requestp->network_type;
		} else {
			if (dp_db_get_property_int(client->dbhandle, ipc_info->id, DP_TABLE_REQUEST, DP_DB_COL_NETWORK_TYPE, &network, &errorcode) < 0) {
				TRACE_ERROR("failed to get %d", dp_print_property(ipc_info->property));
				errorcode = DP_ERROR_NO_DATA;
			}
		}
		int result = __dp_request_feedback_int(client->channel, ipc_info, (void *)&network, errorcode, sizeof(int));
		if (result == DP_ERROR_IO_ERROR) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", client->channel);
		}
		break;
	}
	case DP_PROP_NETWORK_BONDING:
	{
		int network_bonding = 0;
		if (dp_db_get_property_int(client->dbhandle, ipc_info->id, DP_TABLE_REQUEST, DP_DB_COL_NETWORK_BONDING, &network_bonding, &errorcode) < 0) {
			TRACE_ERROR("failed to get %d", dp_print_property(ipc_info->property));
			errorcode = DP_ERROR_NO_DATA;
		}
		int result = __dp_request_feedback_int(client->channel, ipc_info, (void *)&network_bonding, errorcode, sizeof(int));
		if (result == DP_ERROR_IO_ERROR) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", client->channel);
		}
		break;
	}
	case DP_PROP_SAVED_PATH:
	{
		char *string = NULL;
		unsigned length = 0;
		if (dp_db_get_property_string(client->dbhandle, ipc_info->id, DP_TABLE_DOWNLOAD, DP_DB_COL_SAVED_PATH, (unsigned char **)&string, &length, &errorcode) < 0) {
			TRACE_ERROR("failed to get %d", dp_print_property(ipc_info->property));
			errorcode = DP_ERROR_NO_DATA;
		}
		int result = __dp_request_feedback_string(client->channel, ipc_info, string, length, errorcode);
		if (result == DP_ERROR_IO_ERROR) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", client->channel);
		}
		free(string);
		break;
	}
	case DP_PROP_TEMP_SAVED_PATH:
	{
		char *string = NULL;
		unsigned length = 0;
		if (dp_db_get_property_string(client->dbhandle, ipc_info->id, DP_TABLE_DOWNLOAD, DP_DB_COL_TMP_SAVED_PATH, (unsigned char **)&string, &length, &errorcode) < 0) {
			TRACE_ERROR("failed to get %d", dp_print_property(ipc_info->property));
			errorcode = DP_ERROR_NO_DATA;
		}
		int result = __dp_request_feedback_string(client->channel, ipc_info, string, length, errorcode);
		if (result == DP_ERROR_IO_ERROR) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", client->channel);
		}
		free(string);
		break;
	}
	case DP_PROP_MIME_TYPE:
	{
		char *string = NULL;
		unsigned length = 0;
		if (dp_db_get_property_string(client->dbhandle, ipc_info->id, DP_TABLE_DOWNLOAD, DP_DB_COL_MIMETYPE, (unsigned char **)&string, &length, &errorcode) < 0) {
			TRACE_ERROR("failed to get %d", dp_print_property(ipc_info->property));
			errorcode = DP_ERROR_NO_DATA;
		}
		int result = __dp_request_feedback_string(client->channel, ipc_info, string, length, errorcode);
		if (result == DP_ERROR_IO_ERROR) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", client->channel);
		}
		free(string);
		break;
	}
	case DP_PROP_RECEIVED_SIZE:
	{
		unsigned long long recv_size = 0;
		if (requestp != NULL) {
			recv_size = requestp->received_size;
		} else {
			errorcode = DP_ERROR_INVALID_STATE;
		}
		int result = __dp_request_feedback_int(client->channel, ipc_info, (void *)&recv_size, errorcode, sizeof(unsigned long long));
		if (result == DP_ERROR_IO_ERROR) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", client->channel);
		}
		break;
	}
	case DP_PROP_TOTAL_FILE_SIZE:
	{
		unsigned long long file_size = 0;
		if (requestp != NULL) {
			file_size = requestp->file_size;
		} else {
			// load content_size(INT64) from database;
			if (dp_db_get_property_int(client->dbhandle, ipc_info->id, DP_TABLE_DOWNLOAD, DP_DB_COL_CONTENT_SIZE, &file_size, &errorcode) < 0) {
				TRACE_ERROR("failed to get %d", dp_print_property(ipc_info->property));
				errorcode = DP_ERROR_NO_DATA;
			}
		}
		int result = __dp_request_feedback_int(client->channel, ipc_info, (void *)&file_size, errorcode, sizeof(unsigned long long));
		if (result == DP_ERROR_IO_ERROR) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", client->channel);
		}
		break;
	}
	case DP_PROP_CONTENT_NAME:
	{
		char *string = NULL;
		unsigned length = 0;
		if (dp_db_get_property_string(client->dbhandle, ipc_info->id, DP_TABLE_DOWNLOAD, DP_DB_COL_CONTENT_NAME, (unsigned char **)&string, &length, &errorcode) < 0) {
			TRACE_ERROR("failed to get %d", dp_print_property(ipc_info->property));
			errorcode = DP_ERROR_NO_DATA;
		}
		int result = __dp_request_feedback_string(client->channel, ipc_info, string, length, errorcode);
		if (result == DP_ERROR_IO_ERROR) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", client->channel);
		}
		free(string);
		break;
	}
	case DP_PROP_HTTP_STATUS:
	{
		int httpstatus = 0;
		if (dp_db_get_property_int(client->dbhandle, ipc_info->id, DP_TABLE_DOWNLOAD, DP_DB_COL_HTTP_STATUS, &httpstatus, &errorcode) < 0) {
			TRACE_ERROR("failed to get %d", dp_print_property(ipc_info->property));
			errorcode = DP_ERROR_NO_DATA;
		}
		int result = __dp_request_feedback_int(client->channel, ipc_info, (void *)&httpstatus, errorcode, sizeof(int));
		if (result == DP_ERROR_IO_ERROR) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", client->channel);
		}
		break;
	}
	case DP_PROP_ETAG:
	{
		char *string = NULL;
		unsigned length = 0;
		if (dp_db_get_property_string(client->dbhandle, ipc_info->id, DP_TABLE_DOWNLOAD, DP_DB_COL_ETAG, (unsigned char **)&string, &length, &errorcode) < 0) {
			TRACE_ERROR("failed to get %d", dp_print_property(ipc_info->property));
			errorcode = DP_ERROR_NO_DATA;
		}
		int result = __dp_request_feedback_string(client->channel, ipc_info, string, length, errorcode);
		if (result == DP_ERROR_IO_ERROR) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", client->channel);
		}
		free(string);
		break;
	}
	case DP_PROP_STATE:
	{
		int state = DP_STATE_NONE;
		if (requestp != NULL) {
			state = requestp->state;
		} else {
			if (dp_db_get_property_int(client->dbhandle, ipc_info->id, DP_TABLE_LOGGING, DP_DB_COL_STATE, &state, &errorcode) < 0) {
				TRACE_ERROR("failed to get %d", dp_print_property(ipc_info->property));
				errorcode = DP_ERROR_ID_NOT_FOUND;
			}
		}
		int result = __dp_request_feedback_int(client->channel, ipc_info, (void *)&state, errorcode, sizeof(int));
		if (result == DP_ERROR_IO_ERROR) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", client->channel);
		}
		break;
	}
	case DP_PROP_ERROR:
	{
		int errorvalue = DP_ERROR_NONE;
		if (requestp != NULL) {
			errorvalue = requestp->error;
		} else {
			if (dp_db_get_property_int(client->dbhandle, ipc_info->id, DP_TABLE_LOGGING, DP_DB_COL_ERRORCODE, &errorvalue, &errorcode) < 0) {
				TRACE_ERROR("failed to get %d", dp_print_property(ipc_info->property));
				errorcode = DP_ERROR_ID_NOT_FOUND;
			}
		}
		int result = __dp_request_feedback_int(client->channel, ipc_info, (void *)&errorvalue, errorcode, sizeof(int));
		if (result == DP_ERROR_IO_ERROR) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", client->channel);
		}
		break;
	}
	case DP_PROP_NOTIFICATION_TYPE:
	{
		int noti_type = 0;
		if (requestp != NULL) {
			noti_type = requestp->noti_type;
			// if already notification, unregister from notification bar.
		} else {
			if (dp_db_get_property_int(client->dbhandle, ipc_info->id, DP_TABLE_NOTIFICATION, DP_DB_COL_NOTI_TYPE, &noti_type, &errorcode) < 0) {
				TRACE_ERROR("failed to get %d", dp_print_property(ipc_info->property));
				errorcode = DP_ERROR_NO_DATA;
			}
		}
		int result = __dp_request_feedback_int(client->channel, ipc_info, (void *)&noti_type, errorcode, sizeof(int));
		if (result == DP_ERROR_IO_ERROR) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", client->channel);
		}
		break;
	}
	case DP_PROP_NOTIFICATION_SUBJECT:
	{
		char *string = NULL;
		unsigned length = 0;
		if (dp_db_get_property_string(client->dbhandle, ipc_info->id, DP_TABLE_NOTIFICATION, DP_DB_COL_NOTI_SUBJECT, (unsigned char **)&string, &length, &errorcode) < 0) {
			TRACE_ERROR("failed to get %d", dp_print_property(ipc_info->property));
			errorcode = DP_ERROR_NO_DATA;
		}
		int result = __dp_request_feedback_string(client->channel, ipc_info, string, length, errorcode);
		if (result == DP_ERROR_IO_ERROR) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", client->channel);
		}
		free(string);
		break;
	}
	case DP_PROP_NOTIFICATION_DESCRIPTION:
	{
		char *string = NULL;
		unsigned length = 0;
		if (dp_db_get_property_string(client->dbhandle, ipc_info->id, DP_TABLE_NOTIFICATION, DP_DB_COL_NOTI_DESCRIPTION, (unsigned char **)&string, &length, &errorcode) < 0) {
			TRACE_ERROR("failed to get %d", dp_print_property(ipc_info->property));
			errorcode = DP_ERROR_NO_DATA;
		}
		int result = __dp_request_feedback_string(client->channel, ipc_info, string, length, errorcode);
		if (result == DP_ERROR_IO_ERROR) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", client->channel);
		}
		free(string);
		break;
	}
	case DP_PROP_NOTIFICATION_RAW: // read type, send raw binary for type
	{
		int bundle_type = -1;
		errorcode = __dp_request_read_int(client->channel, ipc_info, &bundle_type);
		TRACE_DEBUG("read %s type:%d", dp_print_property(ipc_info->property), bundle_type);
		char *raw_column = NULL;
		if (bundle_type == DP_NOTIFICATION_BUNDLE_TYPE_ONGOING)
			raw_column = DP_DB_COL_NOTI_RAW_ONGOING;
		else if (bundle_type == DP_NOTIFICATION_BUNDLE_TYPE_COMPLETE)
			raw_column = DP_DB_COL_NOTI_RAW_COMPLETE;
		else if (bundle_type == DP_NOTIFICATION_BUNDLE_TYPE_FAILED)
			raw_column = DP_DB_COL_NOTI_RAW_FAIL;

		if (raw_column == NULL) {
			errorcode = DP_ERROR_INVALID_PARAMETER;
			TRACE_ERROR("invalid type %d type:%d", dp_print_property(ipc_info->property), bundle_type);
			if (dp_ipc_query(client->channel, ipc_info->id, ipc_info->section, ipc_info->property, errorcode, 0) < 0) {
				errorcode = DP_ERROR_IO_ERROR;
				TRACE_ERROR("check ipc sock:%d", client->channel);
			}
		}
		if (errorcode == DP_ERROR_NONE) {
			unsigned char *raws_buffer = NULL;
			unsigned length = 0;
			// get blob binary from database by raw_column
			if (dp_db_get_property_string(client->dbhandle, ipc_info->id, DP_TABLE_NOTIFICATION, raw_column, &raws_buffer, &length, &errorcode) < 0) {
				TRACE_ERROR("failed to get %d", dp_print_property(ipc_info->property));
				errorcode = DP_ERROR_NO_DATA;
			}
			int result = __dp_request_feedback_string(client->channel, ipc_info, raws_buffer, (size_t)length, errorcode);
			if (result == DP_ERROR_IO_ERROR) {
				errorcode = DP_ERROR_IO_ERROR;
				TRACE_ERROR("check ipc sock:%d", client->channel);
			}
			free(raws_buffer);
		}
		break;
	}
	case DP_PROP_HTTP_HEADERS:
	{
		// 1. response
		// 2. send the number of header fields by id
		// 3. send response & field string for each fields
		int field_count = dp_db_check_duplicated_int(client->dbhandle, DP_TABLE_HEADERS, DP_DB_COL_ID, ipc_info->id, &errorcode);
		if (field_count < 0 ) {
			TRACE_ERROR("failed to get %d", dp_print_property(ipc_info->property));
			errorcode = DP_ERROR_DISK_BUSY;
			field_count = 0;
		}
		int result = __dp_request_feedback_int(client->channel, ipc_info, (void *)&field_count, errorcode, sizeof(int));
		if (result == DP_ERROR_IO_ERROR) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", client->channel);
		} else if (field_count > 0) {
			// get fields from database.
			int *ids = (int *)calloc(field_count, sizeof(int));
			if (ids == NULL) {
				TRACE_ERROR("failed to allocate the clients");
				errorcode = DP_ERROR_OUT_OF_MEMORY;
			} else {
				// getting ids of clients
				int i = 0;
				int rows_count = dp_db_get_cond_ids(client->dbhandle, DP_TABLE_HEADERS, DP_DB_COL_ROW_ID, DP_DB_COL_ID, ipc_info->id, ids, field_count, &errorcode);
				for (; i < rows_count; i++) {
					char *string = NULL;
					unsigned length = 0;
					if (dp_db_get_cond_string(client->dbhandle, DP_TABLE_HEADERS, DP_DB_COL_ROW_ID, ids[i], DP_DB_COL_HEADER_FIELD, (unsigned char **)&string, &length, &errorcode) < 0) {
						TRACE_ERROR("failed to get %d", dp_print_property(ipc_info->property));
						errorcode = DP_ERROR_NO_DATA;
					}
					int result = __dp_request_feedback_string(client->channel, ipc_info, string, length, errorcode);
					free(string);
					if (result == DP_ERROR_IO_ERROR) {
						errorcode = DP_ERROR_IO_ERROR;
						TRACE_ERROR("check ipc sock:%d", client->channel);
						break;
					}
				}
			}
		}

		break;
	}
	case DP_PROP_HTTP_HEADER:
	{
		// 1. read field string
		// 2. response with extra size
		// 3. send string.
		char *header_field = NULL;
		char *string = NULL;
		unsigned length = 0;
		errorcode = __dp_request_read_string(client->channel, ipc_info, &header_field);
		if (errorcode == DP_ERROR_NONE && header_field != NULL) {
			if (dp_db_get_header_value(client->dbhandle, ipc_info->id, header_field, (unsigned char **)&string, &length, &errorcode) < 0) {
				TRACE_ERROR("failed to get %s", dp_print_property(ipc_info->property));
				errorcode = DP_ERROR_NO_DATA;
			}
		} else {
			if (errorcode != DP_ERROR_NONE) {
				TRACE_ERROR("failed to set %s, error:%s", dp_print_property(ipc_info->property), dp_print_errorcode(errorcode));
			}
			if (header_field == NULL) {
				TRACE_ERROR("failed to set %s", dp_print_property(ipc_info->property));
				errorcode = DP_ERROR_INVALID_PARAMETER;
			}
		}
		int result = __dp_request_feedback_string(client->channel, ipc_info, string, length, errorcode);
		if (result == DP_ERROR_IO_ERROR) {
			errorcode = DP_ERROR_IO_ERROR;
			TRACE_ERROR("check ipc sock:%d", client->channel);
		}
		free(header_field);
		free(string);
		break;
	}
	default:
		errorcode = DP_ERROR_INVALID_PARAMETER;
		break;
	}
	return errorcode;
}

static int __dp_request_set_info(dp_client_slots_fmt *slot, dp_ipc_fmt *ipc_info, dp_request_fmt *requestp)
{
	if (slot == NULL) {
		TRACE_ERROR("check slot memory");
		return DP_ERROR_INVALID_PARAMETER;
	}
	dp_client_fmt *client = &slot->client;
	if (client == NULL || ipc_info == NULL) {
		TRACE_ERROR("check client or ipc info.");
		return DP_ERROR_INVALID_PARAMETER;
	}

	int errorcode = DP_ERROR_NONE;

	// if completed or downloading, invalid state.
	int download_state = DP_STATE_NONE;
	if (requestp != NULL) {
		download_state = requestp->state;
	} else {
		if (dp_db_get_property_int(client->dbhandle, ipc_info->id, DP_TABLE_LOGGING, DP_DB_COL_STATE, &download_state, &errorcode) < 0) {
			TRACE_ERROR("failed to get %s", dp_print_property(ipc_info->property));
			errorcode = DP_ERROR_ID_NOT_FOUND;
			// feedback
			if (dp_ipc_query(client->channel, ipc_info->id, DP_SEC_SET,
					ipc_info->property, errorcode, 0) < 0) {
				TRACE_ERROR("check ipc sock:%d", client->channel);
			}
			return errorcode;
		}
	}
	// should the state be checked ?
	TRACE_DEBUG("state:%s set property:%s", dp_print_state(download_state), dp_print_property(ipc_info->property));

	switch (ipc_info->property) {
	case DP_PROP_URL:
	{
		char *recv_str = NULL;
		errorcode = __dp_request_read_string(client->channel, ipc_info, &recv_str);
		if (errorcode == DP_ERROR_NONE) {
			if (recv_str == NULL) {
				errorcode = DP_ERROR_INVALID_PARAMETER;
			} else {
				// write to database here
				if (dp_db_replace_property(client->dbhandle, ipc_info->id, DP_TABLE_REQUEST, DP_DB_COL_URL, (void *)recv_str, ipc_info->size, 2, &errorcode) < 0) {
					TRACE_ERROR("failed to set %s", dp_print_property(ipc_info->property));
					errorcode = DP_ERROR_DISK_BUSY;
				}
				free(recv_str);
			}
		}
		break;
	}
	case DP_PROP_DESTINATION:
	{
		char *recv_str = NULL;
		errorcode = __dp_request_read_string(client->channel, ipc_info, &recv_str);
		if (errorcode == DP_ERROR_NONE) {
			if (recv_str == NULL) {
				errorcode = DP_ERROR_INVALID_PARAMETER;
			} else {
				if (dp_smack_is_mounted() == 1) {
					// check here destination is available. with checking smack
					char *smack_label = dp_db_get_client_smack_label(slot->pkgname);
					if (smack_label == NULL) {
						TRACE_SECURE_ERROR("[SMACK][%d] no label", ipc_info->id);
						errorcode = DP_ERROR_PERMISSION_DENIED;
					} else if (dp_is_valid_dir(recv_str) != 0) {
						errorcode = DP_ERROR_INVALID_DESTINATION;
					} else if (dp_smack_is_valid_dir(slot->credential.uid, slot->credential.gid, smack_label, recv_str) != 0) {
						errorcode = DP_ERROR_PERMISSION_DENIED;
					}
					free(smack_label);
				}
				if (errorcode == DP_ERROR_NONE &&
						dp_db_replace_property(client->dbhandle, ipc_info->id, DP_TABLE_REQUEST, DP_DB_COL_DESTINATION, (void *)recv_str, ipc_info->size, 2, &errorcode) < 0) {
					TRACE_ERROR("failed to set %s", dp_print_property(ipc_info->property));
					errorcode = DP_ERROR_DISK_BUSY;
				}
				free(recv_str);
			}
		}
		break;
	}
	case DP_PROP_TEMP_SAVED_PATH:
	{
		char *recv_str = NULL;
		errorcode = __dp_request_read_string(client->channel, ipc_info, &recv_str);
		if (errorcode == DP_ERROR_NONE) {
			if (recv_str == NULL) {
				errorcode = DP_ERROR_INVALID_PARAMETER;
			} else {
				if (dp_db_replace_property(client->dbhandle, ipc_info->id, DP_TABLE_REQUEST, DP_DB_COL_TEMP_FILE_PATH, (void *)recv_str, ipc_info->size, 2, &errorcode) < 0) {
					TRACE_ERROR("failed to set %s", dp_print_property(ipc_info->property));
					errorcode = DP_ERROR_DISK_BUSY;
				}
				free(recv_str);
			}
		}
		break;
	}
	case DP_PROP_FILENAME:
	{
		char *recv_str = NULL;
		errorcode = __dp_request_read_string(client->channel, ipc_info, &recv_str);
		if (errorcode == DP_ERROR_NONE) {
			if (recv_str == NULL) {
				errorcode = DP_ERROR_INVALID_PARAMETER;
			} else {
				// write to database here
				if (dp_db_replace_property(client->dbhandle, ipc_info->id, DP_TABLE_REQUEST, DP_DB_COL_FILENAME, (void *)recv_str, ipc_info->size, 2, &errorcode) < 0) {
					TRACE_ERROR("failed to set %s", dp_print_property(ipc_info->property));
					errorcode = DP_ERROR_DISK_BUSY;
				}
				free(recv_str);
			}
		}
		break;
	}
	case DP_PROP_STATE_CALLBACK:
	{
		// check state here
		// DP_ERROR_INVALID_STATE if downloading or finished
		// update database here
		if (requestp != NULL) {
			requestp->state_cb = 1;
		}
		int enable_cb = 1;
		if (dp_db_replace_property(client->dbhandle, ipc_info->id, DP_TABLE_REQUEST, DP_DB_COL_STATE_EVENT, (void *)&enable_cb, ipc_info->size, 0, &errorcode) < 0) {
			TRACE_ERROR("failed to set %s", dp_print_property(ipc_info->property));
			errorcode = DP_ERROR_DISK_BUSY;
		}
		break;
	}
	case DP_PROP_PROGRESS_CALLBACK:
	{
		// check state here
		// DP_ERROR_INVALID_STATE if downloading or finished
		// update database here
		if (requestp != NULL) {
			requestp->progress_cb = 1;
		}
		int enable_cb = 1;
		if (dp_db_replace_property(client->dbhandle, ipc_info->id, DP_TABLE_REQUEST, DP_DB_COL_PROGRESS_EVENT, (void *)&enable_cb, ipc_info->size, 0, &errorcode) < 0) {
			TRACE_ERROR("failed to set %s", dp_print_property(ipc_info->property));
			errorcode = DP_ERROR_DISK_BUSY;
		}
		break;
	}
	case DP_PROP_AUTO_DOWNLOAD:
	{
		// update autodownload property as 1 in database
		int enable_cb = 1;
		if (dp_db_replace_property(client->dbhandle, ipc_info->id, DP_TABLE_LOGGING, DP_DB_COL_AUTO_DOWNLOAD, (void *)&enable_cb, ipc_info->size, 0, &errorcode) < 0) {
			TRACE_ERROR("failed to set %s", dp_print_property(ipc_info->property));
			errorcode = DP_ERROR_DISK_BUSY;
		}
		break;
	}
	case DP_PROP_NETWORK_TYPE:
	{
		int recv_int = -1;
		errorcode = __dp_request_read_int(client->channel, ipc_info, &recv_int);
		if (recv_int <= DP_NETWORK_OFF ||
				recv_int > DP_NETWORK_ALL) {
			errorcode = DP_ERROR_INVALID_PARAMETER;
		} else {
			// update in database
			if (requestp != NULL) {
				if (requestp->state == DP_STATE_QUEUED) {
					dp_queue_manager_clear_queue(requestp);
				} else {
					requestp->network_type = recv_int;
					if (requestp->state == DP_STATE_CONNECTING ||
							requestp->state == DP_STATE_DOWNLOADING) {
						// pause & push queue
						if (dp_pause_agent_download_without_update(requestp->agent_id) < 0) {
							TRACE_ERROR("failed to pause download(%d) id:%d", requestp->agent_id, ipc_info->id);
						} else {
							requestp->state = DP_STATE_PAUSED;
							requestp->error = DP_ERROR_NONE;
							if (dp_queue_manager_push_queue(slot, requestp) < 0) {
								if (dp_db_update_logging(client->dbhandle, ipc_info->id, DP_STATE_FAILED, DP_ERROR_QUEUE_FULL, &errorcode) < 0) {
									TRACE_ERROR("update log sock:%d download-id:%d", client->channel, ipc_info->id);
								}
								requestp->state = DP_STATE_FAILED;
								requestp->error = DP_ERROR_QUEUE_FULL;
								errorcode = DP_ERROR_QUEUE_FULL;
							}
						}
					}
				}
			}
			int enable_cb = recv_int;
			if (dp_db_replace_property(client->dbhandle, ipc_info->id, DP_TABLE_REQUEST, DP_DB_COL_NETWORK_TYPE, (void *)&enable_cb, ipc_info->size, 0, &errorcode) < 0) {
				TRACE_ERROR("failed to set %s", dp_print_property(ipc_info->property));
				errorcode = DP_ERROR_DISK_BUSY;
			}
		}
		break;
	}
	case DP_PROP_NETWORK_BONDING:
	{
		int recv_int = -1;
		errorcode = __dp_request_read_int(client->channel, ipc_info, &recv_int);
		if (errorcode == DP_ERROR_NONE) {
			if(requestp != NULL && requestp->network_type != DP_NETWORK_ALL) {
				errorcode =  DP_ERROR_INVALID_NETWORK_TYPE;
				TRACE_ERROR("[ERROR] wrong network type");
			} else if (dp_db_replace_property(client->dbhandle, ipc_info->id, DP_TABLE_REQUEST, DP_DB_COL_NETWORK_BONDING, (void *)&recv_int, ipc_info->size, 0, &errorcode) < 0) {
				TRACE_ERROR("failed to set %s", dp_print_property(ipc_info->property));
				errorcode = DP_ERROR_DISK_BUSY;
			}
		}
		break;
	}
	case DP_PROP_NOTIFICATION_TYPE:
	{
		int recv_int = -1;
		errorcode = __dp_request_read_int(client->channel, ipc_info, &recv_int);
		if (recv_int == DP_NOTIFICATION_TYPE_NONE ||
				recv_int == DP_NOTIFICATION_TYPE_COMPLETE_ONLY ||
				recv_int == DP_NOTIFICATION_TYPE_ALL) {
			// check state request->state == DP_STATE_COMPLETED
			// DP_ERROR_INVALID_STATE
			// update notificatio type in database
			int noti_type = recv_int;
			if (requestp != NULL) {
				if (recv_int < requestp->noti_type) {
					// if already notification, unregister from notification bar.
					if (recv_int == DP_NOTIFICATION_TYPE_NONE) {
						if (dp_notification_manager_clear_notification(slot, requestp, DP_NOTIFICATION) < 0) {
							TRACE_ERROR("failed to clear notification %s", dp_print_property(ipc_info->property));
						}
					}
					if (dp_notification_manager_clear_notification(slot, requestp, DP_NOTIFICATION_ONGOING) < 0) {
						TRACE_ERROR("failed to clear ongoing %s", dp_print_property(ipc_info->property));
					}
				}
				requestp->noti_type = recv_int;
			}
			if (dp_db_replace_property(client->dbhandle, ipc_info->id, DP_TABLE_NOTIFICATION, DP_DB_COL_NOTI_TYPE, (void *)&noti_type, ipc_info->size, 0, &errorcode) < 0) {
				TRACE_ERROR("failed to set %s", dp_print_property(ipc_info->property));
				errorcode = DP_ERROR_DISK_BUSY;
			}
		} else {
			errorcode = DP_ERROR_INVALID_PARAMETER;
		}
		break;
	}
	case DP_PROP_NOTIFICATION_SUBJECT:
	{
		char *recv_str = NULL;
		errorcode = __dp_request_read_string(client->channel, ipc_info, &recv_str);
		if (errorcode == DP_ERROR_NONE) {
			if (recv_str == NULL) {
				errorcode = DP_ERROR_INVALID_PARAMETER;
			} else {
				// write to database here
				if (dp_db_replace_property(client->dbhandle, ipc_info->id, DP_TABLE_NOTIFICATION, DP_DB_COL_NOTI_SUBJECT, (void *)recv_str, ipc_info->size, 2, &errorcode) < 0) {
					TRACE_ERROR("failed to set %s", dp_print_property(ipc_info->property));
					errorcode = DP_ERROR_DISK_BUSY;
				}
				free(recv_str);
			}
		}
		break;
	}
	case DP_PROP_NOTIFICATION_DESCRIPTION:
	{
		char *recv_str = NULL;
		errorcode = __dp_request_read_string(client->channel, ipc_info, &recv_str);
		if (errorcode == DP_ERROR_NONE) {
			if (recv_str == NULL) {
				errorcode = DP_ERROR_INVALID_PARAMETER;
			} else {
				// write to database here
				if (dp_db_replace_property(client->dbhandle, ipc_info->id, DP_TABLE_NOTIFICATION, DP_DB_COL_NOTI_DESCRIPTION, (void *)recv_str, ipc_info->size, 2, &errorcode) < 0) {
					TRACE_ERROR("failed to set %s", dp_print_property(ipc_info->property));
					errorcode = DP_ERROR_DISK_BUSY;
				}
				free(recv_str);
			}
		}
		break;
	}
	case DP_PROP_NOTIFICATION_RAW: // bundle_type(db column) + bundle_binary
	{
		int bundle_type = -1;
		errorcode = __dp_request_read_int(client->channel, ipc_info, &bundle_type);
		TRACE_DEBUG("read %s type:%d", dp_print_property(ipc_info->property), bundle_type);
		char *raw_column = NULL;
		if (bundle_type == DP_NOTIFICATION_BUNDLE_TYPE_ONGOING)
			raw_column = DP_DB_COL_NOTI_RAW_ONGOING;
		else if (bundle_type == DP_NOTIFICATION_BUNDLE_TYPE_COMPLETE)
			raw_column = DP_DB_COL_NOTI_RAW_COMPLETE;
		else if (bundle_type == DP_NOTIFICATION_BUNDLE_TYPE_FAILED)
			raw_column = DP_DB_COL_NOTI_RAW_FAIL;
		else
			errorcode = DP_ERROR_INVALID_PARAMETER;
		// feedback
		if (dp_ipc_query(client->channel, ipc_info->id, DP_SEC_SET,
				ipc_info->property, errorcode, 0) < 0) {
			TRACE_ERROR("check ipc sock:%d", client->channel);
			errorcode = DP_ERROR_IO_ERROR;
		}
		if (errorcode == DP_ERROR_NONE) {
			dp_ipc_fmt *raw_info = dp_ipc_get_fmt(client->channel);
			if (raw_info == NULL || raw_info->section != ipc_info->section ||
					raw_info->property != ipc_info->property ||
					(raw_info->id != ipc_info->id)) {
				TRACE_ERROR("there is a confusion waiting raw binary in %s section", dp_print_property(ipc_info->property));
				errorcode = DP_ERROR_IO_ERROR;
			}
			if (raw_info != NULL && raw_info->size > 0) {
				unsigned char *recv_raws = (unsigned char *)calloc(raw_info->size, sizeof(unsigned char));
				if (recv_raws == NULL) {
					TRACE_STRERROR("sock:%d check memory length:%d", client->channel, raw_info->size);
					errorcode = DP_ERROR_OUT_OF_MEMORY;
				} else {
					if (dp_ipc_read(client->channel, recv_raws, raw_info->size, __FUNCTION__) <= 0) {
						TRACE_ERROR("sock:%d check ipc length:%d", client->channel, raw_info->size);
						errorcode = DP_ERROR_IO_ERROR;
					} else {
						TRACE_DEBUG("sock:%d length:%d raws", client->channel, raw_info->size);
						// save to database
						if (dp_db_replace_property(client->dbhandle, ipc_info->id, DP_TABLE_NOTIFICATION, raw_column, (void *)recv_raws, raw_info->size, 3, &errorcode) < 0) {
							TRACE_ERROR("failed to set %s", dp_print_property(ipc_info->property));
							errorcode = DP_ERROR_DISK_BUSY;
						}
					}
					free(recv_raws);
				}
			} else {
				errorcode = DP_ERROR_IO_ERROR;
			}
			free(raw_info);
		}
		break;
	}
	case DP_PROP_HTTP_HEADER: //  a request can have one or more fields, a fields can have only one value.
	{
		char *header_field = NULL;
		char *header_value = NULL;
		// 1. read field string
		// 2. response after checking sql status
		// 3. read query IPC for checking IPC
		// 4. read value string
		// 5. response
		errorcode = __dp_request_read_string(client->channel, ipc_info, &header_field);
		if (errorcode == DP_ERROR_NONE && header_field != NULL) {
			// check sql
			int check_field = dp_db_check_duplicated_string(client->dbhandle, ipc_info->id, DP_TABLE_HEADERS, DP_DB_COL_HEADER_FIELD, 0, header_field, &errorcode);
			if (check_field < 0) {
				errorcode = DP_ERROR_DISK_BUSY;
			} else {
				errorcode = DP_ERROR_NONE;
				// feedback
				if (dp_ipc_query(client->channel, ipc_info->id, ipc_info->section,
						ipc_info->property, errorcode, 0) < 0) {
					TRACE_ERROR("check ipc sock:%d", client->channel);
					errorcode = DP_ERROR_IO_ERROR;
				} else {
					dp_ipc_fmt *header_ipc = dp_ipc_get_fmt(client->channel);
					if (header_ipc == NULL || header_ipc->section != ipc_info->section ||
							header_ipc->property != ipc_info->property ||
							(header_ipc->id != ipc_info->id)) {
						TRACE_ERROR("there is a confusion during waiting http string in %s section", dp_print_property(ipc_info->property));
						errorcode = DP_ERROR_IO_ERROR;
					} else {
						errorcode = __dp_request_read_string(client->channel, header_ipc, &header_value);
						if (errorcode == DP_ERROR_NONE && header_value != NULL) {
							if (check_field == 0) { // insert
								if (dp_db_new_header(client->dbhandle, ipc_info->id, header_field, header_value, &errorcode) < 0) {
									TRACE_ERROR("failed to set %s", dp_print_property(ipc_info->property));
									errorcode = DP_ERROR_DISK_BUSY;
								}
							} else { // update
								if (dp_db_update_header(client->dbhandle, ipc_info->id, header_field, header_value, &errorcode) < 0) {
									TRACE_ERROR("failed to set %s", dp_print_property(ipc_info->property));
									errorcode = DP_ERROR_DISK_BUSY;
								}
							}
						} else {
							if (errorcode != DP_ERROR_NONE) {
								TRACE_ERROR("failed to set %s, error:%s", dp_print_property(ipc_info->property), dp_print_errorcode(errorcode));
							}
							if (header_value == NULL) {
								TRACE_ERROR("failed to set %s, do you want to run as unset?", dp_print_property(ipc_info->property));
								errorcode = DP_ERROR_INVALID_PARAMETER;
							}
						}
					}
					free(header_ipc);
				}
			}
		} else {
			if (errorcode != DP_ERROR_NONE) {
				TRACE_ERROR("failed to set %s, error:%s", dp_print_property(ipc_info->property), dp_print_errorcode(errorcode));
			}
			if (header_field == NULL) {
				TRACE_ERROR("failed to set %s", dp_print_property(ipc_info->property));
				errorcode = DP_ERROR_INVALID_PARAMETER;
			}
		}
		free(header_field);
		free(header_value);
		break;
	}
	default:
		errorcode = DP_ERROR_INVALID_PARAMETER;
		break;
	}
	// feedback
	if (dp_ipc_query(client->channel, ipc_info->id, DP_SEC_SET,
			ipc_info->property, errorcode, 0) < 0) {
		TRACE_ERROR("check ipc sock:%d", client->channel);
	}
	return errorcode;
}

static int __dp_request_unset_info(dp_client_fmt *client, dp_ipc_fmt *ipc_info, dp_request_fmt *requestp)
{
	if (client == NULL || ipc_info == NULL) {
		TRACE_ERROR("check client or ipc info.");
		return DP_ERROR_INVALID_PARAMETER;
	}

	int errorcode = DP_ERROR_NONE;

	switch (ipc_info->property) {
	case DP_PROP_URL:
		// it would be run like cancel operation
		if (dp_db_unset_property_string(client->dbhandle, ipc_info->id, DP_TABLE_REQUEST, DP_DB_COL_URL, &errorcode) < 0) {
			TRACE_ERROR("failed to unset %s", dp_print_property(ipc_info->property));
		}
		break;
	case DP_PROP_DESTINATION:
		// if downloading, change destination to da_agent
		if (dp_db_unset_property_string(client->dbhandle, ipc_info->id, DP_TABLE_REQUEST, DP_DB_COL_DESTINATION, &errorcode) < 0) {
			TRACE_ERROR("failed to unset %s", dp_print_property(ipc_info->property));
		}
		break;
	case DP_PROP_FILENAME:
		if (dp_db_unset_property_string(client->dbhandle, ipc_info->id, DP_TABLE_REQUEST, DP_DB_COL_FILENAME, &errorcode) < 0) {
			TRACE_ERROR("failed to unset %s", dp_print_property(ipc_info->property));
		}
		break;
	case DP_PROP_STATE_CALLBACK:
	{
		if (requestp != NULL) {
			requestp->state_cb = 0;
		}
		int enable_cb = 0;
		if (dp_db_replace_property(client->dbhandle, ipc_info->id, DP_TABLE_REQUEST, DP_DB_COL_STATE_EVENT, (void *)&enable_cb, 0, 0, &errorcode) < 0) {
			TRACE_ERROR("failed to unset %s", dp_print_property(ipc_info->property));
			errorcode = DP_ERROR_DISK_BUSY;
		}
		break;
	}
	case DP_PROP_PROGRESS_CALLBACK:
	{
		if (requestp != NULL) {
			requestp->progress_cb = 0;
		}
		int enable_cb = 0;
		if (dp_db_replace_property(client->dbhandle, ipc_info->id, DP_TABLE_REQUEST, DP_DB_COL_PROGRESS_EVENT, (void *)&enable_cb, 0, 0, &errorcode) < 0) {
			TRACE_ERROR("failed to unset %s", dp_print_property(ipc_info->property));
			errorcode = DP_ERROR_DISK_BUSY;
		}
		break;
	}
	case DP_PROP_AUTO_DOWNLOAD:
	{
		// update autodownload property as 0 in database
		int enable_cb = 0;
		if (dp_db_replace_property(client->dbhandle, ipc_info->id, DP_TABLE_LOGGING, DP_DB_COL_AUTO_DOWNLOAD, (void *)&enable_cb, 0, 0, &errorcode) < 0) {
			TRACE_ERROR("failed to unset %s", dp_print_property(ipc_info->property));
			errorcode = DP_ERROR_DISK_BUSY;
		}
		break;
	}
	case DP_PROP_NETWORK_TYPE:
	{
		// check state here
		// update database here
		if (requestp != NULL) {
			requestp->network_type = DP_NETWORK_OFF;
		}
		int enable_cb = DP_NETWORK_OFF;
		if (dp_db_replace_property(client->dbhandle, ipc_info->id, DP_TABLE_REQUEST, DP_DB_COL_NETWORK_TYPE, (void *)&enable_cb, ipc_info->size, 0, &errorcode) < 0) {
			TRACE_ERROR("failed to unset %s", dp_print_property(ipc_info->property));
			errorcode = DP_ERROR_DISK_BUSY;
		}
		break;
	}
	case DP_PROP_NOTIFICATION_TYPE:
	{
		int noti_type = DP_NOTIFICATION_TYPE_NONE;
		if (requestp != NULL) {
			requestp->noti_type = noti_type;
		}
		if (dp_db_replace_property(client->dbhandle, ipc_info->id, DP_TABLE_NOTIFICATION, DP_DB_COL_NOTI_TYPE, (void *)&noti_type, ipc_info->size, 0, &errorcode) < 0) {
			TRACE_ERROR("failed to set %s", dp_print_property(ipc_info->property));
			errorcode = DP_ERROR_DISK_BUSY;
		}
		break;
	}
	case DP_PROP_NOTIFICATION_SUBJECT:
	{
		if (dp_db_unset_property_string(client->dbhandle, ipc_info->id, DP_TABLE_NOTIFICATION, DP_DB_COL_NOTI_SUBJECT, &errorcode) < 0) {
			TRACE_ERROR("failed to unset %s", dp_print_property(ipc_info->property));
		}
		break;
	}
	case DP_PROP_NOTIFICATION_DESCRIPTION:
	{
		if (dp_db_unset_property_string(client->dbhandle, ipc_info->id, DP_TABLE_NOTIFICATION, DP_DB_COL_NOTI_DESCRIPTION, &errorcode) < 0) {
			TRACE_ERROR("failed to unset %s", dp_print_property(ipc_info->property));
		}
		break;
	}
	case DP_PROP_NOTIFICATION_RAW:
	{
		int bundle_type = -1;
		errorcode = __dp_request_read_int(client->channel, ipc_info, &bundle_type);
		TRACE_DEBUG("read %s type:%d", dp_print_property(ipc_info->property), bundle_type);
		char *raw_column = NULL;
		if (bundle_type == DP_NOTIFICATION_BUNDLE_TYPE_ONGOING)
			raw_column = DP_DB_COL_NOTI_RAW_ONGOING;
		else if (bundle_type == DP_NOTIFICATION_BUNDLE_TYPE_COMPLETE)
			raw_column = DP_DB_COL_NOTI_RAW_COMPLETE;
		else if (bundle_type == DP_NOTIFICATION_BUNDLE_TYPE_FAILED)
			raw_column = DP_DB_COL_NOTI_RAW_FAIL;
		if (raw_column != NULL) {
			if (dp_db_unset_property_string(client->dbhandle, ipc_info->id, DP_TABLE_NOTIFICATION, raw_column, &errorcode) < 0) {
				TRACE_ERROR("failed to unset %s", dp_print_property(ipc_info->property));
			}
		} else {
			TRACE_ERROR("invalid param set: %s type:%d", dp_print_property(ipc_info->property), bundle_type);
			errorcode = DP_ERROR_INVALID_PARAMETER;
		}
		break;
	}
	case DP_PROP_HTTP_HEADER: // unset value by field
	{
		char *header_field = NULL;
		errorcode = __dp_request_read_string(client->channel, ipc_info, &header_field);
		if (errorcode == DP_ERROR_NONE && header_field != NULL) {
			int is_present = dp_db_check_duplicated_string(client->dbhandle, ipc_info->id, DP_TABLE_HEADERS, DP_DB_COL_HEADER_FIELD, 0, header_field, &errorcode);
			if (is_present < 0)
				errorcode = DP_ERROR_DISK_BUSY;
			else if (is_present == 0)
				errorcode = DP_ERROR_FIELD_NOT_FOUND;
			else if (dp_db_cond_delete(client->dbhandle, ipc_info->id, DP_TABLE_HEADERS, DP_DB_COL_HEADER_FIELD, header_field, 2, &errorcode) < 0) {
				TRACE_ERROR("failed to unset %s for %s", dp_print_property(ipc_info->property), header_field);
				errorcode = DP_ERROR_DISK_BUSY;
			}
		} else {
			if (errorcode != DP_ERROR_NONE) {
				TRACE_ERROR("failed to set %s, error:%s", dp_print_property(ipc_info->property), dp_print_errorcode(errorcode));
			}
			if (header_field == NULL) {
				TRACE_ERROR("failed to set %s", dp_print_property(ipc_info->property));
				errorcode = DP_ERROR_INVALID_PARAMETER;
			}
		}
		free(header_field);
		break;
	}
	default:
		errorcode = DP_ERROR_INVALID_PARAMETER;
		break;
	}
	// feedback
	if (dp_ipc_query(client->channel, ipc_info->id, DP_SEC_UNSET,
			ipc_info->property, errorcode, 0) < 0) {
		TRACE_ERROR("check ipc sock:%d", client->channel);
	}
	return errorcode;
}

static int __dp_call_cancel_agent(dp_request_fmt *request)
{
	int ret = -1;
	if (request != NULL) {
		if (request->agent_id >= 0) {
			TRACE_INFO("cancel download(%d) id: %d state:%s", request->agent_id,
				request->id, dp_print_state(request->state));
			if (dp_cancel_agent_download_without_update(request->agent_id) == 0)
				ret = 0;
		} else {
			TRACE_ERROR("invalid agent-id:%d id:%d", request->agent_id,
				request->id);
		}
	}
	return ret;
}

static int __dp_request_controls(dp_client_slots_fmt *slot, dp_ipc_fmt *ipc_info, dp_request_fmt *requestp)
{
	if (slot == NULL) {
		TRACE_ERROR("check slot memory");
		return DP_ERROR_INVALID_PARAMETER;
	}
	dp_client_fmt *client = &slot->client;
	if (client == NULL || ipc_info == NULL) {
		TRACE_ERROR("check client or ipc info.");
		return DP_ERROR_INVALID_PARAMETER;
	}

	int errorcode = DP_ERROR_NONE;

	if (ipc_info->property == DP_PROP_CREATE) {
		// check packets again
		if (ipc_info->size != 0 || ipc_info->id != -1) {
			errorcode = DP_ERROR_IO_ERROR;
		} else {
			errorcode = __dp_request_create(client, ipc_info);
		}
	} else {

		// get now state.
		int download_state = DP_STATE_NONE;
		if (requestp != NULL) {
			download_state = requestp->state;
		} else {
			if (dp_db_get_property_int(client->dbhandle, ipc_info->id, DP_TABLE_LOGGING, DP_DB_COL_STATE, &download_state, &errorcode) < 0) {
				TRACE_ERROR("failed to get %s", dp_print_property(ipc_info->property));
				errorcode = DP_ERROR_ID_NOT_FOUND;
				// feedback
				if (dp_ipc_query(client->channel, ipc_info->id, DP_SEC_SET,
						ipc_info->property, errorcode, 0) < 0) {
					TRACE_ERROR("check ipc sock:%d", client->channel);
				}
				return errorcode;
			}
		}
		TRACE_DEBUG("id:%d state:%s set property:%s", ipc_info->id, dp_print_state(download_state), dp_print_property(ipc_info->property));

		if (ipc_info->property == DP_PROP_START) {

			if (download_state == DP_STATE_COMPLETED ||
					download_state == DP_STATE_DOWNLOADING) {
				errorcode = DP_ERROR_INVALID_STATE;
			} else {

				if (requestp == NULL) { // load from databse
					// check state
					// load and add new request to client->requests.
				}
				if (requestp == NULL) {
					TRACE_ERROR("failed to load id:%d from database sock:%d", ipc_info->id, client->channel);
					errorcode = DP_ERROR_DISK_BUSY;
				}
				if (errorcode == DP_ERROR_NONE) {
					// update database
					if (dp_db_update_logging(client->dbhandle, ipc_info->id, DP_STATE_QUEUED, DP_ERROR_NONE, &errorcode) < 0) {
						TRACE_ERROR("update log sock:%d download-id:%d", client->channel, ipc_info->id);
						errorcode = DP_ERROR_DISK_BUSY;
					} else {
						requestp->state = DP_STATE_QUEUED;
						requestp->error = DP_ERROR_NONE;
						// if it's the first request for this client-> push a request at the head of queue.
						// check count queued, connecting.downloading in requets of client
						// else push at the tail of queue.
						// push to queue
						if (dp_queue_manager_push_queue(slot, requestp) < 0) {
							if (dp_db_update_logging(client->dbhandle, ipc_info->id, DP_STATE_FAILED, DP_ERROR_QUEUE_FULL, &errorcode) < 0) {
								TRACE_ERROR("update log sock:%d download-id:%d", client->channel, ipc_info->id);
							}
							requestp->state = DP_STATE_FAILED;
							requestp->error = DP_ERROR_QUEUE_FULL;
							errorcode = DP_ERROR_QUEUE_FULL;
						} else { // push ok
							dp_queue_manager_wake_up();
							// notification
							if (requestp->noti_type == DP_NOTIFICATION_TYPE_ALL) {
								if (dp_notification_manager_push_notification(slot, requestp, DP_NOTIFICATION_ONGOING) < 0) {
									TRACE_ERROR("failed to register notification for id:%d", ipc_info->id);
								}
							}
						}
					}
				}
				TRACE_DEBUG("id:%d check start error:%s", ipc_info->id, dp_print_errorcode(errorcode));
			}

		} else if (ipc_info->property == DP_PROP_PAUSE) {

			if (download_state > DP_STATE_DOWNLOADING) {
				errorcode = DP_ERROR_INVALID_STATE;
			} else { // change state regardless it's on memory or not.
				if (dp_db_update_logging(client->dbhandle, ipc_info->id, DP_STATE_PAUSED, DP_ERROR_NONE, &errorcode) < 0) {
					TRACE_ERROR("update log sock:%d download-id:%d", client->channel, ipc_info->id);
					errorcode = DP_ERROR_DISK_BUSY;
				} else {
					// call da_pause API
					if (requestp != NULL) {
						// pop from queue. if state is queued.
						if (requestp->state == DP_STATE_QUEUED) {
							dp_queue_manager_clear_queue(requestp);
						} else if (requestp->state == DP_STATE_CONNECTING ||
								requestp->state == DP_STATE_DOWNLOADING) {
							if (dp_pause_agent_download_without_update(requestp->agent_id) < 0) {
								TRACE_ERROR("failed to pause download(%d) id:%d", requestp->agent_id, ipc_info->id);
							}
						}
						requestp->state = DP_STATE_PAUSED;
						requestp->error = DP_ERROR_NONE;
					}
				}
			}

		} else if (ipc_info->property == DP_PROP_CANCEL) {

			if (download_state > DP_STATE_COMPLETED) {
				errorcode = DP_ERROR_INVALID_STATE;
			} else { // change state regardless it's on memory or not.
				if (dp_db_update_logging(client->dbhandle, ipc_info->id, DP_STATE_CANCELED, DP_ERROR_NONE, &errorcode) < 0) {
					TRACE_ERROR("update log sock:%d download-id:%d", client->channel, ipc_info->id);
					errorcode = DP_ERROR_DISK_BUSY;
				} else {
					// call da_cancel API
					if (requestp != NULL) {
						// pop from queue. if state is queued.
						if (requestp->state == DP_STATE_QUEUED) {
							dp_queue_manager_clear_queue(requestp);
						} else if (requestp->state == DP_STATE_CONNECTING ||
								requestp->state == DP_STATE_DOWNLOADING) {
							if (__dp_call_cancel_agent(requestp) < 0) {
								TRACE_ERROR("failed to cancel download(%d) id:%d", requestp->agent_id, ipc_info->id);
							}
						}
						requestp->agent_id = -1;
						requestp->state = DP_STATE_CANCELED;
						requestp->error = DP_ERROR_NONE;
						if (requestp->noti_type == DP_NOTIFICATION_TYPE_COMPLETE_ONLY ||
								requestp->noti_type == DP_NOTIFICATION_TYPE_ALL) {
							if (dp_notification_manager_push_notification(slot, requestp, DP_NOTIFICATION) < 0) {
								TRACE_ERROR("failed to register notification for id:%d", ipc_info->id);
							}
						}
					}
				}
			}

		} else if (ipc_info->property == DP_PROP_DESTROY) {

			// check state
			// pop from queue. if state is queued.
			if (requestp != NULL) {
				if (requestp->state == DP_STATE_QUEUED)
					dp_queue_manager_clear_queue(requestp);
				if (requestp->state == DP_STATE_CONNECTING ||
						requestp->state == DP_STATE_DOWNLOADING) {
					// update state property database;
					if (dp_db_update_logging(client->dbhandle, ipc_info->id, DP_STATE_CANCELED, DP_ERROR_NONE, &errorcode) < 0) {
						TRACE_ERROR("update log sock:%d download-id:%d", client->channel, ipc_info->id);
					} else {
						// call da_cancel API
						if (__dp_call_cancel_agent(requestp) < 0) {
							TRACE_ERROR("failed to cancel download(%d) id:%d", requestp->agent_id, ipc_info->id);
						}
					}
					requestp->state = DP_STATE_CANCELED;
				}
				if (requestp->state == DP_STATE_QUEUED || requestp->state == DP_STATE_CANCELED) {

					if (requestp->noti_type == DP_NOTIFICATION_TYPE_COMPLETE_ONLY ||
							requestp->noti_type == DP_NOTIFICATION_TYPE_ALL) {
						if (dp_notification_manager_push_notification(slot, requestp, DP_NOTIFICATION) < 0) {
							TRACE_ERROR("failed to register notification for id:%d", ipc_info->id);
						}
					}
				}
				requestp->agent_id = -1;
			}
			errorcode = dp_request_destroy(client, ipc_info, requestp);

		} else {
			errorcode = DP_ERROR_INVALID_PARAMETER;
			TRACE_ERROR("invalid param - id:%d set property:%s", ipc_info->id, dp_print_property(ipc_info->property));
		}
	}

	// feedback
	if (dp_ipc_query(client->channel, ipc_info->id, DP_SEC_CONTROL,
			ipc_info->property, errorcode, 0) < 0) {
		TRACE_ERROR("check ipc sock:%d", client->channel);
	}

	// workaround. client still request the feedback by cancelation
	if (ipc_info->property == DP_PROP_CANCEL ||
			ipc_info->property == DP_PROP_PAUSE) {
		if (requestp != NULL && requestp->state_cb == 1) {
			if (slot->client.notify < 0 ||
					dp_notify_feedback(slot->client.notify, slot, ipc_info->id, requestp->state, errorcode, 0) < 0) {
				TRACE_ERROR("id:%d disable state callback by IO_ERROR", ipc_info->id);
				requestp->state_cb = 0;
			}
		}
	}

	return errorcode;
}

static int __dp_client_requests(dp_client_slots_fmt *slot, dp_ipc_fmt *ipc_info)
{
	if (slot == NULL) {
		TRACE_ERROR("check slot memory");
		return DP_ERROR_INVALID_PARAMETER;
	}
	dp_client_fmt *client = &slot->client;
	if (client == NULL || ipc_info == NULL) {
		TRACE_ERROR("check client or ipc info.");
		return DP_ERROR_INVALID_PARAMETER;
	}

	int errorcode = DP_ERROR_NONE;

	// check id except create command  /////////// DP_ERROR_ID_NOT_FOUND
	dp_request_fmt *requestp = NULL;
	if (ipc_info->section != DP_SEC_CONTROL ||
			ipc_info->property != DP_PROP_CREATE) {
		// check on requests
		int i = 0;
		requestp = client->requests;
		errorcode = DP_ERROR_ID_NOT_FOUND;
		for (; i < MAX_DOWNLOAD_HANDLE; i++) {
			if (requestp == NULL)
				break;
			//TRACE_DEBUG("link %d info: id:%d access-time:%d", i, requestp->id, requestp->access_time);
			if (requestp->id == ipc_info->id) {
				errorcode = DP_ERROR_NONE;
				break;
			}
			requestp = requestp->next;
		}
		if (errorcode == DP_ERROR_ID_NOT_FOUND) {
			// check in database
			if (dp_db_check_duplicated_int(client->dbhandle, DP_TABLE_LOGGING, DP_DB_COL_ID, ipc_info->id, &errorcode) > 0) {
				//TRACE_DEBUG("found %d from database", ipc_info->id);
				errorcode = DP_ERROR_NONE;
			}
		}
	}

	// Check size for prevent
	if (ipc_info->size > 4294967295U) {
		TRACE_ERROR("Check socket. Invalid size value. sock:%d", client->channel);
		return DP_ERROR_IO_ERROR;
	}
	if (errorcode != DP_ERROR_NONE) { // prechecking
		TRACE_ERROR("precheck errorcode:%s sock:%d id:%d section:%s property:%s",
			dp_print_errorcode(errorcode),
			client->channel, ipc_info->id,
			dp_print_section(ipc_info->section),
			dp_print_property(ipc_info->property));

		// clear followed packets.
		if (ipc_info->size > 0) {
			char garbage[ipc_info->size];
			if (read(client->channel, &garbage, ipc_info->size) == 0) {
				TRACE_ERROR("sock:%d closed peer", client->channel);
				errorcode = DP_ERROR_IO_ERROR;
			}
		}

		if (dp_ipc_query(client->channel, ipc_info->id,
				ipc_info->section, ipc_info->property, errorcode, 0) < 0) {
			TRACE_ERROR("check ipc sock:%d", client->channel);
			errorcode = DP_ERROR_IO_ERROR;
		}
		return errorcode;
	}

	switch (ipc_info->section) {
	case DP_SEC_CONTROL:
		errorcode = __dp_request_controls(slot, ipc_info, requestp);
		break;
	case DP_SEC_GET:
		errorcode = __dp_request_get_info(client, ipc_info, requestp);
		break;
	case DP_SEC_SET:
		errorcode = __dp_request_set_info(slot, ipc_info, requestp);
		break;
	case DP_SEC_UNSET:
		errorcode = __dp_request_unset_info(client, ipc_info, requestp);
		break;
	default:
		errorcode = DP_ERROR_INVALID_PARAMETER;
		break;
	}
	return errorcode;
}

static void __dp_client_stop_all_requests(dp_client_slots_fmt *slot)
{
	unsigned push_count = 0;
	int errorcode = DP_ERROR_NONE;
	int i = 0;
	dp_request_fmt *tailp = slot->client.requests;
	for (; tailp != NULL; i++) {
		TRACE_DEBUG("request %d stop id:%d state:%s", i, tailp->id, dp_print_state(tailp->state));
		int state = tailp->state;
		if (state == DP_STATE_CONNECTING) {
			if (dp_cancel_agent_download_without_update(tailp->agent_id) < 0) {
				TRACE_ERROR("failed to cancel download(%d) id:%d", tailp->agent_id, tailp->id);
			}
		} else if (state == DP_STATE_DOWNLOADING) {
			if (dp_pause_agent_download(tailp->agent_id) < 0) {
				TRACE_ERROR("failed to pause download(%d) id:%d", tailp->agent_id, tailp->id);
			}
		}
		if (state == DP_STATE_DOWNLOADING || state == DP_STATE_CONNECTING) {
			tailp->state = DP_STATE_QUEUED;
			// This is error code for checking the reason when changing ip configuration process
			tailp->error = DP_ERROR_IO_EAGAIN;
			// push to queue now
			// in da callback, check DP_ERROR_IO_EAGAIN, then ignore.
			if (dp_db_update_logging(slot->client.dbhandle, tailp->id, tailp->state, DP_ERROR_NONE, &errorcode) < 0) {
				TRACE_ERROR("update log sock:%d download-id:%d", slot->client.channel, tailp->id);
			}
			if (dp_queue_manager_push_queue(slot, tailp) < 0) {
				TRACE_ERROR("Fail to push queueg sock:%d download-id:%d", slot->client.channel, tailp->id);
				// FIXME later : error case. How can handle this item?
			} else {
				push_count++;
			}
		}
		tailp = tailp->next;
	}
	if (push_count > 0)
		dp_queue_manager_wake_up();
}

void dp_client_sig_handler(int signo)
{
	TRACE_INFO("thread:%0x signal:%d", pthread_self(), signo);
}

void *dp_client_request_thread(void *arg)
{
	dp_client_slots_fmt *slot = (dp_client_slots_fmt *)arg;
	if (slot == NULL) {
		TRACE_ERROR("slot null, can not launch the thread for client");
		return 0;
	}

	// wait detaching thread
	CLIENT_MUTEX_LOCK(&slot->mutex);

	TRACE_INFO("slot %p thread:%0x", slot, slot->thread);

	struct sigaction act = {{0},};
	sigset_t newmask;
	sigemptyset(&newmask);
	sigaddset(&newmask, SIGUSR1);
	act.sa_handler = dp_client_sig_handler;
	sigaction(SIGUSR1, &act, NULL);

	fd_set imask, emask;
	int errorcode = DP_ERROR_NONE;
	dp_client_fmt *client = &slot->client;
	int client_sock = client->channel;
	struct timeval timeout; // for timeout of select

	CLIENT_MUTEX_UNLOCK(&slot->mutex);

	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

	while (slot != NULL && client_sock >= 0 &&
			client_sock == slot->client.channel) {

		memset(&timeout, 0x00, sizeof(struct timeval));
		timeout.tv_sec = DP_CARE_CLIENT_REQUEST_INTERVAL;
		FD_ZERO(&imask );
		FD_ZERO(&emask );
		FD_SET(client_sock, &imask);
		FD_SET(client_sock, &emask);
		if (select(client_sock + 1, &imask, 0, &emask, &timeout) < 0 ) {
			if (slot != NULL && slot->client.channel >= 0) {
				TRACE_INFO("broadcast by client-manager");
				CLIENT_MUTEX_LOCK(&slot->mutex);
				// check all requests
				__dp_client_stop_all_requests(slot);
				CLIENT_MUTEX_UNLOCK(&slot->mutex);
				continue;
			} else {
				TRACE_STRERROR("interrupted by client-manager sock:%d", client_sock);
				break;
			}
		}
		if (FD_ISSET(client_sock, &imask) > 0) {

			CLIENT_MUTEX_LOCK(&slot->mutex);

			if (client->dbhandle == 0 || dp_db_check_connection(client->dbhandle) < 0) {
				if (dp_db_open_client(&client->dbhandle, slot->pkgname) < 0) {
					TRACE_ERROR("failed to open database for sock:%d", client_sock);
					CLIENT_MUTEX_UNLOCK(&slot->mutex);
					break;
				}
			}

			pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
			client->access_time = (int)time(NULL);

			// read ipc_fmt first. below func will deal followed packets
			dp_ipc_fmt *ipc_info = dp_ipc_get_fmt(client_sock);
			if (ipc_info == NULL) {
				TRACE_STRERROR("sock:%d maybe closed", client_sock);
				errorcode = DP_ERROR_IO_ERROR;
			} else {
				TRACE_DEBUG("sock:%d id:%d section:%s property:%s errorcode:%s size:%d",
					client_sock, ipc_info->id,
					dp_print_section(ipc_info->section),
					dp_print_property(ipc_info->property),
					dp_print_errorcode(ipc_info->errorcode),
					ipc_info->size);

				// JOB
				errorcode = __dp_client_requests(slot, ipc_info);
			}
			free(ipc_info);

			pthread_setcancelstate (PTHREAD_CANCEL_ENABLE,  NULL);
			CLIENT_MUTEX_UNLOCK(&slot->mutex);

			if (errorcode == DP_ERROR_IO_ERROR ||
					errorcode == DP_ERROR_OUT_OF_MEMORY ||
					errorcode == DP_ERROR_INVALID_PARAMETER) {
				TRACE_ERROR("disconnect client errorcode:%s sock:%d",
					dp_print_errorcode(errorcode), client_sock);
				break;
			}

		} else if (FD_ISSET(client_sock, &emask) > 0) {
			TRACE_ERROR("[EXCEPTION]");
			break;
		} else {

			// timeout
			if (CLIENT_MUTEX_TRYLOCK(&slot->mutex) == 0) {
				// 1. clear zombie requests. clean requests finished. paused or ready for long time
				dp_client_clear_requests(slot);

				int sql_errorcode = DP_ERROR_NONE;

				// 2. maintain only 1000 rows for each client
				if (dp_db_limit_rows(client->dbhandle, DP_TABLE_LOGGING, DP_LOG_DB_LIMIT_ROWS, &sql_errorcode) < 0) {
					TRACE_INFO("limit rows error:%s", dp_print_errorcode(sql_errorcode));
				}
				// 3. maintain for 48 hours
				if (dp_db_limit_time(client->dbhandle, DP_TABLE_LOGGING, DP_CARE_CLIENT_INFO_PERIOD, &sql_errorcode) < 0) {
					TRACE_INFO("limit rows error:%s", dp_print_errorcode(sql_errorcode));
				}
				// 4. if no requests, exit by itself.
				if (slot->client.requests == NULL) {
					TRACE_DEBUG("no requests");
					CLIENT_MUTEX_UNLOCK(&slot->mutex);
					break;
				}
				CLIENT_MUTEX_UNLOCK(&slot->mutex);
			}
		}
	}

	FD_CLR(client_sock, &imask);
	FD_CLR(client_sock, &emask);

	// if no requests, clear slot after disconnect with client.
	CLIENT_MUTEX_LOCK(&slot->mutex);

	TRACE_INFO("thread done slot %p thread:%0x sock:%d", slot, slot->thread, client_sock);

	slot->thread = 0;// to prevent kill thread twice

	int i = 0;
	dp_request_fmt *tailp = slot->client.requests;
	dp_request_fmt *prevp = NULL;
	for (; tailp != NULL; i++) {
		if (tailp->state != DP_STATE_QUEUED &&
				tailp->state != DP_STATE_CONNECTING &&
				tailp->state != DP_STATE_DOWNLOADING) {
			dp_request_fmt *removep = tailp;
			if (prevp == NULL) // first request.
				client->requests = tailp->next;
			else
				prevp->next = tailp->next;
			tailp = tailp->next;
			TRACE_DEBUG("request %d remove: id:%d state:%s", i, removep->id, dp_print_state(removep->state));
			dp_request_free(removep);
			continue;
		}
		TRACE_DEBUG("request %d remain: id:%d state:%s", i, tailp->id, dp_print_state(tailp->state));
		prevp = tailp;
		tailp = tailp->next;

	}
	// if no requests after clear finished requests.
	if (slot->client.requests == NULL) {
		dp_client_slot_free(slot);
	} else {
		if (slot->client.notify >= 0)
			close(slot->client.notify);
		dp_notify_deinit(slot->credential.pid);
		slot->client.notify = -1;
		if (slot->client.channel > 0)
			close(slot->client.channel);
		slot->client.channel = -1;
	}
	TRACE_INFO("thread done slot %p thread:%0x sock:%d", slot, slot->thread, client_sock);
	CLIENT_MUTEX_UNLOCK(&slot->mutex);
	return 0;
}

