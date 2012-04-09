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
 * @file 	download-provider-view.h
 * @author	Jungki Kwak (jungki.kwak@samsung.com)
 * @brief	Download UI View manager
 */

#ifndef DOWNLOAD_PROVIDER_VIEW_H
#define DOWNLOAD_PROVIDER_VIEW_H

#include <Elementary.h>
#include <libintl.h>

#include <vector>
#include "download-provider-common.h"
#include "download-provider-viewItem.h"
#include "download-provider-dateTime.h"

enum {
	POPUP_EVENT_EXIT = 0,
	POPUP_EVENT_ERR,
};

class DownloadView {
public:
	static DownloadView& getInstance(void) {
		static DownloadView inst;
		return inst;
	}

	Evas_Object *create(void);
	void createView(void);
	void activateWindow(void);
	void show(void);
	void hide(void);

	void attachViewItem(ViewItem *viewItem);
	void detachViewItem(ViewItem *viewItem);

	void changedRegion(void);
	void showErrPopup(string &desc);
	void showOMAPopup(string &msg, ViewItem *viewItem);
	void update();
	void update(ViewItem *viewItem);
	void update(Elm_Object_Item *glItem);
	void showViewItem(int id, const char *title);
	void playContent(int id, const char *title);
	void handleChangedAllCheckedState(void);
	void handleCheckedState(void);
	bool isGenlistEditMode(void);
	void handleGenlistGroupItem(int type);
	void moveRetryItem(ViewItem *viewItem);
	static char *getGenlistGroupLabelCB(void *data, Evas_Object *obj,
		const char *part);

private:
	static void showNotifyInfoCB(void *data, Evas *evas, Evas_Object *obj, void *event);
	static void hideNotifyInfoCB(void *data, Evas *evas, Evas_Object *obj, void *event);
	static void backBtnCB(void *data, Evas_Object *obj, void *event_info);
	static void cbItemDeleteCB(void *data, Evas_Object *obj, void *event_info);
	static void cbItemCancelCB(void *data, Evas_Object *obj, void *event_info);
	static void selectAllClickedCB(void *data, Evas *evas, Evas_Object *obj,
		void *event_info);
	static void selectAllChangedCB(void *data, Evas_Object *obj,
		void *event_info);
	static void genlistClickCB(void *data, Evas_Object *obj, void *event_info);
	static void cancelClickCB(void *data, Evas_Object *obj, void *event_info);
	static void errPopupResponseCB(void *data, Evas_Object *obj, void *event_info);
	static void omaPopupResponseOKCB(void *data, Evas_Object *obj, void *event_info);
	static void omaPopupResponseCancelCB(void *data, Evas_Object *obj, void *event_info);

private:
	DownloadView();
	~DownloadView();

	inline void destroyEvasObj(Evas_Object *e) { if(e) evas_object_del(e); e = NULL;}
	void setTheme(void);
	void setIndicator(Evas_Object *window);
	Evas_Object *createWindow(const char *windowName);
	Evas_Object *createBackground(Evas_Object *window);
	Evas_Object *createLayout(Evas_Object *parent);
	void createTheme(void);
	void createNaviBar(void);
	void createBackBtn(void);
	void createControlBar(void);
	void createBox(void);
	void createList(void);

	void removeTheme(void);

	void addViewItemToGenlist(ViewItem *viewItem);
	void createGenlistItem(ViewItem *viewItem);
	void showEmptyView(void);
	void hideEmptyView(void);

	void removePopup(void);
	void showGenlistEditMode(void);
	void hideGenlistEditMode(void);
	void createSelectAllLayout(void);
	void changeAllCheckedValue(void);
	void destroyCheckedItem(void);
	void showNotifyInfo(int type, int selectedCount);
	void destroyNotifyInfo(void);
	void createNotifyInfo(void);

	DateGroup *getDateGroupObj(int type);
	Elm_Object_Item *getGenlistGroupItem(int type);
	int getGenlistGroupCount(int type);
	void setGenlistGroupItem(int type, Elm_Object_Item *item);
	void increaseGenlistGroupCount(int type);
	void handleUpdateDateGroupType(ViewItem *viewItem);
	void cleanGenlistData();
	char *getGenlistGroupLabel(void *data, Evas_Object *obj, const char *part);

	Evas_Object *eoWindow;
	Evas_Object *eoBackground;
	Evas_Object *eoLayout;
	Elm_Theme *eoTheme;

	Evas_Object *eoEmptyNoContent;
	Evas_Object *eoNaviBar;
	Elm_Object_Item *eoNaviBarItem;
	Evas_Object *eoBackBtn;
	Evas_Object *eoControlBar;
	Elm_Object_Item *eoCbItemDelete;
	Elm_Object_Item *eoCbItemCancel;
	Elm_Object_Item *eoCbItemEmpty;
	Evas_Object *eoBoxLayout;
	Evas_Object *eoBox;
	Evas_Object *eoDldList;
	Evas_Object *eoPopup;
	Evas_Object *eoSelectAllLayout;
	Evas_Object *eoAllCheckedBox;
	Evas_Object *eoNotifyInfo;
	Evas_Object *eoNotifyInfoLayout;
	Elm_Genlist_Item_Class dldGenlistGroupStyle;
	Eina_Bool m_allChecked;

	int m_viewItemCount;
	DateGroup m_today;
	DateGroup m_yesterday;
	DateGroup m_previousDay;
};

#endif /* DOWNLOAD_PROVIDER_VIEW_H */
