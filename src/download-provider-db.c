#include <db-util.h>

#include <string.h>
#include <errno.h>

#include "download-provider-config.h"
#include "download-provider-db.h"
#include "download-provider-log.h"

__thread sqlite3 *g_download_provider_db = 0;

void __download_provider_db_close()
{
	if (g_download_provider_db) {
		db_util_close(g_download_provider_db);
	}
	g_download_provider_db = 0;
}

int __download_provider_db_open()
{
	if (db_util_open(DOWNLOAD_PROVIDER_DOWNLOADING_DB_NAME,
			 &g_download_provider_db,
			 DB_UTIL_REGISTER_HOOK_METHOD) != SQLITE_OK) {
		TRACE_DEBUG_MSG("failed db_util_open [%s][%s]",
				DOWNLOAD_PROVIDER_DOWNLOADING_DB_NAME,
				sqlite3_errmsg(g_download_provider_db));
		__download_provider_db_close();
		return -1;
	}
	return g_download_provider_db ? 0 : -1;
}

void _download_provider_sql_close(sqlite3_stmt *stmt)
{
	if (sqlite3_finalize(stmt) != SQLITE_OK)
		TRACE_DEBUG_MSG("failed sqlite3_finalize [%s]",
				sqlite3_errmsg(g_download_provider_db));

	__download_provider_db_close();
}

int _download_provider_sql_open()
{
	__download_provider_db_close();
	return __download_provider_db_open();
}

int download_provider_db_requestinfo_remove(int uniqueid)
{
	int errorcode;
	sqlite3_stmt *stmt;

	if (uniqueid <= 0) {
		TRACE_DEBUG_MSG("[NULL-CHECK]");
		return -1;
	}

	if (_download_provider_sql_open() < 0) {
		TRACE_DEBUG_MSG("db_util_open is failed [%s]",
				sqlite3_errmsg(g_download_provider_db));
		return -1;
	}

	errorcode =
	    sqlite3_prepare_v2(g_download_provider_db,
			       "delete from downloading where uniqueid = ?",
			       -1, &stmt, NULL);
	if (errorcode != SQLITE_OK) {
		TRACE_DEBUG_MSG("sqlite3_prepare_v2 is failed. [%s]",
				sqlite3_errmsg(g_download_provider_db));
		_download_provider_sql_close(stmt);
		return -1;
	}
	if (sqlite3_bind_int(stmt, 1, uniqueid) != SQLITE_OK) {
		TRACE_DEBUG_MSG("sqlite3_bind_int is failed. [%s]",
				sqlite3_errmsg(g_download_provider_db));
		_download_provider_sql_close(stmt);
		return -1;
	}
	errorcode = sqlite3_step(stmt);
	if (errorcode == SQLITE_OK || errorcode == SQLITE_DONE) {
		_download_provider_sql_close(stmt);
		return 0;
	}
	TRACE_DEBUG_MSG("sqlite3_step is failed [%s]",
			sqlite3_errmsg(g_download_provider_db));
	__download_provider_db_close();
	return -1;
}

