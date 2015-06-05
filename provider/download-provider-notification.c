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

#ifdef SUPPORT_NOTIFICATION
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>

#include "download-provider.h"
#include "download-provider-notification.h"
#include "download-provider-db-defs.h"
#include "download-provider-db.h"
#include "download-provider-log.h"
#include "download-provider-client.h"
#include "download-provider-utils.h"

#include <bundle.h>
#ifdef T30
#include <bundle_internal.h>
#include <notification_internal.h>
#endif
#include <notification.h>

#include <appsvc.h>

#include <vconf.h>
#include <locale.h>
#include <libintl.h>
#define __(s) dgettext(PKG_NAME, s)

#define DP_DOMAIN PKG_NAME
#define DP_LOCALEDIR LOCALE_DIR
#define DP_NOTIFICATION_NO_SUBJECT "No Name"

#ifdef _TIZEN_2_3_UX
#define DP_NOTIFICATION_DRM_ICON_PATH IMAGE_DIR"/download_manager_icon_drm.png"
#define DP_NOTIFICATION_UNKNOWN_ICON_PATH IMAGE_DIR"/download_manager_icon_unknown.png"
#define DP_NOTIFICATION_EXCEL_ICON_PATH IMAGE_DIR"/download_manager_icon_xls.png"
#define DP_NOTIFICATION_HTML_ICON_PATH IMAGE_DIR"/download_manager_icon_html.png"
#define DP_NOTIFICATION_MUSIC_ICON_PATH IMAGE_DIR"/download_manager_icon_music.png"
#define DP_NOTIFICATION_PDF_ICON_PATH IMAGE_DIR"/download_manager_icon_pdf.png"
#define DP_NOTIFICATION_PPT_ICON_PATH IMAGE_DIR"/download_manager_icon_ppt.png"
#define DP_NOTIFICATION_TEXT_ICON_PATH IMAGE_DIR"/download_manager_icon_text.png"
#define DP_NOTIFICATION_WORD_ICON_PATH IMAGE_DIR"/download_manager_icon_word.png"
#define DP_NOTIFICATION_VIDEO_ICON_PATH IMAGE_DIR"/download_manager_icon_movie.png"
#define DP_NOTIFICATION_IMAGE_ICON_PATH IMAGE_DIR"/download_manager_icon_img.png"
#define DP_NOTIFICATION_FALSH_ICON_PATH IMAGE_DIR"/download_manager_icon_swf.png"
#define DP_NOTIFICATION_TPK_ICON_PATH IMAGE_DIR"/download_manager_icon_tpk.png"
#define DP_NOTIFICATION_VCAL_ICON_PATH IMAGE_DIR"/download_manager_icon_date.png"

// base path
#define QP_PRELOAD_NOTI_ICON_PATH "/usr/apps/com.samsung.quickpanel/shared/res/noti_icons"
// each application path
#define QP_PRELOAD_COMMON_PATH QP_PRELOAD_NOTI_ICON_PATH"/Common"
#define DP_NOTIFICATION_COMPLETED_ICON_PATH QP_PRELOAD_COMMON_PATH"/noti_download_complete.png"
#define DP_NOTIFICATION_FAILED_ICON_PATH QP_PRELOAD_COMMON_PATH"/noti_download_failed.png"

#define DP_NOTIFICATION_ONGOING_ICON_PATH "reserved://quickpanel/ani/downloading"
#define DP_NOTIFICATION_DOWNLOADING_ICON_PATH "reserved://indicator/ani/downloading"
#define DP_NOTIFICATION_FAILED_INDICATOR_ICON_PATH IMAGE_DIR"/B03_Processing_download_failed.png"
#define DP_NOTIFICATION_COMPLETED_INDICATOR_ICON_PATH IMAGE_DIR"/B03_Processing_download_complete.png"
#else
#define DP_NOTIFICATION_DRM_ICON_PATH IMAGE_DIR"/U01_icon_drm.png"
#define DP_NOTIFICATION_UNKNOWN_ICON_PATH IMAGE_DIR"/U01_icon_unkown.png"
#define DP_NOTIFICATION_EXCEL_ICON_PATH IMAGE_DIR"/U01_icon_excel.png"
#define DP_NOTIFICATION_HTML_ICON_PATH IMAGE_DIR"/U01_icon_html.png"
#define DP_NOTIFICATION_MUSIC_ICON_PATH IMAGE_DIR"/U01_list_icon_mp3.png"
#define DP_NOTIFICATION_PDF_ICON_PATH IMAGE_DIR"/U01_icon_pdf.png"
#define DP_NOTIFICATION_PPT_ICON_PATH IMAGE_DIR"/U01_icon_ppt.png"
#define DP_NOTIFICATION_TEXT_ICON_PATH IMAGE_DIR"/U01_icon_text.png"
#define DP_NOTIFICATION_WORD_ICON_PATH IMAGE_DIR"/U01_icon_word.png"
#define DP_NOTIFICATION_VIDEO_ICON_PATH IMAGE_DIR"/U01_list_icon_mp4.png"
#define DP_NOTIFICATION_IMAGE_ICON_PATH IMAGE_DIR"/U01_list_icon_image.png"
#define DP_NOTIFICATION_FALSH_ICON_PATH IMAGE_DIR"/U01_icon_swf.png"
#define DP_NOTIFICATION_TPK_ICON_PATH IMAGE_DIR"/U01_icon_tpk.png"
#define DP_NOTIFICATION_VCAL_ICON_PATH IMAGE_DIR"/U01_icon_vcs.png"

