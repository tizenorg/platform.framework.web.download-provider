#!/bin/sh

source /etc/tizen-platform.conf

databasefile=$TZ_USER_DB/.download-provider.db

sqlite3 $databasefile 'PRAGMA journal_mode=PERSIST;
PRAGMA foreign_keys=ON;
CREATE TABLE logging
(
id INTEGER UNIQUE PRIMARY KEY,
state INTEGER DEFAULT 0,
errorcode INTEGER DEFAULT 0,
startcount INTEGER DEFAULT 0,
packagename TEXT DEFAULT NULL,
createtime DATE,
accesstime DATE
);

CREATE TABLE requestinfo
(
id INTEGER UNIQUE PRIMARY KEY,
auto_download BOOLEAN DEFAULT 0,
state_event BOOLEAN DEFAULT 0,
progress_event BOOLEAN DEFAULT 0,
noti_enable BOOLEAN DEFAULT 0,
network_type TINYINT DEFAULT 0,
filename TEXT DEFAULT NULL,
destination TEXT DEFAULT NULL,
url TEXT DEFAULT NULL,
FOREIGN KEY(id) REFERENCES logging(id) ON DELETE CASCADE
);

CREATE TABLE downloadinfo
(
id INTEGER UNIQUE PRIMARY KEY,
http_status INTEGER DEFAULT 0,
content_size UNSIGNED BIG INT DEFAULT 0,
mimetype VARCHAR(64) DEFAULT NULL,
content_name TEXT DEFAULT NULL,
saved_path TEXT DEFAULT NULL,
tmp_saved_path TEXT DEFAULT NULL,
etag TEXT DEFAULT NULL,
FOREIGN KEY(id) REFERENCES logging(id) ON DELETE CASCADE
);

CREATE TABLE httpheaders
(
id INTEGER NOT NULL,
header_field TEXT DEFAULT NULL,
header_data TEXT DEFAULT NULL,
FOREIGN KEY(id) REFERENCES logging(id) ON DELETE CASCADE
);

CREATE TABLE notification
(
id INTEGER NOT NULL,
extra_key TEXT DEFAULT NULL,
extra_data TEXT DEFAULT NULL,
FOREIGN KEY(id) REFERENCES logging(id) ON DELETE CASCADE
);

CREATE UNIQUE INDEX requests_index ON logging (id, state, errorcode, packagename, createtime, accesstime);
'

chown root:$TZ_SYS_USER_GROUP $databasefile
chown root:$TZ_SYS_USER_GROUP $databasefile-journal
chmod 660 $databasefile
chmod 660 $databasefile-journal
