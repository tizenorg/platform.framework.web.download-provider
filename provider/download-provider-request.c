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

#include <time.h>
#include <sys/time.h>

#include "download-provider.h"
#include "download-provider-log.h"

#include "download-provider-slots.h"
#include "download-provider-socket.h"
#include "download-provider-db.h"
#include "download-provider-pthread.h"


///////// below functions are called by main thread of thread-request.c



//////////////////////////////////////////////////////////////////////////
/// @brief		create unique id as integer type
/// @return	unique id combined local time and the special calculation
static int __get_download_request_id(void)
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
		TRACE_INFO("ID : %d", uniquetime);
	} while (last_uniquetime == uniquetime);
	last_uniquetime = uniquetime;	// store
	return uniquetime;
}

char *dp_print_state(dp_state_type state)
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
		case DP_STATE_PAUSE_REQUESTED :
			return "PAUSE_REQUESTED";
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

char *dp_print_errorcode(dp_error_type errorcode)
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
		case DP_ERROR_RESPONSE_TIMEOUT :
			return "RESPONSE_TIMEOUT";
		case DP_ERROR_REQUEST_TIMEOUT :
			return "REQUEST_TIMEOUT";
		case DP_ERROR_SYSTEM_DOWN :
			return "SYSTEM_DOWN";
		case DP_ERROR_CLIENT_DOWN :
			return "CLIENT_DOWN";
		case DP_ERROR_ID_NOT_FOUND :
			return "ID_NOT_FOUND";
		default :
			break;
	}
	return "UNKNOWN";
}

char *dp_strdup(char *src)
{
	char *dest = NULL;
	size_t src_len = 0;

	if (src == NULL) {
		TRACE_ERROR("[CHECK PARAM]");
		return NULL;
	}

	src_len = strlen(src);
	if (src_len <= 0) {
		TRACE_ERROR("[CHECK PARAM] len[%d]", src_len);
		return NULL;
	}

	dest = (char *)calloc(src_len + 1, sizeof(char));
	if (dest == NULL) {
		TRACE_STRERROR("[CHECK] allocation");
		return NULL;
	}
	memcpy(dest, src, src_len * sizeof(char));
	dest[src_len] = '\0';

	return dest;
}

// check param
// create new slot
// fill info to new slot
// make new id
// save info to QUEUE(DB)
dp_error_type dp_request_create(int id, dp_client_group *group, dp_request **empty_slot)
{
	if (id != -1) {
		TRACE_ERROR("[CHECK PROTOCOL] ID not -1");
		return DP_ERROR_INVALID_STATE;
	}
	if (!group || !empty_slot) {
		TRACE_ERROR("[CHECK INTERNAL][%d]", id);
		return DP_ERROR_IO_ERROR;
	}
	// New allocation Slot
	dp_request *new_request = dp_request_new();
	if (!new_request) {
		TRACE_STRERROR("[CHECK MEMORY][%d]", id);
		return DP_ERROR_OUT_OF_MEMORY;
	}

	new_request->id = __get_download_request_id();
	new_request->group = group;
	if (group->pkgname && strlen(group->pkgname) > 1)
		new_request->packagename = dp_strdup(group->pkgname);
	new_request->credential = group->credential;
	if (new_request->packagename == NULL) {
		dp_request_free(new_request);
		TRACE_ERROR("[ERROR][%d] OUT_OF_MEMORY [PACKAGENAME]", id);
		return DP_ERROR_OUT_OF_MEMORY;
	}
	new_request->state = DP_STATE_READY;
	new_request->error = DP_ERROR_NONE;
	if (dp_db_insert_column
			(new_request->id, DP_DB_TABLE_LOG, DP_DB_COL_STATE,
			DP_DB_COL_TYPE_INT, &new_request->state) < 0) {
		TRACE_ERROR("[CHECK SQL][%d]", id);
		dp_request_free(new_request);
		return DP_ERROR_OUT_OF_MEMORY;
	}
	if (dp_db_set_column
			(new_request->id, DP_DB_TABLE_LOG, DP_DB_COL_PACKAGENAME,
			DP_DB_COL_TYPE_TEXT, new_request->packagename) < 0)
		TRACE_ERROR("[CHECK SQL][%d]", id);
	if (dp_db_update_date
			(new_request->id, DP_DB_TABLE_LOG, DP_DB_COL_CREATE_TIME) < 0)
		TRACE_ERROR("[CHECK SQL][%d]", id);

	new_request->create_time = (int)time(NULL);

	*empty_slot = new_request;
	return DP_ERROR_NONE;
}

