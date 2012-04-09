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
 * @file 	download-provider-dateTime.h
 * @author	Jungki Kwak (jungki.kwak@samsung.com)
 * @brief	data and utility APIs for Date and Time
 */

#ifndef DOWNLOAD_PROVIDER_DATE_TIME_H
#define DOWNLOAD_PROVIDER_DATE_TIME_H

#include <time.h>
#include <Elementary.h>
#include <unicode/udat.h>
#include <unicode/udatpg.h>
#include <unicode/ustring.h>

#include "download-provider-common.h"

using namespace std;

namespace DATETIME {
enum {
	DATE_TYPE_NONE = 0,
	DATE_TYPE_LATER,
	DATE_TYPE_TODAY,
	DATE_TYPE_YESTERDAY,
	DATE_TYPE_PREVIOUS,
};
}

namespace LOCALE_STYLE{
enum {
	TIME = 0,
	SHORT_DATE,
	MEDIUM_DATE,
	FULL_DATE
};
}

class DateGroup {
public:
	DateGroup(void);
	~DateGroup(void);

	Elm_Object_Item *glGroupItem() { return m_glGroupItem; }
	void setGlGroupItem(Elm_Object_Item *i) { m_glGroupItem = i; }
	void increaseCount(void) { count++; }
	void decreaseCount(void) { count--; }
	int getCount(void) { return count; }
	void initData(void);
	void setType(int t) { type = t; }
	int getType(void) { return type; }

private:
	int count;
	int type;
	Elm_Object_Item *m_glGroupItem;
};

class DateUtil {
public:
	static DateUtil& getInstance(void) {
		static DateUtil inst;
		return inst;
	}

	inline void setTodayStandardTime(void) { m_todayStandardTime = time(NULL); }
	int getDiffDaysFromToday(void);
	int getDiffDays(time_t nowTime, time_t refTime);
	void updateLocale(void);
	void getDateStr(int style, double time, string &outBuf);
	inline double nowTime(void) { return (double)(time(NULL)); }
	/* yesterday is same to 24*60*60 seconds from now */
	inline double yesterdayTime(void) { return (double)(time(NULL)+24*60*60); }

private:
	DateUtil(void);
	~DateUtil(void);

	UDateFormat *getBestPattern(const char *patternStr,
		UDateTimePatternGenerator *generator, const char *locale);
	void deinitLocaleData(void);
	/* Update this in case of follows
	 * 1. show main view.
	 * 2. add new item
	 * 3. create today group
	**/
	time_t m_todayStandardTime;
	UDateFormat *dateShortFormat;
	UDateFormat *dateMediumFormat;
	UDateFormat *dateFullFormat;
	UDateFormat *timeFormat12H;
	UDateFormat *timeFormat24H;
};

#endif /* DOWNLOAD_PROVIDER_DATE_TIME_H */
