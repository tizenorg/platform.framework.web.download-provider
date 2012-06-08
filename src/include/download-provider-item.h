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
 * @file 	download-provider-item.h
 * @author	Jungki Kwak (jungki.kwak@samsung.com)
 * @brief	Item class
 */
#ifndef DOWNLOAD_PROVIDER_ITEM_H
#define DOWNLOAD_PROVIDER_ITEM_H

#include <string>
#include <memory>
#include "download-provider-event.h"
#include "download-provider-downloadRequest.h"
#include "download-provider-downloadItem.h"
#include "download-provider-util.h"

using namespace std;

namespace ITEM {
enum STATE {
	IDLE,
	REQUESTING,
	PREPARE_TO_RETRY,
	WAITING_USER_RESPONSE,
	RECEIVING_DOWNLOAD_INFO,
	DOWNLOADING,
	REGISTERING_TO_SYSTEM,
	PROCESSING_DOMAIN,
	FINISH_PROCESSING_DOMAIN,
	ACTIVATING,
	NOTIFYING_TO_SERVER,
	SUSPEND,
	FINISH_DOWNLOAD,
	FAIL_TO_DOWNLOAD,
	CANCEL,
	PLAY,
	DESTROY
};
}

class DownloadNoti;

class Item {
public:
	static void create(DownloadRequest &rRequest);
	static Item *createHistoryItem(void);
	~Item(void);

	void attachHistoryItem(void);
	void destroy(void);
	/* SHOULD call this before destrying an item*/
	void deleteFromDB(void);
	void download(void);
	inline void cancel(void)
	{
		if (m_aptr_downloadItem.get())
			m_aptr_downloadItem->cancel();
		return;
	}
	void clearForRetry(void);
	bool retry(void);

	void sendUserResponse(bool res);
	bool play(void);

	inline void subscribe(Observer *o) { m_subjectForView.attach(o); }
	inline void deSubscribe(Observer *o) { m_subjectForView.detach(o); }

	static void updateCBForDownloadObserver(void *data);
	static void netEventCBObserver(void *data);
	void updateFromDownloadItem(void);
	inline void suspend(void) { m_aptr_downloadItem->suspend(); }

	inline int id(void) {
		if (m_aptr_downloadItem.get())
			return m_aptr_downloadItem->downloadId();

		return -1;
	} 	// FIXME create Item's own id

	inline unsigned long int receivedFileSize(void) {
		if (m_aptr_downloadItem.get())
			return m_aptr_downloadItem->receivedFileSize();
		return 0;
	}

	inline unsigned long int fileSize(void) {
		if (m_aptr_downloadItem.get())
			return m_aptr_downloadItem->fileSize();
		return 0;
	}

	inline string &filePath(void) {
		if (m_aptr_downloadItem.get())
			return m_aptr_downloadItem->filePath();
		return m_emptyString;
	}

	inline bool isMidletInstalled(void) {
		if (m_aptr_downloadItem.get())
			return m_aptr_downloadItem->isMidletInstalled();
		return false;
	}

	inline void setHistoryId(unsigned int i) { m_historyId = i; }
	inline unsigned int historyId(void) { return m_historyId; }	// FIXME duplicated with m_id
	inline string &title(void) {return m_title;}
	inline void setTitle(string &title) { m_title = title; }
	string &contentName(void);
	inline void setContentName(string &c) { m_contentName = c; }
	string &vendorName(void);
	inline void setVendorName(string &v) { m_vendorName = v; }
	string &registeredFilePath(void);
	inline void setRegisteredFilePath(string &r) { m_registeredFilePath = r; }
	string &url(void);
	string &cookie(void);
	void setRetryData(string &url, string &cookie);
	int contentType(void) { return m_contentType; }
	inline void setContentType(int t) { m_contentType = t; }
	DL_TYPE::TYPE downloadType(void);
	inline void setDownloadType(DL_TYPE::TYPE t) { m_downloadType = t; }

//	string &getIconPath(void) {return m_iconPath; }
	inline string &iconPath(void) { return m_iconPath; }

	inline void setState(ITEM::STATE state) { m_state = state; }
	inline ITEM::STATE state(void) { return m_state; }

	inline void setErrorCode(ERROR::CODE err) { m_errorCode = err; }
	inline ERROR::CODE errorCode(void) { return m_errorCode; }
	const char *getErrorMessage(void);
	inline void setFinishedTime(double t) { m_finishedTime = t; }
	inline double finishedTime(void) { return m_finishedTime; }

	inline string getMidletPkgName(void) {
		DownloadUtil &util = DownloadUtil::getInstance();
		return util.getMidletPkgName(m_contentName, m_vendorName);
	}

	bool isFinished(void); /* include finish download state with error */
	bool isFinishedWithErr(void);
	bool isPreparingDownload(void); /* Before downloading start */
	bool isCompletedDownload(void); /* After stating installation */

	/* Test code */
	const char *stateStr(void);

private:
	Item(void);
	Item(DownloadRequest &rRequest);

	inline void notify(void) { m_subjectForView.notify(); }

	void createSubscribeData(void);
	void extractTitle(void);
	void extractIconPath(void);

	void startUpdate(void);
	void createHistoryId(void);
	bool isExistedHistoryId(unsigned int id);
	void handleFinishedItem(void);

	auto_ptr<DownloadRequest> m_aptr_request;
	auto_ptr<DownloadItem> m_aptr_downloadItem;
	FileOpener m_fileOpener;

	Subject m_subjectForView;
	auto_ptr<Observer> m_aptr_downloadObserver;
	auto_ptr<Observer> m_aptr_netEventObserver;

	ITEM::STATE m_state;
	ERROR::CODE m_errorCode;
	string m_title;
	unsigned int m_historyId;
	int m_contentType;
	string m_iconPath; // FIXME Later:is it right to exist here? (ViewItem or not)
	string m_emptyString; // FIXME this is temporary to avoid crash when filePath() is called if m_aptr_downloaditem points nothing
	double m_finishedTime;
	DL_TYPE::TYPE m_downloadType;
	string m_contentName;
	string m_vendorName;
	string m_registeredFilePath;
	string m_url;
	string m_cookie;

	bool m_gotFirstData;
};

#endif /* DOWNLOAD_PROVIDER_ITEM_H */
