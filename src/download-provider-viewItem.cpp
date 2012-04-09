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
 * @file	download-provider-viewItem.cpp
 * @author	Jungki Kwak (jungki.kwak@samsung.com)
 * @brief	item data class for download view
 */
#include "download-provider-viewItem.h"
#include "download-provider-items.h"
#include "download-provider-view.h"

ViewItem::ViewItem(Item *item)
	: m_item(item)
	, m_glItem(NULL)
	, m_progressBar(NULL)
	, m_checkedBtn(NULL)
	, m_checked(EINA_FALSE)
	, m_isRetryCase(false)
	, m_dateGroupType(DATETIME::DATE_TYPE_NONE)
{
	// FIXME need to makes exchange subject?? not yet, but keep it in mind!
	if (item) {
		m_aptr_observer = auto_ptr<Observer>(
			new Observer(updateCB, this, "viewItemObserver"));
		item->subscribe(m_aptr_observer.get());
	}

	dldGenlistStyle.item_style = "3text.3icon";
	dldGenlistStyle.func.text_get = getGenlistLabelCB;
	dldGenlistStyle.func.content_get = getGenlistIconCB;
	dldGenlistStyle.func.state_get = NULL;
	dldGenlistStyle.func.del = NULL;
	dldGenlistStyle.decorate_all_item_style = "edit_default";

	dldHistoryGenlistStyle.item_style = "3text.1icon.2";
	dldHistoryGenlistStyle.func.text_get = getGenlistLabelCB;
	dldHistoryGenlistStyle.func.content_get = getGenlistIconCB;
	dldHistoryGenlistStyle.func.state_get = NULL;
	dldHistoryGenlistStyle.func.del = NULL;
	dldHistoryGenlistStyle.decorate_all_item_style = "edit_default";

	dldGenlistSlideStyle.item_style = "3text.1icon.2";
	dldGenlistSlideStyle.func.text_get = getGenlistLabelCB;
	dldGenlistSlideStyle.func.content_get= getGenlistIconCB;
	dldGenlistSlideStyle.func.state_get = NULL;
	dldGenlistSlideStyle.func.del = NULL;
	dldGenlistSlideStyle.decorate_all_item_style = "edit_default";

}

ViewItem::~ViewItem()
{
	DP_LOGD_FUNC();
}

void ViewItem::create(Item *item)
{
	ViewItem *newViewItem = new ViewItem(item);

	DownloadView &view = DownloadView::getInstance();
	view.attachViewItem(newViewItem);
}

void ViewItem::destroy()
{
	DP_LOGD("ViewItem::destroy");
	/* After item is destory,
	   view item also will be destroyed through event system */
	if (m_item)
		m_item->destroy();
}

void ViewItem::updateCB(void *data)
{
	if (data)
		static_cast<ViewItem*>(data)->updateFromItem();
}

void ViewItem::updateFromItem()
{
	DownloadView &view = DownloadView::getInstance();
	DP_LOGD("ViewItem::updateFromItem() ITEM::[%d]", state());
	if (state() == ITEM::DESTROY) {
		int tempType = 0;
		DP_LOGD("ViewItem::updateFromItem() ITEM::DESTROY");
		if (m_item)
			m_item->deSubscribe(m_aptr_observer.get());
		m_aptr_observer->clear();
		elm_object_item_del(m_glItem);
		m_glItem = NULL;
		tempType = dateGroupType();
		view.detachViewItem(this);
		view.handleGenlistGroupItem(tempType);
		return;
	}

	if (state() == ITEM::WAITING_USER_RESPONSE) {
		string buf;
		buf.append("Name : ");
		buf.append(m_item->contentName());
		buf.append("<br>");
		buf.append("Size : ");
		buf.append(getHumanFriendlyBytesStr(fileSize(), false));
		buf.append("<br>");
		buf.append("Vendor : ");
		buf.append(m_item->vendorName());
		buf.append("<br>");
		if (m_item->isMidletInstalled()) {
			buf.append("<br>");
			buf.append(S_("IDS_COM_POP_ALREDY_EXISTS"));
			buf.append("<br>");
			buf.append("Want to update?");
		}
		view.showOMAPopup(buf, this);
		return;
	}

	if (state() == ITEM::DOWNLOADING) {
		if (fileSize() > 0 && m_progressBar) {
			double percentageProgress = 0.0;
			percentageProgress = (double)(receivedFileSize()) /
				(double)(fileSize());
			DP_LOGD("progress value[%.2f]",percentageProgress);
			elm_progressbar_value_set(m_progressBar, percentageProgress);
		}
		elm_genlist_item_fields_update(m_glItem,"elm.text.2",
			ELM_GENLIST_ITEM_FIELD_TEXT);
	} else if (m_isRetryCase && state() == ITEM::RECEIVING_DOWNLOAD_INFO) {
		elm_genlist_item_item_class_update(m_glItem, &dldGenlistStyle);
	} else if (!isFinished()) {
		elm_genlist_item_update(m_glItem);
	} else {/* finished state */
		if (state() == ITEM::FINISH_DOWNLOAD)
			elm_genlist_item_item_class_update(m_glItem, &dldHistoryGenlistStyle);
		else
			elm_genlist_item_item_class_update(m_glItem, &dldGenlistSlideStyle);
		if (view.isGenlistEditMode())
			elm_object_item_disabled_set(m_glItem, EINA_FALSE);
	}
}

