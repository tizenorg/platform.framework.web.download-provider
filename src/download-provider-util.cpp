/*
 * Copyright 2012  Samsung Electronics Co., Ltd
 *
 * Licensed under the Flora License, Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.tizenopensource.org/license
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * @file	download-provider-util.cpp
 * @author	Jungki Kwak (jungki.kwak@samsung.com)
 * @brief   Utility APIs and interface with content player
 */

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include "aul.h"
#include "xdgmime.h"
#include "app_service.h"

#include "download-provider-util.h"

#define JAVA_ENG_PATH "/usr/lib/libjava-installer.so"

struct MimeTableType
{
	const char *mime;
	int contentType;
};

#define MAX_MIME_TABLE_NUM 22
const char *ambiguousMIMETypeList[] = {
		"text/plain",
		"application/octet-stream"
};

struct MimeTableType MimeTable[]={
		// PDF
		{"application/pdf",DP_CONTENT_PDF},
		// word
		{"application/msword",DP_CONTENT_WORD},
		{"application/vnd.openxmlformats-officedocument.wordprocessingml.document",DP_CONTENT_WORD},
		// ppt
		{"application/vnd.ms-powerpoint",DP_CONTENT_PPT},
		{"application/vnd.openxmlformats-officedocument.presentationml.presentation",DP_CONTENT_PPT},
		// excel
		{"application/vnd.ms-excel",DP_CONTENT_EXCEL},
		{"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",DP_CONTENT_EXCEL},
		// html
		{"text/html",DP_CONTENT_HTML},
		// txt
		{"text/txt",DP_CONTENT_TEXT},
		{"text/palin",DP_CONTENT_TEXT},
		// ringtone
		{"text/x-iMelody",DP_CONTENT_RINGTONE},//10
		{"application/x-smaf",DP_CONTENT_RINGTONE},
		{"audio/midi",DP_CONTENT_RINGTONE},
		{"audio/AMR",DP_CONTENT_RINGTONE},
		{"audio/AMR-WB",DP_CONTENT_RINGTONE},
		{"audio/x-xmf",DP_CONTENT_RINGTONE},
		// DRM
		{"application/vnd.oma.drm.content",DP_CONTENT_DRM},
		{"application/vnd.oma.drm.message",DP_CONTENT_DRM},
		// JAVA
		{"application/x-java-archive",DP_CONTENT_JAVA},
		{"application/java-archive",DP_CONTENT_JAVA},
		// SVG
		{"image/svg+xml",DP_CONTENT_SVG}, //20
		// FLASH
		{"application/x-shockwave-flash",DP_CONTENT_FLASH}
};

bool FileOpener::openFile(string &path, int contentType)
{
	service_h handle = NULL;
	string filePath;
	DP_LOG_FUNC();

	if (path.empty())
		return false;

	DP_LOG("path [%s]", path.c_str());
	if (service_create(&handle) < 0) {
		DP_LOGE("Fail to create service handle");
		return false;
	}

	if (!handle) {
		DP_LOGE("service handle is null");
		return false;
	}

	if (service_set_operation(handle, SERVICE_OPERATION_VIEW) < 0) {
		DP_LOGE("Fail to set service operation");
		service_destroy(handle);
		return false;
	}

	if (contentType == DP_CONTENT_HTML) {
		filePath = "file://";
		filePath.append(path.c_str());	
	} else {
		filePath = path;
	}
	if (service_set_uri(handle, filePath.c_str()) < 0) {
		DP_LOGE("Fail to set uri");
		service_destroy(handle);
		return false;
	}

	if (service_send_launch_request(handle, NULL, NULL) < 0) {
		DP_LOGE("Fail to launch service");
		service_destroy(handle);
		return false;
	}

	service_destroy(handle);

	return true;
}

bool FileOpener::openApp(string &pkgName)
{
	service_h handle = NULL;
	DP_LOG_FUNC();
	if (pkgName.empty())
		return false;

	if (service_create(&handle) < 0) {
		DP_LOGE("Fail to create service handle");
		return false;
	}

	if (!handle) {
		DP_LOGE("service handle is null");
		return false;
	}

	if (service_set_package(handle, pkgName.c_str()) < 0) {
		DP_LOGE("Fail to set service operation");
		service_destroy(handle);
		return false;
	}

	if (service_send_launch_request(handle, NULL, NULL) < 0) {
		DP_LOGE("Fail to aul open");
		service_destroy(handle);
		return false;
	}

	service_destroy(handle);

	return true;
}


