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
 * @file	download-provider-view.cpp
 * @author	Jungki Kwak (jungki.kwak@samsung.com)
 * @brief	UI manager class for download list view and delete view
 */
#include <sstream>
#include <queue>
#include "download-provider-view.h"
#include "download-provider-history-db.h"
#include "download-provider-downloadItem.h"

static void destroy_window_cb(void *data, Evas_Object *obj, void *event);

enum {
	DOWNLOAD_NOTIFY_SELECTED,
	DOWNLOAD_NOTIFY_DELETED
};

DownloadView::DownloadView(void)
	: eoWindow(NULL)
	, eoBackground(NULL)
	, eoLayout(NULL)
	, eoNaviBar(NULL)
	, eoNaviBarItem(NULL)
	, eoBackBtn(NULL)
	, eoControlBar(NULL)
	, eoCbItemDelete(NULL)
	, eoCbItemCancel(NULL)
	, eoCbItemEmpty(NULL)
	, eoBoxLayout(NULL)
	, eoBox(NULL)
	, eoDldList(NULL)
	, eoPopup(NULL)
	, eoSelectAllLayout(NULL)
	, eoAllCheckedBox(NULL)
	, eoNotifyInfo(NULL)
	, eoNotifyInfoLayout(NULL)
	, m_allChecked(EINA_FALSE)
	, m_viewItemCount(0)
{
// FIXME Later : init private members
	DownloadEngine &engine = DownloadEngine::getInstance();
	engine.initEngine();
	DateUtil &inst = DateUtil::getInstance();
	inst.updateLocale();
	dldGenlistGroupStyle.item_style = "grouptitle";
	dldGenlistGroupStyle.func.text_get = getGenlistGroupLabelCB;
	dldGenlistGroupStyle.func.content_get = NULL;
	dldGenlistGroupStyle.func.state_get = NULL;
	dldGenlistGroupStyle.func.del = NULL;
	
	m_today.setType(DATETIME::DATE_TYPE_TODAY);
	m_yesterday.setType(DATETIME::DATE_TYPE_YESTERDAY);
	m_previousDay.setType(DATETIME::DATE_TYPE_PREVIOUS);
}

DownloadView::~DownloadView()
{
	DP_LOG_FUNC();
	DownloadEngine &engine = DownloadEngine::getInstance();
	engine.deinitEngine();
}

Evas_Object *DownloadView::create(void)
{
	Evas_Object *window = NULL;
	window = createWindow(PACKAGE);
	if (!window)
		return NULL;

	createBackground(window);
	createLayout(window);
	setIndicator(window);
	createView();

	return window;
}

void DownloadView::show()
{
	DP_LOG_FUNC();
	elm_win_raise(eoWindow);
	handleUpdateDateGroupType(NULL);
}

void DownloadView::hide()
{
	DP_LOG_FUNC();
	removePopup();
	destroyNotifyInfo();
	if (isGenlistEditMode()) {
		hideGenlistEditMode();
	}
	elm_win_lower(eoWindow);
}

void DownloadView::activateWindow()
{
	if (!eoWindow)
		create();

	show();
}

void DownloadView::showViewItem(int id, const char *title)
{
	DP_LOG_FUNC();
}

/* This is called by AUL view mode */
void DownloadView::playContent(int id, const char *title)
{
	DP_LOG_FUNC();
}

void DownloadView::setIndicator(Evas_Object *window)
{
	elm_win_indicator_mode_set(window, ELM_WIN_INDICATOR_SHOW);
}

Evas_Object *DownloadView::createWindow(const char *windowName)
{
	eoWindow = elm_win_add(NULL, windowName, ELM_WIN_BASIC);
	if (eoWindow) {
		elm_win_title_set(eoWindow, __("IDS_BR_HEADER_DOWNLOAD_MANAGER"));
		elm_win_borderless_set(eoWindow, EINA_TRUE);
		elm_win_conformant_set(eoWindow, 1);
		evas_object_smart_callback_add(eoWindow, "delete,request",
				destroy_window_cb,	static_cast<void*>(this));
	}
	return eoWindow;
}