#define DP_NOTIFICATION_FAILED_ICON_PATH IMAGE_DIR"/noti_download_failed.png"
#define DP_NOTIFICATION_COMPLETED_ICON_PATH IMAGE_DIR"/noti_download_complete.png"

#define DP_NOTIFICATION_ONGOING_ICON_PATH "reserved://quickpanel/ani/downloading"
#define DP_NOTIFICATION_DOWNLOADING_ICON_PATH "reserved://indicator/ani/downloading"
#define DP_NOTIFICATION_FAILED_INDICATOR_ICON_PATH IMAGE_DIR"/B03_processing_download_fail.png"
#define DP_NOTIFICATION_COMPLETED_INDICATOR_ICON_PATH IMAGE_DIR"/B03_processing_download_complete.png"
#endif

#define DP_MAX_ICONS_TABLE_COUNT 15

char *file_icons_table[DP_MAX_ICONS_TABLE_COUNT]={
		//unknown file type
		DP_NOTIFICATION_UNKNOWN_ICON_PATH,
		//image
		DP_NOTIFICATION_IMAGE_ICON_PATH,
		//video
		DP_NOTIFICATION_VIDEO_ICON_PATH,
		// audio /music
		DP_NOTIFICATION_MUSIC_ICON_PATH,
		// PDF
		DP_NOTIFICATION_PDF_ICON_PATH,
		// word
		DP_NOTIFICATION_WORD_ICON_PATH,
		// ppt
		DP_NOTIFICATION_PPT_ICON_PATH,
		// excel
		DP_NOTIFICATION_EXCEL_ICON_PATH,
		// html
		DP_NOTIFICATION_HTML_ICON_PATH,
		// txt
		DP_NOTIFICATION_TEXT_ICON_PATH,
		// DRM
		DP_NOTIFICATION_DRM_ICON_PATH,
		DP_NOTIFICATION_DRM_ICON_PATH,
		DP_NOTIFICATION_FALSH_ICON_PATH,
		DP_NOTIFICATION_TPK_ICON_PATH,
		DP_NOTIFICATION_VCAL_ICON_PATH,
};

