#include <app.h>
#include <app_ui_notification.h>
#include <app_service.h>

#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "download-provider-notification.h"
#include "download-provider-utils.h"
#include "download-provider-log.h"

#include <libintl.h>

#define S_(s) dgettext("sys_string", s)

#define DP_NOTIFICATION_ICON_PATH IMAGE_DIR"/Q02_Notification_Download_failed.png"

void print_app_error_message(int ret)
{
	switch (ret) {
	case APP_ERROR_NONE:
		TRACE_DEBUG_MSG("APP_ERROR_NONE");
		break;
	case APP_ERROR_INVALID_PARAMETER:
		TRACE_DEBUG_MSG("APP_ERROR_INVALID_PARAMETER");
		break;
	case APP_ERROR_OUT_OF_MEMORY:
		TRACE_DEBUG_MSG("APP_ERROR_OUT_OF_MEMORY");
		break;
	case APP_ERROR_INVALID_CONTEXT:
		TRACE_DEBUG_MSG("APP_ERROR_INVALID_CONTEXT");
		break;
	case APP_ERROR_NO_SUCH_FILE:
		TRACE_DEBUG_MSG("APP_ERROR_NO_SUCH_FILE");
		break;
	case APP_ERROR_ALREADY_RUNNING:
		TRACE_DEBUG_MSG("APP_ERROR_ALREADY_RUNNING");
		break;
	default:
		TRACE_DEBUG_MSG("Unknown error");
		break;
	}
}

void print_notification_error_message(int ret)
{
	switch (ret) {
	case UI_NOTIFICATION_ERROR_NONE:
		TRACE_DEBUG_MSG("UI_NOTIFICATION_ERROR_NONE");
		break;
	case UI_NOTIFICATION_ERROR_INVALID_PARAMETER:
		TRACE_DEBUG_MSG("UI_NOTIFICATION_ERROR_INVALID_PARAMETER");
		break;
	case UI_NOTIFICATION_ERROR_OUT_OF_MEMORY:
		TRACE_DEBUG_MSG("UI_NOTIFICATION_ERROR_OUT_OF_MEMORY");
		break;
	case UI_NOTIFICATION_ERROR_DB_FAILED:
		TRACE_DEBUG_MSG("UI_NOTIFICATION_ERROR_DB_FAILED");
		break;
	case UI_NOTIFICATION_ERROR_NO_SUCH_FILE:
		TRACE_DEBUG_MSG("UI_NOTIFICATION_ERROR_NO_SUCH_FILE");
		break;
	case UI_NOTIFICATION_ERROR_INVALID_STATE:
		TRACE_DEBUG_MSG("UI_NOTIFICATION_ERROR_INVALID_STATE");
		break;
	default:
		TRACE_DEBUG_MSG("Unknown error");
		break;
	}
}

char *__get_string_status(int state)
{
	char *message = NULL;
	switch (state) {
	case DOWNLOAD_STATE_FINISHED:
		//message = S_("IDS_COM_POP_SUCCESS");
		message = "Completed";
		break;
	case DOWNLOAD_STATE_STOPPED:
		//message = S_("IDS_COM_POP_CANCELLED");
		message = "Canclled";
		break;
	case DOWNLOAD_STATE_FAILED:
		//message = S_("IDS_COM_POP_FAILED");
		message = "Failed";
		break;
	default:
		break;
	}
	return message;
}

