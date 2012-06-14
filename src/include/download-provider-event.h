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