DownloadUtil::DownloadUtil()
{
	initDrm();
}

string DownloadUtil::getMidletPkgName(string& name, string& vendor)
{
	string pkgName = string();
	char appName[MAX_FILE_PATH_LEN] = {0,};
	void *handle = NULL;
	int (*java_get_pkgname )(char *,char *,char *) = NULL;

	if (name.length() < 1 || vendor.length() < 1)
		return pkgName;

	handle = dlopen(JAVA_ENG_PATH, RTLD_LAZY);
	if ( handle == NULL ) {
		DP_LOGE("dlopen error\n");
		if(handle)
			dlclose(handle);
		return pkgName;
	}

	java_get_pkgname = (int(*)(char *,char *,char *)) dlsym( handle, "jim_get_suite_pkgname_with_vendor_suite_name");
	if ( java_get_pkgname == NULL ) {
		DP_LOGE("dlopen error2\n");
		dlclose(handle);
		return pkgName;
	}
	DP_LOG("vendor name[%s] midlet name[%s]",vendor.c_str(),name.c_str());

	if (java_get_pkgname((char *)vendor.c_str(), (char *)name.c_str(), appName)) {
		DP_LOG("pkg name[%s]\n",appName);
		pkgName = appName;
	}
	dlclose(handle);
	return pkgName;
}

void DownloadUtil::initDrm()
{
	DP_LOG_FUNC();
}

int DownloadUtil::getContentType(const char *mime, const char *filePath)
{
	int i = 0;
	int type = DP_CONTENT_UNKOWN;
	int ret = 0;
	char tempMime[MAX_FILE_PATH_LEN] = {0,};
	DP_LOGD_FUNC();
	if (mime == NULL || strlen(mime) < 1)
		return DP_CONTENT_UNKOWN;

	DP_LOG("mime[%s]",mime);
	strncpy(tempMime, mime, MAX_FILE_PATH_LEN-1);
	if (isAmbiguousMIMEType(mime)) {
		if (filePath) {
			memset(tempMime, 0x00, MAX_FILE_PATH_LEN);
			ret = aul_get_mime_from_file(filePath,tempMime,sizeof(tempMime));
			if (ret < AUL_R_OK )
				strncpy(tempMime, mime, MAX_FILE_PATH_LEN-1);
			else
				DP_LOG("mime from extension name[%s]",tempMime);
		}
	}

	/* Search a content type from mime table. */
	for (i = 0; i < MAX_MIME_TABLE_NUM; i++)
	{
		//DP_LOG("TableMime[%d][%s]",i,MimeTable[i].mime);
		if (strncmp(MimeTable[i].mime, tempMime, strlen(tempMime)) == 0){
			type = MimeTable[i].contentType;
			break;
		}
	}
	/* If there is no mime at mime table, check the category with the first
	 *   domain of mime string
	 * ex) video/... => video type */
	if (type == DP_CONTENT_UNKOWN)
	{
		const char *unaliasedMime = NULL;
		/* unaliased_mimetype means representative mime among similar types */
		unaliasedMime = xdg_mime_unalias_mime_type(tempMime);

		if (unaliasedMime != NULL) {
			DP_LOG("unaliased mime type[%s]\n",unaliasedMime);
			if (strstr(unaliasedMime,"video/") != NULL)
				type = DP_CONTENT_VIDEO;
			else if (strstr(unaliasedMime,"audio/") != NULL)
				type = DP_CONTENT_MUSIC;
			else if (strstr(unaliasedMime,"image/") != NULL)
				type = DP_CONTENT_IMAGE;
		}
	}
	DP_LOG("type[%d]\n",type);
	return type;
}

bool DownloadUtil::isAmbiguousMIMEType(const char *mimeType)
{

	if (!mimeType)
		return false;

	int index = 0;
	int listSize = sizeof(ambiguousMIMETypeList) / sizeof(const char *);
	for (index = 0; index < listSize; index++) {
		if (0 == strncmp(mimeType, ambiguousMIMETypeList[index],
				strlen(ambiguousMIMETypeList[index]))) {
			DP_LOG("It is ambiguous! [%s]", ambiguousMIMETypeList[index]);
			return true;
		}
	}

	return false;
}

