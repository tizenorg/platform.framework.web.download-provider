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

#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>

#include "bundle.h"
#include "notification.h"
#include "appsvc.h"

#include "download-provider-defs.h"

#include "download-provider-notification.h"
#include "download-provider-request.h"
#include "download-provider-db.h"
#include "download-provider-log.h"

#include <libintl.h>
#define S_(s) dgettext("sys_string", s)
#define __(s) dgettext(PKG_NAME, s)

#define DP_NOTIFICATION_ICON_PATH IMAGE_DIR"/Q02_Notification_download_complete.png"
#define DP_NOTIFICATION_ONGOING_ICON_PATH IMAGE_DIR"/Notification_download_animation.gif"
#define DP_NOTIFICATION_DOWNLOADING_ICON_PATH "reserved://indicator/ani/downloading"
#define DP_NOTIFICATION_FAILED_ICON_PATH IMAGE_DIR"/Q02_Notification_download_failed.png"

static const char *__noti_error_str(
		notification_error_e err)
{
	switch (err) {
	case NOTIFICATION_ERROR_INVALID_DATA:
		return "NOTIFICATION_ERROR_INVALID_DATA";
	case NOTIFICATION_ERROR_NO_MEMORY:
		return "NOTIFICATION_ERROR_NO_MEMORY";
	case NOTIFICATION_ERROR_FROM_DB:
		return "NOTIFICATION_ERROR_FROM_DB";
	case NOTIFICATION_ERROR_ALREADY_EXIST_ID:
		return "NOTIFICATION_ERROR_ALREADY_EXIST_ID";
	case NOTIFICATION_ERROR_FROM_DBUS:
		return "NOTIFICATION_ERROR_FROM_DBUS";
	case NOTIFICATION_ERROR_NOT_EXIST_ID:
		return "NOTIFICATION_ERROR_NOT_EXIST_ID";
	default:
		break;
	}
	return "Unknown error";
}

static char *__get_string_sender(char *url)
{
	char *temp = NULL;
	char *found = NULL;
	char *found1 = NULL;
	char *sender = NULL;
	char *credential_sender = NULL;

	if (url == NULL)
		return NULL;

	found = strstr(url, "://");
	if (found) {
		temp = found + 3;
	} else {
		temp = url;
	}
	found = strchr(temp, '/');
	if (found) {
		int len = 0;
		len = found - temp;
		sender = calloc(len + 1, sizeof(char));
		if (sender == NULL)
			return NULL;
		snprintf(sender, len + 1, "%s", temp);
	} else {
		sender = dp_strdup(temp);
	}

	// For credential URL
	found = strchr(sender, '@');
	found1 = strchr(sender, ':');
	if (found && found1 && found1 < found) {
		int len = 0;
		found = found + 1;
		len = strlen(found);
		credential_sender = calloc(len + 1, sizeof(char));
		if (credential_sender == NULL) {
			free(sender);
			return NULL;
		}
		snprintf(credential_sender, len + 1, "%s", found);
		free(sender);
		return credential_sender;
	} else {
		return sender;
	}
}

static char *__get_string_size(long long file_size)
{
	const char *unitStr[4] = {"B", "KB", "MB", "GB"};
	double doubleTypeBytes = 0.0;
	int unit = 0;
	long long bytes = file_size;
	long long unitBytes = bytes;
	char *temp = NULL;

	/* using bit operation to avoid floating point arithmetic */
	for (unit = 0; (unitBytes > 1024 && unit < 4); unit++) {
		unitBytes = unitBytes >> 10;
	}
	unitBytes = 1 << (10 * unit);

	if (unit > 3)
		unit = 3;

	char str[64] = {0};
	if (unit == 0) {
		snprintf(str, sizeof(str), "%lld %s", bytes, unitStr[unit]);
	} else {
		doubleTypeBytes = ((double)bytes / (double)unitBytes);
		snprintf(str, sizeof(str), "%.2f %s", doubleTypeBytes, unitStr[unit]);
	}

	str[63] = '\0';
	temp = dp_strdup(str);
	return temp;
}

