/*
Copyright 2018 Nia Catlin

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include "uiMessage.h"
#include <Windows.h>
#include "base_thread.h"
#include <vector>

#pragma once
class tradeInterface : public base_thread
{
public:
	tradeInterface(SafeQueue<UI_MESSAGE> *uiq) {
		uiMsgQueue = uiq;
	}
	~tradeInterface() {};

	bool connected() { return isConnected; }
	bool tradeAlert(QString coincode, double price);
	void sendPrice(QString coincode, double price);
	bool startFeedPipe(); 
	void stopFeedPipe();
	void sendToFeed(QString msg);

private:
	void main_loop();
	void connectFeedPipe();

	HANDLE hPipe = NULL;

	SafeQueue<UI_MESSAGE> *uiMsgQueue;

	bool pipeStarted = false;
	bool isConnected = false;
	double balance;
	std::mutex commsMutex;
	std::list<QString> itemslist;
};

