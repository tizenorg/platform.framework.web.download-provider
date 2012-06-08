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
 * @file	download-provider-downloadItem.cpp
 * @author	Jungki Kwak (jungki.kwak@samsung.com)
 * @brief	download item class for interface of download agent
 */

#include "download-provider-downloadItem.h"

#include "download-provider-common.h"
#include <Ecore.h>
#include <iostream>

/* Download Agent CallBacks */
static void __notify_cb(user_notify_info_t *notify_info, void *user_data);
static void __update_download_info_cb(user_download_info_t *download_info, void *user_data);
static void __get_dd_info_cb(user_dd_info_t *dd_info, void *user_data);

static Ecore_Pipe *ecore_pipe = NULL;
static void __ecore_cb_pipe_update(void *data, void *buffer, unsigned int nbyte);

namespace DA_CB {
enum TYPE {
	NOTIFY = 1,
	UPDATE,
	DD_INFO
};
}

class CbData {
public:
	CbData() {}
	~CbData() {}

	void updateDownloadItem();

	inline void setType(DA_CB::TYPE type) { m_type = type; }
	inline void setUserData(void *userData) { m_userData = userData; }
	inline void setDownloadId(int id) { m_da_req_id = id; }
	inline void setReceivedFileSize(unsigned long int size) { m_receivedFileSize = size; }
	inline void setFileSize(unsigned long int size) { m_fileSize = size; }
	inline void setFilePath(const char *path) { if (path) m_filePath = path; }
	inline void setRegisteredFilePath(const char *path) { if (path) m_registeredFilePath = path; }
	inline void setMimeType(const char *mime) { m_mimeType = mime; }
	inline void setState(da_state state) { m_da_state = state; }
	inline void setErrorCode(int err) { m_da_error = err;	}
	inline void setIsMidletInstalled(bool b) { m_isMidletInstalled = b; }
	inline void setIsJad(bool b) { m_isJad = b;}
	inline void setContentName(const char *name) { m_contentName = name; }
	inline void setVendorName(const char *name) { m_vendorName = name; }

private:
	DA_CB::TYPE m_type;
	void *m_userData;
	da_handle_t m_da_req_id;
	da_state m_da_state;
	int m_da_error;
	unsigned long int m_receivedFileSize;
	unsigned long int m_fileSize;
	string m_filePath;
	string m_registeredFilePath;
	string m_mimeType;
	bool m_isMidletInstalled;
	bool m_isJad;
	string m_contentName;
	string m_vendorName;
};

struct pipe_data_t {
	CbData *cbData;
};

DownloadEngine::DownloadEngine()
{
}

DownloadEngine::~DownloadEngine()
{
	DP_LOG_FUNC();
}

void DownloadEngine::initEngine(void)
{
	da_client_cb_t da_cb = {
		__notify_cb,	/* da_notify_cb */
		__get_dd_info_cb,	/* da_send_dd_info_cb */
		__update_download_info_cb,	/* da_update_download_info_cb */
		NULL	/* da_go_to_next_url_cb */
	};

	int da_ret = da_init(&da_cb, DA_DOWNLOAD_MANAGING_METHOD_AUTO);
	if (DA_RESULT_OK == da_ret) {
		ecore_pipe = ecore_pipe_add(__ecore_cb_pipe_update, NULL);
	}
}

void DownloadEngine::deinitEngine(void)
{
	da_deinit();
	if (ecore_pipe) {
		ecore_pipe_del(ecore_pipe);
		ecore_pipe = NULL;
	}
}