dp_error_type dp_request_set_url(int id, dp_request *request, char *url)
{
	int length = 0;
	if (!url || (length = strlen(url)) <= 1)
		return DP_ERROR_INVALID_URL;

	if (request != NULL) {
		if (request->state == DP_STATE_CONNECTING ||
			request->state == DP_STATE_DOWNLOADING ||
			request->state == DP_STATE_COMPLETED) {
			TRACE_ERROR
			("[ERROR][%d] now[%s]", id, dp_print_state(request->state));
			return DP_ERROR_INVALID_STATE;
		}
	} else {
		// check id in logging table.
		dp_state_type state =
			dp_db_get_int_column(id, DP_DB_TABLE_LOG, DP_DB_COL_STATE);

		if (state <= DP_STATE_NONE) {
			TRACE_ERROR("[ERROR][%d] state[%s]", id, dp_print_state(state));
			return DP_ERROR_ID_NOT_FOUND;
		}
		// check again from logging table
		if (state == DP_STATE_CONNECTING ||
			state == DP_STATE_DOWNLOADING ||
			state == DP_STATE_COMPLETED) {
			TRACE_ERROR("[ERROR][%d] now[%s]", id, dp_print_state(state));
			return DP_ERROR_INVALID_STATE;
		}
	}

	if (dp_db_replace_column
			(id, DP_DB_TABLE_REQUEST_INFO, DP_DB_COL_URL,
			DP_DB_COL_TYPE_TEXT, url) < 0) {
		TRACE_ERROR("[CHECK SQL][%d]", id);
		return DP_ERROR_IO_ERROR;
	}
	return DP_ERROR_NONE;
}

dp_error_type dp_request_set_destination(int id, dp_request *request, char *dest)
{
	int length = 0;
	if (!dest || (length = strlen(dest)) <= 1)
		return DP_ERROR_INVALID_DESTINATION;

	if (request != NULL) {
		if (request->state == DP_STATE_CONNECTING ||
			request->state == DP_STATE_DOWNLOADING ||
			request->state == DP_STATE_COMPLETED) {
			TRACE_ERROR
			("[ERROR][%d] now[%s]", id, dp_print_state(request->state));
			return DP_ERROR_INVALID_STATE;
		}
	} else {
		// check id in logging table.
		dp_state_type state =
			dp_db_get_int_column(id, DP_DB_TABLE_LOG, DP_DB_COL_STATE);

		if (state <= DP_STATE_NONE) {
			TRACE_ERROR("[ERROR][%d] state[%s]", id, dp_print_state(state));
			return DP_ERROR_ID_NOT_FOUND;
		}
		// check again from logging table
		if (state == DP_STATE_CONNECTING ||
			state == DP_STATE_DOWNLOADING ||
			state == DP_STATE_COMPLETED) {
			TRACE_ERROR("[ERROR][%d] now[%s]", id, dp_print_state(state));
			return DP_ERROR_INVALID_STATE;
		}
	}

	if (dp_db_replace_column
			(id, DP_DB_TABLE_REQUEST_INFO, DP_DB_COL_DESTINATION,
			DP_DB_COL_TYPE_TEXT, dest) < 0) {
		TRACE_ERROR("[CHECK SQL][%d]", id);
		return DP_ERROR_IO_ERROR;
	}
	return DP_ERROR_NONE;
}

dp_error_type dp_request_set_filename(int id, dp_request *request, char *filename)
{
	int length = 0;
	if (!filename || (length = strlen(filename)) <= 1)
		return DP_ERROR_INVALID_PARAMETER;

	if (request != NULL) {
		if (request->state == DP_STATE_CONNECTING ||
			request->state == DP_STATE_DOWNLOADING ||
			request->state == DP_STATE_COMPLETED) {
			TRACE_ERROR
			("[ERROR][%d] now[%s]", id, dp_print_state(request->state));
			return DP_ERROR_INVALID_STATE;
		}
	} else {
		// check id in logging table.
		dp_state_type state =
			dp_db_get_int_column(id, DP_DB_TABLE_LOG, DP_DB_COL_STATE);

		if (state <= DP_STATE_NONE) {
			TRACE_ERROR("[ERROR][%d] state[%s]", id, dp_print_state(state));
			return DP_ERROR_ID_NOT_FOUND;
		}
		// check again from logging table
		if (state == DP_STATE_CONNECTING ||
			state == DP_STATE_DOWNLOADING ||
			state == DP_STATE_COMPLETED) {
			TRACE_ERROR("[ERROR][%d] now[%s]", id, dp_print_state(state));
			return DP_ERROR_INVALID_STATE;
		}
	}

	if (dp_db_replace_column
			(id, DP_DB_TABLE_REQUEST_INFO, DP_DB_COL_FILENAME,
			DP_DB_COL_TYPE_TEXT, filename) < 0) {
		TRACE_ERROR("[CHECK SQL][%d]", id);
		return DP_ERROR_IO_ERROR;
	}

	TRACE_INFO("ID [%d] Filename[%s]", id, filename);
	return DP_ERROR_NONE;
}

