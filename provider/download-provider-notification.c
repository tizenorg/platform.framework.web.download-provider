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
static void __free_char_pointer_array(char **array, int count)
{
	int i = 0;
	if (count < 0)
		count = 0;
	for (i = 0; i < count; i++) {
		if (array[i])
			free(array[i]);
	}
	free(array);
}

static int __set_extra_data(bundle *b, int id)
{
	db_conds_list_fmt conds_p;
	int conds_count = 0;
	int count = 0;
	// get count of key list.
	conds_count = 1;
	memset(&conds_p, 0x00, sizeof(db_conds_list_fmt));
	conds_p.column = DP_DB_COL_ID;
	conds_p.type = DP_DB_COL_TYPE_INT;
	conds_p.value = &id;
	count = dp_db_get_conds_rows_count(DP_DB_TABLE_NOTIFICATION,
			DP_DB_COL_DISTINCT_EXTRA_KEY, "AND", 1, &conds_p);
	if (count > 0) {
		char **keys_array = NULL;
		int i = 0;
		keys_array = (char **)calloc(count, sizeof(char *));
		if (keys_array == NULL) {
			TRACE_ERROR("[FAIL] calloc failed");
			return -1;
		}
		// get key list
		int keys_count =
			dp_db_get_conds_list(DP_DB_TABLE_NOTIFICATION,
					DP_DB_COL_DISTINCT_EXTRA_KEY , DP_DB_COL_TYPE_TEXT,
					(void **)keys_array, count, -1, NULL, NULL,
					"AND", 1, &conds_p);
		if (keys_count <= 0) {
			TRACE_ERROR("[FAIL] Wrong db data");
			__free_char_pointer_array(keys_array, 0);
			return -1;
		}
		for (i = 0; i < keys_count; i++) {
			db_conds_list_fmt conds_p2[2];
			int conds_count2 = 2;
			char **values_array = NULL;
			int check_rows = 0;
			int values_count = 0;

			memset(conds_p2, 0x00, conds_count2 * sizeof(db_conds_list_fmt));
			conds_p2[0].column = DP_DB_COL_ID;
			conds_p2[0].type = DP_DB_COL_TYPE_INT;
			conds_p2[0].value = &id;
			conds_p2[1].column = DP_DB_COL_EXTRA_KEY;
			conds_p2[1].type = DP_DB_COL_TYPE_TEXT;
			conds_p2[1].value = (void *)(keys_array[i]);
			// get value list
			check_rows = dp_db_get_conds_rows_count(
					DP_DB_TABLE_NOTIFICATION, DP_DB_COL_EXTRA_VALUE, "AND",
					conds_count2, conds_p2);
			if (check_rows <= 0) {
				TRACE_ERROR("[FAIL] No values about key");
				__free_char_pointer_array(keys_array, keys_count);
				return -1;
			}
			values_array = (char **)calloc(check_rows, sizeof(char *));
			if (values_array == NULL) {
				TRACE_ERROR("[FAIL] calloc failed");
				__free_char_pointer_array(keys_array, keys_count);
				return -1;
			}
			values_count =
				dp_db_get_conds_list(DP_DB_TABLE_NOTIFICATION,
					DP_DB_COL_EXTRA_VALUE, DP_DB_COL_TYPE_TEXT,
					(void **)values_array, check_rows, -1, NULL, NULL,
					"AND", conds_count2, conds_p2);
			if (values_count <= 0) {
				TRACE_ERROR("[FAIL] No values about key");
				__free_char_pointer_array(keys_array, keys_count);
				__free_char_pointer_array(values_array, 0);
				return -1;
			}
			if (values_count == 1) {
				char *key = keys_array[i];
				char *value = values_array[values_count-1];
				if (appsvc_add_data(b, key, value) !=
					APPSVC_RET_OK) {
					TRACE_ERROR("[FAIL] set add data");
					__free_char_pointer_array(keys_array, keys_count);
					__free_char_pointer_array(values_array, values_count);
					return -1;
				}
			} else {
				char *key = keys_array[i];
				if (appsvc_add_data_array(b, key, (const char **)values_array,
						values_count) !=	APPSVC_RET_OK) {
					TRACE_ERROR("[FAIL] set add data");
					__free_char_pointer_array(keys_array, keys_count);
					__free_char_pointer_array(values_array, values_count);
					return -1;
				}
			}
			__free_char_pointer_array(values_array, values_count);
		}
		__free_char_pointer_array(keys_array, keys_count);
	}
	return 0;
}

int dp_set_downloadinginfo_notification(int id, char *packagename)
{
	notification_h noti_handle = NULL;
	notification_error_e err = NOTIFICATION_ERROR_NONE;
	int privId = 0;
	bundle *b = NULL;

	noti_handle = notification_create(NOTIFICATION_TYPE_ONGOING);
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
		TRACE_ERROR("[FAIL] set title [%s]", __noti_error_str(err));
		notification_free(noti_handle);
		return -1;
	}

	err = notification_set_image(noti_handle,
			NOTIFICATION_IMAGE_TYPE_ICON, DP_NOTIFICATION_ICON_PATH);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set icon [%s]", __noti_error_str(err));
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

	if (__set_extra_data(b, id) < 0) {
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

	err = notification_set_property(noti_handle,
			NOTIFICATION_PROP_DISABLE_TICKERNOTI);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set property [%s]", __noti_error_str(err));
		notification_free(noti_handle);
		return -1;
	}

	err = notification_insert(noti_handle, &privId);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set insert [%s]", __noti_error_str(err));
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
			TRACE_ERROR("[FAIL] delete notification handle [%s]",
					__noti_error_str(err));
		}
	}

	noti_handle = notification_create(NOTIFICATION_TYPE_NOTI);

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
		TRACE_ERROR("[FAIL] set title [%s]", __noti_error_str(err));
		notification_free(noti_handle);
		return -1;
	}

	err = notification_set_text(noti_handle,
			NOTIFICATION_TEXT_TYPE_CONTENT,
			__get_string_status(state), NULL,
			NOTIFICATION_VARIABLE_TYPE_NONE);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set text [%s]", __noti_error_str(err));
		notification_free(noti_handle);
		return -1;
	}
	time_t tt = time(NULL);

	err = notification_set_time(noti_handle, tt);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set time [%s]", __noti_error_str(err));
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
		if (err != NOTIFICATION_ERROR_NONE) {
			TRACE_ERROR("[FAIL] set execute option[%s]", __noti_error_str(err));
			bundle_free(b);
			notification_free(noti_handle);
			return -1;
		}

	} else if (state == DP_STATE_CANCELED || state == DP_STATE_FAILED) {
		if (__set_extra_data(b, id) < 0) {
			bundle_free(b);
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

	err = notification_set_image(noti_handle, NOTIFICATION_IMAGE_TYPE_ICON,
			DP_NOTIFICATION_ICON_PATH);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set icon [%s]", __noti_error_str(err));
		notification_free(noti_handle);
		return -1;
	}

	err = notification_set_property(noti_handle,
			NOTIFICATION_PROP_DISABLE_TICKERNOTI);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set property [%s]", __noti_error_str(err));
		notification_free(noti_handle);
		return -1;
	}

	err = notification_insert(noti_handle, &privId);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] set insert [%s]", __noti_error_str(err));
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