static char *__get_string_status(dp_state_type state)
{
	char *message = NULL;
	switch (state) {
	case DP_STATE_COMPLETED:
		message = __("IDS_DM_HEADER_DOWNLOAD_COMPLETE");
		break;
	case DP_STATE_CANCELED:
	case DP_STATE_FAILED:
		message = S_("IDS_COM_POP_DOWNLOAD_FAILED");
		break;
	default:
		break;
	}
	return message;
}

int dp_set_downloadinginfo_notification(int id, char *packagename)
{
	notification_h noti_handle = NULL;
	notification_error_e err = NOTIFICATION_ERROR_NONE;
	int privId = 0;
	bundle *b = NULL;
	void *blob_data = NULL;
	bundle_raw* b_raw = NULL;
	int length;

	noti_handle = notification_create(NOTIFICATION_TYPE_ONGOING);
	if (!noti_handle) {
		TRACE_ERROR("[FAIL] create notification handle");
		return -1;
	}

	char *title = dp_db_get_text_column(id,
		DP_DB_TABLE_NOTIFICATION, DP_DB_COL_TITLE);
	if(title != NULL) {
		err = notification_set_text(noti_handle, NOTIFICATION_TEXT_TYPE_TITLE,
			title, NULL, NOTIFICATION_VARIABLE_TYPE_NONE);
		free(title);
		if (err != NOTIFICATION_ERROR_NONE) {
			TRACE_ERROR("[FAIL] set title [%s]", __noti_error_str(err));
			notification_free(noti_handle);
			return -1;
		}
	} else {
		char *content_name =
			dp_db_get_text_column
				(id, DP_DB_TABLE_DOWNLOAD_INFO, DP_DB_COL_CONTENT_NAME);

		if (content_name == NULL)
			content_name = strdup("No Name");

		err = notification_set_text(noti_handle,
				NOTIFICATION_TEXT_TYPE_TITLE, content_name, NULL,
				NOTIFICATION_VARIABLE_TYPE_NONE);
		if (content_name)
			free(content_name);
	}

	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set title [%s]", __noti_error_str(err));
		notification_free(noti_handle);
		return -1;
	}

	err = notification_set_image(noti_handle,
			NOTIFICATION_IMAGE_TYPE_ICON, DP_NOTIFICATION_ONGOING_ICON_PATH);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set icon [%s]", __noti_error_str(err));
		notification_free(noti_handle);
		return -1;
	}

	blob_data = dp_db_get_blob_column
				(id, DP_DB_TABLE_NOTIFICATION, DP_DB_COL_RAW_BUNDLE_ONGOING, &length);
	if (blob_data != NULL) {
		b_raw = (bundle_raw*)blob_data;
		if (b_raw != NULL) {
			b = bundle_decode_raw(b_raw, length);
			if (b != NULL) {
				err = notification_set_execute_option(noti_handle,
					NOTIFICATION_EXECUTE_TYPE_SINGLE_LAUNCH, "View", NULL, b);

				if (err != NOTIFICATION_ERROR_NONE) {
					TRACE_ERROR("[FAIL] set execute option [%s]", __noti_error_str(err));
					bundle_free_encoded_rawdata(&b_raw);
					bundle_free(b);
					notification_free(noti_handle);
					return -1;
				}
				bundle_free_encoded_rawdata(&b_raw);
				bundle_free(b);
			}
		}
	} else {
		b = bundle_create();
		if (!b) {
			TRACE_ERROR("[FAIL] create bundle");
			notification_free(noti_handle);
			return -1;
		}

		if (packagename &&
			appsvc_set_pkgname(b, packagename) != APPSVC_RET_OK) {
			TRACE_ERROR("[FAIL] set pkg name");
			bundle_free(b);
			notification_free(noti_handle);
			return -1;
		}
		err = notification_set_execute_option(noti_handle,
			NOTIFICATION_EXECUTE_TYPE_SINGLE_LAUNCH, "View", NULL, b);

		if (err != NOTIFICATION_ERROR_NONE) {
			TRACE_ERROR("[FAIL] set execute option [%s]", __noti_error_str(err));
			bundle_free(b);
			notification_free(noti_handle);
			return -1;
		}
		bundle_free(b);
	}
	err = notification_set_property(noti_handle,
			NOTIFICATION_PROP_DISABLE_TICKERNOTI);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set property [%s]", __noti_error_str(err));
		notification_free(noti_handle);
		return -1;
	}

	err = notification_set_image(noti_handle,
		NOTIFICATION_IMAGE_TYPE_ICON_FOR_INDICATOR, DP_NOTIFICATION_DOWNLOADING_ICON_PATH);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set icon indicator [%s]", __noti_error_str(err));
		notification_free(noti_handle);
		return -1;
	}

	err = notification_set_display_applist(noti_handle, NOTIFICATION_DISPLAY_APP_ALL);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set disable icon [%s]", __noti_error_str(err));
		notification_free(noti_handle);
		return -1;
	}

	err = notification_insert(noti_handle, &privId);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set insert [%s]", __noti_error_str(err));
		notification_free(noti_handle);
		return -1;
	}

	TRACE_DEBUG("m_noti_id [%d]", privId);
	notification_free(noti_handle);
	return privId;
}

