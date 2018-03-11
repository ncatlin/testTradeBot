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
#include <QtWidgets/QMainWindow>
#include "uiMessage.h"
#include "base_thread.h"
#include "marketHistory.h"
#include <rapidjson\document.h>
#include "dataReplayThread.h"

struct tradeResults {
	float profitPercent = 0;
	int totalWins = 0;
	int partialWins = 0;
	int partialLosses = 0;
	int totalLosses = 0;
};

class tradeEngine : public base_thread
{
public:
	tradeEngine(SafeQueue<UI_MESSAGE> *uiq, SafeQueue<UI_MESSAGE> *datafeedq);
	~tradeEngine();
	void main_loop(); 
	void settradeInterface(tradeInterface *iface) { tradeinterface = iface; };
	QString tradeEngine::paircodeToLongname(QString paircode);
	void loadData();
	void loadOneCoinData(QString pair);
	void setLiveMode();
	void setHistoryMode();
	bool isAutosaveEnabled() { return autosave; }
	bool replayData(QString exchange, QString  pair, time_t sessionStart);
	void setDisplayReplayInCurrencyPane(bool state) { updateCurrenciesOnReplay = state; };
	void setAutosave(bool state) {		autosave = state;	}
	bool replayActive() { 
		if (replaythread == NULL) return false;
		if (replaythread->is_alive()) return true;
		return false;
	}
	void resetPumps() {
		pumpCount = 0; currentPumps.clear(); resetTradeResults();
	}
	void setAlertSettings(alertSettingsStruct *strt) { 
		alertSettingsStruct *old = alertsConfig;  alertsConfig = strt; if(old) delete old;
	}
	void setTradeSettings(tradeSettingsStruct *strt) {
		if (!tradesConfig) tradesConfig = strt;
		else
		{
			*tradesConfig = *strt;
			delete strt;
		}
	}
	
private:
	void processNewTradeFeedData(PAIR_TRADE_RECORD record);
	void loadLongnameDict(QString filename);
	void loadMarketHistories(QString recordDirectory, QString exchangeName);
	void loadMarketPairHistory(QString exchangeName, QString paircode);
	void sendCurrencyPaneUpdate(marketHistory *coinHistory, QString pairc, double price);
	void resetTradeResults();
	bool testForTradeTrigger(marketHistory *coinHist, PAIR_TRADE_RECORD *record, time_t &builduptime);

private:
	bool liveMode = true;
	bool updateCurrenciesOnReplay = false;
	bool autosave = false;

	unsigned long pumpCount = 0;

	SafeQueue<UI_MESSAGE> *uiMsgQueue; //read by ui thread, written by all others
	SafeQueue<UI_MESSAGE> *feedDataQueue; //read by data engine, written by feed piper reader

	map <QString, QString> longnamesDict;
	map <QString, pumpRecord> currentPumps;

	map<QString, map<QString, marketHistory *>> mktHistory;
	dataReplayThread *replaythread = NULL;

	alertSettingsStruct *alertsConfig = NULL;
	tradeSettingsStruct *tradesConfig = NULL;
	tradeResults *virtualTradeResults;
	tradeInterface *tradeinterface = NULL;
};