bool download_provider_appfw_notification_cb(ui_notification_h notification,
							void *user_data)
{
	if (!user_data)
		return false;
	download_clientinfo *clientinfo = (download_clientinfo *) user_data;
	if (!clientinfo->downloadinfo)
		return false;

	bool checkInfo = false;
	char *title = NULL;
	char *content = NULL;
	ui_notification_get_title(notification, &title);
	ui_notification_get_content(notification, &content);

	TRACE_DEBUG_MSG("title [%s]", title);
	TRACE_DEBUG_MSG("content [%s]", content);

	// relatively unique
	if (title) {
		int title_length = strlen(title);
		int content_name_length =
			strlen(clientinfo->downloadinfo->content_name);
		if (title_length == content_name_length)
			if (strncmp
				(title, clientinfo->downloadinfo->content_name,
				title_length) == 0)
				checkInfo = true;
	}
	// Only when matched title.
	if (checkInfo && content) {
		checkInfo = false;
		char *failed_content =
			__get_string_status(DOWNLOAD_STATE_FAILED);
		if (failed_content) {
			int content_length = strlen(content);
			int content_name_length = strlen(failed_content);
			if (content_length == content_name_length)
				if (strncmp
					(content, failed_content,
					content_length) == 0)
					checkInfo = true;
			free(failed_content);
		}
		if (!checkInfo) {
			char *stopped_content =
				__get_string_status(DOWNLOAD_STATE_STOPPED);
			if (stopped_content) {
				int content_length = strlen(content);
				int content_name_length =
					strlen(stopped_content);
				if (content_length == content_name_length)
					if (strncmp
						(content, stopped_content,
						content_length) == 0)
						checkInfo = true;
				free(stopped_content);
			}
		}
	}

	if (title)
		free(title);
	if (content)
		free(content);

	if (checkInfo) {	// compare info with noti info.
		TRACE_DEBUG_MSG("ui_notification_cancel");
		ui_notification_cancel(notification);
		return false;	// do not search noti item anymore.
	}
	return true;
}

int destroy_appfw_service(download_clientinfo *clientinfo)
{
	if (!clientinfo)
		return -1;

	if (clientinfo->service_handle) {
		service_destroy(clientinfo->service_handle);
	}
	clientinfo->service_handle = NULL;
	return 0;
}

int create_appfw_service(download_clientinfo *clientinfo, bool ongoing)
{
	if (!clientinfo)
		return -1;

	if (ongoing) {
		if (!clientinfo->service_handle) {
			if (service_create(&clientinfo->service_handle) < 0) {
				TRACE_DEBUG_MSG("failed service_create (%s)", strerror(errno));
				return false;
			}
			if (clientinfo->requestinfo
				&& clientinfo->requestinfo->client_packagename.str) {
				if (service_set_package(clientinfo->service_handle,
							clientinfo->requestinfo->
							client_packagename.str) < 0)
					TRACE_DEBUG_MSG("failed service_set_package (%s)",
							strerror(errno));
			}
		}
	} else {
		if (clientinfo->service_handle) {
			destroy_appfw_service(clientinfo);
		}
		if (service_create(&clientinfo->service_handle) < 0) {
			TRACE_DEBUG_MSG("failed service_create (%s)", strerror(errno));
			return false;
		}
	}
	return 0;
}

int destroy_appfw_notification(download_clientinfo *clientinfo)
{
	if (!clientinfo)
		return -1;

	destroy_appfw_service(clientinfo);
	if (clientinfo->ui_notification_handle) {
		bool ongoing = 0;
		ui_notification_is_ongoing(clientinfo->ui_notification_handle,
						&ongoing);
		if (ongoing) {
			if (ui_notification_cancel
				(clientinfo->ui_notification_handle) < 0)
				TRACE_DEBUG_MSG("Fail ui_notification_cancel");
		}
		ui_notification_destroy(clientinfo->ui_notification_handle);
	}
	clientinfo->ui_notification_handle = NULL;
	return 0;
}

