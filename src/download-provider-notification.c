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

#include "bundle.h"
#include "notification.h"
#include "appsvc.h"

#include "download-provider-notification.h"
#include "download-provider-request.h"
#include "download-provider-db.h"
#include "download-provider-log.h"

#include <libintl.h>
#define S_(s) dgettext("sys_string", s)

#define DP_NOTIFICATION_ICON_PATH IMAGE_DIR"/Q02_Notification_Download_failed.png"
/* This should be same value of SERVICE_OPERATION_DOWNLOAD_NOTIFICATION from download.h */
#define DP_DOWNLOAD_NOTI_OPERATION "http://tizen.org/appcontrol/operation/download_notification"

static void __print_app_error_message(int ret)
{
	switch (ret) {
	case APPSVC_RET_OK:
		TRACE_INFO("APPSVC_RET_OK");
		break;
	case APPSVC_RET_ELAUNCH:
		TRACE_ERROR("APPSVC_RET_ELAUNCH");
		break;
	case APPSVC_RET_ENOMATCH:
		TRACE_ERROR("APPSVC_RET_ENOMATCH");
		break;
	case APPSVC_RET_EINVAL:
		TRACE_ERROR("APPSVC_RET_EINVAL");
		break;
	case APPSVC_RET_ERROR:
		TRACE_ERROR("APPSVC_RET_ERROR");
		break;
	}
}

static void __print_notification_error_message(int ret)
{
	switch (ret) {
	case NOTIFICATION_ERROR_INVALID_DATA:
		TRACE_ERROR("NOTIFICATION_ERROR_INVALID_DATA");
		break;
	case NOTIFICATION_ERROR_NO_MEMORY:
		TRACE_ERROR("NOTIFICATION_ERROR_NO_MEMORY");
		break;
	case NOTIFICATION_ERROR_FROM_DB:
		TRACE_ERROR("NOTIFICATION_ERROR_FROM_DB");
		break;
	case NOTIFICATION_ERROR_ALREADY_EXIST_ID:
		TRACE_ERROR("NOTIFICATION_ERROR_ALREADY_EXIST_ID");
		break;
	case NOTIFICATION_ERROR_FROM_DBUS:
		TRACE_ERROR("NOTIFICATION_ERROR_FROM_DBUS");
		break;
	case NOTIFICATION_ERROR_NOT_EXIST_ID:
		TRACE_ERROR("NOTIFICATION_ERROR_NOT_EXIST_ID");
		break;
	default:
		TRACE_ERROR("Unknown error");
		break;
	}
}

static char *__get_string_status(dp_state_type state)
{
	char *message = NULL;
	switch (state) {
	case DP_STATE_COMPLETED:
		//message = S_("IDS_COM_POP_SUCCESS");
		message = "Completed";
		break;
	case DP_STATE_CANCELED:
		//message = S_("IDS_COM_POP_CANCELLED");
		message = "Canceled";
		break;
	case DP_STATE_FAILED:
		//message = S_("IDS_COM_POP_FAILED");
		message = "Failed";
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

#ifdef NOTI_NEW_VERSION_API
	noti_handle = notification_create(NOTIFICATION_TYPE_ONGOING);
#else
	noti_handle = notification_new(NOTIFICATION_TYPE_ONGOING,
		NOTIFICATION_GROUP_ID_NONE, NOTIFICATION_PRIV_ID_NONE);
#endif

	if (!noti_handle) {
		TRACE_ERROR("[FAIL] create notification handle");
		return -1;
	}

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

	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set title [%d]", err);
		notification_free(noti_handle);
		return -1;
	}

	err = notification_set_image(noti_handle,
			NOTIFICATION_IMAGE_TYPE_ICON, DP_NOTIFICATION_ICON_PATH);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set icon [%d]", err);
		notification_free(noti_handle);
		return -1;
	}

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

	if (appsvc_set_operation(b, DP_DOWNLOAD_NOTI_OPERATION) !=
		APPSVC_RET_OK) {
		TRACE_ERROR("[FAIL] set noti operation");
		bundle_free(b);
		notification_free(noti_handle);
		return -1;
	}

	char *extra_key =
		dp_db_get_text_column(id, DP_DB_TABLE_NOTIFICATION,
			DP_DB_COL_EXTRA_KEY);
	char *extra_value =
		dp_db_get_text_column(id, DP_DB_TABLE_NOTIFICATION,
			DP_DB_COL_EXTRA_VALUE);

	if (extra_key && extra_value) {
		if (appsvc_add_data(b, extra_key, extra_value) != APPSVC_RET_OK) {
			TRACE_ERROR("[FAIL] set add data");
			free(extra_key);
			free(extra_value);
			bundle_free(b);
			notification_free(noti_handle);
			return -1;
		}
	}
	if (extra_key)
		free(extra_key);
	if (extra_value)
		free(extra_value);

	err = notification_set_execute_option(noti_handle,
			NOTIFICATION_EXECUTE_TYPE_SINGLE_LAUNCH, "View", NULL, b);

	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set execute option [%d]", err);
		bundle_free(b);
		notification_free(noti_handle);
		return -1;
	}
	bundle_free(b);

	err = notification_set_property(noti_handle,
			NOTIFICATION_PROP_DISABLE_TICKERNOTI);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set property [%d]", err);
		notification_free(noti_handle);
		return -1;
	}

	err = notification_insert(noti_handle, &privId);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set insert [%d]", err);
		notification_free(noti_handle);
		return -1;
	}

	TRACE_INFO("m_noti_id [%d]", privId);
	notification_free(noti_handle);
	return privId;
}

