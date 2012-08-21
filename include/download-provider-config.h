#ifndef DOWNLOAD_PROVIDER_CONFIG_H
#define DOWNLOAD_PROVIDER_CONFIG_H

#include <app_ui_notification.h>
#include <app_service.h>

#include <download-provider.h>

#define DOWNLOAD_PROVIDER_IPC "/tmp/download-provider"
#define DOWNLOAD_PROVIDER_LOCK_PID "/tmp/download-provider.lock"

#define DOWNLOAD_PROVIDER_CARE_CLIENT_MIN_INTERVAL 5
#define DOWNLOAD_PROVIDER_CARE_CLIENT_MAX_INTERVAL 3600

#define DOWNLOAD_PROVIDER_DOWNLOADING_DB_NAME DATABASE_DIR"/"DATABASE_NAME

#define MAX_CLIENT 64		// Backgound Daemon should has the limitation of resource.

#define DOWNLOAD_PROVIDER_REQUESTID_LEN 20

#define DOWNLOAD_PROVIDER_HISTORY_DB_LIMIT_ROWS 1000

typedef struct {
	pid_t pid;
	uid_t uid;
	gid_t gid;
} download_client_credential;

typedef struct {
	pthread_t thread_pid;
	pthread_mutex_t client_mutex;
	int clientfd;		// socket for client
	download_client_credential credentials;
	ui_notification_h ui_notification_handle;	// notification bar
	service_h service_handle;	// launch the special app from notification bar
	int req_id;
	download_request_info *requestinfo;
	downloading_state_info *downloadinginfo;
	download_content_info *downloadinfo;
	char *tmp_saved_path;
	download_states state;
	download_error err;
} download_clientinfo;

typedef struct {
	download_clientinfo *clientinfo;
} download_clientinfo_slot;
#endif