int download_provider_db_requestinfo_new(download_clientinfo *clientinfo)
{
	int errorcode;
	sqlite3_stmt *stmt;

	if (!clientinfo || !clientinfo->requestinfo
		|| clientinfo->requestinfo->requestid <= 0) {
		TRACE_DEBUG_MSG("[NULL-CHECK]");
		return -1;
	}

	if (_download_provider_sql_open() < 0) {
		TRACE_DEBUG_MSG("db_util_open is failed [%s]",
				sqlite3_errmsg(g_download_provider_db));
		return -1;
	}

	errorcode =
	    sqlite3_prepare_v2(g_download_provider_db,
			       "INSERT INTO downloading (uniqueid, packagename, notification, installpath, filename, creationdate, state, url, mimetype, savedpath) VALUES (?, ?, ?, ?, ?, DATETIME('now'), ?, ?, ?, ?)",
			       -1, &stmt, NULL);
	if (errorcode != SQLITE_OK) {
		TRACE_DEBUG_MSG("sqlite3_prepare_v2 is failed. [%s]",
				sqlite3_errmsg(g_download_provider_db));
		_download_provider_sql_close(stmt);
		return -1;
	}
	if (sqlite3_bind_int(stmt, 1, clientinfo->requestinfo->requestid) !=
	    SQLITE_OK) {
		TRACE_DEBUG_MSG("sqlite3_bind_int is failed. [%s]",
				sqlite3_errmsg(g_download_provider_db));
		_download_provider_sql_close(stmt);
		return -1;
	}
	if (clientinfo->requestinfo->client_packagename.length > 0) {
		if (sqlite3_bind_text
		    (stmt, 2, clientinfo->requestinfo->client_packagename.str,
		     -1, NULL) != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_bind_text is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
	}
	// notification
	if (sqlite3_bind_int(stmt, 3, clientinfo->requestinfo->notification) !=
	    SQLITE_OK) {
		TRACE_DEBUG_MSG("sqlite3_bind_int is failed. [%s]",
				sqlite3_errmsg(g_download_provider_db));
		_download_provider_sql_close(stmt);
		return -1;
	}
	if (clientinfo->downloadinfo && clientinfo->downloadinfo->content_name) {
		if (sqlite3_bind_text
		    (stmt, 5, clientinfo->downloadinfo->content_name, -1,
		     NULL) != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_bind_text is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
	}
	if (sqlite3_bind_int(stmt, 6, clientinfo->state) != SQLITE_OK) {
		TRACE_DEBUG_MSG("sqlite3_bind_int is failed. [%s]",
				sqlite3_errmsg(g_download_provider_db));
		_download_provider_sql_close(stmt);
		return -1;
	}
	if (clientinfo->requestinfo->url.length > 0) {
		if (sqlite3_bind_text
		    (stmt, 7, clientinfo->requestinfo->url.str, -1,
		     NULL) != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_bind_text is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
	}
	if (clientinfo->downloadinfo && clientinfo->downloadinfo->mime_type) {
		if (sqlite3_bind_text
		    (stmt, 8, clientinfo->downloadinfo->mime_type, -1,
		     NULL) != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_bind_text is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
	}
	if (clientinfo->tmp_saved_path) {
		if (sqlite3_bind_text
		    (stmt, 9, clientinfo->tmp_saved_path, -1,
		     NULL) != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_bind_text is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
	}
	errorcode = sqlite3_step(stmt);
	if (errorcode == SQLITE_OK || errorcode == SQLITE_DONE) {
		_download_provider_sql_close(stmt);
		return 0;
	}
	TRACE_DEBUG_MSG("sqlite3_step is failed [%s]",
			sqlite3_errmsg(g_download_provider_db));
	__download_provider_db_close();
	return -1;
}

int download_provider_db_requestinfo_update_column(download_clientinfo *clientinfo,
						   download_db_column_type type)
{
	int errorcode;
	sqlite3_stmt *stmt;

	if (!clientinfo || !clientinfo->requestinfo
		|| clientinfo->requestinfo->requestid <= 0) {
		TRACE_DEBUG_MSG("[NULL-CHECK]");
		return -1;
	}

	if (_download_provider_sql_open() < 0) {
		TRACE_DEBUG_MSG("db_util_open is failed [%s]",
				sqlite3_errmsg(g_download_provider_db));
		return -1;
	}

	switch (type) {
	case DOWNLOAD_DB_PACKAGENAME:
		if (clientinfo->requestinfo->client_packagename.length <= 0
			|| !clientinfo->requestinfo->client_packagename.str) {
			TRACE_DEBUG_MSG("[NULL-CHECK] type [%d]", type);
			_download_provider_sql_close(stmt);
			return -1;
		}
		errorcode =
			sqlite3_prepare_v2(g_download_provider_db,
						"UPDATE downloading SET packagename = ? WHERE uniqueid = ?",
						-1, &stmt, NULL);
		if (errorcode != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_prepare_v2 is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
		if (sqlite3_bind_text
			(stmt, 1, clientinfo->requestinfo->client_packagename.str,
			 -1, NULL) != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_bind_int is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
		break;
	case DOWNLOAD_DB_NOTIFICATION:
		errorcode =
			sqlite3_prepare_v2(g_download_provider_db,
						"UPDATE downloading SET notification = ? WHERE uniqueid = ?",
						-1, &stmt, NULL);
		if (errorcode != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_prepare_v2 is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
		if (sqlite3_bind_int
			(stmt, 1,
			clientinfo->requestinfo->notification) != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_bind_int is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
		break;
	case DOWNLOAD_DB_STATE:
		errorcode =
			sqlite3_prepare_v2(g_download_provider_db,
						"UPDATE downloading SET state = ? WHERE uniqueid = ?",
						-1, &stmt, NULL);
		if (errorcode != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_prepare_v2 is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
		if (sqlite3_bind_int(stmt, 1, clientinfo->state) != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_bind_int is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
		break;
	case DOWNLOAD_DB_MIMETYPE:
		if (!clientinfo->downloadinfo) {
			TRACE_DEBUG_MSG("[NULL-CHECK] type [%d]", type);
			_download_provider_sql_close(stmt);
			return -1;
		}
		errorcode =
			sqlite3_prepare_v2(g_download_provider_db,
						"UPDATE downloading SET mimetype = ? WHERE uniqueid = ?",
						-1, &stmt, NULL);
		if (errorcode != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_prepare_v2 is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
		if (sqlite3_bind_text
			(stmt, 1, clientinfo->downloadinfo->mime_type, -1,
			NULL) != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_bind_int is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
		break;
	case DOWNLOAD_DB_FILENAME:
		if (!clientinfo->downloadinfo
			|| !clientinfo->downloadinfo->content_name) {
			TRACE_DEBUG_MSG("[NULL-CHECK] type [%d]", type);
			_download_provider_sql_close(stmt);
			return -1;
		}
		errorcode =
			sqlite3_prepare_v2(g_download_provider_db,
						"UPDATE downloading SET filename = ? WHERE uniqueid = ?",
						-1, &stmt, NULL);
		if (errorcode != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_prepare_v2 is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
		if (sqlite3_bind_text
			(stmt, 1, clientinfo->downloadinfo->content_name, -1,
			NULL) != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_bind_int is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
		break;
	case DOWNLOAD_DB_SAVEDPATH:
		if (!clientinfo->tmp_saved_path) {
			TRACE_DEBUG_MSG("[NULL-CHECK] type [%d]", type);
			_download_provider_sql_close(stmt);
			return -1;
		}
		errorcode =
			sqlite3_prepare_v2(g_download_provider_db,
						"UPDATE downloading SET savedpath = ? WHERE uniqueid = ?",
						-1, &stmt, NULL);
		if (errorcode != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_prepare_v2 is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
		if (sqlite3_bind_text
			(stmt, 1, clientinfo->tmp_saved_path, -1,
			NULL) != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_bind_int is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
		break;
	default:
		TRACE_DEBUG_MSG("Wrong type [%d]", type);
		return -1;
	}

	if (sqlite3_bind_int(stmt, 2, clientinfo->requestinfo->requestid) !=
		SQLITE_OK) {
		TRACE_DEBUG_MSG("sqlite3_bind_int is failed. [%s]",
				sqlite3_errmsg(g_download_provider_db));
		_download_provider_sql_close(stmt);
		return -1;
	}

	errorcode = sqlite3_step(stmt);
	if (errorcode == SQLITE_OK || errorcode == SQLITE_DONE) {
		_download_provider_sql_close(stmt);
		return 0;
	}
	TRACE_DEBUG_MSG("sqlite3_step is failed [%s]",
			sqlite3_errmsg(g_download_provider_db));
	__download_provider_db_close();
	return -1;
}

download_dbinfo_list *download_provider_db_get_list(int state)
{
	int errorcode;
	int listcount;
	int i = 0;
	int buffer_length = 0;
	sqlite3_stmt *stmt;
	char *buffer;
	download_dbinfo_list *m_list = NULL;

	listcount = download_provider_db_list_count(state);
	if (listcount <= 0)
		return NULL;

	if (_download_provider_sql_open() < 0) {
		TRACE_DEBUG_MSG("db_util_open is failed [%s]",
				sqlite3_errmsg(g_download_provider_db));
		return NULL;
	}

	if (state != DOWNLOAD_STATE_NONE) {
		errorcode =
			sqlite3_prepare_v2(g_download_provider_db,
						"SELECT uniqueid, packagename, notification, installpath, filename, creationdate, state, url, mimetype, savedpath FROM downloading WHERE state = ?",
						-1, &stmt, NULL);
		if (errorcode != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_prepare_v2 is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return NULL;
		}
		if (sqlite3_bind_int(stmt, 1, state) != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_bind_int is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return NULL;
		}
	} else {
		errorcode =
			sqlite3_prepare_v2(g_download_provider_db,
						"SELECT uniqueid, packagename, notification, installpath, filename, creationdate, state, url, mimetype, savedpath FROM downloading",
						-1, &stmt, NULL);
		if (errorcode != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_prepare_v2 is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return NULL;
		}
	}
	m_list = (download_dbinfo_list *) calloc(1, sizeof(download_dbinfo_list));
	m_list->item =
		(download_dbinfo *) calloc(listcount, sizeof(download_dbinfo));
	m_list->count = listcount;

	while ((errorcode = sqlite3_step(stmt)) == SQLITE_ROW
			&& (i < listcount)) {
		m_list->item[i].requestid = sqlite3_column_int(stmt, 0);
		buffer = (char *)(sqlite3_column_text(stmt, 1));
		m_list->item[i].packagename = NULL;
		if (buffer) {
			buffer_length = strlen(buffer);
			m_list->item[i].packagename
				= (char *)calloc(buffer_length + 1, sizeof(char));
			memcpy(m_list->item[i].packagename, buffer,
					buffer_length * sizeof(char));
			m_list->item[i].packagename[buffer_length] = '\0';
		}
		m_list->item[i].notification = sqlite3_column_int(stmt, 2);
		buffer = (char *)(sqlite3_column_text(stmt, 3));
		m_list->item[i].installpath = NULL;
		if (buffer) {
			buffer_length = strlen(buffer);
			m_list->item[i].installpath
				= (char *)calloc(buffer_length + 1, sizeof(char));
			memcpy(m_list->item[i].installpath, buffer,
					buffer_length * sizeof(char));
			m_list->item[i].installpath[buffer_length] = '\0';
		}
		buffer = (char *)(sqlite3_column_text(stmt, 4));
		m_list->item[i].filename = NULL;
		if (buffer) {
			buffer_length = strlen(buffer);
			m_list->item[i].filename
				= (char *)calloc(buffer_length + 1, sizeof(char));
			memcpy(m_list->item[i].filename, buffer,
					buffer_length * sizeof(char));
			m_list->item[i].filename[buffer_length] = '\0';
		}
		buffer = (char *)(sqlite3_column_text(stmt, 5));
		m_list->item[i].createdate = NULL;
		if (buffer) {
			buffer_length = strlen(buffer);
			m_list->item[i].createdate
				= (char *)calloc(buffer_length + 1, sizeof(char));
			memcpy(m_list->item[i].createdate, buffer,
					buffer_length * sizeof(char));
			m_list->item[i].createdate[buffer_length] = '\0';
		}
		m_list->item[i].state = sqlite3_column_int(stmt, 6);
		buffer = (char *)(sqlite3_column_text(stmt, 7));
		m_list->item[i].url = NULL;
		if (buffer) {
			buffer_length = strlen(buffer);
			m_list->item[i].url
				= (char *)calloc(buffer_length + 1, sizeof(char));
			memcpy(m_list->item[i].url, buffer,
					buffer_length * sizeof(char));
			m_list->item[i].url[buffer_length] = '\0';
		}
		buffer = (char *)(sqlite3_column_text(stmt, 8));
		m_list->item[i].mimetype = NULL;
		if (buffer) {
			buffer_length = strlen(buffer);
			m_list->item[i].mimetype
				= (char *)calloc(buffer_length + 1, sizeof(char));
			memcpy(m_list->item[i].mimetype, buffer,
					buffer_length * sizeof(char));
			m_list->item[i].mimetype[buffer_length] = '\0';
		}
		buffer = (char *)(sqlite3_column_text(stmt, 9));
		m_list->item[i].saved_path = NULL;
		if (buffer) {
			buffer_length = strlen(buffer);
			m_list->item[i].saved_path
				= (char *)calloc(buffer_length + 1, sizeof(char));
			memcpy(m_list->item[i].saved_path, buffer,
				buffer_length * sizeof(char));
			m_list->item[i].saved_path[buffer_length] = '\0';
		}
		i++;
	}
	m_list->count = i;

	if (i <= 0) {
		TRACE_DEBUG_MSG("sqlite3_step is failed. [%s]",
				sqlite3_errmsg(g_download_provider_db));
		__download_provider_db_close();
		download_provider_db_list_free(m_list);
		return NULL;
	}
	_download_provider_sql_close(stmt);
	return m_list;
}

int download_provider_db_list_count(int state)
{
	int errorcode;
	int count = 0;
	sqlite3_stmt *stmt;

	if (_download_provider_sql_open() < 0) {
		TRACE_DEBUG_MSG("db_util_open is failed [%s]",
				sqlite3_errmsg(g_download_provider_db));
		return -1;
	}

	if (state != DOWNLOAD_STATE_NONE) {
		errorcode =
			sqlite3_prepare_v2(g_download_provider_db,
						"SELECT count(*) FROM downloading WHERE state = ?",
						-1, &stmt, NULL);
		if (errorcode != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_prepare_v2 is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
		if (sqlite3_bind_int(stmt, 1, state) != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_bind_int is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
	} else {
		errorcode =
			sqlite3_prepare_v2(g_download_provider_db,
						"SELECT count(*) FROM downloading",
						-1, &stmt, NULL);
		if (errorcode != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_prepare_v2 is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
	}
	errorcode = sqlite3_step(stmt);
	if (errorcode == SQLITE_ROW) {
		count = sqlite3_column_int(stmt, 0);
		_download_provider_sql_close(stmt);
		return count;
	}
	TRACE_DEBUG_MSG("sqlite3_step is failed. [%s]",
			sqlite3_errmsg(g_download_provider_db));
	__download_provider_db_close();
	return 0;
}

void download_provider_db_list_free(download_dbinfo_list *list)
{
	TRACE_DEBUG_MSG("");

	int i = 0;
	if (!list)
		return;

	if (list->count > 0 && list->item) {
		for (i = 0; i < list->count; i++) {
			list->item[i].requestid = 0;
			if (list->item[i].packagename)
				free(list->item[i].packagename);
			list->item[i].packagename = NULL;
			if (list->item[i].installpath)
				free(list->item[i].installpath);
			list->item[i].installpath = NULL;
			if (list->item[i].filename)
				free(list->item[i].filename);
			list->item[i].filename = NULL;
			if (list->item[i].createdate)
				free(list->item[i].createdate);
			list->item[i].createdate = NULL;
			if (list->item[i].url)
				free(list->item[i].url);
			list->item[i].url = NULL;
			if (list->item[i].mimetype)
				free(list->item[i].mimetype);
			list->item[i].mimetype = NULL;
			if (list->item[i].etag)
				free(list->item[i].etag);
			list->item[i].etag = NULL;
			if (list->item[i].saved_path)
				free(list->item[i].saved_path);
			list->item[i].saved_path = NULL;
		}
		free(list->item);
		list->item = NULL;
	}
	free(list);
	list = NULL;
}

download_request_info *download_provider_db_get_requestinfo(download_dbinfo *dbinfo)
{
	if (!dbinfo || dbinfo->requestid <= 0)
		return NULL;

	download_request_info *requestinfo =
		(download_request_info *) calloc(1, sizeof(download_request_info));
	requestinfo->requestid = dbinfo->requestid;
	if (dbinfo->packagename) {
		requestinfo->client_packagename.length =
			strlen(dbinfo->packagename);
		if (requestinfo->client_packagename.length > 0) {
			requestinfo->client_packagename.str
				=
				(char *)
				calloc((requestinfo->client_packagename.length + 1),
				sizeof(char));
			memcpy(requestinfo->client_packagename.str,
					dbinfo->packagename,
					requestinfo->client_packagename.length *
				sizeof(char));
			requestinfo->client_packagename.str[requestinfo->
								client_packagename.
								length] = '\0';
		}
	}
	if (dbinfo->url) {
		requestinfo->url.length = strlen(dbinfo->url);
		if (requestinfo->url.length > 0) {
			requestinfo->url.str
				=
				(char *)calloc((requestinfo->url.length + 1),
						sizeof(char));
			memcpy(requestinfo->url.str, dbinfo->url,
					requestinfo->url.length * sizeof(char));
			requestinfo->url.str[requestinfo->url.length] = '\0';
		}
	}
	if (dbinfo->installpath) {
		requestinfo->install_path.length = strlen(dbinfo->installpath);
		if (requestinfo->install_path.length > 0) {
			requestinfo->install_path.str
				=
				(char *)
				calloc((requestinfo->install_path.length + 1),
					sizeof(char));
			memcpy(requestinfo->install_path.str,
					dbinfo->installpath,
					requestinfo->install_path.length * sizeof(char));
			requestinfo->install_path.str[requestinfo->install_path.
								length] = '\0';
		}
	}
	if (dbinfo->filename) {
		requestinfo->filename.length = strlen(dbinfo->filename);
		if (requestinfo->filename.length > 0) {
			requestinfo->filename.str
				=
				(char *)calloc((requestinfo->filename.length + 1),
						sizeof(char));
			memcpy(requestinfo->filename.str, dbinfo->filename,
					requestinfo->filename.length * sizeof(char));
			requestinfo->filename.str[requestinfo->filename.
							length] = '\0';
		}
	}
	// disable callback.
	memset(&requestinfo->callbackinfo, 0x00, sizeof(callback_info));
	requestinfo->notification = dbinfo->notification;
	return requestinfo;
}

int download_provider_db_history_new(download_clientinfo *clientinfo)
{
	int errorcode;
	sqlite3_stmt *stmt;

	if (!clientinfo || !clientinfo->requestinfo
		|| clientinfo->requestinfo->requestid <= 0) {
		TRACE_DEBUG_MSG("[NULL-CHECK]");
		return -1;
	}

	if (_download_provider_sql_open() < 0) {
		TRACE_DEBUG_MSG("db_util_open is failed [%s]",
				sqlite3_errmsg(g_download_provider_db));
		return -1;
	}

	errorcode =
		sqlite3_prepare_v2(g_download_provider_db,
					"INSERT INTO history (uniqueid, packagename, filename, creationdate, state, mimetype, savedpath) VALUES (?, ?, ?, DATETIME('now'), ?, ?, ?)",
					-1, &stmt, NULL);
	if (errorcode != SQLITE_OK) {
		TRACE_DEBUG_MSG("sqlite3_prepare_v2 is failed. [%s]",
				sqlite3_errmsg(g_download_provider_db));
		_download_provider_sql_close(stmt);
		return -1;
	}
	if (sqlite3_bind_int(stmt, 1, clientinfo->requestinfo->requestid) !=
		SQLITE_OK) {
		TRACE_DEBUG_MSG("sqlite3_bind_int is failed. [%s]",
				sqlite3_errmsg(g_download_provider_db));
		_download_provider_sql_close(stmt);
		return -1;
	}
	if (clientinfo->requestinfo->client_packagename.length > 0) {
		if (sqlite3_bind_text
			(stmt, 2, clientinfo->requestinfo->client_packagename.str,
			 -1, NULL) != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_bind_text is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
	}
	if (clientinfo->downloadinfo && clientinfo->downloadinfo->content_name) {
		if (sqlite3_bind_text
			(stmt, 3, clientinfo->downloadinfo->content_name, -1,
			NULL) != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_bind_text is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
	}
	if (sqlite3_bind_int(stmt, 4, clientinfo->state) != SQLITE_OK) {
		TRACE_DEBUG_MSG("sqlite3_bind_int is failed. [%s]",
				sqlite3_errmsg(g_download_provider_db));
		_download_provider_sql_close(stmt);
		return -1;
	}
	if (clientinfo->downloadinfo && clientinfo->downloadinfo->mime_type) {
		if (sqlite3_bind_text
			(stmt, 5, clientinfo->downloadinfo->mime_type, -1,
			NULL) != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_bind_text is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
	}
	if (clientinfo->tmp_saved_path) {
		if (sqlite3_bind_text
			(stmt, 6, clientinfo->tmp_saved_path, -1,
			NULL) != SQLITE_OK) {
			TRACE_DEBUG_MSG("sqlite3_bind_text is failed. [%s]",
					sqlite3_errmsg(g_download_provider_db));
			_download_provider_sql_close(stmt);
			return -1;
		}
	}
	errorcode = sqlite3_step(stmt);
	if (errorcode == SQLITE_OK || errorcode == SQLITE_DONE) {
		_download_provider_sql_close(stmt);
		download_provider_db_history_limit_rows();
		return 0;
	}
	TRACE_DEBUG_MSG("sqlite3_step is failed [%s]",
			sqlite3_errmsg(g_download_provider_db));
	__download_provider_db_close();
	return -1;
}

int download_provider_db_history_remove(int uniqueid)
{
	int errorcode;
	sqlite3_stmt *stmt;

	if (uniqueid <= 0) {
		TRACE_DEBUG_MSG("[NULL-CHECK]");
		return -1;
	}

	if (_download_provider_sql_open() < 0) {
		TRACE_DEBUG_MSG("db_util_open is failed [%s]",
				sqlite3_errmsg(g_download_provider_db));
		return -1;
	}

	errorcode =
		sqlite3_prepare_v2(g_download_provider_db,
					"delete from history where uniqueid = ?",
					-1, &stmt, NULL);
	if (errorcode != SQLITE_OK) {
		TRACE_DEBUG_MSG("sqlite3_prepare_v2 is failed. [%s]",
				sqlite3_errmsg(g_download_provider_db));
		_download_provider_sql_close(stmt);
		return -1;
	}
	if (sqlite3_bind_int(stmt, 1, uniqueid) != SQLITE_OK) {
		TRACE_DEBUG_MSG("sqlite3_bind_int is failed. [%s]",
				sqlite3_errmsg(g_download_provider_db));
		_download_provider_sql_close(stmt);
		return -1;
	}
	errorcode = sqlite3_step(stmt);
	if (errorcode == SQLITE_OK || errorcode == SQLITE_DONE) {
		_download_provider_sql_close(stmt);
		return 0;
	}
	TRACE_DEBUG_MSG("sqlite3_step is failed [%s]",
			sqlite3_errmsg(g_download_provider_db));
	__download_provider_db_close();
	return -1;
}

int download_provider_db_history_limit_rows()
{
	int errorcode;
	sqlite3_stmt *stmt;

	if (_download_provider_sql_open() < 0) {
		TRACE_DEBUG_MSG("db_util_open is failed [%s]",
				sqlite3_errmsg(g_download_provider_db));
		return -1;
	}

	errorcode =
		sqlite3_prepare_v2(g_download_provider_db,
					"DELETE FROM history where uniqueid NOT IN (SELECT uniqueid FROM history ORDER BY id DESC LIMIT ?)",
					-1, &stmt, NULL);
	if (errorcode != SQLITE_OK) {
		TRACE_DEBUG_MSG("sqlite3_prepare_v2 is failed. [%s]",
				sqlite3_errmsg(g_download_provider_db));
		_download_provider_sql_close(stmt);
		return -1;
	}
	if (sqlite3_bind_int(stmt, 1, DOWNLOAD_PROVIDER_HISTORY_DB_LIMIT_ROWS)
		!= SQLITE_OK) {
		TRACE_DEBUG_MSG("sqlite3_bind_int is failed. [%s]",
				sqlite3_errmsg(g_download_provider_db));
		_download_provider_sql_close(stmt);
		return -1;
	}
	errorcode = sqlite3_step(stmt);
	if (errorcode == SQLITE_OK || errorcode == SQLITE_DONE) {
		_download_provider_sql_close(stmt);
		return 0;
	}
	TRACE_DEBUG_MSG("sqlite3_step is failed [%s]",
			sqlite3_errmsg(g_download_provider_db));
	__download_provider_db_close();
	return -1;
}
