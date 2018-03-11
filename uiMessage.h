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
#pragma once
#include <string>
#include <QQueue>
#include "cppsemaphore.h"
#include "tradesettings.h"

template <class T>
class SafeQueue
{
public:
	SafeQueue() {};
	~SafeQueue() {};

	void addItem(T * item)
	{
		mymutex.lock();
		q.push_back(item);
		sem.notify();
		mymutex.unlock();
	}

	T *waitItem() {
		sem.wait();
		mymutex.lock();
		T *item = q.dequeue();
		mymutex.unlock();
		return item;
		
	}

	bool empty() { return q.empty(); }

private:
	QQueue<T *> q;
	semaphore sem;
	std::mutex mymutex;
};


enum uiMsgType {eTextLog, eTradeFeedData, eCurrencyUpdate, 
	eSessionRecordsLoaded, ePumpStart, ePumpEnd, eReplaySchedule, eReplayDone, eFeedConnect
};

class UI_MESSAGE
{
public:
	uiMsgType msgType;
};

class UI_STRING_MESSAGE : public UI_MESSAGE
{
public:
	QString stringData;
};

enum pairTradeType { eBuy, eSell, eINVALID };

struct PAIR_TRADE_RECORD {
	pairTradeType tradeType;
	double price;
	double quantity;
	time_t timecode;
	QString paircode;
	QString exchange;
};

struct SHORT_PAIR_TRADE_RECORD {
	pairTradeType tradeType;
	double price;
	double quantity;
	time_t timecode;
	QString paircode, longname;
};


class TRADEFEED_UPDATE : public UI_MESSAGE
{
public:
	TRADEFEED_UPDATE() {
		msgType = uiMsgType::eTradeFeedData;
	}
	PAIR_TRADE_RECORD rec;
};

class UI_CURRENCY_PAIR_UPDATE : public UI_MESSAGE
{
public:
	UI_CURRENCY_PAIR_UPDATE() {
		msgType = uiMsgType::eCurrencyUpdate;
	}
	QString paircode;
	double price;
	float delta2, delta5, delta10, delta30, delta60;
	float vol10m, delta5V, delta10V, delta30V;
	float twominbuys, fiveminbuys, tenminbuys , thirtyminbuys;
};

class UI_PUMP_START_NOTIFY : public UI_MESSAGE
{
public:
	UI_PUMP_START_NOTIFY() {
		msgType = uiMsgType::ePumpStart;
	}

	QString paircode;
	time_t timestart;
	size_t pumpID;
	float delta2Low, delta5Low, delta10Low, delta30Low, delta60Low;
	float vol5, vol10, vol30, vol60;
	float buys2, buys5, buys10, buys30, buys60;
	float sellbuypct;
	float sessionTime;
	double buyPrice;
};

class UI_PUMP_DONE_NOTIFY : public UI_MESSAGE
{
public:
	UI_PUMP_DONE_NOTIFY() {
		msgType = uiMsgType::ePumpEnd;
	}

	time_t timeend;
	size_t pumpID;
	float deltaPeriodLow, deltaPeriod, deltaPEriodHigh;
	float volPeriod;
	time_t lowtime, hightime;
	float percentTraded;
	double endPrice;

	eTradeResult tradeResult;
	double tradeProfit;
	time_t tradeDuration;
};

class UI_SESSION_RECORDS_LOADED : public UI_MESSAGE
{
public:
	UI_SESSION_RECORDS_LOADED() {
		msgType = uiMsgType::eSessionRecordsLoaded;
	}
	QString exchangeName, currencyPair;
	time_t startTime, endTime;
	unsigned long recordCount;
};

class UI_QUEUE_REPLAY : public UI_MESSAGE
{
public:
	UI_QUEUE_REPLAY() {
		msgType = uiMsgType::eReplaySchedule;
	}
	void *itam;
	time_t sessionstart;
};

class UI_FINISHED_REPLAY : public UI_MESSAGE
{
public:
	UI_FINISHED_REPLAY() {
		msgType = uiMsgType::eReplayDone;
	}
};

class UI_FEED_CONNECTION : public UI_MESSAGE
{
public:
	UI_FEED_CONNECTION() {
		msgType = uiMsgType::eFeedConnect;
	}
	bool tradeFeed = false;
	bool tradeInterface = false;
	bool connected = false;
};

void addUILogMsg(QString msg, SafeQueue<UI_MESSAGE> *uiq);