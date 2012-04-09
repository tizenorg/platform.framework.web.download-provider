/*
 * Copyright (c) 2000 - 2012 Samsung Electronics Co., Ltd All Rights Reserved 
 *
 * This file is part of Download Provider
 * Written by Jungki Kwak <jungki.kwak@samsung.com>
 *
 * PROPRIETARY/CONFIDENTIAL
 *
 * This software is the confidential and proprietary information of SAMSUNG ELECTRONICS
 * ("Confidential Information"). You shall not disclose such Confidential Information
 * and shall use it only in accordance with the terms of the license agreement
 * you entered into with SAMSUNG ELECTRONICS.
 *
 * SAMSUNG make no representations or warranties about the suitability of the software,
 * either express or implied, including but not limited to the implied warranties of merchantability,
 * fitness for a particular purpose, or non-infringement.
 *
 * SAMSUNG shall not be liable for any damages suffered by licensee as a result of using,
 * modifying or distributing this software or its derivatives.
 */
/*
 * @file	download-provider-util.h
 * @author	Jungki Kwak (jungki.kwak@samsung.com)
 * @brief   Utility APIs and interface with content player
 */

#ifndef DOWNLOAD_PROVIDER_UTIL_H
#define DOWNLOAD_PROVIDER_UTIL_H

#include <string>
#include "download-provider-common.h"

using namespace std;
class FileOpener {
public:
	FileOpener() {}
	~FileOpener() {}

	bool openFile(string &path, int contentType);
	bool openApp(string &pkgName);
};

class DownloadUtil
{
public:
	static DownloadUtil& getInstance(void) {
		static DownloadUtil inst;
		return inst;
	}

	string getMidletPkgName(string& name, string& vendor);
	bool checkJavaDRMContentType(const char *filePath);
	int getContentType(const char *mimem, const char *filePath);

private:
	DownloadUtil(void);
	~DownloadUtil(void) {}
	void initDrm(void);
	bool isAmbiguousMIMEType(const char *mimeType);
	bool getMimeFromDrmContent(const char *path, string& out_mimeType);
};

#endif//DOWNLOAD_PROVIDER_UTIL_H