char *ViewItem::getGenlistLabelCB(void *data, Evas_Object *obj, const char *part)
{
//	DP_LOGD_FUNC();

	if(!data || !obj || !part)
		return NULL;

	ViewItem *item = static_cast<ViewItem *>(data);
	return item->getGenlistLabel(obj, part);
}

char *ViewItem::getGenlistLabel(Evas_Object *obj, 	const char *part)
{
	DP_LOGD("ViewItem::getListLabel:part[%s]", part);

	if (strncmp(part, "elm.text.1", strlen("elm.text.1")) == 0) {
		return strdup(getTitle());
	} else if (strncmp(part, "elm.text.2", strlen("elm.text.2")) == 0) {
		return strdup(getMessage());
	} else if (strncmp(part, "elm.text.3", strlen("elm.text.3")) == 0) {
		if (!isFinished()) {
			return NULL;
		} else {
			string outBuf;
			DateUtil &inst = DateUtil::getInstance();
			double udateTime = finishedTime() * 1000;
			if (dateGroupType() == DATETIME::DATE_TYPE_PREVIOUS 
				|| dateGroupType() == DATETIME::DATE_TYPE_LATER)
				inst.getDateStr(LOCALE_STYLE::SHORT_DATE, udateTime, outBuf);
			else
				inst.getDateStr(LOCALE_STYLE::TIME, udateTime, outBuf);
			return strdup(outBuf.c_str());
		}
	} else {
		DP_LOGD("No Implementation");
		return NULL;
	}
}

Evas_Object *ViewItem::getGenlistIconCB(void *data, Evas_Object *obj,
	const char *part)
{
//	DP_LOGD_FUNC();
	if(!data || !obj || !part) {
		DP_LOGE("parameter is NULL");
		return NULL;
	}

	ViewItem *item = static_cast<ViewItem *>(data);
	return item->getGenlistIcon(obj, part);
}

