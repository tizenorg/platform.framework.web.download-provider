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
