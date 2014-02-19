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

#ifndef DOWNLOAD_PROVIDER_DEFS_H
#define DOWNLOAD_PROVIDER_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

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
	DP_ERROR_NO_DATA = DP_ERROR_NONE + 17,
	DP_ERROR_UNHANDLED_HTTP_CODE = DP_ERROR_NONE + 18,
	DP_ERROR_CANNOT_RESUME = DP_ERROR_NONE + 19,
	DP_ERROR_PERMISSION_DENIED = DP_ERROR_NONE + 20,
	DP_ERROR_RESPONSE_TIMEOUT = DP_ERROR_NONE + 50,
	DP_ERROR_REQUEST_TIMEOUT = DP_ERROR_NONE + 55,
	DP_ERROR_SYSTEM_DOWN = DP_ERROR_NONE + 60,
	DP_ERROR_CLIENT_DOWN = DP_ERROR_NONE + 65,
	DP_ERROR_ID_NOT_FOUND = DP_ERROR_NONE + 90,
	DP_ERROR_IO_EAGAIN = DP_ERROR_NONE + 97,
	DP_ERROR_IO_EINTR = DP_ERROR_NONE + 98,
	DP_ERROR_IO_TIMEOUT = DP_ERROR_NONE + 99,
	DP_ERROR_UNKNOWN = DP_ERROR_NONE + 100
} dp_error_type;

typedef enum {
	DP_NETWORK_TYPE_OFF = -1,
	DP_NETWORK_TYPE_ALL = 0,
	DP_NETWORK_TYPE_WIFI = 1,
	DP_NETWORK_TYPE_DATA_NETWORK = 2,
	DP_NETWORK_TYPE_ETHERNET = 3,
	DP_NETWORK_TYPE_WIFI_DIRECT = 4
} dp_network_type;

typedef enum {
	DP_NOTIFICATION_BUNDLE_TYPE_ONGOING = 0, // Ongoing, Failed
	DP_NOTIFICATION_BUNDLE_TYPE_COMPLETE, // Completed
	DP_NOTIFICATION_BUNDLE_TYPE_FAILED // Failed
} dp_notification_bundle_type;

typedef enum {
	DP_NOTIFICATION_TYPE_NONE = 0, // Not register Noti.
	DP_NOTIFICATION_TYPE_COMPLETE_ONLY, // Success, Failed
	DP_NOTIFICATION_TYPE_ALL // Ongoing, Success, Failed
} dp_notification_type;

#ifdef __cplusplus
}
#endif
#endif