Evas_Object *DownloadView::createBackground(Evas_Object *window)
{
	if (!window)
		return NULL;

	eoBackground = elm_bg_add(window);
	if (eoBackground) {
		evas_object_size_hint_weight_set(eoBackground, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
		elm_win_resize_object_add(window, eoBackground);
		evas_object_show(eoBackground);
	} else {
		DP_LOGE("Fail to create bg object");
	}
	return eoBackground;
}

Evas_Object *DownloadView::createLayout(Evas_Object *parent)
{
	if (!parent) {
		DP_LOGE("Invalid Paramter");
		return NULL;
	}

	eoLayout = elm_layout_add(parent);
	if (eoLayout) {
		if (!elm_layout_theme_set(eoLayout, "layout", "application", "default" ))
			DP_LOGE("Fail to set elm_layout_theme_set");

		evas_object_size_hint_weight_set(eoLayout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
		elm_win_resize_object_add(parent, eoLayout);

		edje_object_signal_emit(elm_layout_edje_get(eoLayout), "elm,state,show,indicator", "elm");
		evas_object_show(eoLayout);
	} else {
		DP_LOGE("Fail to create layout");
	}
	return eoLayout;
}

void DownloadView::createView()
{
	DP_LOG_FUNC();
	createNaviBar();
	createList();
	if (m_viewItemCount < 1)
		showEmptyView();
}

void DownloadView::createNaviBar()
{
	DP_LOGD_FUNC();
	eoNaviBar = elm_naviframe_add(eoLayout);
	elm_object_part_content_set(eoLayout, "elm.swallow.content", eoNaviBar);
	createBackBtn();
	createBox();
	eoNaviBarItem = elm_naviframe_item_push(eoNaviBar,
		__("IDS_BR_HEADER_DOWNLOAD_MANAGER"),eoBackBtn, NULL, eoBoxLayout, NULL);
	createControlBar();
	evas_object_show(eoNaviBar);

}

void DownloadView::createBackBtn()
{
	DP_LOGD_FUNC();
	eoBackBtn = elm_button_add(eoNaviBar);
	elm_object_style_set(eoBackBtn, "naviframe/end_btn/default");
	evas_object_smart_callback_add(eoBackBtn, "clicked", backBtnCB,NULL);
	evas_object_show(eoBackBtn);
}

void DownloadView::createControlBar()
{
	DP_LOGD_FUNC();

	eoControlBar = elm_toolbar_add(eoNaviBar);
	if (eoControlBar == NULL)
		return;
	elm_toolbar_shrink_mode_set(eoControlBar, ELM_TOOLBAR_SHRINK_EXPAND);
	eoCbItemDelete = elm_toolbar_item_append(eoControlBar, NULL,
		S_("IDS_COM_OPT_DELETE"), cbItemDeleteCB, eoNaviBar);
	eoCbItemEmpty = elm_toolbar_item_append(eoControlBar, NULL, NULL, NULL, NULL);
	elm_object_item_part_content_set(eoNaviBarItem, "controlbar",
		eoControlBar);
	elm_object_item_disabled_set(eoCbItemDelete, EINA_TRUE);
	elm_object_item_disabled_set(eoCbItemEmpty, EINA_TRUE);
	evas_object_show(eoControlBar);
}

void DownloadView::createBox()
{
	DP_LOGD_FUNC();
	eoBoxLayout = elm_layout_add(eoNaviBar);
	elm_layout_theme_set(eoBoxLayout, "layout", "application", "noindicator");
	evas_object_size_hint_weight_set(eoBoxLayout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

	eoBox = elm_box_add(eoBoxLayout);
	elm_object_part_content_set(eoBoxLayout, "elm.swallow.content", eoBox );

	evas_object_show(eoBox);
	evas_object_show(eoBoxLayout);
}

void DownloadView::createList()
{
	//DP_LOGD_FUNC();
	eoDldList = elm_genlist_add(eoBoxLayout);
	DP_LOGD("create::eoDldList[%p]",eoDldList);
/* When using ELM_LIST_LIMIT, the window size is broken at the landscape mode */
	evas_object_size_hint_weight_set(eoDldList, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(eoDldList, EVAS_HINT_FILL, EVAS_HINT_FILL);
	elm_genlist_homogeneous_set(eoDldList, EINA_TRUE);
	elm_genlist_block_count_set(eoDldList,8);

	elm_box_pack_end(eoBox, eoDldList);
	evas_object_show(eoDldList);
}

void destroy_window_cb(void *data, Evas_Object *obj, void *event)
{
	DP_LOG_FUNC();
	elm_exit();
}

void DownloadView::changedRegion()
{
	DateUtil &inst = DateUtil::getInstance();
	inst.updateLocale();
	elm_genlist_realized_items_update(eoDldList);
}

void DownloadView::attachViewItem(ViewItem *viewItem)
{
	DP_LOG_FUNC();
	if (m_viewItemCount < 1) {
		hideEmptyView();
		createList();
	}
	if (viewItem) {
		addViewItemToGenlist(viewItem);
		m_viewItemCount++;
	}
}

void DownloadView::detachViewItem(ViewItem *viewItem)
{
	DP_LOG("delete viewItem[%p]",viewItem);
	if (viewItem) {
		delete viewItem;
		m_viewItemCount--;
	}
	if (!isGenlistEditMode() &&
			(m_viewItemCount < 1))
		showEmptyView();
}

void DownloadView::update()
{
	Elm_Object_Item *it = NULL;
	DP_LOG_FUNC();
	if (!eoDldList) {
		DP_LOGE("download list is NULL");
		return;
	}
	it = elm_genlist_first_item_get(eoDldList);
	while (it) {
		DP_LOGD("glItem[%p]",it);
		elm_genlist_item_update(it);
		it = elm_genlist_item_next_get(it);
	}
}

void DownloadView::update(ViewItem *viewItem)
{
	if (!viewItem)
		return;

	DP_LOG("DownloadView::update viewItem [%p]", viewItem);
	elm_genlist_item_update(viewItem->genlistItem());
}

void DownloadView::update(Elm_Object_Item *glItem)
{
	if (!glItem)
		return;

	DP_LOG("DownloadView::update glItem [%p]", glItem);
	elm_genlist_item_update(glItem);
}

void DownloadView::addViewItemToGenlist(ViewItem *viewItem)
{
	DP_LOG_FUNC();
	handleUpdateDateGroupType(viewItem);
	createGenlistItem(viewItem);
}

void DownloadView::createGenlistItem(ViewItem *viewItem)
{
	Elm_Object_Item *glItem = NULL;
	Elm_Object_Item *glGroupItem = NULL;
	/* EAPI Elm_Object_Item *elm_genlist_item_prepend(
	 * 	Evas_Object *obj,
	 * 	const Elm_Genlist_Item_Class *itc,
	 * 	const void *data,
	 * 	Elm_Object_Item *parent,
	 * 	Elm_Genlist_Item_Type flags,
	 * 	Evas_Smart_Cb func,
	 * 	const void *func_data) EINA_ARG_NONNULL(1); */
	glGroupItem = getGenlistGroupItem(viewItem->dateGroupType());
	DP_LOGD("group item[%p]",glGroupItem);
	if (!glGroupItem) {
		DateGroup *dateGrpObj = getDateGroupObj(viewItem->dateGroupType());
		if (!viewItem->isFinished()) {
			glGroupItem = elm_genlist_item_prepend(
				eoDldList,
				&dldGenlistGroupStyle,
				static_cast<const void*>(dateGrpObj),
				NULL,
				ELM_GENLIST_ITEM_GROUP,
				NULL,
				NULL);
		} else {
			/* Download History Item */
			glGroupItem = elm_genlist_item_append(
				eoDldList,
				&dldGenlistGroupStyle,
				static_cast<const void*>(dateGrpObj),
				NULL,
				ELM_GENLIST_ITEM_GROUP,
				NULL,
				NULL);
		}
		if (!glGroupItem)
			DP_LOGE("Fail to add a genlist group item");
		else
			elm_genlist_item_select_mode_set(glGroupItem,
				ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);
		setGenlistGroupItem(viewItem->dateGroupType(), glGroupItem);
	}
	increaseGenlistGroupCount(viewItem->dateGroupType());
	if (!viewItem->isFinished()) {
		glItem = elm_genlist_item_insert_after(
			eoDldList,
			viewItem->elmGenlistStyle(),
			static_cast<const void*>(viewItem),
			glGroupItem,
			glGroupItem,
			ELM_GENLIST_ITEM_NONE,
			genlistClickCB,
			static_cast<const void*>(viewItem));
	} else {
		/* Download History Item */
		glItem = elm_genlist_item_append(
			eoDldList,
			viewItem->elmGenlistStyle(),
			static_cast<const void*>(viewItem),
			glGroupItem,
			ELM_GENLIST_ITEM_NONE,
			genlistClickCB,
			static_cast<const void*>(viewItem));
	}
	if (!glItem)
		DP_LOGE("Fail to add a genlist item");

	DP_LOGD("genlist groupItem[%p] item[%p] viewItem[%p]", glGroupItem, glItem, viewItem);
	viewItem->setGenlistItem(glItem);
	/* Move scrollbar to top.
	 * When groupItem means today group in case of addtion of download link item
	**/
	if (!viewItem->isFinished())
		elm_genlist_item_show(glGroupItem, ELM_GENLIST_ITEM_SCROLLTO_TOP);

}

void DownloadView::showEmptyView()
{
	DP_LOGD_FUNC();
	if (!eoEmptyNoContent) {
		eoEmptyNoContent = elm_layout_add(eoLayout);
		elm_layout_theme_set(eoEmptyNoContent, "layout", "nocontents", "text");
		evas_object_size_hint_weight_set(eoEmptyNoContent, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
		evas_object_size_hint_align_set(eoEmptyNoContent, EVAS_HINT_FILL, EVAS_HINT_FILL);
		elm_object_part_text_set(eoEmptyNoContent, "elm.text",
			__("IDS_DL_BODY_NO_DOWNLOADS"));
		evas_object_size_hint_weight_set (eoEmptyNoContent,
			EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

		if (eoDldList) {
			elm_box_unpack(eoBox,eoDldList);
			/* Detection code */
			DP_LOGD("del::eoDldList[%p]",eoDldList);
			evas_object_del(eoDldList);
			eoDldList = NULL;
		}
		elm_box_pack_start(eoBox, eoEmptyNoContent);
	}
	evas_object_show(eoEmptyNoContent);
	elm_object_item_disabled_set(eoCbItemDelete, EINA_TRUE);
}

void DownloadView::hideEmptyView()
{
	DP_LOGD_FUNC();
	if(eoEmptyNoContent) {
		elm_box_unpack(eoBox, eoEmptyNoContent);
		evas_object_del(eoEmptyNoContent);
		eoEmptyNoContent = NULL;
	}
	elm_object_item_disabled_set(eoCbItemDelete, EINA_FALSE);
}

bool DownloadView::isGenlistEditMode()
{
	return (bool)elm_genlist_decorate_mode_get(eoDldList);
}

void DownloadView::createSelectAllLayout()
{
	eoSelectAllLayout = elm_layout_add(eoBox);
	elm_layout_theme_set(eoSelectAllLayout, "genlist", "item",
		"select_all/default");
	evas_object_size_hint_weight_set(eoSelectAllLayout, EVAS_HINT_EXPAND,
		EVAS_HINT_FILL);
	evas_object_size_hint_align_set(eoSelectAllLayout, EVAS_HINT_FILL,
		EVAS_HINT_FILL);
	evas_object_event_callback_add(eoSelectAllLayout, EVAS_CALLBACK_MOUSE_DOWN,
		selectAllClickedCB, NULL);
	eoAllCheckedBox = elm_check_add(eoSelectAllLayout);
	elm_check_state_pointer_set(eoAllCheckedBox, &m_allChecked);
	evas_object_smart_callback_add(eoAllCheckedBox, "changed",
		selectAllChangedCB, NULL);
	evas_object_propagate_events_set(eoAllCheckedBox, EINA_FALSE);
	elm_object_part_content_set(eoSelectAllLayout, "elm.icon", eoAllCheckedBox);
	elm_object_text_set(eoSelectAllLayout, S_("IDS_COM_BODY_SELECT_ALL"));
	elm_box_pack_start(eoBox, eoSelectAllLayout);
	evas_object_show(eoSelectAllLayout);
	m_allChecked = EINA_FALSE;
}

void DownloadView::changeAllCheckedValue()
{
	m_allChecked = !m_allChecked;
	elm_check_state_pointer_set(eoAllCheckedBox, &m_allChecked);
	handleChangedAllCheckedState();
}

void DownloadView::destroyCheckedItem()
{
	Eina_List *list = NULL;
	Elm_Object_Item *it = NULL;
	ViewItem *item = NULL;
	int checkedCount = 0;
	queue <unsigned int> deleteQueue;

	DP_LOGD_FUNC();

	it = elm_genlist_first_item_get(eoDldList);
	
	while (it) {
		item = (ViewItem *)elm_object_item_data_get(it);
		/* elm_genlist_item_select_mode_get is needed to check group item */
		if (elm_genlist_item_select_mode_get(it) !=
				ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY &&
				item && item->checkedValue()) {
			list = eina_list_append(list, it);
		}
		it = elm_genlist_item_next_get(it);
	}

	if (!list) {
		DP_LOGD("There is no delete item");
		return;
	}

	checkedCount = eina_list_count(list);
	if (checkedCount < 1)
		return;
	DP_LOGD("checkedCount[%d]", checkedCount);

	for (int i = 0; i < checkedCount; i++)
	{
		it = (Elm_Object_Item *)eina_list_data_get(list);
		if (it)
			item = (ViewItem *)elm_object_item_data_get(it);
		else
			DP_LOGE("genlist item is null");
		list = eina_list_next(list);
		if (item) {
			deleteQueue.push(item->historyId());
			item->destroy();
		} else {
			DP_LOGE("viewItem is null");
		}
	}
	if (list)
		eina_list_free(list);

	DownloadHistoryDB::deleteMultipleItem(deleteQueue);
	showNotifyInfo(DOWNLOAD_NOTIFY_DELETED, checkedCount);
	hideGenlistEditMode();
}

void DownloadView::showGenlistEditMode()
{
	DP_LOG_FUNC();
	/* Initialize notify info widget */
	destroyNotifyInfo();
	elm_object_item_text_set(eoNaviBarItem, S_("IDS_COM_OPT_DELETE"));
	/* Change layoutbackground color to edit mode color */
	elm_object_style_set(eoBackground, "edit_mode");
	/* Disable the back button of control bar */
	elm_object_item_part_content_unset(eoNaviBarItem, "prev_btn");
	destroyEvasObj(eoBackBtn);

	if (eoCbItemEmpty)
		elm_object_item_del(eoCbItemEmpty);
	eoCbItemCancel = elm_toolbar_item_append(eoControlBar, NULL,
		S_("IDS_COM_SK_CANCEL"), cbItemCancelCB, eoNaviBar);

	/* Append 'Select All' layout */
	createSelectAllLayout();
	/* Set reorder end edit mode */
	elm_genlist_reorder_mode_set(eoDldList, EINA_TRUE);
	elm_genlist_decorate_mode_set(eoDldList, EINA_TRUE);
	/* This means even if the ouside of checked box is selected,
	   it is same to click a check box. */
	elm_genlist_select_mode_set(eoDldList, ELM_OBJECT_SELECT_MODE_ALWAYS);

	Elm_Object_Item *it = NULL;
	ViewItem *viewItem = NULL;
	it = elm_genlist_first_item_get(eoDldList);
	while (it) {
		viewItem = (ViewItem *)elm_object_item_data_get(it);
		if (elm_genlist_item_select_mode_get(it) !=
				ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY &&
				viewItem && !(viewItem->isFinished()))
			elm_object_item_disabled_set(it, EINA_TRUE);
		it = elm_genlist_item_next_get(it);
	}
	elm_object_item_disabled_set(eoCbItemDelete, EINA_TRUE);
}

void DownloadView::hideGenlistEditMode()
{
	DP_LOG_FUNC();

	elm_object_item_text_set(eoNaviBarItem, __("IDS_BR_HEADER_DOWNLOAD_MANAGER"));
	elm_object_style_set(eoBackground, "default");

	/* Recreate back button */
	createBackBtn();
	elm_object_item_part_content_set(eoNaviBarItem, "prev_btn", eoBackBtn);

	if (eoCbItemCancel) {
		elm_object_item_del(eoCbItemCancel);
		eoCbItemCancel = NULL;
	}
	eoCbItemEmpty = elm_toolbar_item_append(eoControlBar, NULL, NULL, NULL, NULL);
	elm_object_item_disabled_set(eoCbItemEmpty, EINA_TRUE);

	elm_box_unpack(eoBox, eoSelectAllLayout);

	destroyEvasObj(eoAllCheckedBox);
	destroyEvasObj(eoSelectAllLayout);

	elm_genlist_reorder_mode_set(eoDldList, EINA_FALSE);
	elm_genlist_decorate_mode_set(eoDldList, EINA_FALSE);
	elm_genlist_select_mode_set(eoDldList, ELM_OBJECT_SELECT_MODE_DEFAULT);

	Elm_Object_Item *it = NULL;
	ViewItem *viewItem = NULL;
	it = elm_genlist_first_item_get(eoDldList);
	while (it) {
		viewItem = (ViewItem *)elm_object_item_data_get(it);
		if (elm_genlist_item_select_mode_get(it) !=
				ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY && viewItem) {
			if (elm_object_item_disabled_get(it))
				elm_object_item_disabled_set(it, EINA_FALSE);
			viewItem->setCheckedValue(EINA_FALSE);
			viewItem->setCheckedBtn(NULL);
		}
		it = elm_genlist_item_next_get(it);
	}

	m_allChecked = EINA_FALSE;

	if (m_viewItemCount < 1) {
		elm_object_item_disabled_set(eoCbItemDelete, EINA_TRUE);
		showEmptyView();
	} else
		elm_object_item_disabled_set(eoCbItemDelete, EINA_FALSE);
}

void DownloadView::handleChangedAllCheckedState()
{
	int checkedCount = 0;
	Elm_Object_Item *it = NULL;
	ViewItem *viewItem = NULL;
	it = elm_genlist_first_item_get(eoDldList);
	while (it) {
		viewItem = (ViewItem *)elm_object_item_data_get(it);
		if (elm_genlist_item_select_mode_get(it) !=
			ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY && viewItem) {
			if (viewItem->isFinished()) {
				viewItem->setCheckedValue(m_allChecked);
				viewItem->updateCheckedBtn();
				checkedCount++;
			}
		}
		it = elm_genlist_item_next_get(it);
	}

	if (m_allChecked && checkedCount > 0) {
		elm_object_item_disabled_set(eoCbItemDelete, EINA_FALSE);
		showNotifyInfo(DOWNLOAD_NOTIFY_SELECTED, checkedCount);
	} else {
		elm_object_item_disabled_set(eoCbItemDelete, EINA_TRUE);
		showNotifyInfo(DOWNLOAD_NOTIFY_SELECTED, 0);
	}
}

void DownloadView::handleCheckedState()
{
	int checkedCount = 0;
	int deleteAbleTotalCount = 0;

	DP_LOGD_FUNC();

	Elm_Object_Item *it = NULL;
	ViewItem *viewItem = NULL;
	it = elm_genlist_first_item_get(eoDldList);
	while (it) {
		viewItem = (ViewItem *)elm_object_item_data_get(it);
		if (elm_genlist_item_select_mode_get(it) !=
			ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY && viewItem) {
			if (viewItem->checkedValue())
				checkedCount++;
			if (viewItem->isFinished())
				deleteAbleTotalCount++;
		}
		it = elm_genlist_item_next_get(it);
	}

	if (checkedCount == deleteAbleTotalCount)
		m_allChecked = EINA_TRUE;
	else
		m_allChecked = EINA_FALSE;
	elm_check_state_pointer_set(eoAllCheckedBox, &m_allChecked);

	if (checkedCount == 0) {
		elm_object_item_disabled_set(eoCbItemDelete, EINA_TRUE);
		destroyNotifyInfo();
	} else
		elm_object_item_disabled_set(eoCbItemDelete, EINA_FALSE);
	showNotifyInfo(DOWNLOAD_NOTIFY_SELECTED, checkedCount);
}
void DownloadView::createNotifyInfo()
{
	DP_LOGD_FUNC();
	eoNotifyInfo = elm_notify_add(eoBoxLayout);
	elm_notify_orient_set(eoNotifyInfo, ELM_NOTIFY_ORIENT_BOTTOM);
	evas_object_size_hint_weight_set(eoNotifyInfo, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(eoNotifyInfo, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_event_callback_add(eoNotifyInfo, EVAS_CALLBACK_SHOW, showNotifyInfoCB, eoLayout);
	evas_object_event_callback_add(eoNotifyInfo, EVAS_CALLBACK_HIDE, hideNotifyInfoCB, eoLayout);
	eoNotifyInfoLayout = elm_layout_add(eoNotifyInfo);
	elm_object_content_set(eoNotifyInfo, eoNotifyInfoLayout);
}

void DownloadView::showNotifyInfo(int type, int selectedCount)
{
	string buf;
	DP_LOGD_FUNC();

	if (selectedCount == 0) {
		destroyNotifyInfo();
		return;
	}

	if (!eoNotifyInfo)
		createNotifyInfo();

	elm_layout_theme_set(eoNotifyInfoLayout, "standard", "selectioninfo",
		"vertical/bottom_12");
	buf.append(" ");
	if (type == DOWNLOAD_NOTIFY_SELECTED) {
		stringstream countStr;
		countStr << selectedCount;
		buf = S_("IDS_COM_BODY_SELECTED");
		buf.append(" (");
		buf.append(countStr.str());
		buf.append(")");
	} else if (type == DOWNLOAD_NOTIFY_DELETED) {
		buf = S_("IDS_COM_POP_DELETED");
		elm_notify_timeout_set(eoNotifyInfo, 3);
	}
	edje_object_part_text_set(_EDJ(eoNotifyInfoLayout), "elm.text", buf.c_str());
	evas_object_show(eoNotifyInfo);
}

void DownloadView::destroyNotifyInfo()
{
	DP_LOGD_FUNC();
	destroyEvasObj(eoNotifyInfoLayout);
	destroyEvasObj(eoNotifyInfo);
	eoNotifyInfoLayout = NULL;
	eoNotifyInfo = NULL;
}

/* Static callback function */
void DownloadView::showNotifyInfoCB(void *data, Evas *evas, Evas_Object *obj,
	void *event)
{
	Evas_Object *layout = (Evas_Object *)data;
	if (!data) {
		DP_LOGE("data is NULL");
		return;
	}
	edje_object_signal_emit(_EDJ(layout), "elm,layout,content,bottom_padding",
		"layout");
}

void DownloadView::hideNotifyInfoCB(void *data, Evas *evas, Evas_Object *obj,
	void *event)
{
	Evas_Object *layout = (Evas_Object *)data;
	if (!data) {
		DP_LOGE("data is NULL");
		return;
	}
	edje_object_signal_emit(_EDJ(layout), "elm,layout,content,default", "layout");
}

void DownloadView::selectAllClickedCB(void *data, Evas *evas, Evas_Object *obj,
	void *event_info)
{
	DownloadView &view = DownloadView::getInstance();
	DP_LOGD_FUNC();
	view.changeAllCheckedValue();
}

void DownloadView::selectAllChangedCB(void *data, Evas_Object *obj,
	void *event_info)
{
	DownloadView &view = DownloadView::getInstance();
	DP_LOGD_FUNC();
	view.handleChangedAllCheckedState();
}

void DownloadView::backBtnCB(void *data, Evas_Object *obj, void *event_info)
{
	DownloadView& view = DownloadView::getInstance();
	view.hide();
}

void DownloadView::cbItemDeleteCB(void *data, Evas_Object *obj, void *event_info)
{
	
	DownloadView& view = DownloadView::getInstance();
	if (!view.isGenlistEditMode())
		view.showGenlistEditMode();
	else
		view.destroyCheckedItem();
}

void DownloadView::cbItemCancelCB(void *data, Evas_Object *obj, void *event_info)
{
	DownloadView& view = DownloadView::getInstance();
	view.destroyNotifyInfo();
	view.hideGenlistEditMode();
}

void DownloadView::genlistClickCB(void *data, Evas_Object *obj, void *event_info)
{
	ViewItem *item = reinterpret_cast<ViewItem *>(data);
	DP_LOGD_FUNC();
	if (!data) {
		DP_LOGE("data is NULL");
		return;
	}
	item->clickedGenlistItem();
}

void DownloadView::cancelClickCB(void *data, Evas_Object *obj, void *event_info)
{
	ViewItem *item = NULL;

	DP_LOGD_FUNC();

	if (!data) {
		DP_LOGE("data is NULL");
		return;
	}
	item = reinterpret_cast<ViewItem *>(data);
	item->requestCancel();

}

void DownloadView::showErrPopup(string &desc)
{
	removePopup();

	eoPopup = elm_popup_add(eoWindow);
	evas_object_size_hint_weight_set(eoPopup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_text_set(eoPopup, desc.c_str());
	elm_popup_timeout_set(eoPopup, 2);
	evas_object_smart_callback_add(eoPopup, "response", errPopupResponseCB, NULL);
	evas_object_show(eoPopup);
}

void DownloadView::showOMAPopup(string &msg, ViewItem *viewItem)
{
	Evas_Object *btn1 = NULL;
	Evas_Object *btn2 = NULL;
	DP_LOGD_FUNC();
	/* If another popup is shown, delete it*/
	removePopup();
	eoPopup = elm_popup_add(eoWindow);
	evas_object_size_hint_weight_set(eoPopup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_text_set(eoPopup, msg.c_str());
	elm_object_part_text_set(eoPopup, "title,text", __("IDS_BR_POP_DOWNLOAD_Q"));
	btn1 = elm_button_add(eoPopup);
	elm_object_text_set(btn1, S_("IDS_COM_SK_OK"));
	elm_object_part_content_set(eoPopup, "button1", btn1);
	evas_object_smart_callback_add(btn1, "clicked", omaPopupResponseOKCB,
		viewItem);
	btn2 = elm_button_add(eoPopup);
	elm_object_text_set(btn2, S_("IDS_COM_POP_CANCEL"));
	elm_object_part_content_set(eoPopup, "button2", btn2);
	evas_object_smart_callback_add(btn2, "clicked", omaPopupResponseCancelCB,
		viewItem);
	evas_object_show(eoPopup);
}

void DownloadView::errPopupResponseCB(void *data, Evas_Object *obj, void *event_info)
{
	DP_LOGD_FUNC();
	DownloadView& view = DownloadView::getInstance();
	view.removePopup();
}

void DownloadView::omaPopupResponseOKCB(void *data, Evas_Object *obj,
	void *event_info)
{
	ViewItem *viewItem = (ViewItem *)data;
	DownloadView& view = DownloadView::getInstance();

	DP_LOGD_FUNC();
	if (viewItem)
		viewItem->sendUserResponse(true);
	else
		DP_LOGE("No viewItem");
	view.removePopup();
}

void DownloadView::omaPopupResponseCancelCB(void *data, Evas_Object *obj,
	void *event_info)
{
	ViewItem *viewItem = (ViewItem *)data;
	DownloadView& view = DownloadView::getInstance();

	DP_LOGD_FUNC();
	if (viewItem)
		viewItem->sendUserResponse(false);
	else
		DP_LOGE("No viewItem");
	view.removePopup();
}

void DownloadView::removePopup()
{
	DP_LOGD_FUNC();
	destroyEvasObj(eoPopup);
}

DateGroup *DownloadView::getDateGroupObj(int type)
{
	DateGroup *obj = NULL;
	switch (type) {
	case DATETIME::DATE_TYPE_LATER:
	case DATETIME::DATE_TYPE_TODAY:
		obj = &m_today;
		break;
	case DATETIME::DATE_TYPE_YESTERDAY:
		obj = &m_yesterday;
		break;
	case DATETIME::DATE_TYPE_PREVIOUS:
		obj = &m_previousDay;
		break;
	default:
		obj = NULL;
		DP_LOGE("Cannot enter here");
	}
	return obj;
}

Elm_Object_Item *DownloadView::getGenlistGroupItem(int type)
{
	DateGroup *obj = getDateGroupObj(type);
	if (!obj)
		return NULL;
	return obj->glGroupItem();	
}

void DownloadView::setGenlistGroupItem(int type, Elm_Object_Item *item)
{
	DateGroup *obj = getDateGroupObj(type);
	if (!obj)
		return;
	obj->setGlGroupItem(item);
}

void DownloadView::increaseGenlistGroupCount(int type)
{
	DateGroup *obj = getDateGroupObj(type);
	if (!obj)
		return;
	if (type == DATETIME::DATE_TYPE_TODAY || type == DATETIME::DATE_TYPE_LATER) {
		if (m_today.getCount() < 1) {
			DateUtil &inst = DateUtil::getInstance();
			inst.setTodayStandardTime();
		}
	}
	obj->increaseCount();
	DP_LOGD("increased count[%d]",obj->getCount());
}

int DownloadView::getGenlistGroupCount(int type)
{
	DateGroup *obj = getDateGroupObj(type);
	if (!obj)
		return 0;
	DP_LOGD("Group count[%d]",obj->getCount());
	return obj->getCount();
}

void DownloadView::handleGenlistGroupItem(int type)
{
	DateGroup *obj = getDateGroupObj(type);
	if (!obj)
		return;
	obj->decreaseCount();
	DP_LOGD("count[%d]type[%d]",obj->getCount(),type);
	if (obj->getCount() < 1) {
		//DP_LOGD("Group Item[%p][%d]", obj->glGroupItem(),type);
		elm_object_item_del(obj->glGroupItem());
		obj->setGlGroupItem(NULL);
	}
}

void DownloadView::handleUpdateDateGroupType(ViewItem *viewItem)
{
	int diffDays = 0;
	DateUtil &inst = DateUtil::getInstance();
	DP_LOGD_FUNC();
	diffDays = inst.getDiffDaysFromToday();
	if (viewItem) {
	/* Update a view item which is added now
	 * This should be only called when attaching item
	**/
		viewItem->extractDateGroupType();
	} else if (diffDays != 0) {
	/* FIXME later 
	 * Now, it is recreated download items and group items.
	 * Consider to move only group item later.	
	 * This should be only called from show() function
	**/
/* FIXME later
 * Another problem is happend becuase eina list is used repacing with vector */
#if 0
		cleanGenlistData();
		Elm_Object_Item *it = NULL;
		ViewItem *viewItem = NULL;
		it = elm_genlist_first_item_get(eoDldList);
		while (it) {
			viewItem = (ViewItem *)elm_object_item_data_get(it);
			if (!viewItem || elm_genlist_item_select_mode_get(it) !=
					ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY)
				continue;
			viewItem->extractDateGroupType();
			createGenlistItem(viewItem);
			it = elm_genlist_item_next_get(it);
		}
#endif
	}
	inst.setTodayStandardTime();
}

void DownloadView::moveRetryItem(ViewItem *viewItem)
{
	Elm_Object_Item *todayGroupItem = NULL;
	Elm_Object_Item *firstItem = NULL;
	DP_LOGD_FUNC();
	if (!viewItem) {
		DP_LOGE("view item is NULL");
		return;
	}
	firstItem = elm_genlist_first_item_get(eoDldList);
	if (firstItem) {
		DP_LOGD("groupItem[%p] viewItem[%p]", firstItem, viewItem);
		/* This is group item */
		if (elm_genlist_item_select_mode_get(firstItem) ==
				ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY) {
			/* The top item is the item after group item */
			firstItem = elm_genlist_item_next_get(firstItem); 
			DP_LOGD("firstItem[%p], present item[%p]", firstItem, viewItem->genlistItem());
			if (firstItem == viewItem->genlistItem()) {
				DP_LOGD("This is already top item. Don't need to move");
				return;
			}
		}
	}
	elm_object_item_del(viewItem->genlistItem());
	viewItem->setGenlistItem(NULL);
	handleGenlistGroupItem(viewItem->dateGroupType());
	todayGroupItem = getGenlistGroupItem(DATETIME::DATE_TYPE_TODAY);
	if (!todayGroupItem) {
		DateGroup *dateGrpObj = getDateGroupObj(DATETIME::DATE_TYPE_TODAY);
		todayGroupItem = elm_genlist_item_prepend(
				eoDldList,
				&dldGenlistGroupStyle,
				static_cast<const void*>(dateGrpObj),
				NULL,
				ELM_GENLIST_ITEM_GROUP,
				NULL,
				NULL);
		setGenlistGroupItem(DATETIME::DATE_TYPE_TODAY, todayGroupItem);
		if (!todayGroupItem)
			DP_LOGE("Fail to add a genlist group item");
		else
			elm_genlist_item_select_mode_set(todayGroupItem,
				ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);
	}
	increaseGenlistGroupCount(DATETIME::DATE_TYPE_TODAY);
	Elm_Object_Item *glItem = elm_genlist_item_insert_after(
			eoDldList,
			viewItem->elmGenlistStyle(),
			static_cast<const void*>(viewItem),
			todayGroupItem,
			todayGroupItem,
			ELM_GENLIST_ITEM_NONE,
			genlistClickCB,
			static_cast<const void*>(viewItem));
	if (!glItem)
		DP_LOGE("Fail to add a genlist item");
	DP_LOGD("genlist groupItem[%p] item[%p] viewItem[%p]", todayGroupItem,
		glItem, viewItem);
	viewItem->setGenlistItem(glItem);
	elm_genlist_item_show(todayGroupItem, ELM_GENLIST_ITEM_SCROLLTO_TOP);
	viewItem->extractDateGroupType();
}

char *DownloadView::getGenlistGroupLabel(void *data, Evas_Object *obj, const char *part)
{
	DateGroup *dateGrp = static_cast<DateGroup *>(data);

	if(!data || !obj || !part)
		return NULL;

	DP_LOGD("ViewItem::getListGroupLabel:part[%s] groupDateType[%d] obj[%p]", part, dateGrp->getType(), obj);
	if (strncmp(part, "elm.text", strlen("elm.text")) == 0) {
		DateUtil &inst = DateUtil::getInstance();
		string msg;
		string outBuf;
		double udateTime = 0;
		switch (dateGrp->getType()) {
		case DATETIME::DATE_TYPE_PREVIOUS:
			msg = S_("IDS_COM_BODY_PREVIOUS_DAYS");
			break;
		case DATETIME::DATE_TYPE_YESTERDAY:
			udateTime = inst.yesterdayTime()*1000;
			msg = S_("IDS_COM_BODY_YESTERDAY");
			msg += " (";
			inst.getDateStr(LOCALE_STYLE::FULL_DATE, udateTime, outBuf);
			msg += outBuf;
			msg += ")";
			break;
		case DATETIME::DATE_TYPE_LATER:
		case DATETIME::DATE_TYPE_TODAY:
			udateTime = inst.nowTime()*1000;
			msg = S_("IDS_COM_BODY_TODAY");
			msg += " (";
			inst.getDateStr(LOCALE_STYLE::FULL_DATE, udateTime, outBuf);
			msg += outBuf;
			msg += ")";
			break;
		default :
			DP_LOGE("Cannot enter here");
			return NULL;
		}
		return strdup(msg.c_str());
	}
	return NULL;
}

char *DownloadView::getGenlistGroupLabelCB(void *data, Evas_Object *obj, const char *part)
{
//	DP_LOGD_FUNC();
	if(!data || !obj || !part)
		return NULL;

	DownloadView &view = DownloadView::getInstance();
	return view.getGenlistGroupLabel(data, obj, part);
}

void DownloadView::cleanGenlistData()
{
	Elm_Object_Item *grpItem = NULL;
	DP_LOGD_FUNC();
	grpItem = m_today.glGroupItem();
	if (grpItem)
		elm_object_item_del(grpItem);
	m_today.initData();
	grpItem = m_yesterday.glGroupItem();
	if (grpItem)
		elm_object_item_del(grpItem);
	m_yesterday.initData();
	grpItem = m_previousDay.glGroupItem();
	if (grpItem)
		elm_object_item_del(grpItem);
	m_previousDay.initData();
	elm_genlist_clear(eoDldList);
}