Evas_Object *ViewItem::getGenlistIcon(Evas_Object *obj, const char *part)
{
	//DP_LOGD("ViewItem::getGenlistIcon:part[%s]state[%s]", part, stateStr());

	if (elm_genlist_decorate_mode_get(obj) && isFinished()) {
		if (strncmp(part,"elm.edit.icon.1", strlen("elm.edit.icon.1")) == 0) {
			Evas_Object *checkBtn = elm_check_add(obj);
			elm_check_state_pointer_set(checkBtn, &m_checked);
			evas_object_smart_callback_add(checkBtn, "changed", checkChangedCB,
				this);
			m_checkedBtn = checkBtn;
			return checkBtn;
		} else if (strncmp(part,"elm.edit.icon.2", strlen("elm.edit.icon.2")) ==
			0) {
			return NULL;
		}

	}
	/* elm.icon.2 should be checked prior to elm.icon */
	if (strncmp(part,"elm.icon.2", strlen("elm.icon.2")) == 0) {
		if (state() == ITEM::RECEIVING_DOWNLOAD_INFO || 
			state() == ITEM::PREPARE_TO_RETRY ||
			state() == ITEM::DOWNLOADING ||
			isPreparingDownload())
			return createCancelBtn(obj);
		else
			return NULL;
	} else if (strncmp(part,"elm.icon.1", strlen("elm.icon.1")) == 0 ||
		strncmp(part, "elm.icon", strlen("elm.icon")) == 0) {
//	if (strncmp(part,"elm.icon.1", strlen("elm.icon.1")) == 0) {
		Evas_Object *icon = elm_icon_add(obj);
		elm_icon_file_set(icon, getIconPath(), NULL);
		evas_object_size_hint_aspect_set(icon, EVAS_ASPECT_CONTROL_VERTICAL,1,1);
		return icon;
	} else if (strcmp(part,"elm.swallow.progress") == 0) {
		return createProgressBar(obj);
	} else {
		DP_LOGE("Cannot enter here");
		return NULL;
	}
}

void ViewItem::checkChangedCB(void *data, Evas_Object *obj,
	void *event_info)
{
	DownloadView &view = DownloadView::getInstance();
	DP_LOGD_FUNC();
	//ViewItem *item = static_cast<ViewItem *>(data);
	//DP_LOGD("checked[%d] viewItem[%p]",(bool)(item->checkedValue()),item);
	view.handleCheckedState();
}

void ViewItem::clickedDeleteButton()
{
	DP_LOGD("ViewItem::clickedDeleteButton()");
	m_item->deleteFromDB();
	destroy();
}

void ViewItem::clickedCancelButton()
{
	DP_LOG("ViewItem::clickedCancelButton()");
	requestCancel();
}

void ViewItem::requestCancel()
{
	if (m_item) {
		m_item->cancel();
	}
}

void ViewItem::clickedRetryButton()
{
	DP_LOG_FUNC();
	retryViewItem();
}

void ViewItem::clickedGenlistItem()
{
	DownloadView &view = DownloadView::getInstance();
	DP_LOG_FUNC();
	if (!m_item) {
		DP_LOGE("m_item is NULL");
		return;
	}
	if (view.isGenlistEditMode()) {
		m_checked = !m_checked;
		if (m_checkedBtn)
			elm_genlist_item_fields_update(genlistItem(),"elm.edit.icon.1",
				ELM_GENLIST_ITEM_FIELD_CONTENT);
		else
			DP_LOGE("m_checkedBtn is NULL");
		view.handleCheckedState();
	} else if (state() == ITEM::FINISH_DOWNLOAD) {
		bool ret = m_item->play();
		if (ret == false) {
			string desc = __("IDS_BR_POP_UNABLE_TO_OPEN_FILE");
			view.showErrPopup(desc);
		}
	} else if (isFinishedWithErr()) {
		retryViewItem();
	}
	elm_genlist_item_selected_set(genlistItem(), EINA_FALSE);
}

Elm_Genlist_Item_Class *ViewItem::elmGenlistStyle()
{
	/* Change the genlist style class in case of download history item */
	if (state() == ITEM::FINISH_DOWNLOAD)
		return &dldHistoryGenlistStyle;
	else if (isFinishedWithErr())
		 return &dldGenlistSlideStyle;
	else
		return &dldGenlistStyle;
}

