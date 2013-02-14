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

#ifndef DOWNLOAD_PROVIDER2_H
#define DOWNLOAD_PROVIDER2_H

#ifdef __cplusplus
extern "C" {
#endif

#define DP_IPC "/tmp/download-provider"

#define DP_MAX_STR_LEN_64 64
#define DP_MAX_STR_LEN 256
#define DP_MAX_PATH_LEN DP_MAX_STR_LEN
#define DP_MAX_URL_LEN 2048

#ifdef DP_SUPPORT_DBUS_ACTIVATION
#define DP_DBUS_ACTIVATION
#define DP_DBUS_SERVICE_DBUS		"org.download-provider"
#endif

#define DP_ECHO_TEST

	typedef enum {
		DP_CMD_NONE = 0,
		DP_CMD_CREATE = DP_CMD_NONE + 1,
		DP_CMD_START = DP_CMD_NONE + 2,
		DP_CMD_PAUSE = DP_CMD_NONE + 3,
		DP_CMD_CANCEL = DP_CMD_NONE + 4,
		DP_CMD_DESTROY = DP_CMD_NONE + 9,
		DP_CMD_FREE = DP_CMD_NONE + 10,
		DP_CMD_ECHO = DP_CMD_NONE + 15,
		DP_CMD_SET_URL = DP_CMD_NONE + 21,
		DP_CMD_SET_DESTINATION = DP_CMD_NONE + 22,
		DP_CMD_SET_FILENAME = DP_CMD_NONE + 23,
		DP_CMD_SET_NOTIFICATION = DP_CMD_NONE + 24,
		DP_CMD_SET_STATE_CALLBACK = DP_CMD_NONE + 25,
		DP_CMD_SET_PROGRESS_CALLBACK = DP_CMD_NONE + 26,
		DP_CMD_SET_AUTO_DOWNLOAD = DP_CMD_NONE + 28,
		DP_CMD_SET_NETWORK_TYPE = DP_CMD_NONE + 29,
		DP_CMD_SET_HTTP_HEADER = DP_CMD_NONE + 30,
		DP_CMD_SET_EXTRA_PARAM = DP_CMD_NONE + 31,
		DP_CMD_DEL_HTTP_HEADER = DP_CMD_NONE + 35,
		DP_CMD_GET_URL = DP_CMD_NONE + 41,
		DP_CMD_GET_DESTINATION = DP_CMD_NONE + 42,
		DP_CMD_GET_FILENAME = DP_CMD_NONE + 43,
		DP_CMD_GET_NOTIFICATION = DP_CMD_NONE + 44,
		DP_CMD_GET_STATE_CALLBACK = DP_CMD_NONE + 45,
		DP_CMD_GET_PROGRESS_CALLBACK = DP_CMD_NONE + 46,
		DP_CMD_GET_HTTP_HEADERS = DP_CMD_NONE + 47,
		DP_CMD_GET_AUTO_DOWNLOAD = DP_CMD_NONE + 48,
		DP_CMD_GET_NETWORK_TYPE = DP_CMD_NONE + 49,
		DP_CMD_GET_SAVED_PATH = DP_CMD_NONE + 50,
		DP_CMD_GET_TEMP_SAVED_PATH = DP_CMD_NONE + 51,
		DP_CMD_GET_MIME_TYPE = DP_CMD_NONE + 52,
		DP_CMD_GET_HTTP_HEADER = DP_CMD_NONE + 53,
		DP_CMD_GET_EXTRA_PARAM = DP_CMD_NONE + 54,
		DP_CMD_GET_RECEIVED_SIZE = DP_CMD_NONE + 71,
		DP_CMD_GET_TOTAL_FILE_SIZE = DP_CMD_NONE + 72,
		DP_CMD_GET_CONTENT_NAME = DP_CMD_NONE + 73,
		DP_CMD_GET_HTTP_STATUS = DP_CMD_NONE + 74,
		DP_CMD_GET_ETAG = DP_CMD_NONE + 75,
		DP_CMD_GET_STATE = DP_CMD_NONE + 81,
		DP_CMD_GET_ERROR = DP_CMD_NONE + 91,
		DP_CMD_SET_COMMAND_SOCKET = DP_CMD_NONE + 100,
		DP_CMD_SET_EVENT_SOCKET = DP_CMD_NONE + 101
	} dp_command_type;

	typedef enum {
		DP_STATE_NONE = 0,
		DP_STATE_READY = DP_STATE_NONE + 5, // created id, set some info.
		DP_STATE_QUEUED = DP_STATE_NONE + 10, // request to start
		DP_STATE_CONNECTING = DP_STATE_NONE + 15, // try to connect to url
		DP_STATE_DOWNLOADING = DP_STATE_NONE + 20, // started
		DP_STATE_PAUSE_REQUESTED = DP_STATE_NONE + 25,
		DP_STATE_PAUSED = DP_STATE_NONE + 30, // paused actually
		DP_STATE_COMPLETED = DP_STATE_NONE + 40,
		DP_STATE_CANCELED = DP_STATE_NONE + 45, // stopped with error
		DP_STATE_FAILED = DP_STATE_NONE + 50 // failed with error
	} dp_state_type;

	typedef enum {
		DP_ERROR_NONE = 0,
		DP_ERROR_INVALID_PARAMETER = DP_ERROR_NONE + 1,
		DP_ERROR_OUT_OF_MEMORY = DP_ERROR_NONE + 2,
		DP_ERROR_IO_ERROR = DP_ERROR_NONE + 3,
		DP_ERROR_NETWORK_UNREACHABLE = DP_ERROR_NONE + 4,
		DP_ERROR_CONNECTION_TIMED_OUT = DP_ERROR_NONE + 5,
		DP_ERROR_NO_SPACE = DP_ERROR_NONE + 6,
		DP_ERROR_FIELD_NOT_FOUND = DP_ERROR_NONE + 7,
		DP_ERROR_INVALID_STATE = DP_ERROR_NONE + 8,
		DP_ERROR_CONNECTION_FAILED = DP_ERROR_NONE + 9,
		DP_ERROR_INVALID_URL = DP_ERROR_NONE + 10,
		DP_ERROR_INVALID_DESTINATION = DP_ERROR_NONE + 11,
		DP_ERROR_QUEUE_FULL = DP_ERROR_NONE + 12,
		DP_ERROR_ALREADY_COMPLETED = DP_ERROR_NONE + 13,
		DP_ERROR_FILE_ALREADY_EXISTS = DP_ERROR_NONE + 14,
		DP_ERROR_TOO_MANY_DOWNLOADS = DP_ERROR_NONE + 15,
		DP_ERROR_NO_DATA = DP_ERROR_NONE + 17,
		DP_ERROR_UNHANDLED_HTTP_CODE = DP_ERROR_NONE + 18,
		DP_ERROR_CANNOT_RESUME = DP_ERROR_NONE + 19,
		DP_ERROR_RESPONSE_TIMEOUT = DP_ERROR_NONE + 50,
		DP_ERROR_REQUEST_TIMEOUT = DP_ERROR_NONE + 55,
		DP_ERROR_SYSTEM_DOWN = DP_ERROR_NONE + 60,
		DP_ERROR_CLIENT_DOWN = DP_ERROR_NONE + 65,
		DP_ERROR_ID_NOT_FOUND = DP_ERROR_NONE + 90,
		DP_ERROR_UNKNOWN = DP_ERROR_NONE + 100
	} dp_error_type;

	typedef enum {
		DP_NETWORK_TYPE_OFF = -1,
		DP_NETWORK_TYPE_ALL = 0,
		DP_NETWORK_TYPE_WIFI = 1,
		DP_NETWORK_TYPE_DATA_NETWORK = 2,
		DP_NETWORK_TYPE_ETHERNET = 3,
	} dp_network_type;

	typedef struct {
		unsigned int length;
		char *str;
	} dp_string;

	typedef struct {
		int id;
		dp_state_type state;
		dp_error_type err;
		unsigned long long received_size;
	} dp_event_info;

	typedef struct {
		dp_command_type cmd;
		int id;
	} dp_command;

	// Usage IPC : send(dp_command);send(dp_string);

#ifdef __cplusplus
}
#endif
#endif