void CbData::updateDownloadItem()
{
//	DP_LOGD_FUNC();

	if (!m_userData) {
		DP_LOGE("download item is NULL");
		return;
	}
	

	DownloadItem *downloadItem = static_cast<DownloadItem*>(m_userData);
	if (downloadItem->state() == DL_ITEM::FAILED) {
		DP_LOGE("download item is already failed");
		return;
	}

	switch(m_type) {
	case DA_CB::NOTIFY:
	{
		DP_LOGD("DA_CB::NOTIFY");
		downloadItem->setDownloadId(m_da_req_id);
		DL_ITEM::STATE dl_state = downloadItem->_convert_da_state_to_download_state(m_da_state);
		DP_LOG("DL_ITEM::STATE [%d]", dl_state);
		//if (dl_state != DL_ITEM::IGNORE)
		if (dl_state == DL_ITEM::IGNORE)
			return;
		downloadItem->setState(dl_state);

		if (dl_state == DL_ITEM::FAILED)
			downloadItem->setErrorCode(downloadItem->_convert_da_error(m_da_error));
	}
		break;

	case DA_CB::UPDATE:
		DP_LOGD("DA_CB::UPDATE");
		DP_LOGD("DA_CB::UPDATE state[%d] receivedFileSize[%d]", downloadItem->state(), m_receivedFileSize);
//		downloadItem->setDownloadId(m_da_req_id);
		downloadItem->setState(DL_ITEM::UPDATING);
		downloadItem->setReceivedFileSize(m_receivedFileSize);
		downloadItem->setFileSize(m_fileSize);

		if (!m_mimeType.empty())
			downloadItem->setMimeType(m_mimeType);

		if (!m_filePath.empty())
			downloadItem->setFilePath(m_filePath);

		if (!m_registeredFilePath.empty()) {
			DP_LOGD("registeredFilePath[%s]", m_registeredFilePath.c_str());
			downloadItem->setRegisteredFilePath(m_registeredFilePath);
		}
		break;

	case DA_CB::DD_INFO:
		DP_LOG("DA_CB::DD_INFO");
		downloadItem->setDownloadId(m_da_req_id);
		downloadItem->setFileSize(m_fileSize);
		downloadItem->setIsMidletInstalled(m_isMidletInstalled);
		if (m_isJad)
			downloadItem->setDownloadType(DL_TYPE::MIDP_DOWNLOAD);
		else
			downloadItem->setDownloadType(DL_TYPE::OMA_DOWNLOAD);
		downloadItem->setContentName(m_contentName);
		downloadItem->setVendorName(m_vendorName);
		downloadItem->setState(DL_ITEM::WAITING_CONFIRM);
		break;

	default:
		break;
	}

	downloadItem->notify();
}

void __ecore_cb_pipe_update(void *data, void *buffer, unsigned int nbyte)
{
//	DP_LOGD_FUNC();

	if (!buffer)
		return;
	pipe_data_t *pipe_data = static_cast<pipe_data_t *>(buffer);
	CbData *cbData = pipe_data->cbData;
	if (!cbData)
		return;

	cbData->updateDownloadItem();
	delete cbData;
}

void __notify_cb(user_notify_info_t *notify_info, void* user_data)
{
	DP_LOGD_FUNC();

	if (!notify_info || !user_data)
		return;

	DP_LOGD("__notify_cb: id[%d] state[%d] err[%d]", notify_info->da_dl_req_id, notify_info->state, notify_info->err);

	CbData *cbData = new CbData();
	cbData->setType(DA_CB::NOTIFY);
	cbData->setDownloadId(notify_info->da_dl_req_id);
	cbData->setUserData(user_data);
	cbData->setState(notify_info->state);
	cbData->setErrorCode(notify_info->err);

	pipe_data_t pipe_data;
	pipe_data.cbData = cbData;
	ecore_pipe_write(ecore_pipe, &pipe_data, sizeof(pipe_data_t));
}

void __update_download_info_cb(user_download_info_t *download_info, void *user_data)
{
//	DP_LOGD_FUNC();

	if (!download_info || !user_data)
		return;

	CbData *cbData = new CbData();
	cbData->setType(DA_CB::UPDATE);
	cbData->setDownloadId(download_info->da_dl_req_id);
	cbData->setUserData(user_data);
	cbData->setFileSize(download_info->file_size);
	cbData->setReceivedFileSize(download_info->total_received_size);

//	DP_LOGD("tmp path [%s] path [%s] type [%s]", download_info->tmp_saved_path, download_info->saved_path, download_info->file_type);
	DownloadItem *item = static_cast<DownloadItem*>(user_data);
	if(download_info->file_type && item->mimeType().empty())
		cbData->setMimeType(download_info->file_type);

	if(download_info->tmp_saved_path && item->filePath().empty())
		cbData->setFilePath(download_info->tmp_saved_path);

	if(download_info->saved_path && item->registeredFilePath().empty())
		cbData->setRegisteredFilePath(download_info->saved_path);

	pipe_data_t pipe_data;
	pipe_data.cbData = cbData;
	ecore_pipe_write(ecore_pipe, &pipe_data, sizeof(pipe_data_t));
}

