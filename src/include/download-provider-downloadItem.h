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
 * @file 	download-provider-downloadItem.h
 * @author	Jungki Kwak (jungki.kwak@samsung.com)
 * @brief	download item class
 */

#ifndef DOWNLOAD_PROVIDER_DOWNLOAD_ITEM_H
#define DOWNLOAD_PROVIDER_DOWNLOAD_ITEM_H

#include "download-provider-common.h"
#include "download-provider-downloadRequest.h"
#include "download-provider-event.h"
#include "download-agent-interface.h"
#include <memory>

namespace DL_ITEM {
enum STATE {
	IGNORE,
	STARTED,
	WAITING_CONFIRM,
	UPDATING,
	COMPLETE_DOWNLOAD,
	INSTALL_NOTIFY,
	START_DRM_DOMAIN,
	FINISH_DRM_DOMAIN,
	WAITING_RO,
	SUSPENDED,
	RESUMED,
	FINISHED,
	CANCELED,
	FAILED
};
}

class DownloadItem {
public:
	DownloadItem();	/* FIXME remove after cleanup ecore_pipe */
	DownloadItem(auto_ptr<DownloadRequest> request);
	~DownloadItem();

	void start(void);
	void cancel(void);
	void retry(void);
	bool sendUserResponse(bool res);
	void suspend(void);
	void resume(void);

	inline int downloadId(void) { return m_da_req_id;}
	inline void setDownloadId(int id) { m_da_req_id = id; }

	inline unsigned long int receivedFileSize(void) { return m_receivedFileSize; }
	inline void setReceivedFileSize(unsigned long int size) { m_receivedFileSize = size; }

	inline unsigned long int fileSize(void) { return m_fileSize; }
	inline void setFileSize(unsigned long int size) { m_fileSize = size; }

	inline string &filePath(void) { return m_filePath; }
	inline void setFilePath(const char *path) { if (path) m_filePath = path; }
	inline void setFilePath(string &path) { m_filePath = path; }

	inline string &registeredFilePath(void) { return m_registeredFilePath; }
	inline void setRegisteredFilePath(string &path) { m_registeredFilePath = path; }

	inline string &mimeType(void) { return m_mimeType; }
	inline void setMimeType(const char *mime) { m_mimeType = mime; }
	inline void setMimeType(string &mime) { m_mimeType = mime; }

	inline DL_ITEM::STATE state(void) { return m_state; }
	inline void setState(DL_ITEM::STATE state) { m_state = state; }

	inline ERROR::CODE errorCode(void) { return m_errorCode; }
	inline void setErrorCode(ERROR::CODE err) { m_errorCode = err;	}
	inline bool isMidletInstalled(void) { return m_isMidletInstalled; }
	inline void setIsMidletInstalled(bool b) { m_isMidletInstalled = b; }
	inline DL_TYPE::TYPE downloadType(void) { return m_downloadType; }
	inline void setDownloadType(DL_TYPE::TYPE t) { m_downloadType = t;}
	inline string &contentName(void) { return m_contentName; }
	inline void setContentName(string &name) { m_contentName = name; }
	inline string &vendorName(void) { return m_vendorName; }
	inline void setVendorName(string &name) { m_vendorName = name; }

	inline void notify(void) { m_subject.notify(); }
	inline void subscribe(Observer *o) { if (o) m_subject.attach(o); }
	inline void deSubscribe(Observer *o) { if (o) m_subject.detach(o); }
	inline string &url(void) { return m_aptr_request->getUrl(); }
	inline string &cookie(void) { return m_aptr_request->getCookie(); }

	DL_ITEM::STATE _convert_da_state_to_download_state(int da_state);
	ERROR::CODE _convert_da_error(int da_err);

private:
	auto_ptr<DownloadRequest> m_aptr_request;
	Subject m_subject;
	da_handle_t m_da_req_id;
	DL_ITEM::STATE m_state;
	ERROR::CODE m_errorCode;
	unsigned long int m_receivedFileSize;
	unsigned long int m_fileSize;
	string m_filePath;
	string m_registeredFilePath;
	string m_mimeType;
	DL_TYPE::TYPE m_downloadType;
	bool m_isMidletInstalled; /* For MIDP */
	string m_contentName; /* For OMA & MIDP */
	string m_vendorName; /* For MIDP */
};

class DownloadEngine {
public:
	static DownloadEngine &getInstance(void) {
		static DownloadEngine downloadEngine;
		return downloadEngine;
	}

	void initEngine(void);
	void deinitEngine(void);
private:
	DownloadEngine(void);
	~DownloadEngine(void);

};


#endif /* DOWNLOAD_PROVIDER_DOWNLOAD_ITEM_H */