dp_error_type dp_request_set_notification(int id, dp_request *request, unsigned enable)
{
	if (request != NULL) {
		if (request->state == DP_STATE_COMPLETED) {
			TRACE_ERROR
			("[ERROR][%d] now[%s]", id, dp_print_state(request->state));
			return DP_ERROR_INVALID_STATE;
		}
	} else {
		// check id in logging table.
		dp_state_type state =
			dp_db_get_int_column(id, DP_DB_TABLE_LOG, DP_DB_COL_STATE);

		if (state <= DP_STATE_NONE) {
			TRACE_ERROR("[ERROR][%d] state[%s]", id, dp_print_state(state));
			return DP_ERROR_ID_NOT_FOUND;
		}
		// check again from logging table
		if (state == DP_STATE_COMPLETED) {
			TRACE_ERROR("[ERROR][%d] now[%s]", id, dp_print_state(state));
			return DP_ERROR_INVALID_STATE;
		}
	}

	// update queue DB
	if (dp_db_replace_column
			(id, DP_DB_TABLE_REQUEST_INFO,
			DP_DB_COL_NOTIFICATION_ENABLE, DP_DB_COL_TYPE_INT,
			&enable) < 0) {
		TRACE_ERROR("[CHECK SQL][%d]", id);
		return DP_ERROR_IO_ERROR;
	}
	// update memory
	if (request)
		request->auto_notification = enable;
	return DP_ERROR_NONE;
}

dp_error_type dp_request_set_auto_download(int id, dp_request *request, unsigned enable)
{
	if (request != NULL) {
		if (request->state == DP_STATE_COMPLETED) {
			TRACE_ERROR
			("[ERROR][%d] now[%s]", id, dp_print_state(request->state));
			return DP_ERROR_INVALID_STATE;
		}
	} else {
		// check id in logging table.
		dp_state_type state =
			dp_db_get_int_column(id, DP_DB_TABLE_LOG, DP_DB_COL_STATE);

		if (state <= DP_STATE_NONE) {
			TRACE_ERROR("[ERROR][%d] state[%s]", id, dp_print_state(state));
			return DP_ERROR_ID_NOT_FOUND;
		}
		// check again from logging table
		if (state == DP_STATE_COMPLETED) {
			TRACE_ERROR("[ERROR][%d] now[%s]", id, dp_print_state(state));
			return DP_ERROR_INVALID_STATE;
		}
	}

	// update queue DB
	if (dp_db_replace_column
			(id, DP_DB_TABLE_REQUEST_INFO, DP_DB_COL_AUTO_DOWNLOAD,
			DP_DB_COL_TYPE_INT, &enable) < 0) {
		TRACE_ERROR("[CHECK SQL][%d]", id);
		return DP_ERROR_IO_ERROR;
	}
	return DP_ERROR_NONE;
}

dp_error_type dp_request_set_state_event(int id, dp_request *request, unsigned enable)
{
	if (request == NULL) {
		// check id in logging table.
		dp_state_type state =
			dp_db_get_int_column(id, DP_DB_TABLE_LOG, DP_DB_COL_STATE);

		if (state <= DP_STATE_NONE) {
			TRACE_ERROR("[ERROR][%d] state[%s]", id, dp_print_state(state));
			return DP_ERROR_ID_NOT_FOUND;
		}
	}

	// update queue DB
	if (dp_db_replace_column
			(id, DP_DB_TABLE_REQUEST_INFO, DP_DB_COL_STATE_EVENT,
			DP_DB_COL_TYPE_INT, &enable) < 0) {
		TRACE_ERROR("[CHECK SQL][%d]", id);
		return DP_ERROR_IO_ERROR;
	}
	// update memory
	if (request)
		request->state_cb = enable;
	return DP_ERROR_NONE;
}

