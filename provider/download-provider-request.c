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

#include <sys/time.h>
#include <sys/statfs.h>
#include <sys/smack.h>
#include "download-provider.h"
#include "download-provider-log.h"

#include "download-provider-slots.h"
#include "download-provider-socket.h"
#include "download-provider-db.h"
#include "download-provider-pthread.h"

#include "download-provider-notification.h"

///////// below functions are called by main thread of thread-request.c

//////////////////////////////////////////////////////////////////////////
/// @brief		create unique id as integer type
/// @return	unique id combined local time and the special calculation
static int __get_download_request_id(void)
{
	static int last_uniquetime = 0;
	int uniquetime = 0;

	do {
		struct timeval tval;
		int cipher = 1;
		int c = 0;

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
		uniquetime = tval.tv_sec + ((tval.tv_usec << 2) * 100) +
				((tval.tv_usec >> (cipher - 1)) * disit_unit) +
				((tval.tv_usec + (tval.tv_usec % 10)) & 0x0fff);
	} while (last_uniquetime == uniquetime);
	last_uniquetime = uniquetime;
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
		case DP_ERROR_ID_NOT_FOUND :
			return "ID_NOT_FOUND";
		default :
			break;
	}
	return "UNKNOWN";
}

int dp_is_smackfs_mounted()
{
	if(smack_smackfs_path() != NULL)
	  return 1;
	TRACE_ERROR("[SMACK DISABLE]");
	return 0;
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
	if (group == NULL || empty_slot == NULL) {
		TRACE_ERROR("[CHECK INTERNAL][%d]", id);
		return DP_ERROR_INVALID_PARAMETER;
	}
	// New allocation Slot
	dp_request *new_request = dp_request_new();
	if (new_request == NULL) {
		TRACE_STRERROR("[CHECK MEMORY][%d]", id);
		return DP_ERROR_OUT_OF_MEMORY;
	}

	int check_id = -1;
	do {
		new_request->id = __get_download_request_id();
		check_id = dp_db_get_int_column(new_request->id,
							DP_DB_TABLE_LOG, DP_DB_COL_ID);
	} while (check_id == new_request->id); // means duplicated id

	new_request->group = group;
	if (group->pkgname != NULL && strlen(group->pkgname) > 1)
		new_request->packagename = dp_strdup(group->pkgname);
	if (new_request->packagename == NULL) {
		dp_request_free(new_request);
		TRACE_ERROR("[ERROR][%d] OUT_OF_MEMORY [PACKAGENAME]", id);
		return DP_ERROR_OUT_OF_MEMORY;
	}
	new_request->state = DP_STATE_READY;
	new_request->error = DP_ERROR_NONE;
	new_request->create_time = (int)time(NULL);

	if (dp_db_request_new_logging(new_request->id, new_request->state,
			new_request->packagename) < 0) {
		dp_request_free(new_request);
		if (dp_db_is_full_error() == 0) {
			TRACE_ERROR("[SQLITE_FULL]");
			return DP_ERROR_NO_SPACE;
		}
		return DP_ERROR_OUT_OF_MEMORY;
	}
	*empty_slot = new_request;
	return DP_ERROR_NONE;
}

dp_error_type dp_request_set_url(int id, dp_request *request, char *url)
{
	int length = 0;
	if (url == NULL || (length = strlen(url)) <= 1)
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
		dp_state_type state = dp_db_get_state(id);
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
		if (dp_db_is_full_error() == 0) {
			TRACE_ERROR("[SQLITE_FULL][%d]", id);
			return DP_ERROR_NO_SPACE;
		}
		return DP_ERROR_OUT_OF_MEMORY;
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
		dp_state_type state = dp_db_get_state(id);
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
		if (dp_db_is_full_error() == 0) {
			TRACE_ERROR("[SQLITE_FULL][%d]", id);
			return DP_ERROR_NO_SPACE;
		}
		return DP_ERROR_OUT_OF_MEMORY;
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
		dp_state_type state = dp_db_get_state(id);
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
		if (dp_db_is_full_error() == 0) {
			TRACE_ERROR("[SQLITE_FULL][%d]", id);
			return DP_ERROR_NO_SPACE;
		}
		return DP_ERROR_OUT_OF_MEMORY;
	}

	TRACE_SECURE_DEBUG("ID [%d] Filename[%s]", id, filename);
	return DP_ERROR_NONE;
}

