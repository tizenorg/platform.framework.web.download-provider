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
/**
 * @file 	download-provider-downloadRequest.h
 * @author	Jungki Kwak (jungki.kwak@samsung.com)
 * @brief	Download Request class
 */

#ifndef DOWNLOAD_PROVIDER_DOWNLOAD_REQUEST_H
#define DOWNLOAD_PROVIDER_DOWNLOAD_REQUEST_H

#include <string>

using namespace std;

class DownloadRequest
{
public:
//	DownloadRequest();
	DownloadRequest(string url, string cookie);
	DownloadRequest(DownloadRequest &rRequest);
	~DownloadRequest();

	string &getUrl();
	string &getCookie();
	bool isUrlEmpty();
	bool isCookieEmpty();
	void setUrl(string url);
	void setCookie(string cookie);
private:
	string m_url;
	string m_cookie;
};

#endif /* DOWNLOAD_PROVIDER_DOWNLOAD_REQUEST_H */