static const char *__dp_noti_error_str(int err)
{
	switch (err) {
	case NOTIFICATION_ERROR_INVALID_PARAMETER:
		return "NOTIFICATION_ERROR_INVALID_PARAMETER";
	case NOTIFICATION_ERROR_OUT_OF_MEMORY:
		return "NOTIFICATION_ERROR_OUT_OF_MEMORY";
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

char *__dp_noti_get_sender(char *url)
{
	char *temp = NULL;
	char *found = NULL;
	char *found1 = NULL;
	char *sender = NULL;
	char *credential_sender = NULL;

	if (url == NULL)
		return NULL;

	found = strstr(url, "://");
	if (found != NULL) {
		temp = found + 3;
	} else {
		temp = url;
	}
	found = strchr(temp, '/');
	if (found != NULL) {
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
	if (found != NULL && found1 != NULL && found1 < found) {
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

static char *__dp_noti_status(dp_state_type state)
{
	char *message = NULL;
	switch (state) {
	case DP_STATE_COMPLETED:
		message = "IDS_DM_HEADER_DOWNLOAD_COMPLETE";
		break;
	case DP_STATE_CANCELED:
	case DP_STATE_FAILED:
		message = "IDS_DM_BODY_DOWNLOAD_FAILED_M_STATUS_ABB";
		break;
	default:
		break;
	}
	return message;
}

void __lang_changed_cb(keynode_t *key, void* data)
{
	char *str = NULL;
	str = vconf_get_str(VCONFKEY_LANGSET);
	if (str != NULL) {
		setlocale(LC_ALL, str);
		bindtextdomain(PKG_NAME, LOCALE_DIR);
		textdomain(PKG_NAME);
	}
	free(str);
}

void dp_notification_set_locale()
{
	// move to notification.c
	// locale
	__lang_changed_cb(NULL, NULL);
	if (vconf_notify_key_changed(VCONFKEY_LANGSET, __lang_changed_cb, NULL) != 0)
		TRACE_ERROR("Fail to set language changed vconf callback");
}

void dp_notification_clear_locale()
{
	// move to notification.c
	if (vconf_ignore_key_changed(VCONFKEY_LANGSET, __lang_changed_cb) != 0)
		TRACE_ERROR("Fail to unset language changed vconf callback");
}

void dp_notification_clear_ongoings()
{
	int err = NOTIFICATION_ERROR_NONE;
	err = notification_delete_all_by_type(NULL, NOTIFICATION_TYPE_ONGOING);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("[FAIL] clear noti [%s]", __dp_noti_error_str(err));
	}
	return;
}

int dp_notification_ongoing_new(const char *pkgname, const char *subject, unsigned char *raws_buffer, const int raws_length)
{
	int err = NOTIFICATION_ERROR_NONE;
	notification_h noti_handle = NULL;
	noti_handle = notification_create(NOTIFICATION_TYPE_ONGOING);
	if (noti_handle == NULL) {
		TRACE_ERROR("failed to create notification handle");
		return -1;
	}

	if (subject != NULL) {
		err = notification_set_text(noti_handle,
				NOTIFICATION_TEXT_TYPE_TITLE, subject, NULL,
				NOTIFICATION_VARIABLE_TYPE_NONE);
	} else {
		err = notification_set_text(noti_handle,
				NOTIFICATION_TEXT_TYPE_TITLE, DP_NOTIFICATION_NO_SUBJECT, NULL,
				NOTIFICATION_VARIABLE_TYPE_NONE);
	}
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("failed to set subject error:%s", __dp_noti_error_str(err));
	}

	err = notification_set_image(noti_handle,
			NOTIFICATION_IMAGE_TYPE_ICON,
			DP_NOTIFICATION_ONGOING_ICON_PATH);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("failed to set icon error:%s", __dp_noti_error_str(err));
	}

	if (raws_buffer != NULL && raws_length > 0) {
		bundle *b = NULL;
		b = bundle_decode_raw((bundle_raw *)raws_buffer, raws_length);
		if (b != NULL) {
			err = notification_set_execute_option(noti_handle, NOTIFICATION_EXECUTE_TYPE_SINGLE_LAUNCH, "View", NULL, b);
		} else {
			b = bundle_create();
			if (b != NULL && pkgname != NULL) {
				if (appsvc_set_pkgname(b, pkgname) != APPSVC_RET_OK) {
					TRACE_ERROR("failed to set set pkgname");
				} else {
					err = notification_set_execute_option(noti_handle, NOTIFICATION_EXECUTE_TYPE_SINGLE_LAUNCH, "View", NULL, b);
				}
			} else {
				TRACE_ERROR("failed to create bundle");
			}
		}
		if (err != NOTIFICATION_ERROR_NONE) {
			TRACE_ERROR("failed to set service error:%s", __dp_noti_error_str(err));
		}
		if (b != NULL)
			bundle_free(b);
	}

	err = notification_set_property(noti_handle,
			NOTIFICATION_PROP_DISABLE_TICKERNOTI);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("failed to set property error:%s", __dp_noti_error_str(err));
		notification_free(noti_handle);
		return -1;
	}
	err = notification_set_image(noti_handle,
			NOTIFICATION_IMAGE_TYPE_ICON_FOR_INDICATOR,
			DP_NOTIFICATION_DOWNLOADING_ICON_PATH);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("failed to set icon indicator error:%s", __dp_noti_error_str(err));
		notification_free(noti_handle);
		return -1;
	}
	err = notification_set_display_applist(noti_handle,
			NOTIFICATION_DISPLAY_APP_ALL);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("failed to set display app all error:%s", __dp_noti_error_str(err));
		notification_free(noti_handle);
		return -1;
	}

	int priv_id = 0;
	err = notification_insert(noti_handle, &priv_id);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("failed to set priv_id error:%s", __dp_noti_error_str(err));
		notification_free(noti_handle);
		return -1;
	}

	//TRACE_DEBUG("m_noti_id [%d]", priv_id);
	notification_free(noti_handle);

	return priv_id; // store on memory for reuse it
}