dp_error_type dp_request_set_title(int id, dp_request *request, char *title)
{
	int length = 0;
	if (!title || (length = strlen(title)) <= 1)
		return DP_ERROR_INVALID_PARAMETER;

	if (request != NULL) {
		if (request->state == DP_STATE_COMPLETED) {
			TRACE_ERROR
			("[ERROR][%d] now[%s]", id, dp_print_state(request->state));
			return DP_ERROR_INVALID_STATE;
		}
	} else {
		// check id in logging table.
		dp_state_type state = dp_db_get_state(id);
		// check again from logging table
		if (state == DP_STATE_COMPLETED) {
			TRACE_ERROR("[ERROR][%d] now[%s]", id, dp_print_state(state));
			return DP_ERROR_INVALID_STATE;
		}
	}

	if (dp_db_replace_column
			(id, DP_DB_TABLE_NOTIFICATION, DP_DB_COL_TITLE,
			DP_DB_COL_TYPE_TEXT, title) < 0) {
		TRACE_ERROR("[CHECK SQL][%d]", id);
		if (dp_db_is_full_error() == 0) {
			TRACE_ERROR("[SQLITE_FULL][%d]", id);
			return DP_ERROR_NO_SPACE;
		}
		return DP_ERROR_OUT_OF_MEMORY;
	}

	TRACE_SECURE_DEBUG("ID [%d] title[%s]", id, title);
	return DP_ERROR_NONE;
}

dp_error_type dp_request_set_bundle(int id, dp_request *request, int type, bundle_raw *b, unsigned length)
{
	char *column = NULL;
	if (b == NULL || (length  < 1))
		return DP_ERROR_INVALID_PARAMETER;

	if (request != NULL) {
		if (request->state == DP_STATE_COMPLETED) {
			TRACE_ERROR
			("[ERROR][%d] now[%s]", id, dp_print_state(request->state));
			return DP_ERROR_INVALID_STATE;
		}
	} else {
		// check id in logging table.
		dp_state_type state = dp_db_get_state(id);
		// check again from logging table
		if (state == DP_STATE_COMPLETED) {
			TRACE_ERROR("[ERROR][%d] now[%s]", id, dp_print_state(state));
			return DP_ERROR_INVALID_STATE;
		}
	}

	switch(type) {
	case DP_NOTIFICATION_BUNDLE_TYPE_ONGOING:
		column = DP_DB_COL_RAW_BUNDLE_ONGOING;
		break;
	case DP_NOTIFICATION_BUNDLE_TYPE_COMPLETE:
		column = DP_DB_COL_RAW_BUNDLE_COMPLETE;
		break;
	case DP_NOTIFICATION_BUNDLE_TYPE_FAILED:
		column = DP_DB_COL_RAW_BUNDLE_FAIL;
		break;
	default:
		TRACE_ERROR("[CHECK TYPE][%d]", type);
		return DP_ERROR_INVALID_PARAMETER;
	}
	if (dp_db_replace_blob_column
			(id, DP_DB_TABLE_NOTIFICATION, column, b, length) < 0) {
		TRACE_ERROR("[CHECK SQL][%d]", id);
		if (dp_db_is_full_error() == 0) {
			TRACE_ERROR("[SQLITE_FULL][%d]", id);
			return DP_ERROR_NO_SPACE;
		}
		return DP_ERROR_OUT_OF_MEMORY;
	}

	//TRACE_SECURE_DEBUG("ID [%d] title[%s]", id, title);
	return DP_ERROR_NONE;
}

dp_error_type dp_request_set_description(int id, dp_request *request, char *description)
{
	int length = 0;
	if (!description || (length = strlen(description)) <= 1)
		return DP_ERROR_INVALID_PARAMETER;

	if (request != NULL) {
		if (request->state == DP_STATE_COMPLETED) {
			TRACE_ERROR
			("[ERROR][%d] now[%s]", id, dp_print_state(request->state));
			return DP_ERROR_INVALID_STATE;
		}
	} else {
		// check id in logging table.
		dp_state_type state = dp_db_get_state(id);
		// check again from logging table
		if (state == DP_STATE_COMPLETED) {
			TRACE_ERROR("[ERROR][%d] now[%s]", id, dp_print_state(state));
			return DP_ERROR_INVALID_STATE;
		}
	}

	if (dp_db_replace_column
			(id, DP_DB_TABLE_NOTIFICATION, DP_DB_COL_DESCRIPTION,
			DP_DB_COL_TYPE_TEXT, description) < 0) {
		TRACE_ERROR("[CHECK SQL][%d]", id);
		if (dp_db_is_full_error() == 0) {
			TRACE_ERROR("[SQLITE_FULL][%d]", id);
			return DP_ERROR_NO_SPACE;
		}
		return DP_ERROR_OUT_OF_MEMORY;
	}

	TRACE_SECURE_DEBUG("ID [%d] description[%s]", id, description);
	return DP_ERROR_NONE;
}

