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
 * @file	download-provier-downloadRequest.cpp
 * @author	Jungki Kwak (jungki.kwak@samsung.com)
 * @brief	download request data class
 */
#include "download-provider-downloadRequest.h"

/*DownloadRequest::DownloadRequest()
	: m_url(NULL)
	, m_cookie(NULL)
{
}*/

DownloadRequest::DownloadRequest(string url, string cookie)
	: m_url(url)
	, m_cookie(cookie)
{
}

DownloadRequest::DownloadRequest(DownloadRequest &rRequest)
{
	m_url.assign(rRequest.getUrl());
	m_cookie.assign(rRequest.getCookie());
}

DownloadRequest::~DownloadRequest()
{
//	DP_LOG_FUNC();
}

string &DownloadRequest::getUrl()
{
	return m_url;
}

string &DownloadRequest::getCookie()
{
	return m_cookie;
}

bool DownloadRequest::isUrlEmpty()
{
	return m_url.empty();
}

bool DownloadRequest::isCookieEmpty()
{
	return m_cookie.empty();
}

void DownloadRequest::setUrl(string url)
{
	m_url.assign(url);
}

void DownloadRequest::setCookie(string cookie)
{
	m_cookie.assign(cookie);
}