const char *ViewItem::getMessage()
{
	DP_LOGD("ViewItem state() ITEM::[%d]", state());
	const char *buff = NULL;
	switch(state()) {
	case ITEM::IDLE:
	case ITEM::REQUESTING:
	case ITEM::PREPARE_TO_RETRY:
	case ITEM::WAITING_USER_RESPONSE:
	case ITEM::RECEIVING_DOWNLOAD_INFO:
	/* Do not display string and show only the progress bar */
//		buff = __("Check for download");
		buff = "";
		break;
	case ITEM::DOWNLOADING:
		buff = getHumanFriendlyBytesStr(receivedFileSize(), true);
//		DP_LOGD("%s", buff);
		break;
	case ITEM::CANCEL:
		buff = S_("IDS_COM_POP_CANCELLED");
		break;
	case ITEM::FAIL_TO_DOWNLOAD:
		buff = getErrMsg();
		break;
	case ITEM::REGISTERING_TO_SYSTEM:
		buff = S_("IDS_COM_POP_INSTALLING_ING");
		break;
	case ITEM::ACTIVATING:
		buff = S_("IDS_COM_POP_ACTIVATING");
		break;
	case ITEM::NOTIFYING_TO_SERVER:
		buff =  __("IDS_BR_BODY_NOTIFYING_ING");
		break;
	case ITEM::PROCESSING_DOMAIN:
		buff = S_("IDS_COM_POP_PROCESSING");
		break;
	case ITEM::FINISH_PROCESSING_DOMAIN:
		buff = __("IDS_BR_BODY_PROCESSING_COMPLETED");
		break;
	case ITEM::FINISH_DOWNLOAD:
		buff =  __("IDS_EMAIL_BODY_COMPLETE");
		break;
	default:
		buff = "";
		break;
	}
	return buff;
}

const char *ViewItem::getHumanFriendlyBytesStr(unsigned long int bytes,
	bool progressOption)
{
	double doubleTypeBytes = 0.0;
	const char *unitStr[4] = {"B", "KB", "MB", "GB"};
	int unit = 0;
	unsigned long int unitBytes = bytes;

	/* using bit operation to avoid floating point arithmetic */
	for (unit=0; (unitBytes > 1024 && unit < 4) ; unit++) {
		unitBytes = unitBytes >> 10;
	}

	unitBytes = 1 << (10*unit);
	doubleTypeBytes = ((double)bytes / (double)(unitBytes));
	// FIXME following code should be broken into another function, but leave it now to save function call time.s
	char str[32] = {0};
	if (progressOption && fileSize() != 0) {
		/* using fixed point arithmetic to avoid floating point arithmetic */
		const int fixed_point = 6;
		unsigned long long int receivedBytes = receivedFileSize() << fixed_point;
		unsigned long long int result = (receivedBytes*100) / fileSize();
		unsigned long long int result_int = result >> fixed_point;
		unsigned long long int result_fraction = result &
			~(0xFFFFFFFF << fixed_point);
		if (unit == 0)
			snprintf(str, sizeof(str), "%lu %s / %llu.%.2llu %%",
				bytes, unitStr[unit], result_int, result_fraction);
		else
			snprintf(str, sizeof(str), "%.2f %s / %llu.%.2llu %%",
				doubleTypeBytes, unitStr[unit], result_int, result_fraction);
	} else {
		if (unit == 0)
			snprintf(str, sizeof(str), "%lu %s", bytes, unitStr[unit]);
		else
			snprintf(str, sizeof(str), "%.2f %s", doubleTypeBytes, unitStr[unit]);
	}
	return string(str).c_str();
}

unsigned long int ViewItem::receivedFileSize()
{
	if (m_item)
		return m_item->receivedFileSize();

	return 0;
}

unsigned long int ViewItem::fileSize()
{
	if (m_item)
		return m_item->fileSize();

	return 0;
}

const char *ViewItem::getTitle()
{
	const char *title = NULL;
	if (m_item)
		title = m_item->title().c_str();

	if (!title)
		return S_("IDS_COM_BODY_NO_NAME");

	return title;
}

Evas_Object *ViewItem::createProgressBar(Evas_Object *parent)
{
	Evas_Object *progress = NULL;
	if (!parent) {
		DP_LOGE("parent is NULL");
		return NULL;
	}
	progress = elm_progressbar_add(parent);
	setProgressBar(progress);
	if (isFinished()) {
		DP_LOGE("Cannot enter here. finished item has othere genlist style");
		return NULL;
	}

	if (fileSize() == 0 || isPreparingDownload()) {
		//DP_LOGD("Pending style::progressBar[%p]",progress);
		elm_object_style_set(progress, "pending_list");
		elm_progressbar_horizontal_set(progress, EINA_TRUE);
		evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
		evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND,
			EVAS_HINT_EXPAND);
		elm_progressbar_pulse(progress, EINA_TRUE);
	} else {
		//DP_LOGD("List style::progressBar[%p] fileSize[%d] state[%d]",progress, fileSize(),state());
		elm_object_style_set(progress, "list_progress");
		elm_progressbar_horizontal_set(progress, EINA_TRUE);

		if (isCompletedDownload())
			elm_progressbar_value_set(progress, 1.0);
		/* When realized event is happened, the progress is created.
		   This is needed for that case */
		else if (state() == ITEM::DOWNLOADING) {
			double percentageProgress = 0.0;
			percentageProgress = (double)(receivedFileSize()) /
				(double)(fileSize());
			elm_progressbar_value_set(progress, percentageProgress);
		}
	}
	evas_object_show(progress);
	return progress;
}