dp_error_type dp_request_set_noti_type(int id, dp_request *request, unsigned type)
{
	if (request != NULL) {
		if (request->state == DP_STATE_COMPLETED) {
			TRACE_ERROR
			("[ERROR][%d] now[%s]", id, dp_print_state(request->state));
			return DP_ERROR_INVALID_STATE;
		}
	} else {
		// check id in logging table.
		dp_state_type state = dp_db_get_state(id);
		// check again from logging table
		if (state == DP_STATE_COMPLETED) {
			TRACE_ERROR("[ERROR][%d] now[%s]", id, dp_print_state(state));
			return DP_ERROR_INVALID_STATE;
		}
	}

	if (dp_db_replace_column
			(id, DP_DB_TABLE_NOTIFICATION, DP_DB_COL_NOTI_TYPE,
			DP_DB_COL_TYPE_INT, &type) < 0) {
		TRACE_ERROR("[CHECK SQL][%d]", id);
		if (dp_db_is_full_error() == 0) {
			TRACE_ERROR("[SQLITE_FULL][%d]", id);
			return DP_ERROR_NO_SPACE;
		}
		return DP_ERROR_OUT_OF_MEMORY;
	}
	if (request)
	{
		if(!type)
			request->auto_notification = 0;
		else
			request->auto_notification = 1;
	}
	TRACE_SECURE_DEBUG("ID [%d] enable[%d]", id, type);
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
		dp_state_type state = dp_db_get_state(id);
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
		return DP_ERROR_OUT_OF_MEMORY;
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
		dp_state_type state = dp_db_get_state(id);
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
		return DP_ERROR_OUT_OF_MEMORY;
	}
	return DP_ERROR_NONE;
}

dp_error_type dp_request_set_state_event(int id, dp_request *request, unsigned enable)
{
	if (request == NULL) {
		// check id in logging table.
		dp_state_type state = dp_db_get_state(id);

		if (state == DP_STATE_DOWNLOADING ||
			state == DP_STATE_COMPLETED) {
			TRACE_ERROR("[ERROR][%d] now[%s]", id, dp_print_state(state));
			return DP_ERROR_INVALID_STATE;
		}
	}

	// update queue DB
	if (dp_db_replace_column
			(id, DP_DB_TABLE_REQUEST_INFO, DP_DB_COL_STATE_EVENT,
			DP_DB_COL_TYPE_INT, &enable) < 0) {
		TRACE_ERROR("[CHECK SQL][%d]", id);
		return DP_ERROR_OUT_OF_MEMORY;
	}
	// update memory
	if (request != NULL)
		request->state_cb = enable;
	return DP_ERROR_NONE;
}

dp_error_type dp_request_set_progress_event(int id, dp_request *request, unsigned enable)
{
	if (request == NULL) {
		// check id in logging table.
		dp_state_type state = dp_db_get_state(id);

		if (state == DP_STATE_DOWNLOADING ||
			state == DP_STATE_COMPLETED) {
			TRACE_ERROR("[ERROR][%d] now[%s]", id, dp_print_state(state));
			return DP_ERROR_INVALID_STATE;
		}
	}

	// update queue DB
	if (dp_db_replace_column
			(id, DP_DB_TABLE_REQUEST_INFO, DP_DB_COL_PROGRESS_EVENT,
			DP_DB_COL_TYPE_INT, &enable) < 0) {
		TRACE_ERROR("[CHECK SQL][%d]", id);
		return DP_ERROR_OUT_OF_MEMORY;
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
		dp_state_type state = dp_db_get_state(id);
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
		return DP_ERROR_OUT_OF_MEMORY;
	}
	// update memory
	if (request)
		request->network_type = type;
	return DP_ERROR_NONE;
}

char *dp_request_get_url(int id, dp_error_type *errorcode)
{
	char *url = NULL;
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
	filename = dp_db_get_text_column
				(id, DP_DB_TABLE_REQUEST_INFO, DP_DB_COL_FILENAME);
	if (filename == NULL) {
		*errorcode = DP_ERROR_NO_DATA;
		return NULL;
	}
	return filename;
}

char *dp_request_get_title(int id, dp_request *request, dp_error_type *errorcode)
{
	char *title = NULL;
	title = dp_db_get_text_column
				(id, DP_DB_TABLE_NOTIFICATION, DP_DB_COL_TITLE);
	if (title == NULL) {
		*errorcode = DP_ERROR_NO_DATA;
		return NULL;
	}
	return title;
}