int create_appfw_notification(download_clientinfo *clientinfo, bool ongoing)
{
	if (!clientinfo)
		return -1;

	int ret = 0;

	if (ui_notification_create(ongoing, &clientinfo->ui_notification_handle)
		< 0) {
		TRACE_DEBUG_MSG("Fail to create notification handle");
		return -1;
	}

	if (clientinfo->downloadinfo) {
		TRACE_DEBUG_MSG("###content_name[%s]", clientinfo->downloadinfo->content_name);
		char *title = clientinfo->downloadinfo->content_name;
		if (!title)
			//title = S_("IDS_COM_BODY_NO_NAME");
			title = "No name";
		if (ui_notification_set_title(
				clientinfo->ui_notification_handle, title) < 0) {
			TRACE_DEBUG_MSG
				("failed ui_notification_set_title (%s)",
				strerror(errno));
			destroy_appfw_notification(clientinfo);
			return -1;
		}
	}

	if (ui_notification_set_icon
		(clientinfo->ui_notification_handle,
		DP_NOTIFICATION_ICON_PATH) < 0) {
		TRACE_DEBUG_MSG("Fail ui_notification_set_icon (%s)",
				strerror(errno));
		destroy_appfw_notification(clientinfo);
		return -1;
	}


	create_appfw_service(clientinfo, ongoing);
	if (!ongoing) {
		// view the special viewer by contents
		if (clientinfo->downloadinginfo
			&& sizeof(clientinfo->downloadinginfo->saved_path) > 0
			&& clientinfo->state == DOWNLOAD_STATE_FINISHED) {
			if (service_set_operation
				(clientinfo->service_handle,
				SERVICE_OPERATION_VIEW) < 0) {
				TRACE_DEBUG_MSG
					("Fail service_set_operation");
				destroy_appfw_service(clientinfo);
			}
			if (service_set_uri(clientinfo->service_handle,
						clientinfo->downloadinginfo->saved_path)
				< 0) {
				TRACE_DEBUG_MSG("Fail service_set_uri");
				destroy_appfw_service(clientinfo);
			}
		} else {
			if (service_set_package(clientinfo->service_handle,
						clientinfo->requestinfo->
						client_packagename.str) < 0)
				TRACE_DEBUG_MSG("failed service_set_package (%s)",
						strerror(errno));
		}
	}
	if (ui_notification_set_service
		(clientinfo->ui_notification_handle,
		clientinfo->service_handle) < 0) {
		TRACE_DEBUG_MSG("Fail ui_notification_set_service");
		destroy_appfw_service(clientinfo);
	}

	if ((ret =
		ui_notification_post(clientinfo->ui_notification_handle)) !=
		UI_NOTIFICATION_ERROR_NONE) {
		TRACE_DEBUG_MSG("Fail to post [%d]", ret);
		print_notification_error_message(ret);
		destroy_appfw_notification(clientinfo);
		return -1;
	}

	return 0;
}

int set_downloadinginfo_appfw_notification(download_clientinfo *clientinfo)
{
	if (!clientinfo || !clientinfo->downloadinginfo)
		return -1;

	if (!clientinfo->ui_notification_handle) {
		create_appfw_notification(clientinfo, true);
	}

	if (!clientinfo->ui_notification_handle)
		return -1;

	if (clientinfo->downloadinfo && clientinfo->downloadinfo->file_size > 0) {
		double progress =
			(double)clientinfo->downloadinginfo->received_size /
			(double)clientinfo->downloadinfo->file_size;
		if (ui_notification_update_progress
			(clientinfo->ui_notification_handle,
			UI_NOTIFICATION_PROGRESS_TYPE_PERCENTAGE, progress) < 0) {
			TRACE_DEBUG_MSG("Fail to update progress");
			destroy_appfw_notification(clientinfo);
			return -1;
		}
	} else {
		if (ui_notification_update_progress
			(clientinfo->ui_notification_handle,
			UI_NOTIFICATION_PROGRESS_TYPE_SIZE,
			(double)(clientinfo->downloadinginfo->received_size)) <
			0) {
			TRACE_DEBUG_MSG("Fail to update size");
			destroy_appfw_notification(clientinfo);
			return -1;
		}
	}

	return 0;
}

int set_downloadedinfo_appfw_notification(download_clientinfo *clientinfo)
{
	if (!clientinfo)
		return -1;

	destroy_appfw_notification(clientinfo);

	if (!clientinfo->ui_notification_handle)
		create_appfw_notification(clientinfo, false);

	// fill downloaded info to post to notification bar.
	char *message = __get_string_status(clientinfo->state);
	TRACE_DEBUG_MSG("message : [%s]", message);
	if (message) {
		if (ui_notification_set_content
			(clientinfo->ui_notification_handle, message) < 0)
			TRACE_DEBUG_MSG("Fail to set content");
	}

	time_t tt = time(NULL);
	struct tm *localTime = localtime(&tt);

	if (ui_notification_set_time
		(clientinfo->ui_notification_handle, localTime) < 0)
		TRACE_DEBUG_MSG("Fail to set time");

	if (ui_notification_update(clientinfo->ui_notification_handle) < 0)
		TRACE_DEBUG_MSG("Fail to ui_notification_update");

	destroy_appfw_notification(clientinfo);
	return 0;
}

void clear_downloadinginfo_appfw_notification()
{
	ui_notification_cancel_all_by_type(true);
	return;
}