int dp_set_downloadedinfo_notification(int priv_id, int id, char *packagename, dp_state_type state)
{
	notification_h noti_handle = NULL;
	notification_error_e err = NOTIFICATION_ERROR_NONE;
	int privId = 0;
	bundle *b = NULL;

	if (priv_id >= 0) {
		err = notification_delete_by_priv_id(NULL, NOTIFICATION_TYPE_ONGOING,
				priv_id);
		if (err != NOTIFICATION_ERROR_NONE) {
			TRACE_ERROR("[FAIL] delete notification handle, err", err);
		}
	}

#ifdef NOTI_NEW_VERSION_API
	noti_handle = notification_create(NOTIFICATION_TYPE_NOTI);
#else
	noti_handle = notification_new(NOTIFICATION_TYPE_NOTI,
		NOTIFICATION_GROUP_ID_NONE, NOTIFICATION_PRIV_ID_NONE);
#endif

	if (!noti_handle) {
		TRACE_ERROR("[FAIL] create notification handle");
		return -1;
	}

	char *content_name =
		dp_db_get_text_column
			(id, DP_DB_TABLE_DOWNLOAD_INFO, DP_DB_COL_CONTENT_NAME);

	if (content_name == NULL)
		content_name = strdup("No Name");

	err = notification_set_text(noti_handle,
			NOTIFICATION_TEXT_TYPE_TITLE, content_name,
			NULL, NOTIFICATION_VARIABLE_TYPE_NONE);
	if (content_name)
		free(content_name);

	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set title [%d]", err);
		notification_free(noti_handle);
		return -1;
	}

	err = notification_set_text(noti_handle,
			NOTIFICATION_TEXT_TYPE_CONTENT,
			__get_string_status(state), NULL,
			NOTIFICATION_VARIABLE_TYPE_NONE);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set text [%d]", err);
		notification_free(noti_handle);
		return -1;
	}
	time_t tt = time(NULL);

	err = notification_set_time(noti_handle, tt);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set time [%d]", err);
		notification_free(noti_handle);
		return -1;
	}

	b = bundle_create();
	if (!b) {
		TRACE_ERROR("[FAIL] create bundle");
		notification_free(noti_handle);
		return -1;
	}

	if (state == DP_STATE_COMPLETED) {
		if (appsvc_set_operation(b, APPSVC_OPERATION_VIEW) != APPSVC_RET_OK) {
			TRACE_ERROR("[FAIL] appsvc set operation");
			bundle_free(b);
			notification_free(noti_handle);
			return -1;
		}
		
		char *savedpath =
			dp_db_get_text_column
				(id, DP_DB_TABLE_DOWNLOAD_INFO, DP_DB_COL_SAVED_PATH);
		if (savedpath && appsvc_set_uri(b, savedpath) !=
				APPSVC_RET_OK) {
			TRACE_ERROR("[FAIL] appsvc set uri");
			free(savedpath);
			bundle_free(b);
			notification_free(noti_handle);
			return -1;
		}
		if (savedpath)
			free(savedpath);
		err = notification_set_execute_option(noti_handle,
				NOTIFICATION_EXECUTE_TYPE_SINGLE_LAUNCH, "View", NULL, b);
	} else if (state == DP_STATE_CANCELED || state == DP_STATE_FAILED) {
		if (appsvc_set_operation(b, DP_DOWNLOAD_NOTI_OPERATION) !=
			APPSVC_RET_OK) {
			TRACE_ERROR("[FAIL] set noti operation [%d]", err);
			bundle_free(b);
			notification_free(noti_handle);
			return -1;
		}

		char *extra_key =
			dp_db_get_text_column(id, DP_DB_TABLE_NOTIFICATION,
				DP_DB_COL_EXTRA_KEY);
		char *extra_value =
			dp_db_get_text_column(id, DP_DB_TABLE_NOTIFICATION,
				DP_DB_COL_EXTRA_VALUE);
		if (extra_key && extra_value) {
			if (appsvc_add_data(b, extra_key, extra_value) !=
				APPSVC_RET_OK) {
				TRACE_ERROR("[FAIL] set add data");
				free(extra_key);
				free(extra_value);
				bundle_free(b);
				notification_free(noti_handle);
				return -1;
			}
		}
		if (extra_key)
			free(extra_key);
		if (extra_value)
			free(extra_value);

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
	} else {
		TRACE_ERROR("[CRITICAL] invalid state");
		bundle_free(b);
		notification_free(noti_handle);
		return -1;
	}

	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set time [%d]", err);
		notification_free(noti_handle);
		bundle_free(b);
		return -1;
	}

	bundle_free(b);

	err = notification_set_image(noti_handle, NOTIFICATION_IMAGE_TYPE_ICON,
			DP_NOTIFICATION_ICON_PATH);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set icon [%d]", err);
		notification_free(noti_handle);
		return -1;
	}

	err = notification_set_property(noti_handle,
			NOTIFICATION_PROP_DISABLE_TICKERNOTI);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set property [%d]", err);
		notification_free(noti_handle);
		return -1;
	}

	err = notification_insert(noti_handle, &privId);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set insert [%d]", err);
		notification_free(noti_handle);
		return -1;
	}

	TRACE_INFO("m_noti_id [%d]", privId);
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
			TRACE_ERROR("[FAIL] update noti progress[%d]", err);
	} else {
		err = notification_update_size(NULL, priv_id, received_size);
		if (err != NOTIFICATION_ERROR_NONE)
			TRACE_ERROR("[FAIL]  update noti progress[%d]", err);
	}
}

void dp_clear_downloadinginfo_notification()
{
	notification_error_e err = NOTIFICATION_ERROR_NONE;
	err = notification_delete_all_by_type(NULL, NOTIFICATION_TYPE_ONGOING);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] clear noti [%d]", err);
	}
	return;
}
