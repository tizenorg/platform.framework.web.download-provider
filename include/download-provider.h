#ifndef DOWNLOAD_PROVIDER_H
#define DOWNLOAD_PROVIDER_H

#ifdef __cplusplus
extern "C" {
#endif

#define DOWNLOAD_PROVIDER_IPC "/tmp/download-provider"

#define DP_MAX_STR_LEN_64 64
#define DP_MAX_STR_LEN 256
#define DP_MAX_PATH_LEN DP_MAX_STR_LEN
#define DP_MAX_URL_LEN 2048

	typedef enum {
		DOWNLOAD_CONTROL_START = 1,
		DOWNLOAD_CONTROL_STOP = 2,
		DOWNLOAD_CONTROL_PAUSE = 3,
		DOWNLOAD_CONTROL_RESUME = 4,
		DOWNLOAD_CONTROL_GET_DOWNLOADING_INFO = 11,
		DOWNLOAD_CONTROL_GET_STATE_INFO = 13,
		DOWNLOAD_CONTROL_GET_DOWNLOAD_INFO = 14,
		DOWNLOAD_CONTROL_GET_REQUEST_STATE_INFO = 15
	} download_controls;

	typedef enum {
		DOWNLOAD_STATE_NONE = 0,
		DOWNLOAD_STATE_READY = 1,
		DOWNLOAD_STATE_PENDED = 2,
		DOWNLOAD_STATE_PAUSE_REQUESTED = 3,
		DOWNLOAD_STATE_PAUSED = 4,
		DOWNLOAD_STATE_DOWNLOADING = 5,
		DOWNLOAD_STATE_INSTALLING = 6,
		DOWNLOAD_STATE_FINISHED = 7,
		DOWNLOAD_STATE_STOPPED = 10,
		DOWNLOAD_STATE_FAILED = 11
	} download_states;

	typedef enum {
		DOWNLOAD_ERROR_NONE = 0,
		DOWNLOAD_ERROR_INVALID_PARAMETER = 1,
		DOWNLOAD_ERROR_OUT_OF_MEMORY = 2,
		DOWNLOAD_ERROR_IO_ERROR = 3,
		DOWNLOAD_ERROR_NETWORK_UNREACHABLE = 4,
		DOWNLOAD_ERROR_CONNECTION_TIMED_OUT = 5,
		DOWNLOAD_ERROR_NO_SPACE = 6,
		DOWNLOAD_ERROR_FILED_NOT_FOUND = 7,
		DOWNLOAD_ERROR_INVALID_STATE = 8,
		DOWNLOAD_ERROR_CONNECTION_FAILED = 9,
		DOWNLOAD_ERROR_INVALID_URL = 10,
		DOWNLOAD_ERROR_INVALID_DESTINATION = 11,
		DOWNLOAD_ERROR_TOO_MANY_DOWNLOADS = 12,
		DOWNLOAD_ERROR_ALREADY_COMPLETED = 13,
		DOWNLOAD_ERROR_INSTALL_FAIL = 20,
		DOWNLOAD_ERROR_FAIL_INIT_AGENT = 100,
		DOWNLOAD_ERROR_UNKOWN = 900
	} download_error;

	typedef struct {
		unsigned int started;
		unsigned int paused;
		unsigned int completed;
		unsigned int stopped;
		unsigned int progress;
	} callback_info;

	typedef struct {
		unsigned int length;
		char *str;
	} download_flexible_string;

	typedef struct {
		unsigned int rows;
		download_flexible_string *str;
	} download_flexible_double_string;

	typedef struct {
		unsigned int received_size;
		char saved_path[DP_MAX_PATH_LEN];
	} downloading_state_info;

	typedef struct {
		download_states state;
		download_error err;
	} download_state_info;

	typedef struct {
		unsigned int file_size;
		char mime_type[DP_MAX_STR_LEN];
		char content_name[DP_MAX_STR_LEN];
	} download_content_info;

	typedef struct {
		callback_info callbackinfo;
		unsigned int notification;
		int requestid;
		download_flexible_string client_packagename;
		download_flexible_string url;
		download_flexible_string install_path;
		download_flexible_string filename;
		download_flexible_double_string headers;
	} download_request_info;

	typedef struct {
		download_state_info stateinfo;
		int requestid;
	} download_request_state_info;

#ifdef __cplusplus
}
#endif
#endif
