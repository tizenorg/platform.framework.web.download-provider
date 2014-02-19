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

#ifndef __DOWNLOAD_PROVIDER_INTERFACE_H__
#define __DOWNLOAD_PROVIDER_INTERFACE_H__

#ifndef EXPORT_API
#define EXPORT_API __attribute__((visibility("default")))
#endif

#include <tizen.h>
#include <bundle.h>

#ifdef __cplusplus
extern "C"
{
#endif

// sync with url-download
typedef enum
{
	DOWNLOAD_ADPATOR_STATE_NONE,
	DOWNLOAD_ADPATOR_STATE_READY,
	DOWNLOAD_ADPATOR_STATE_QUEUED,
	DOWNLOAD_ADPATOR_STATE_DOWNLOADING,
	DOWNLOAD_ADPATOR_STATE_PAUSED,
	DOWNLOAD_ADPATOR_STATE_COMPLETED,
	DOWNLOAD_ADPATOR_STATE_FAILED,
	DOWNLOAD_ADPATOR_STATE_CANCELED,
} download_adaptor_state_e;

typedef enum
{
	DOWNLOAD_ADAPTOR_NETWORK_DATA_NETWORK,
	DOWNLOAD_ADAPTOR_NETWORK_WIFI,
	DOWNLOAD_ADAPTOR_NETWORK_WIFI_DIRECT,
	DOWNLOAD_ADAPTOR_NETWORK_ALL
} download_adaptor_network_type_e ;

typedef enum
{
	DOWNLOAD_ADAPTOR_ERROR_NONE = TIZEN_ERROR_NONE, /**< Successful */
	DOWNLOAD_ADAPTOR_ERROR_INVALID_PARAMETER = TIZEN_ERROR_INVALID_PARAMETER, /**< Invalid parameter */
	DOWNLOAD_ADAPTOR_ERROR_OUT_OF_MEMORY = TIZEN_ERROR_OUT_OF_MEMORY, /**< Out of memory */
	DOWNLOAD_ADAPTOR_ERROR_NETWORK_UNREACHABLE = TIZEN_ERROR_NETWORK_UNREACHABLE, /**< Network is unreachable */
	DOWNLOAD_ADAPTOR_ERROR_CONNECTION_TIMED_OUT = TIZEN_ERROR_CONNECTION_TIME_OUT, /**< Http session time-out */
	DOWNLOAD_ADAPTOR_ERROR_NO_SPACE = TIZEN_ERROR_FILE_NO_SPACE_ON_DEVICE, /**< No space left on device */
	DOWNLOAD_ADAPTOR_ERROR_FIELD_NOT_FOUND = TIZEN_ERROR_KEY_NOT_AVAILABLE, /**< Specified field not found */
	DOWNLOAD_ADAPTOR_ERROR_PERMISSION_DENIED = TIZEN_ERROR_PERMISSION_DENIED, /**< Permission denied */
	DOWNLOAD_ADAPTOR_ERROR_INVALID_STATE = TIZEN_ERROR_WEB_CLASS | 0x21, /**< Invalid state */
	DOWNLOAD_ADAPTOR_ERROR_CONNECTION_FAILED = TIZEN_ERROR_WEB_CLASS | 0x22, /**< Connection failed */
	DOWNLOAD_ADAPTOR_ERROR_INVALID_URL = TIZEN_ERROR_WEB_CLASS | 0x24, /**< Invalid URL */
	DOWNLOAD_ADAPTOR_ERROR_INVALID_DESTINATION = TIZEN_ERROR_WEB_CLASS | 0x25, /**< Invalid destination */
	DOWNLOAD_ADAPTOR_ERROR_TOO_MANY_DOWNLOADS = TIZEN_ERROR_WEB_CLASS | 0x26, /**< Full of available simultaneous downloads */
	DOWNLOAD_ADAPTOR_ERROR_QUEUE_FULL = TIZEN_ERROR_WEB_CLASS | 0x27, /**< Full of available downloading items from server*/
	DOWNLOAD_ADAPTOR_ERROR_ALREADY_COMPLETED = TIZEN_ERROR_WEB_CLASS | 0x28, /**< The download is already completed */
	DOWNLOAD_ADAPTOR_ERROR_FILE_ALREADY_EXISTS = TIZEN_ERROR_WEB_CLASS | 0x29, /**< It is failed to rename the downloaded file */
	DOWNLOAD_ADAPTOR_ERROR_CANNOT_RESUME = TIZEN_ERROR_WEB_CLASS | 0x2a, /**< It cannot resume */
	DOWNLOAD_ADAPTOR_ERROR_TOO_MANY_REDIRECTS = TIZEN_ERROR_WEB_CLASS | 0x30, /**< In case of too may redirects from http response header*/
	DOWNLOAD_ADAPTOR_ERROR_UNHANDLED_HTTP_CODE = TIZEN_ERROR_WEB_CLASS | 0x31,  /**< The download cannot handle the http status value */
	DOWNLOAD_ADAPTOR_ERROR_REQUEST_TIMEOUT = TIZEN_ERROR_WEB_CLASS | 0x32, /**< There are no action after client create a download id*/
	DOWNLOAD_ADAPTOR_ERROR_RESPONSE_TIMEOUT = TIZEN_ERROR_WEB_CLASS | 0x33, /**< It does not call start API in some time although the download is created*/
	DOWNLOAD_ADAPTOR_ERROR_SYSTEM_DOWN = TIZEN_ERROR_WEB_CLASS | 0x34, /**< There are no response from client after rebooting download daemon*/
	DOWNLOAD_ADAPTOR_ERROR_ID_NOT_FOUND = TIZEN_ERROR_WEB_CLASS | 0x35, /**< The download id is not existed in download service module*/
	DOWNLOAD_ADAPTOR_ERROR_NO_DATA = TIZEN_ERROR_NO_DATA, /**< No data because the set API is not called */
	DOWNLOAD_ADAPTOR_ERROR_IO_ERROR = TIZEN_ERROR_IO_ERROR , /**< Internal I/O error */
} download_adaptor_error_e;

// sync types with url-download..
typedef void (*dp_interface_state_changed_cb) (int id, int state, void *user_data);
typedef void (*dp_interface_progress_cb) (int id, unsigned long long received, void *user_data);

EXPORT_API int dp_interface_set_state_changed_cb
	(const int id, dp_interface_state_changed_cb callback, void *user_data);
EXPORT_API int dp_interface_unset_state_changed_cb(int id);
EXPORT_API int dp_interface_set_progress_cb
	(const int id, dp_interface_progress_cb callback, void *user_data);
EXPORT_API int dp_interface_unset_progress_cb(const int id);

EXPORT_API int dp_interface_create(int *id);
EXPORT_API int dp_interface_destroy(const int id);

EXPORT_API int dp_interface_start(const int id);
EXPORT_API int dp_interface_pause(const int id);
EXPORT_API int dp_interface_cancel(const int id);

EXPORT_API int dp_interface_set_url(const int id, const char *url);
EXPORT_API int dp_interface_get_url(const int id, char **url);
EXPORT_API int dp_interface_set_network_type(const int id, int net_type);
EXPORT_API int dp_interface_get_network_type(const int id, int *net_type);
EXPORT_API int dp_interface_set_destination(const int id, const char *path);
EXPORT_API int dp_interface_get_destination(const int id, char **path);
EXPORT_API int dp_interface_set_file_name(const int id, const char *file_name);
EXPORT_API int dp_interface_get_file_name(const int id, char **file_name);
EXPORT_API int dp_interface_set_notification(const int id, int enable);
EXPORT_API int dp_interface_get_notification(const int id, int *enable);
EXPORT_API int dp_interface_set_notification_extra_param(const int id, char *key, char *value);
EXPORT_API int dp_interface_get_notification_extra_param(const int id, char **key, char **value);
EXPORT_API int dp_interface_get_downloaded_file_path(const int id, char **path);
EXPORT_API int dp_interface_get_mime_type(const int id, char **mime_type);
EXPORT_API int dp_interface_set_auto_download(const int id, int enable);
EXPORT_API int dp_interface_get_auto_download(const int id, int *enable);
EXPORT_API int dp_interface_add_http_header_field(const int id, const char *field, const char *value);
EXPORT_API int dp_interface_get_http_header_field(const int id, const char *field, char **value);
EXPORT_API int dp_interface_get_http_header_field_list(const int id, char ***fields, int *length);
EXPORT_API int dp_interface_remove_http_header_field(const int id, const char *field);
EXPORT_API int dp_interface_get_state(const int id, int *state);
EXPORT_API int dp_interface_get_temp_path(const int id, char **temp_path);
EXPORT_API int dp_interface_get_content_name(const int id, char **content_name);
EXPORT_API int dp_interface_get_content_size(const int id, unsigned long long *content_size);
EXPORT_API int dp_interface_get_error(const int id, int *error);
EXPORT_API int dp_interface_get_http_status(const int id, int *http_status);

// Notification Extra Param
// N values per a key
EXPORT_API int dp_interface_add_noti_extra(const int id, const char *key, const char **values, const unsigned length);
EXPORT_API int dp_interface_get_noti_extra_values(const int id, const char *key, char ***values, unsigned *length);
EXPORT_API int dp_interface_remove_noti_extra_key(const int id, const char *key);

EXPORT_API int dp_interface_set_notification_bundle(const int id, int type, bundle *b);
EXPORT_API int dp_interface_get_notification_bundle(const int id, int type, bundle **b);
EXPORT_API int dp_interface_set_notification_title(const int id, const char *title);
EXPORT_API int dp_interface_get_notification_title(const int id, char **title);
EXPORT_API int dp_interface_set_notification_description(const int id, const char *description);
EXPORT_API int dp_interface_get_notification_description(const int id, char **description);
EXPORT_API int dp_interface_set_notification_type(const int id, int type);
EXPORT_API int dp_interface_get_notification_type(const int id, int *type);
#ifdef __cplusplus
}
#endif

#endif /* __DOWNLOAD_PROVIDER_INTERFACE_H__ */