int dp_notification_ongoing_update(const int noti_priv_id, const double received_size, const double file_size, const char *subject)
{
	if (noti_priv_id > 0) {
		int err = NOTIFICATION_ERROR_NONE;
		if (file_size > 0)
			err = notification_update_progress(NULL, noti_priv_id, (received_size / file_size));
		else
			err = notification_update_size(NULL, noti_priv_id, received_size);
		if (err != NOTIFICATION_ERROR_NONE) {
			TRACE_ERROR("failed to update error:%s", __dp_noti_error_str(err));
			// return 0 because progress is called frequently
		}
		if (subject != NULL) {
			notification_h noti_handle = NULL;
			noti_handle = notification_load(NULL, noti_priv_id);
			if (noti_handle != NULL) {
				err = notification_set_text(noti_handle,
					NOTIFICATION_TEXT_TYPE_TITLE, subject, NULL,
					NOTIFICATION_VARIABLE_TYPE_NONE);
				err = notification_update(noti_handle);
				if (err != NOTIFICATION_ERROR_NONE) {
					TRACE_ERROR("failed to update by priv_id:%d", noti_priv_id);
				}
				notification_free(noti_handle);
			} else {
				TRACE_ERROR("failed to load handle by priv_id:%d", noti_priv_id);
				return -1;
			}
		}
	}
	return 0;
}


int dp_notification_delete_ongoing(const int noti_priv_id)
{
	int err = NOTIFICATION_ERROR_NONE;
	err = notification_delete_by_priv_id(NULL, NOTIFICATION_TYPE_ONGOING, noti_priv_id);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("failed to delete notification by priv_id:%d error:%s", noti_priv_id, __dp_noti_error_str(err));
	}
	return 0;
}

int dp_notification_delete(const int noti_priv_id)
{
	int err = NOTIFICATION_ERROR_NONE;
	err = notification_delete_by_priv_id(NULL, NOTIFICATION_TYPE_NOTI, noti_priv_id);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("failed to delete notification by priv_id:%d error:%s", noti_priv_id, __dp_noti_error_str(err));
	}
	return 0;
}