void __get_dd_info_cb(user_dd_info_t *dd_info, void *user_data)
{
	DP_LOGD_FUNC();

	if (!dd_info || !user_data)
		return;

	CbData *cbData = new CbData();
	cbData->setType(DA_CB::DD_INFO);
	cbData->setDownloadId(dd_info->da_dl_req_id);
	cbData->setUserData(user_data);
	cbData->setIsMidletInstalled(dd_info->is_installed);
	cbData->setIsJad(dd_info->is_jad);
	cbData->setFileSize(dd_info->size);

	if (dd_info->name)
		cbData->setContentName(dd_info->name);
	if (dd_info->vendor)
		cbData->setVendorName(dd_info->vendor);
	if (dd_info->type)
		cbData->setMimeType(dd_info->type);

	pipe_data_t pipe_data;
	pipe_data.cbData = cbData;
	ecore_pipe_write(ecore_pipe, &pipe_data, sizeof(pipe_data_t));
}

DownloadItem::DownloadItem()
	: m_da_req_id(DA_INVALID_ID)
	, m_state(DL_ITEM::IGNORE)
	, m_errorCode(ERROR::NONE)
	, m_receivedFileSize(0)
	, m_fileSize(0)
	, m_downloadType(DL_TYPE::HTTP_DOWNLOAD)
	, m_isMidletInstalled(false)
{
}

DownloadItem::DownloadItem(auto_ptr<DownloadRequest> request)
	: m_aptr_request(request)
	, m_da_req_id(DA_INVALID_ID)
	, m_state(DL_ITEM::IGNORE)
	, m_errorCode(ERROR::NONE)
	, m_receivedFileSize(0)
	, m_fileSize(0)
	, m_downloadType(DL_TYPE::HTTP_DOWNLOAD)
	, m_isMidletInstalled(false)
{
}

DownloadItem::~DownloadItem()
{
	DP_LOGD_FUNC();
}

void DownloadItem::start(void)
{
	int da_ret = 0;
	DP_LOGD_FUNC();

	if (m_aptr_request->getCookie().empty()) {
		da_ret = da_start_download_with_extension(
			m_aptr_request->getUrl().c_str(),
			&m_da_req_id,
			DA_FEATURE_USER_DATA,
			static_cast<void*>(this),
			NULL);
	} else {
		char *requestHeader[1] = {NULL,};
		int reqHeaderCount = 1;
		string reqHeaderStr = string("Cookie:");
		reqHeaderStr.append(m_aptr_request->getCookie().c_str());
		requestHeader[0] = strdup(reqHeaderStr.c_str());
		da_ret = da_start_download_with_extension(
			m_aptr_request->getUrl().c_str(),
			&m_da_req_id,
			DA_FEATURE_REQUEST_HEADER,
			requestHeader,
			&reqHeaderCount,
			DA_FEATURE_USER_DATA,
			static_cast<void*>(this),
			NULL);
		if (requestHeader[0]) {
			free(requestHeader[0]);
			requestHeader[0] = NULL;
		}
	}
	if (da_ret != DA_RESULT_OK) {
		m_state = DL_ITEM::FAILED;
		m_errorCode = ERROR::ENGINE_FAIL;
		notify();
	}
}

DL_ITEM::STATE DownloadItem::_convert_da_state_to_download_state(int da_state)
{
	switch (da_state) {
	case DA_STATE_DOWNLOAD_STARTED:
		return DL_ITEM::STARTED;
//	case DA_STATE_DOWNLOADING:
//		return DL_ITEM::UPDATING;
	case DA_STATE_DOWNLOAD_COMPLETE:
		return DL_ITEM::COMPLETE_DOWNLOAD;
	case DA_STATE_WAITING_USER_CONFIRM:
		return DL_ITEM::WAITING_CONFIRM;
	case DA_STATE_START_INSTALL_NOTIFY:
		return DL_ITEM::INSTALL_NOTIFY;
	case DA_STATE_DRM_JOIN_LEAVE_DOMAIN_START:
		return DL_ITEM::START_DRM_DOMAIN;
	case DA_STATE_DRM_JOIN_LEAVE_DOMAIN_FINISH:
		return DL_ITEM::FINISH_DRM_DOMAIN;
	case DA_STATE_WAITING_RO:
		return DL_ITEM::WAITING_RO;
	case DA_STATE_SUSPENDED:
		return DL_ITEM::SUSPENDED;
	case DA_STATE_RESUMED:
		return DL_ITEM::RESUMED;
	case DA_STATE_CANCELED:
		return DL_ITEM::CANCELED;
	case DA_STATE_FINISHED:
		return DL_ITEM::FINISHED;
	case DA_STATE_FAILED:
		return DL_ITEM::FAILED;
	case DA_STATE_DOWNLOADING:
	case DA_STATE_DONE_INSTALL_NOTIFY:
	case DA_STATE_DRM_ROAP_START:
	case DA_STATE_DRM_ROAP_FINISH:
	default:
		return DL_ITEM::IGNORE;
	}
}

