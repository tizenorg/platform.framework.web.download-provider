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

#include "download-provider-defs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DP_IPC "/tmp/download-provider"

#define DP_MAX_STR_LEN_64 64
#define DP_MAX_STR_LEN 256
#define DP_MAX_PATH_LEN DP_MAX_STR_LEN
#define DP_MAX_URL_LEN 2048
#define DP_DEFAULT_BUFFER_SIZE 1024

typedef enum {
	DP_CMD_NONE = 0,
	DP_CMD_ECHO,
	DP_CMD_CREATE,
	DP_CMD_START,
	DP_CMD_SET_COMMAND_SOCKET,
	DP_CMD_SET_EVENT_SOCKET,
	DP_CMD_FREE,

	DP_CMD_ACTION_SECT = 100,
	DP_CMD_PAUSE,
	DP_CMD_CANCEL,
	DP_CMD_DESTROY,

	DP_CMD_GET_SECT = 200,
	DP_CMD_GET_URL,
	DP_CMD_GET_DESTINATION,
	DP_CMD_GET_FILENAME,
	DP_CMD_GET_NOTIFICATION,
	DP_CMD_GET_STATE_CALLBACK,
	DP_CMD_GET_PROGRESS_CALLBACK,
	DP_CMD_GET_HTTP_HEADERS,
	DP_CMD_GET_AUTO_DOWNLOAD,
	DP_CMD_GET_NETWORK_TYPE,
	DP_CMD_GET_SAVED_PATH,
	DP_CMD_GET_TEMP_SAVED_PATH,
	DP_CMD_GET_MIME_TYPE,
	DP_CMD_GET_HTTP_HEADER,
	DP_CMD_GET_HTTP_HEADER_LIST,
	DP_CMD_GET_EXTRA_PARAM,
	DP_CMD_GET_RECEIVED_SIZE,
	DP_CMD_GET_TOTAL_FILE_SIZE,
	DP_CMD_GET_CONTENT_NAME,
	DP_CMD_GET_HTTP_STATUS,
	DP_CMD_GET_ETAG,
	DP_CMD_GET_STATE,
	DP_CMD_GET_ERROR,
	DP_CMD_GET_NOTIFICATION_BUNDLE,
	DP_CMD_GET_NOTIFICATION_TITLE,
	DP_CMD_GET_NOTIFICATION_DESCRIPTION,
	DP_CMD_GET_NOTIFICATION_TYPE,

	DP_CMD_SET_SECT = 300,
	DP_CMD_SET_URL,
	DP_CMD_SET_DESTINATION,
	DP_CMD_SET_FILENAME,
	DP_CMD_SET_NOTIFICATION,
	DP_CMD_SET_STATE_CALLBACK,
	DP_CMD_SET_PROGRESS_CALLBACK,
	DP_CMD_SET_AUTO_DOWNLOAD,
	DP_CMD_SET_NETWORK_TYPE,
	DP_CMD_SET_HTTP_HEADER,
	DP_CMD_SET_EXTRA_PARAM, // prevent build error
	DP_CMD_DEL_HTTP_HEADER,
	DP_CMD_ADD_EXTRA_PARAM,
	DP_CMD_REMOVE_EXTRA_PARAM,
	DP_CMD_SET_NOTIFICATION_BUNDLE,
	DP_CMD_SET_NOTIFICATION_TITLE,
	DP_CMD_SET_NOTIFICATION_DESCRIPTION,
	DP_CMD_SET_NOTIFICATION_TYPE,

	DP_CMD_LAST_SECT = 400
} dp_command_type;

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
