#ifndef DOWNLOAD_PROVIDER_DB_H
#define DOWNLOAD_PROVIDER_DB_H

#include "download-provider-config.h"

typedef struct {
	download_states state;
	int notification;
	int retrycount;
	int requestid;
	char *packagename;
	char *installpath;
	char *filename;
	char *createdate;
	char *url;
	char *mimetype;
	char *etag;
	char *saved_path;
} download_dbinfo;

typedef struct {
	unsigned int count;
	download_dbinfo *item;
} download_dbinfo_list;

typedef enum {
	DOWNLOAD_DB_UNIQUEID = 0,
	DOWNLOAD_DB_PACKAGENAME = 1,
	DOWNLOAD_DB_NOTIFICATION = 2,
	DOWNLOAD_DB_INSTALLPATH = 3,
	DOWNLOAD_DB_FILENAME = 4,
	DOWNLOAD_DB_RETRYCOUNT = 5,
	DOWNLOAD_DB_STATE = 6,
	DOWNLOAD_DB_URL = 7,
	DOWNLOAD_DB_MIMETYPE = 10,
	DOWNLOAD_DB_ETAG = 11,
	DOWNLOAD_DB_SAVEDPATH = 12
} download_db_column_type;

int download_provider_db_requestinfo_new(download_clientinfo *clientinfo);
int download_provider_db_requestinfo_remove(int uniqueid);
int download_provider_db_requestinfo_update_column(download_clientinfo *clientinfo,
							download_db_column_type type);
download_dbinfo_list *download_provider_db_get_list(int state);
void download_provider_db_list_free(download_dbinfo_list *list);
int download_provider_db_list_count(int state);
download_request_info *download_provider_db_get_requestinfo(download_dbinfo *dbinfo);
int download_provider_db_history_new(download_clientinfo *clientinfo);
int download_provider_db_history_remove(int uniqueid);
int download_provider_db_history_limit_rows();

#endif