bundle_raw *dp_request_get_bundle(int id, dp_request *request, dp_error_type *errorcode, char* column, int *length)
{
	void *blob_data = NULL;
	blob_data = dp_db_get_blob_column
				(id, DP_DB_TABLE_NOTIFICATION, column, length);
	if (blob_data == NULL) {
		*errorcode = DP_ERROR_NO_DATA;
		return NULL;
	}
	return (bundle_raw*)blob_data;
}


char *dp_request_get_description(int id, dp_request *request, dp_error_type *errorcode)
{
	char *description = NULL;
	description = dp_db_get_text_column
				(id, DP_DB_TABLE_NOTIFICATION, DP_DB_COL_DESCRIPTION);
	if (description == NULL) {
		*errorcode = DP_ERROR_NO_DATA;
		return NULL;
	}
	return description;
}

int dp_request_get_noti_type(int id, dp_request *request, dp_error_type *errorcode)
{
	int type = -1;
	type = dp_db_get_int_column
				(id, DP_DB_TABLE_NOTIFICATION, DP_DB_COL_NOTI_TYPE);
	if (type == -1)
		*errorcode = DP_ERROR_NO_DATA;
	return type;
}



char *dp_request_get_contentname(int id, dp_request *request, dp_error_type *errorcode)
{
	char *content = NULL;
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
	mimetype = dp_db_get_text_column
				(id, DP_DB_TABLE_DOWNLOAD_INFO, DP_DB_COL_MIMETYPE);
	if (mimetype == NULL) {
		*errorcode = DP_ERROR_NO_DATA;
		return NULL;
	}
	return mimetype;
}

char *dp_request_get_pkg_name(int id, dp_request *request, dp_error_type *errorcode)
{
	char *pkg_name = NULL;
	pkg_name = dp_db_get_text_column
				(id, DP_DB_TABLE_LOG, DP_DB_COL_PACKAGENAME);
	if (pkg_name == NULL) {
		*errorcode = DP_ERROR_NO_DATA;
		return NULL;
	}
	return pkg_name;
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


void dp_request_state_response(dp_request *request)
{
	if (request == NULL) {
		return ;
	}

	TRACE_INFO("[INFO][%d] state:%s error:%s", request->id,
			dp_print_state(request->state),
			dp_print_errorcode(request->error));

	if (dp_db_request_update_status(request->id, request->state,
			request->error) < 0)
		TRACE_ERROR("[ERROR][%d][SQL]", request->id);

	if (request->group != NULL && request->group->event_socket >= 0 &&
			request->state_cb == 1) {
		dp_ipc_send_event(request->group->event_socket, request->id,
			request->state, request->error, 0);
	}

	if (request->state == DP_STATE_DOWNLOADING) {
		if (request->auto_notification == 1 &&
				request->packagename != NULL) {
			if (request->noti_priv_id != -1) {
				dp_update_downloadinginfo_notification
					(request->noti_priv_id,
					(double)request->received_size,
					(double)request->file_size);
			} else {
				request->noti_priv_id = dp_set_downloadinginfo_notification
					(request->id, request->packagename);
			}
		} else {
			int noti_type = dp_db_get_int_column(request->id,
					DP_DB_TABLE_NOTIFICATION, DP_DB_COL_NOTI_TYPE);
			if (noti_type == DP_NOTIFICATION_TYPE_ALL &&
					request->packagename != NULL) {
				if (request->noti_priv_id != -1) {
					dp_update_downloadinginfo_notification
						(request->noti_priv_id,
						(double)request->received_size,
						(double)request->file_size);
				} else {
					request->noti_priv_id = dp_set_downloadinginfo_notification
						(request->id, request->packagename);
				}
			}
		}
		request->start_time = (int)time(NULL);
		request->pause_time = 0;
		request->stop_time = 0;
	} else if (request->state == DP_STATE_PAUSED) {
		if (request->group != NULL)
			request->group->queued_count--;
		request->pause_time = (int)time(NULL);
	} else {
		if (request->group != NULL )
			request->group->queued_count--;

		if (request->auto_notification == 1 &&
				request->packagename != NULL) {
			request->noti_priv_id = dp_set_downloadedinfo_notification
					(request->noti_priv_id, request->id,
					request->packagename, request->state);

		} else {
			int noti_type = dp_db_get_int_column(request->id,
					DP_DB_TABLE_NOTIFICATION, DP_DB_COL_NOTI_TYPE);
			if (noti_type > DP_NOTIFICATION_TYPE_NONE &&
					request->packagename != NULL)
				request->noti_priv_id = dp_set_downloadedinfo_notification
						(request->noti_priv_id, request->id,
						request->packagename, request->state);
		}

		request->stop_time = (int)time(NULL);
	}
}
