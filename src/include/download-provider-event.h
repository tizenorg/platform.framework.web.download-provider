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
 * @file 	download-provider-event.h
 * @author	Jungki Kwak (jungki.kwak@samsung.com)
 * @brief	Download event class
 */
#ifndef DOWNLOAD_PROVIDER_EVENT_H
#define DOWNLOAD_PROVIDER_EVENT_H

#include <vector>
/* For debug */
#include <string>

using namespace std;

class Observer;
class Subject
{
public:
	Subject(){}
	~Subject(){}

	void attach(Observer *);
	void detach(Observer *);
	void notify(void);

private:
	vector<Observer*> _observers;
};

typedef void (*updateFunction)(void *data);

class Observer
{
public:
	/* For debug */
	Observer(updateFunction uf, void *data, const char *name);
	//Observer(updateFunction uf, void *data);
	~Observer(){}

	void update(Subject *s);
	void set(updateFunction uf, void *data);
	void clear(void);
	void *getUserData(void) { return m_userData; }
	/* For debug */
	string name(void) { return observerName; }

private:
	void call(void);

	updateFunction m_updateFunction;
	void *m_userData;
	/* For debug */
	string observerName;
};

#endif /* DOWNLOAD_PROVIDER_EVENT_H */