dp_error_type dp_request_set_progress_event(int id, dp_request *request, unsigned enable)
{
	if (request == NULL) {
		// check id in logging table.
		dp_state_type state =
			dp_db_get_int_column(id, DP_DB_TABLE_LOG, DP_DB_COL_STATE);

		if (state <= DP_STATE_NONE) {
			TRACE_ERROR("[ERROR][%d] state[%s]", id, dp_print_state(state));
			return DP_ERROR_ID_NOT_FOUND;
		}
	}

	// update queue DB
	if (dp_db_replace_column
			(id, DP_DB_TABLE_REQUEST_INFO, DP_DB_COL_PROGRESS_EVENT,
			DP_DB_COL_TYPE_INT, &enable) < 0) {
		TRACE_ERROR("[CHECK SQL][%d]", id);
		return DP_ERROR_IO_ERROR;
	}
	// update memory
	if (request)
		request->progress_cb = enable;
	return DP_ERROR_NONE;
}

dp_error_type dp_request_set_network_type(int id, dp_request *request, int type)
{
	if (request != NULL) {
		if (request->state == DP_STATE_CONNECTING ||
			request->state == DP_STATE_DOWNLOADING ||
			request->state == DP_STATE_COMPLETED) {
			TRACE_ERROR
			("[ERROR][%d] now[%s]", id, dp_print_state(request->state));
			return DP_ERROR_INVALID_STATE;
		}
	} else {
		// check id in logging table.
		dp_state_type state =
			dp_db_get_int_column(id, DP_DB_TABLE_LOG, DP_DB_COL_STATE);

		if (state <= DP_STATE_NONE) {
			TRACE_ERROR("[ERROR][%d] state[%s]", id, dp_print_state(state));
			return DP_ERROR_ID_NOT_FOUND;
		}
		// check again from logging table
		if (state == DP_STATE_CONNECTING ||
			state == DP_STATE_DOWNLOADING ||
			state == DP_STATE_COMPLETED) {
			TRACE_ERROR("[ERROR][%d] now[%s]", id, dp_print_state(state));
			return DP_ERROR_INVALID_STATE;
		}
	}

	// update queue DB
	if (dp_db_replace_column
			(id, DP_DB_TABLE_REQUEST_INFO, DP_DB_COL_NETWORK_TYPE,
			DP_DB_COL_TYPE_INT, &type) < 0) {
		TRACE_ERROR("[CHECK SQL][%d]", id);
		return DP_ERROR_IO_ERROR;
	}
	// update memory
	if (request)
		request->network_type = type;
	return DP_ERROR_NONE;
}

char *dp_request_get_url(int id, dp_request *request, dp_error_type *errorcode)
{
	char *url = NULL;

	if (request == NULL) {
		dp_state_type state =
			dp_db_get_int_column(id, DP_DB_TABLE_LOG, DP_DB_COL_STATE);

		if (state <= DP_STATE_NONE) {
			TRACE_ERROR("[ERROR][%d] state[%s]", id, dp_print_state(state));
			*errorcode = DP_ERROR_ID_NOT_FOUND;
			return NULL;
		}
	}
	url = dp_db_get_text_column
				(id, DP_DB_TABLE_REQUEST_INFO, DP_DB_COL_URL);
	if (url == NULL) {
		*errorcode = DP_ERROR_NO_DATA;
		return NULL;
	}
	return url;
}

char *dp_request_get_destination(int id, dp_request *request, dp_error_type *errorcode)
{
	char *dest = NULL;

	if (request == NULL) {
		dp_state_type state =
			dp_db_get_int_column(id, DP_DB_TABLE_LOG, DP_DB_COL_STATE);

		if (state <= DP_STATE_NONE) {
			TRACE_ERROR("[ERROR][%d] state[%s]", id, dp_print_state(state));
			*errorcode = DP_ERROR_ID_NOT_FOUND;
			return NULL;
		}
	}
	dest = dp_db_get_text_column
				(id, DP_DB_TABLE_REQUEST_INFO, DP_DB_COL_DESTINATION);
	if (dest == NULL) {
		*errorcode = DP_ERROR_NO_DATA;
		return NULL;
	}
	return dest;
}

char *dp_request_get_filename(int id, dp_request *request, dp_error_type *errorcode)
{
	char *filename = NULL;

	if (request == NULL) {
		dp_state_type state =
			dp_db_get_int_column(id, DP_DB_TABLE_LOG, DP_DB_COL_STATE);

		if (state <= DP_STATE_NONE) {
			TRACE_ERROR("[ERROR][%d] state[%s]", id, dp_print_state(state));
			*errorcode = DP_ERROR_ID_NOT_FOUND;
			return NULL;
		}
	}
	filename = dp_db_get_text_column
				(id, DP_DB_TABLE_REQUEST_INFO, DP_DB_COL_FILENAME);
	if (filename == NULL) {
		*errorcode = DP_ERROR_NO_DATA;
		return NULL;
	}
	return filename;
}