void ViewItem::updateCheckedBtn()
{
	if (m_checkedBtn)
		elm_check_state_pointer_set(m_checkedBtn,&m_checked);
}

void ViewItem::deleteBtnClickedCB(void *data, Evas_Object *obj, void *event_info)
{
	DP_LOGD_FUNC();
	if (!data) {
		DP_LOGE("data is NULL");
		return;
	}
	ViewItem *viewItem = static_cast<ViewItem *>(data);
	viewItem->clickedDeleteButton();
}

void ViewItem::retryBtnClickedCB(void *data, Evas_Object *obj, void *event_info)
{
	DP_LOGD_FUNC();
	if (!data) {
		DP_LOGE("data is NULL");
		return;
	}
	ViewItem *viewItem = static_cast<ViewItem *>(data);
	viewItem->clickedRetryButton();
}

Evas_Object *ViewItem::createCancelBtn(Evas_Object *parent)
{
	DP_LOGD_FUNC();
	Evas_Object *button = elm_button_add(parent);
	elm_object_part_content_set(parent, "btn_style1", button);
	elm_object_style_set(button, "style1/auto_expand");
	evas_object_size_hint_aspect_set(button, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
	elm_object_text_set(button, S_("IDS_COM_SK_CANCEL"));
	evas_object_propagate_events_set(button, EINA_FALSE);
	evas_object_smart_callback_add(button,"clicked", cancelBtnClickedCB, this);
	return button;
}

void ViewItem::cancelBtnClickedCB(void *data, Evas_Object *obj, void *event_info)
{
	DP_LOGD_FUNC();
	if (!data) {
		DP_LOGE("data is NULL");
		return;
	}
	ViewItem *viewItem = static_cast<ViewItem *>(data);
	viewItem->clickedCancelButton();
}

void ViewItem::extractDateGroupType()
{
	DP_LOGD_FUNC();
	/* History Item */
	//DP_LOGD("state[%s],finishedTime[%ld]",stateStr(),finishedTime());
	if (isFinished() && finishedTime() > 0) {
		int diffDay = 0;
		DateUtil &inst = DateUtil::getInstance();
		double nowTime = inst.nowTime();
		double finishTime = finishedTime();
		diffDay = inst.getDiffDays((time_t)nowTime, (time_t)finishTime);
		if (diffDay == 0)
			m_dateGroupType = DATETIME::DATE_TYPE_TODAY;
		else if (diffDay == 1)
			m_dateGroupType = DATETIME::DATE_TYPE_YESTERDAY;
		else if (diffDay > 1)
			m_dateGroupType = DATETIME::DATE_TYPE_PREVIOUS;
		else 
			m_dateGroupType = DATETIME::DATE_TYPE_LATER;
		return;
	}
	/* Item which is added now or retrying item */
	m_dateGroupType = DATETIME::DATE_TYPE_TODAY;
}


void ViewItem::retryViewItem(void)
{
	DownloadView &view = DownloadView::getInstance();
	DP_LOGD_FUNC();
	if (m_item) {
		m_isRetryCase = true;
		m_item->clearForRetry();
		if (!m_item->retry()) {
			DownloadView &view = DownloadView::getInstance();
			string desc = S_("IDS_COM_POP_FAILED");
			view.showErrPopup(desc);
			m_item->deleteFromDB();
			m_item->destroy();
			return;
		}
		/* Move a item to Today group, if it is not included to Today group */
		view.moveRetryItem(this);
	}
}