ERROR::CODE DownloadItem::_convert_da_error(int da_err)
{
	DP_LOGD("download agent error[%d]", da_err);

	switch (da_err) {
	case DA_ERR_NETWORK_FAIL:
	case DA_ERR_HTTP_TIMEOUT:
	case DA_ERR_UNREACHABLE_SERVER :
		return ERROR::NETWORK_FAIL;

	case DA_ERR_INVALID_URL:
		return ERROR::INVALID_URL;

	case DA_ERR_DISK_FULL:
		return ERROR::NOT_ENOUGH_MEMORY;

	case DA_ERR_FAIL_TO_INSTALL_FILE:
	case DA_ERR_DLOPEN_FAIL:
		return ERROR::FAIL_TO_INSTALL;

	case DA_ERR_USER_RESPONSE_WAITING_TIME_OUT:
		return ERROR::OMA_POPUP_TIME_OUT;

	case DA_ERR_PARSE_FAIL:
		return ERROR::FAIL_TO_PARSE_DESCRIPTOR;

	default :
		return ERROR::UNKNOWN;
	}

}

void DownloadItem::cancel()
{
	DP_LOGD("DownloadItem::cancel");
	if (m_state == DL_ITEM::CANCELED) {
		DP_LOGD("It is already canceled");
		notify();
		return;
	}
	int da_ret = da_cancel_download(m_da_req_id);
	if (da_ret != DA_RESULT_OK) {
		DP_LOGE("Fail to cancel download : reqId[%d]  reason[%d]",
			m_da_req_id, da_ret);
		m_state = DL_ITEM::FAILED;
		m_errorCode = ERROR::ENGINE_FAIL;
		notify();
	}
	return;
}

void DownloadItem::retry()
{
	DP_LOGD_FUNC();
	m_da_req_id = DA_INVALID_ID;
	m_state = DL_ITEM::IGNORE;
	m_errorCode = ERROR::NONE;
	m_receivedFileSize = 0;
	m_fileSize = 0;
	m_downloadType = DL_TYPE::HTTP_DOWNLOAD;
	m_isMidletInstalled = false;
	start();
}

bool DownloadItem::sendUserResponse(bool res)
{
	int da_ret = da_send_back_user_confirm(m_da_req_id, res);
	if (da_ret != DA_RESULT_OK) {
		DP_LOGE("Fail to send back user confirm : reqId[%d]  reason[%d]",
			m_da_req_id, da_ret);
		m_state = DL_ITEM::FAILED;
		m_errorCode = ERROR::ENGINE_FAIL;
		notify();
		return false;
	}
	return true;
}

void DownloadItem::suspend()
{
	int da_ret = DA_RESULT_OK;
	da_ret = da_suspend_download(m_da_req_id);
	if (da_ret != DA_RESULT_OK) {
		DP_LOGE("Fail to suspend download : reqId[%d] err[%d]", m_da_req_id, da_ret);
		m_state = DL_ITEM::FAILED;
		m_errorCode = ERROR::ENGINE_FAIL;
		notify();
	}
}

void DownloadItem::resume()
{
	int da_ret = DA_RESULT_OK;
	da_ret = da_resume_download(m_da_req_id);
	if (da_ret != DA_RESULT_OK) {
		DP_LOGE("Fail to resume download : reqId[%d] err[%d]", m_da_req_id, da_ret);
		m_state = DL_ITEM::FAILED;
		m_errorCode = ERROR::ENGINE_FAIL;
		notify();
	}
}