int dp_set_downloadedinfo_notification(int priv_id, int id, char *packagename, dp_state_type state)
{
	notification_h noti_handle = NULL;
	notification_error_e err = NOTIFICATION_ERROR_NONE;
	int privId = 0;
	bundle *b = NULL;
	void *blob_data = NULL;
	bundle_raw* b_raw = NULL;
	int length = 0;

	if (priv_id >= 0) {
		err = notification_delete_by_priv_id(NULL, NOTIFICATION_TYPE_ONGOING,
				priv_id);
		if (err != NOTIFICATION_ERROR_NONE) {
			TRACE_ERROR("[FAIL] delete notification handle [%s]",
					__noti_error_str(err));
		}
	}

	noti_handle = notification_create(NOTIFICATION_TYPE_NOTI);

	if (!noti_handle) {
		TRACE_ERROR("[FAIL] create notification handle");
		return -1;
	}

	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set title [%s]", __noti_error_str(err));
		notification_free(noti_handle);
		return -1;
	}

	err = notification_set_text(noti_handle, NOTIFICATION_TEXT_TYPE_TITLE,
			__get_string_status(state), NULL, NOTIFICATION_VARIABLE_TYPE_NONE);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set title [%s]", __noti_error_str(err));
		notification_free(noti_handle);
		return -1;
	}

	char *description =
		dp_db_get_text_column
			(id, DP_DB_TABLE_NOTIFICATION, DP_DB_COL_DESCRIPTION);
	if(description != NULL) {
		err = notification_set_text(noti_handle,
				NOTIFICATION_TEXT_TYPE_INFO_2, description,
				NULL, NOTIFICATION_VARIABLE_TYPE_NONE);
		free(description);
		if (err != NOTIFICATION_ERROR_NONE) {
			TRACE_ERROR("[FAIL] set description [%s]", __noti_error_str(err));
			notification_free(noti_handle);
			return -1;
		}
	} else {
		char *url = NULL;
		url = dp_db_get_text_column
					(id, DP_DB_TABLE_REQUEST_INFO, DP_DB_COL_URL);
		if (url) {
			char *sender_str = __get_string_sender(url);
			err = notification_set_text(noti_handle, NOTIFICATION_TEXT_TYPE_INFO_2,
					sender_str, NULL, NOTIFICATION_VARIABLE_TYPE_NONE);
			if (err != NOTIFICATION_ERROR_NONE) {
				TRACE_ERROR("[FAIL] set title [%s]", __noti_error_str(err));
				free(sender_str);
				free(url);
				notification_free(noti_handle);
				return -1;
			}
			free(sender_str);
			free(url);
		}
	}

	char *title = dp_db_get_text_column(id,
		DP_DB_TABLE_NOTIFICATION, DP_DB_COL_TITLE);
	if(title != NULL) {
		err = notification_set_text(noti_handle, NOTIFICATION_TEXT_TYPE_CONTENT,
			title, NULL, NOTIFICATION_VARIABLE_TYPE_NONE);
		free(title);
		if (err != NOTIFICATION_ERROR_NONE) {
			TRACE_ERROR("[FAIL] set title [%s]", __noti_error_str(err));
			notification_free(noti_handle);
			return -1;
		}
	} else {
		char *content_name =
				dp_db_get_text_column
					(id, DP_DB_TABLE_DOWNLOAD_INFO, DP_DB_COL_CONTENT_NAME);
		if (content_name == NULL)
			content_name = strdup("No Name");

		err = notification_set_text(noti_handle,
				NOTIFICATION_TEXT_TYPE_CONTENT, content_name,
				NULL, NOTIFICATION_VARIABLE_TYPE_NONE);
		free(content_name);
	}

	time_t tt = time(NULL);
	err = notification_set_time_to_text(noti_handle,
			NOTIFICATION_TEXT_TYPE_INFO_SUB_1, tt);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set time [%s]", __noti_error_str(err));
		notification_free(noti_handle);
		return -1;
	}
	if (state == DP_STATE_COMPLETED) {
		blob_data = dp_db_get_blob_column
					(id, DP_DB_TABLE_NOTIFICATION, DP_DB_COL_RAW_BUNDLE_COMPLETE, &length);
		if (blob_data != NULL) {
			b_raw = (bundle_raw*)blob_data;
			if (b_raw != NULL) {
				b = bundle_decode_raw(b_raw, length);
				if (b != NULL) {
					err = notification_set_execute_option(noti_handle,
						NOTIFICATION_EXECUTE_TYPE_SINGLE_LAUNCH, "View", NULL, b);

					if (err != NOTIFICATION_ERROR_NONE) {
						TRACE_ERROR("[FAIL] set execute option [%s]", __noti_error_str(err));
						bundle_free_encoded_rawdata(&b_raw);
						bundle_free(b);
						notification_free(noti_handle);
						return -1;
					}
					bundle_free_encoded_rawdata(&b_raw);
				}
			}
		} else {
			char *file_path = NULL;
			b = bundle_create();
			if (!b) {
				TRACE_ERROR("[FAIL] create bundle");
				notification_free(noti_handle);
				return -1;
			}
			if (appsvc_set_operation(b, APPSVC_OPERATION_VIEW) != APPSVC_RET_OK) {
				TRACE_ERROR("[FAIL] appsvc set operation");
				bundle_free(b);
				notification_free(noti_handle);
				return -1;
			}
			file_path = dp_db_get_text_column
					(id, DP_DB_TABLE_DOWNLOAD_INFO, DP_DB_COL_SAVED_PATH);
			if (file_path == NULL) {
				TRACE_ERROR("[FAIL] get file path");
				bundle_free(b);
				notification_free(noti_handle);
				return -1;

			}
			if (appsvc_set_uri(b, file_path) != APPSVC_RET_OK) {
				TRACE_ERROR("[FAIL] appsvc set uri");
				bundle_free(b);
				notification_free(noti_handle);
				free(file_path);
				return -1;
			}
			free(file_path);
			err = notification_set_execute_option(noti_handle,
				NOTIFICATION_EXECUTE_TYPE_SINGLE_LAUNCH, "View", NULL, b);
			if (err != NOTIFICATION_ERROR_NONE) {
				TRACE_ERROR("[FAIL] set execute option[%s]", __noti_error_str(err));
				bundle_free(b);
				notification_free(noti_handle);
				return -1;
			}
		}
		long long file_size = dp_db_get_int64_column(id,
				DP_DB_TABLE_DOWNLOAD_INFO, DP_DB_COL_CONTENT_SIZE);
		char *size_str = __get_string_size(file_size);

		err = notification_set_text(noti_handle,	NOTIFICATION_TEXT_TYPE_INFO_1,
				size_str, NULL, NOTIFICATION_VARIABLE_TYPE_NONE);

		if (err != NOTIFICATION_ERROR_NONE) {
			TRACE_ERROR("[FAIL] set title [%s]", __noti_error_str(err));
			free(size_str);
			bundle_free(b);
			notification_free(noti_handle);
			return -1;
		}
		free(size_str);

		err = notification_set_image(noti_handle, NOTIFICATION_IMAGE_TYPE_ICON,
				DP_NOTIFICATION_ICON_PATH);
		if (err != NOTIFICATION_ERROR_NONE) {
			TRACE_ERROR("[FAIL] set icon [%s]", __noti_error_str(err));
			notification_free(noti_handle);
			return -1;
		}
	} else if (state == DP_STATE_CANCELED || state == DP_STATE_FAILED) {
		blob_data = dp_db_get_blob_column
					(id, DP_DB_TABLE_NOTIFICATION, DP_DB_COL_RAW_BUNDLE_FAIL, &length);
		if (blob_data != NULL) {
			b_raw = (bundle_raw *)blob_data;
			if (b_raw != NULL) {
				b = bundle_decode_raw(b_raw, length);
				if (b != NULL) {
					err = notification_set_execute_option(noti_handle,
						NOTIFICATION_EXECUTE_TYPE_SINGLE_LAUNCH, "View", NULL, b);

					if (err != NOTIFICATION_ERROR_NONE) {
						TRACE_ERROR("[FAIL] set execute option [%s]", __noti_error_str(err));
						bundle_free_encoded_rawdata(&b_raw);
						bundle_free(b);
						notification_free(noti_handle);
						return -1;
					}
					bundle_free_encoded_rawdata(&b_raw);
				}
			}
		} else {
			b = bundle_create();
			if (!b) {
				TRACE_ERROR("[FAIL] create bundle");
				notification_free(noti_handle);
				return -1;
			}
			if (packagename &&
				appsvc_set_pkgname(b, packagename) !=
				APPSVC_RET_OK) {
				TRACE_ERROR("[FAIL] set pkg name");
				bundle_free(b);
				notification_free(noti_handle);
				return -1;
			}
			err = notification_set_execute_option(noti_handle,
					NOTIFICATION_EXECUTE_TYPE_SINGLE_LAUNCH, "View", NULL, b);
			if (err != NOTIFICATION_ERROR_NONE) {
				TRACE_ERROR("[FAIL] set execute option[%s]", __noti_error_str(err));
				bundle_free(b);
				notification_free(noti_handle);
				return -1;
			}
		}
		err = notification_set_image(noti_handle, NOTIFICATION_IMAGE_TYPE_ICON,
				DP_NOTIFICATION_FAILED_ICON_PATH);
		if (err != NOTIFICATION_ERROR_NONE) {
			TRACE_ERROR("[FAIL] set icon [%s]", __noti_error_str(err));
			bundle_free(b);
			notification_free(noti_handle);
			return -1;
		}
	} else {
		TRACE_ERROR("[CRITICAL] invalid state");
		bundle_free(b);
		notification_free(noti_handle);
		return -1;
	}

	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set time [%s]", __noti_error_str(err));
		notification_free(noti_handle);
		bundle_free(b);
		return -1;
	}

	bundle_free(b);

	err = notification_set_property(noti_handle,
			NOTIFICATION_PROP_DISABLE_TICKERNOTI);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set property [%s]", __noti_error_str(err));
		notification_free(noti_handle);
		return -1;
	}

	err = notification_set_display_applist(noti_handle,
			NOTIFICATION_DISPLAY_APP_ALL ^ NOTIFICATION_DISPLAY_APP_INDICATOR);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set disable icon [%s]", __noti_error_str(err));
		notification_free(noti_handle);
		return -1;
	}

	err = notification_insert(noti_handle, &privId);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set insert [%s]", __noti_error_str(err));
		notification_free(noti_handle);
		return -1;
	}

	TRACE_DEBUG("m_noti_id [%d]", privId);
	notification_free(noti_handle);
	return privId;
}

void dp_update_downloadinginfo_notification(int priv_id, double received_size, double file_size)
{
	notification_error_e err = NOTIFICATION_ERROR_NONE;
	if (priv_id < 0) {
		TRACE_ERROR("[FAIL] Invalid priv_id[%d]", priv_id);
		return;
	}

	if (file_size > 0) {
		double progress;
		progress = received_size / file_size;
		err = notification_update_progress(NULL, priv_id, progress);
		if (err != NOTIFICATION_ERROR_NONE)
			TRACE_ERROR("[FAIL] update noti progress[%s]",
					__noti_error_str(err));
	} else {
		err = notification_update_size(NULL, priv_id, received_size);
		if (err != NOTIFICATION_ERROR_NONE)
			TRACE_ERROR("[FAIL]  update noti progress[%s]",
					__noti_error_str(err));
	}
}

void dp_clear_downloadinginfo_notification()
{
	notification_error_e err = NOTIFICATION_ERROR_NONE;
	err = notification_delete_all_by_type(NULL, NOTIFICATION_TYPE_ONGOING);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] clear noti [%s]", __noti_error_str(err));
	}
	return;
}
