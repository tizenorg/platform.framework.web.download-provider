#!/bin/sh

PKG_NAME=download-provider

DUMP_DIR=$1/$PKG_NAME
/bin/mkdir -p $DUMP_DIR

# Download DB
DB_DIR=/opt/usr/data/download-provider/database
if [ "$DB_DIR" ]
then
	/bin/echo "copy download DB ..."
	/bin/cp -rf ${DB_DIR}* $DUMP_DIR
fi