char *dp_request_get_contentname(int id, dp_request *request, dp_error_type *errorcode)
{
	char *content = NULL;

	if (request == NULL) {
		dp_state_type state =
			dp_db_get_int_column(id, DP_DB_TABLE_LOG, DP_DB_COL_STATE);

		if (state <= DP_STATE_NONE) {
			TRACE_ERROR("[ERROR][%d] state[%s]", id, dp_print_state(state));
			*errorcode = DP_ERROR_ID_NOT_FOUND;
			return NULL;
		}
	}
	content = dp_db_get_text_column
				(id, DP_DB_TABLE_DOWNLOAD_INFO, DP_DB_COL_CONTENT_NAME);
	if (content == NULL) {
		*errorcode = DP_ERROR_NO_DATA;
		return NULL;
	}
	return content;
}

char *dp_request_get_etag(int id, dp_request *request, dp_error_type *errorcode)
{
	char *etag = NULL;

	if (request == NULL) {
		dp_state_type state =
			dp_db_get_int_column(id, DP_DB_TABLE_LOG, DP_DB_COL_STATE);

		if (state <= DP_STATE_NONE) {
			TRACE_ERROR("[ERROR][%d] state[%s]", id, dp_print_state(state));
			*errorcode = DP_ERROR_ID_NOT_FOUND;
			return NULL;
		}
	}
	etag = dp_db_get_text_column
				(id, DP_DB_TABLE_DOWNLOAD_INFO, DP_DB_COL_ETAG);
	if (etag == NULL) {
		*errorcode = DP_ERROR_NO_DATA;
		return NULL;
	}
	return etag;
}

char *dp_request_get_savedpath(int id, dp_request *request, dp_error_type *errorcode)
{
	char *savedpath = NULL;

	if (request == NULL) {
		dp_state_type state =
			dp_db_get_int_column(id, DP_DB_TABLE_LOG, DP_DB_COL_STATE);

		if (state <= DP_STATE_NONE) {
			TRACE_ERROR("[ERROR][%d] state[%s]", id, dp_print_state(state));
			*errorcode = DP_ERROR_ID_NOT_FOUND;
			return NULL;
		}
	}
	savedpath = dp_db_get_text_column
				(id, DP_DB_TABLE_DOWNLOAD_INFO, DP_DB_COL_SAVED_PATH);
	if (savedpath == NULL) {
		*errorcode = DP_ERROR_NO_DATA;
		return NULL;
	}
	return savedpath;
}

char *dp_request_get_tmpsavedpath(int id, dp_request *request, dp_error_type *errorcode)
{
	char *tmppath = NULL;

	if (request == NULL) {
		dp_state_type state =
			dp_db_get_int_column(id, DP_DB_TABLE_LOG, DP_DB_COL_STATE);

		if (state <= DP_STATE_NONE) {
			TRACE_ERROR("[ERROR][%d] state[%s]", id, dp_print_state(state));
			*errorcode = DP_ERROR_ID_NOT_FOUND;
			return NULL;
		}
	}
	tmppath = dp_db_get_text_column
				(id, DP_DB_TABLE_DOWNLOAD_INFO, DP_DB_COL_TMP_SAVED_PATH);
	if (tmppath == NULL) {
		*errorcode = DP_ERROR_NO_DATA;
		return NULL;
	}
	return tmppath;
}

char *dp_request_get_mimetype(int id, dp_request *request, dp_error_type *errorcode)
{
	char *mimetype = NULL;

	if (request == NULL) {
		dp_state_type state =
			dp_db_get_int_column(id, DP_DB_TABLE_LOG, DP_DB_COL_STATE);

		if (state <= DP_STATE_NONE) {
			TRACE_ERROR("[ERROR][%d] state[%s]", id, dp_print_state(state));
			*errorcode = DP_ERROR_ID_NOT_FOUND;
			return NULL;
		}
	}
	mimetype = dp_db_get_text_column
				(id, DP_DB_TABLE_DOWNLOAD_INFO, DP_DB_COL_MIMETYPE);
	if (mimetype == NULL) {
		*errorcode = DP_ERROR_NO_DATA;
		return NULL;
	}
	return mimetype;
}

dp_request *dp_request_load_from_log(int id, dp_error_type *errorcode)
{
	dp_request *request = NULL;

	request = dp_db_load_logging_request(id);
	if (request == NULL) {
		*errorcode = DP_ERROR_ID_NOT_FOUND;
		return NULL;
	}
	if (request->state == DP_STATE_COMPLETED) {
		TRACE_ERROR
			("[ERROR][%d] now[%s]", id, dp_print_state(request->state));
		*errorcode =  DP_ERROR_INVALID_STATE;
		dp_request_free(request);
		return NULL;
	}
	return request;
}

