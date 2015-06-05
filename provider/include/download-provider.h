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

#ifndef DOWNLOAD_PROVIDER_H
#define DOWNLOAD_PROVIDER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	DP_STATE_NONE = 0,
	DP_STATE_READY = DP_STATE_NONE + 5, // created id, set some info.
	DP_STATE_QUEUED = DP_STATE_NONE + 10, // request to start
	DP_STATE_CONNECTING = DP_STATE_NONE + 15, // try to connect to url
	DP_STATE_DOWNLOADING = DP_STATE_NONE + 20, // started
	DP_STATE_PAUSED = DP_STATE_NONE + 30, // paused actually
	DP_STATE_COMPLETED = DP_STATE_NONE + 40,
	DP_STATE_CANCELED = DP_STATE_NONE + 45, // stopped with error
	DP_STATE_FAILED = DP_STATE_NONE + 50 // failed with error
} dp_state_type;

typedef enum {
	DP_ERROR_NONE = 10,
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
	DP_ERROR_NETWORK_ERROR = DP_ERROR_NONE + 16,
	DP_ERROR_NO_DATA = DP_ERROR_NONE + 17,
	DP_ERROR_UNHANDLED_HTTP_CODE = DP_ERROR_NONE + 18,
	DP_ERROR_CANNOT_RESUME = DP_ERROR_NONE + 19,
	DP_ERROR_PERMISSION_DENIED = DP_ERROR_NONE + 20,
	DP_ERROR_INVALID_NETWORK_TYPE = DP_ERROR_NONE + 21,
	DP_ERROR_RESPONSE_TIMEOUT = DP_ERROR_NONE + 50,
	DP_ERROR_REQUEST_TIMEOUT = DP_ERROR_NONE + 55,
	DP_ERROR_SYSTEM_DOWN = DP_ERROR_NONE + 60,
	DP_ERROR_CLIENT_DOWN = DP_ERROR_NONE + 65,
	DP_ERROR_DISK_BUSY = DP_ERROR_NONE + 70,
	DP_ERROR_ID_NOT_FOUND = DP_ERROR_NONE + 90,
	DP_ERROR_IO_EAGAIN = DP_ERROR_NONE + 97,
	DP_ERROR_IO_EINTR = DP_ERROR_NONE + 98,
	DP_ERROR_IO_TIMEOUT = DP_ERROR_NONE + 99,
	DP_ERROR_UNKNOWN = DP_ERROR_NONE + 100
} dp_error_type;

typedef enum {
	DP_NOTIFICATION_BUNDLE_TYPE_ONGOING = 0, // Ongoing, Failed
	DP_NOTIFICATION_BUNDLE_TYPE_COMPLETE, // Completed
	DP_NOTIFICATION_BUNDLE_TYPE_FAILED // Failed
} dp_notification_bundle_type;

typedef enum {
	DP_NOTIFICATION_SERVICE_TYPE_ONGOING = 0, // Ongoing, Failed
	DP_NOTIFICATION_SERVICE_TYPE_COMPLETE, // Completed
	DP_NOTIFICATION_SERVICE_TYPE_FAILED // Failed
} dp_notification_service_type;


typedef enum {
	DP_NOTIFICATION_TYPE_NONE = 0, // Not register Noti.
	DP_NOTIFICATION_TYPE_COMPLETE_ONLY, // Success, Failed
	DP_NOTIFICATION_TYPE_ALL // Ongoing, Success, Failed
} dp_notification_type;


#ifndef IPC_SOCKET
#define IPC_SOCKET "/opt/data/download-provider/download-provider.sock"
#endif

#define MAX_DOWNLOAD_HANDLE 32
#define DP_MAX_STR_LEN 2048
#define DP_DEFAULT_BUFFER_SIZE 1024

// string to check invalid characters in path before using open() and fopen() API's
#define DP_INVALID_PATH_STRING ";\\\":*?<>|()"


#include <unistd.h>

typedef struct {
	pid_t pid;
	uid_t uid;
	gid_t gid;
} dp_credential;

typedef enum {
	DP_SEC_NONE = 0,
	DP_SEC_INIT,
	DP_SEC_DEINIT,
	DP_SEC_CONTROL,
	DP_SEC_GET,
	DP_SEC_SET,
	DP_SEC_UNSET
} dp_ipc_section_defs;

typedef enum {
	DP_PROP_NONE = 0,
	DP_PROP_CREATE,
	DP_PROP_START,
	DP_PROP_PAUSE,
	DP_PROP_CANCEL,
	DP_PROP_DESTROY,
	DP_PROP_URL,
	DP_PROP_DESTINATION,
	DP_PROP_FILENAME,
	DP_PROP_STATE_CALLBACK,
	DP_PROP_PROGRESS_CALLBACK,
	DP_PROP_AUTO_DOWNLOAD,
	DP_PROP_NETWORK_TYPE,
	DP_PROP_NETWORK_BONDING,
	DP_PROP_SAVED_PATH,
	DP_PROP_TEMP_SAVED_PATH,
	DP_PROP_MIME_TYPE,
	DP_PROP_RECEIVED_SIZE,
	DP_PROP_TOTAL_FILE_SIZE,
	DP_PROP_CONTENT_NAME,
	DP_PROP_HTTP_STATUS,
	DP_PROP_ETAG,
	DP_PROP_STATE,
	DP_PROP_ERROR,
	DP_PROP_HTTP_HEADERS,
	DP_PROP_HTTP_HEADER,
	DP_PROP_NOTIFICATION_RAW,
	DP_PROP_NOTIFICATION_SUBJECT,
	DP_PROP_NOTIFICATION_DESCRIPTION,
	DP_PROP_NOTIFICATION_TYPE
} dp_ipc_property_defs;

typedef enum {
	DP_CONTENT_UNKNOWN = 0,
	DP_CONTENT_IMAGE,
	DP_CONTENT_VIDEO,
	DP_CONTENT_MUSIC,
	DP_CONTENT_PDF,
	DP_CONTENT_WORD,
	DP_CONTENT_PPT, // 5
	DP_CONTENT_EXCEL,
	DP_CONTENT_HTML,
	DP_CONTENT_TEXT,
	DP_CONTENT_DRM,
	DP_CONTENT_SD_DRM, //10
	DP_CONTENT_FLASH,
	DP_CONTENT_TPK,
	DP_CONTENT_VCAL, //13
} dp_content_type;


typedef struct {
	short section;
	unsigned property;
	int id;
	int errorcode;
	size_t size; // followed extra packet size
} dp_ipc_fmt;

typedef struct {
	int id;
	int state;
	int errorcode;
	unsigned long long received_size;
} dp_ipc_event_fmt;

#ifdef __cplusplus
}
#endif
#endif
