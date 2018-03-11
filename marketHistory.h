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
#include <vector>
#include <map>
#include <time.h>
#include <iostream>
#include "qfileinfo.h"
#include <rapidjson\document.h>
#include "uiMessage.h"
#include "tradeInterface.h"
#include "tradesettings.h"


struct pumpRecord {
	size_t pumpID;
	time_t startTime;
};

struct periodData {
	list<SHORT_PAIR_TRADE_RECORD> periodRecords;
	float totalvolume, volumedelta, priceHighDelta, priceLowDelta, buyQty, sellQty;
	double priceLowest, priceHighest;
	time_t priceHighTime, priceLowTime;
};

struct pendingTradeData {
	bool isPending = false;
	double targetPrice = 0;
	double lossPrice = 0;
	double earlyExitPrice = 0;
	double startPrice = 0;
	double profit = 0;

	double initialBuyCapital = 0;
	double buyCapitalSpent = 0;
	double buyCapitalRemaining = 0;
	double sellTargetRemaining = 0;
	double capitalReturned = 0;

	eTradeResult result;
	time_t secondsStarted, duration;
};

class marketHistory
{
public:
	marketHistory(QString pairCode, QString fullName, QString exchange, bool *autosaveFlag, tradeInterface *tradeif, tradeSettingsStruct *tradesconf);
	~marketHistory();

	void startLiveSession(time_t firstTx, bool live) {
		assert(live);
		cout << "Coin " << paircode.toStdString() << " starting new live session " << endl;
		activePeriodStart = firstTx;
		activePeriod = &historyPeriods[activePeriodStart];
		if (live)
		{
			inLiveSession = true;
			inReplaySession = false;
		}


		isWatchedPump = false;
		activePeriodStart = firstTx;
	}	

	void endSession();
	void resetBins();

	void add_trade_update(PAIR_TRADE_RECORD tradeUpdate, bool islive);
	void add_live_trade_update(PAIR_TRADE_RECORD tradeUpdate);
	void add_replay_trade_update(PAIR_TRADE_RECORD tradeUpdate);

	void save(time_t periodTime);
	void save(string filename, time_t periodTime);
	bool load(string filename); 
	bool loadAll(QString dirname);

	float pricePercentDelta(int minutes); 
	float periodLowDeltaFromLast(int minutes, time_t &secsfromend);
	float periodHighDeltaFromLast(int minutes, time_t &secsfromend);
	float periodLowDeltaFromFirst(int minutes, time_t &secsfromstart);
	float periodHighDeltaFromFirst(int minutes, time_t &secsfromstart);
	float periodDeltaLastFromFirst(int minutes);
	float getBuyPercentage(int minutes);

	void setActiveTrade(double buyPrice, time_t buytime);
	bool isActiveTrade() { return activeTrade.isPending; }
	bool wasActiveTrade() { return hasBeenActive; }
	double getTargetPrice() { return activeTrade.targetPrice; }
	double getStoplossPrice() { return activeTrade.lossPrice; }
	double getBuyPrice() { return activeTrade.startPrice; }
	void setTradeComplete(double profit, time_t stoptime);
	void getTradeResults(eTradeResult &res, double &profit, time_t &duration) {
		res = activeTrade.result;
		profit = activeTrade.profit;
		duration = activeTrade.duration;
	}

	float volumePerMin(int minutes); 
	//float marketHistory::periodTransValuePerMin(int minutes);

	void startPumpWatch(size_t pumpID, PAIR_TRADE_RECORD record, time_t buildupTime, alertSettingsStruct *alertsconf);
	void stopPumpWatch() { isWatchedPump = false; }
	bool watchedPump(pumpRecord *rec) { if (isWatchedPump) { rec->pumpID = pumprec.pumpID; rec->startTime = pumprec.startTime; }  return isWatchedPump; }
	time_t sessionLength(time_t totime) { return totime - this->activePeriodStart; }

	std::vector<SHORT_PAIR_TRADE_RECORD> * startReplaySession(time_t sessiontime) { 

		cout << "Coin " << paircode.toStdString() << " starting replay session " << endl;

		inReplaySession = true;
		inLiveSession = false;
		isWatchedPump = false;
		this->activePeriodStart = sessiontime;
		return &historyPeriods.at(sessiontime); 
	}

	float minutesSinceStarted() { return minutesSinceStarted(activePeriodStart); }
	float minutesSinceStarted(time_t sessionStart);

	void generateNewRecordAlerts(QString exchgname);

	static SafeQueue<UI_MESSAGE> *uiMsgQueue;
	static ofstream *logfile;


private:
	pumpRecord pumprec;
	bool loadToJSON(QString filename, rapidjson::Document *jsondoc);
	string getSaveDir();
	string getSavePath2(time_t sessiontime);
	void clearOldBinRecords(); 
	void recalculateHighLows(periodData *dat);
	void update_bins_with_transaction(SHORT_PAIR_TRADE_RECORD rec);
	void resetBin(periodData *dat);
	void resetActiveTrade();

	bool *autosave;
	QString paircode, longname, exchange;
	std::map<time_t, std::vector<SHORT_PAIR_TRADE_RECORD>> historyPeriods;

	std::vector<SHORT_PAIR_TRADE_RECORD> *activePeriod = NULL;
	time_t activePeriodStart;

	std::map <int, periodData> periodBins;


	list<pair<time_t,string>> period30RecordStrings;
	time_t lastPeriodDump = 0;


	bool inLiveSession = false;
	bool inReplaySession = false;
	bool isWatchedPump = false;
	bool hasBeenActive = false;

	pendingTradeData activeTrade;
	

	time_t lastMsgTime = 0;
	time_t lastSave = 0;
	std::mutex activityMutex;
	tradeSettingsStruct *tradesConfig = NULL;
	tradeInterface *tradeinterface = NULL;
};