int dp_notification_new(void *dbhandle, const int download_id, const int state, int content_type, const char *pkgname)
{
	int errorcode = DP_ERROR_NONE;
	int err = NOTIFICATION_ERROR_NONE;

	if (state != DP_STATE_COMPLETED &&
			state != DP_STATE_CANCELED &&
			state != DP_STATE_FAILED) {
		TRACE_ERROR("deny by invalid state:%d id:%d", state, download_id);
		return -1;
	}


	notification_h noti_handle = NULL;
	noti_handle = notification_create(NOTIFICATION_TYPE_NOTI);
	if (noti_handle == NULL) {
		TRACE_ERROR("failed to create notification handle");
		return -1;
	}

	err = notification_set_layout(noti_handle, NOTIFICATION_LY_NOTI_EVENT_SINGLE);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("Fail to set notification layout [%d]", err);
		notification_free(noti_handle);
		return -1;
	}

	err = notification_set_text_domain(noti_handle, DP_DOMAIN, DP_LOCALEDIR);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("Fail to set text domain [%d]", err);
		notification_free(noti_handle);
		return -1;
	}

	char *string = NULL;
	unsigned length = 0;
	if (dp_db_get_property_string(dbhandle, download_id, DP_TABLE_NOTIFICATION, DP_DB_COL_NOTI_DESCRIPTION, (unsigned char **)&string, &length, &errorcode) < 0) {
		TRACE_ERROR("failed to get description id:%d error:%s", download_id, dp_print_errorcode(errorcode));
	}
	if (string != NULL) {
#ifdef _TIZEN_2_3_UX
		err = notification_set_text(noti_handle,
				NOTIFICATION_TEXT_TYPE_CONTENT, string, NULL,
				NOTIFICATION_VARIABLE_TYPE_NONE);
#else
		err = notification_set_text(noti_handle,
				NOTIFICATION_TEXT_TYPE_TITLE, string, NULL,
				NOTIFICATION_VARIABLE_TYPE_NONE);
#endif
		free(string);
		if (err != NOTIFICATION_ERROR_NONE) {
			TRACE_ERROR("failed to set state id:%d error:%s", download_id, __dp_noti_error_str(err));
		}
	} else {
		string = __dp_noti_status(state);
#ifdef _TIZEN_2_3_UX
		err = notification_set_text(noti_handle,
				NOTIFICATION_TEXT_TYPE_CONTENT, __(string), string,
					NOTIFICATION_VARIABLE_TYPE_NONE);
#else
		err = notification_set_text(noti_handle,
				NOTIFICATION_TEXT_TYPE_TITLE, __(string), string,
					NOTIFICATION_VARIABLE_TYPE_NONE);
#endif
		if (err != NOTIFICATION_ERROR_NONE) {
			TRACE_ERROR("failed to set state id:%d error:%s", download_id, __dp_noti_error_str(err));
		}
	}

	string = NULL;
	if (dp_db_get_property_string(dbhandle, download_id, DP_TABLE_NOTIFICATION, DP_DB_COL_NOTI_SUBJECT, (unsigned char **)&string, &length, &errorcode) < 0) {
		TRACE_ERROR("failed to get subject id:%d error:%s", download_id, dp_print_errorcode(errorcode));
	}
	err = NOTIFICATION_ERROR_NONE;
	if (string == NULL && dp_db_get_property_string(dbhandle, download_id, DP_TABLE_DOWNLOAD, DP_DB_COL_CONTENT_NAME, (unsigned char **)&string, &length, &errorcode) < 0) {
		TRACE_ERROR("failed to get content_name id:%d error:%s", download_id, dp_print_errorcode(errorcode));
	}
	if (string == NULL)
		string = dp_strdup(DP_NOTIFICATION_NO_SUBJECT);
	if (string != NULL) {
#ifdef _TIZEN_2_3_UX
		err = notification_set_text(noti_handle,
				NOTIFICATION_TEXT_TYPE_TITLE, string, NULL,
				NOTIFICATION_VARIABLE_TYPE_NONE);
#else
		err = notification_set_text(noti_handle,
				NOTIFICATION_TEXT_TYPE_CONTENT, string, NULL,
				NOTIFICATION_VARIABLE_TYPE_NONE);
#endif
		free(string);
		string = NULL;
		if (err != NOTIFICATION_ERROR_NONE) {
			TRACE_ERROR("failed to set state id:%d error:%s", download_id, __dp_noti_error_str(err));
		}
	}
	err = notification_set_time(noti_handle, time(NULL));
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("failed to set time id:%d error:%s", download_id, __dp_noti_error_str(err));
	}

	bundle *b = NULL;
	bundle_raw *raws_buffer = NULL;
	if (state == DP_STATE_COMPLETED) {

		if (dp_db_get_property_string(dbhandle, download_id, DP_TABLE_NOTIFICATION, DP_DB_COL_NOTI_RAW_COMPLETE, &raws_buffer, &length, &errorcode) < 0) {
			TRACE_ERROR("failed to get bundle raws id:%d", download_id);
		}
		if (raws_buffer != NULL) {
			b = bundle_decode_raw(raws_buffer, length);
			bundle_free_encoded_rawdata(&raws_buffer);
		}
		if (b != NULL) {
			err = notification_set_execute_option(noti_handle, NOTIFICATION_EXECUTE_TYPE_SINGLE_LAUNCH, "View", NULL, b);
		} else {
			b = bundle_create();
			if (dp_db_get_property_string(dbhandle, download_id, DP_TABLE_DOWNLOAD, DP_DB_COL_SAVED_PATH, (unsigned char **)&string, &length, &errorcode) < 0) {
				TRACE_ERROR("failed to get saved_path id:%d error:%s", download_id, dp_print_errorcode(errorcode));
			}
			if (b != NULL && string != NULL) {
				if (appsvc_set_operation(b, APPSVC_OPERATION_VIEW) != APPSVC_RET_OK) {
					TRACE_ERROR("failed to set service operation id:%d", download_id);
				} else {
					if (appsvc_set_uri(b, string) != APPSVC_RET_OK) {
						TRACE_ERROR("failed to set service uri id:%d", download_id);
					} else {
						err = notification_set_execute_option(noti_handle,
							NOTIFICATION_EXECUTE_TYPE_SINGLE_LAUNCH, "View", NULL, b);
					}
				}
			} else {
				TRACE_SECURE_ERROR("failed to create bundle id:%d path:%s", download_id, string);
			}
			free(string);
			string = NULL;
		}
		if (err != NOTIFICATION_ERROR_NONE) {
			TRACE_ERROR("failed to set service id:%d error:%s", download_id, __dp_noti_error_str(err));
		}

		char *file_type_icon = DP_NOTIFICATION_UNKNOWN_ICON_PATH;
		if (content_type > 0 && content_type < DP_MAX_ICONS_TABLE_COUNT)
			file_type_icon = file_icons_table[content_type];

		err = notification_set_image(noti_handle, NOTIFICATION_IMAGE_TYPE_ICON, file_type_icon);
		if (err != NOTIFICATION_ERROR_NONE) {
			TRACE_ERROR("failed to set icon id:%d error:%s", download_id, __dp_noti_error_str(err));
		}
		err = notification_set_image(noti_handle, NOTIFICATION_IMAGE_TYPE_ICON_FOR_INDICATOR,
				DP_NOTIFICATION_COMPLETED_INDICATOR_ICON_PATH);
		if (err != NOTIFICATION_ERROR_NONE) {
			TRACE_ERROR("failed to set icon id:%d error:%s", download_id, __dp_noti_error_str(err));
		}

	} else if (state == DP_STATE_CANCELED || state == DP_STATE_FAILED) {

		if (dp_db_get_property_string(dbhandle, download_id, DP_TABLE_NOTIFICATION, DP_DB_COL_NOTI_RAW_FAIL, &raws_buffer, &length, &errorcode) < 0) {
			TRACE_ERROR("failed to get bundle raws id:%d", download_id);
		}
		if (raws_buffer != NULL) {
			b = bundle_decode_raw(raws_buffer, length);
			bundle_free_encoded_rawdata(&raws_buffer);
		}
		if (b != NULL) {
			err = notification_set_execute_option(noti_handle, NOTIFICATION_EXECUTE_TYPE_SINGLE_LAUNCH, "View", NULL, b);
		} else {
			b = bundle_create();
			if (b != NULL && pkgname != NULL) {
				if (appsvc_set_pkgname(b, pkgname) != APPSVC_RET_OK) {
					TRACE_ERROR("failed to set set pkgname id:%d", download_id);
				} else {
					err = notification_set_execute_option(noti_handle,
							NOTIFICATION_EXECUTE_TYPE_SINGLE_LAUNCH, "View", NULL, b);
				}
			} else {
				TRACE_ERROR("failed to create bundle id:%d", download_id);
			}
		}
		if (err != NOTIFICATION_ERROR_NONE) {
			TRACE_ERROR("failed to set service id:%d error:%s", download_id, __dp_noti_error_str(err));
		}

		err = notification_set_image(noti_handle, NOTIFICATION_IMAGE_TYPE_ICON,
				DP_NOTIFICATION_FAILED_ICON_PATH);
		if (err != NOTIFICATION_ERROR_NONE) {
			TRACE_ERROR("failed to set icon id:%d error:%s", download_id, __dp_noti_error_str(err));
		}
		err = notification_set_image(noti_handle, NOTIFICATION_IMAGE_TYPE_ICON_FOR_INDICATOR,
				DP_NOTIFICATION_FAILED_INDICATOR_ICON_PATH);
		if (err != NOTIFICATION_ERROR_NONE) {
			TRACE_ERROR("failed to set icon id:%d error:%s", download_id, __dp_noti_error_str(err));
		}

	}

	if (b != NULL)
		bundle_free(b);

	err = notification_set_property(noti_handle,
			NOTIFICATION_PROP_DISABLE_TICKERNOTI);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("failed to set property id:%d error:%s", download_id, __dp_noti_error_str(err));
	}
	err = notification_set_display_applist(noti_handle,
			NOTIFICATION_DISPLAY_APP_ALL ^ NOTIFICATION_DISPLAY_APP_INDICATOR);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("failed to set display app all id:%d error:%s", download_id, __dp_noti_error_str(err));
	}

	int priv_id = 0;
	err = notification_insert(noti_handle, &priv_id);
	if (err != NOTIFICATION_ERROR_NONE) {
		TRACE_ERROR("failed to set priv_id id:%d error:%s", download_id, __dp_noti_error_str(err));
	}

	//TRACE_DEBUG("m_noti_id [%d]", priv_id);
	notification_free(noti_handle);

	return priv_id;
}
#else
void dp_notification_clear_ongoings()
{
	return;
}
#endif
