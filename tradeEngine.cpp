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

#include "tradeEngine.h"
#include <iostream>
#include <Windows.h>
#include <rapidjson\reader.h>
#include <rapidjson\filereadstream.h>
#include "qdiriterator.h"
#include "qtimer.h"
#include <sstream>

tradeEngine::tradeEngine(SafeQueue<UI_MESSAGE> *uiq, SafeQueue<UI_MESSAGE> *datafeedq) :
	feedDataQueue(datafeedq),	uiMsgQueue(uiq)
{
	srand(clock());
	loadLongnameDict("C:\\Users\\nia\\PycharmProjects\\autotrader\\trainingdata\\bittrex\\cryptocurrencies.json");
	virtualTradeResults = new tradeResults;
	resetTradeResults();
}


tradeEngine::~tradeEngine()
{
}

void tradeEngine::loadData()
{
	loadMarketHistories("C:\\Users\\nia\\PycharmProjects\\autotrader\\trainingdata\\bittrex\\pairHistories\\", "BTRX");
}

void tradeEngine::resetTradeResults()
{
	virtualTradeResults->partialLosses = 0;
	virtualTradeResults->partialWins = 0;
	virtualTradeResults->totalLosses = 0;
	virtualTradeResults->totalWins = 0;
	virtualTradeResults->profitPercent = 0;
}


void tradeEngine::loadOneCoinData(QString pair)
{
	loadMarketPairHistory("BTRX", pair);
}

void tradeEngine::loadMarketPairHistory(QString exchangeName, QString paircode)
{
	map<QString, marketHistory *> *exchangeHistories = &mktHistory[exchangeName];

	PAIR_TRADE_RECORD dummyRecord;
	dummyRecord.paircode = paircode;
	dummyRecord.tradeType = pairTradeType::eINVALID;
	dummyRecord.exchange = exchangeName;

	processNewTradeFeedData(dummyRecord);

	marketHistory *hist = exchangeHistories->at(dummyRecord.paircode);
	hist->generateNewRecordAlerts(exchangeName);
}

void tradeEngine::loadMarketHistories(QString recordDirectory, QString exchangeName)
{
	QDirIterator coinit(recordDirectory, QDir::AllDirs, QDirIterator::Subdirectories);
	int recs = 0;
	do 
	{
		coinit.next();
		QString pair = coinit.fileName();
		if (pair == "." || pair == "..") continue;

		loadMarketPairHistory(exchangeName, pair);

		recs++;

	} while (coinit.hasNext());
}

QString tradeEngine::paircodeToLongname(QString paircode)
{
	int i = 0;
	for (; paircode.at(i) != '/' && paircode.at(i) != '-' && i < paircode.size()-1; i++);
	if (i >= paircode.size()-1)
		return "ERRNOSLASHDASHCODE";

	QStringRef coincodeRef(&paircode, 0, i);
	auto codeit = longnamesDict.find(coincodeRef.toString());
	if (codeit != longnamesDict.end()) {
		return codeit->second;
	}

	return "ERRNOTFOUNDCODE";
}

void tradeEngine::loadLongnameDict(QString filename)
{

	rapidjson::Document longnamesJSON;
	char buffer[65536];

	QFileInfo targfile(filename);
	if (!targfile.exists())
	{
		QString errorstring("Error: cant load json lonfilenames dict: - ");
		errorstring.append(filename);
		addUILogMsg(errorstring, uiMsgQueue);
	}

	FILE* pFile;
	fopen_s(&pFile, filename.toStdString().c_str(), "rb");
	if (!pFile)
	{
		QString errorstring("Error: cant open json lonfilenames dict: - ");
		errorstring.append(filename);
		addUILogMsg(errorstring, uiMsgQueue);
	}

	rapidjson::FileReadStream is(pFile, buffer, sizeof(buffer));
	longnamesJSON.ParseStream<0, rapidjson::UTF8<>, rapidjson::FileReadStream>(is);

	if (!longnamesJSON.IsObject())
	{
		QString errorstring("Error: Corrupt json lonfilenames dict, abandoning load");
		return;
	}

	fclose(pFile);

	for (rapidjson::Value::ConstMemberIterator itr = longnamesJSON.MemberBegin();
		itr != longnamesJSON.MemberEnd(); ++itr)
	{
		longnamesDict[QString::fromStdString(itr->name.GetString())] = QString::fromStdString(itr->value.GetString());
	}
	
}

void tradeEngine::sendCurrencyPaneUpdate(marketHistory *coinHistory, QString pairc, double price)
{
	float timeSinceFirstReading = coinHistory->minutesSinceStarted();

	//currencies window
	UI_CURRENCY_PAIR_UPDATE *uicpu = new UI_CURRENCY_PAIR_UPDATE;
	uicpu->paircode = pairc;
	uicpu->price = price;

	float recentVolume = coinHistory->volumePerMin(5);

	time_t bleh;
	if (timeSinceFirstReading >= 2)
	{
		uicpu->delta2 = coinHistory->periodDeltaLastFromFirst(2);
		uicpu->twominbuys = coinHistory->getBuyPercentage(2);
	}
	else
	{
		uicpu->delta2 = -93337;
	}

	if (timeSinceFirstReading >= 5)
	{
		//float deltaCurrentFromHIstoric = ((recentVolume / coinHistory->volumePerMin(5)) - 1) * 100;
		uicpu->delta5 = coinHistory->periodDeltaLastFromFirst(5);
		//uicpu->delta5V = deltaCurrentFromHIstoric;
		uicpu->delta5V = -93337;
		uicpu->fiveminbuys = coinHistory->getBuyPercentage(5);
	}
	else
	{
		uicpu->delta5 = -93337;
		uicpu->delta5V = -93337;
	}

	if (timeSinceFirstReading >= 10)
	{
		uicpu->vol10m = coinHistory->volumePerMin(10);
		uicpu->delta10V = ((recentVolume / uicpu->vol10m) - 1) * 100;
		uicpu->delta10 = coinHistory->periodDeltaLastFromFirst(10);
		uicpu->tenminbuys = coinHistory->getBuyPercentage(10);
	}
	else
	{
		uicpu->delta10 = -93337;
		uicpu->delta10V = -93337;
		uicpu->vol10m = 0;
	}

	if (timeSinceFirstReading >= 30)
	{
		uicpu->delta30 = coinHistory->periodDeltaLastFromFirst(30);

		float deltaCurrentFromHIstoric = ((recentVolume / coinHistory->volumePerMin(30)) - 1) * 100;
		uicpu->delta30V = deltaCurrentFromHIstoric;
		uicpu->thirtyminbuys = coinHistory->getBuyPercentage(30);
	}
	else
	{
		uicpu->delta30 = -93337;
	}

	if (timeSinceFirstReading >= 60)
	{
		uicpu->delta60 = coinHistory->periodDeltaLastFromFirst(60);
	}
	else
	{
		uicpu->delta60 = -93337;
	}

	uiMsgQueue->addItem(uicpu);
}

bool tradeEngine::testForTradeTrigger(marketHistory *coinHistory, PAIR_TRADE_RECORD *record, time_t &builduptime)
{
	time_t discard;
	builduptime = 8008;
	bool alertTriggered = false;
	bool alertPossible = true;
	time_t sessionlen = coinHistory->sessionLength(record->timecode);

	//does alert require a minimum btc volume?
	if (alertsConfig->minVolRequired)
	{
		if (sessionlen < (alertsConfig->minimumBtcVolPeriod * 60))
			return false;

		float vol = coinHistory->volumePerMin(alertsConfig->minimumBtcVolPeriod);
		if (vol >= alertsConfig->minimumBtcVolumeCoin)
			alertTriggered = true;
		else
			return false;
	}

	//does alert require a price increase?
	if (alertsConfig->priceIncreaseRequired && alertPossible)
	{
		if (sessionlen < (alertsConfig->alertPriceIncreasePeriod * 60))
			return false;

		//float deltaCurrentFromPeriodLowest = coinHistory->periodLowDeltaFromLast(alertsConfig->alertPriceIncreasePeriod, builduptime);
		float deltaCurrentFromLast = coinHistory->periodDeltaLastFromFirst(alertsConfig->alertPriceIncreasePeriod);
		if (deltaCurrentFromLast >= alertsConfig->alertPriceIncreaseDelta)// && builduptime > 12)
		{
			alertTriggered = true;
		}
		else 
			return false;
	}
	//does alert require a price decrease?
	else if (alertsConfig->priceDecreaseRequired && alertPossible)
	{
		if (sessionlen < (alertsConfig->alertPriceDecreasePeriod * 60))
			return false;

		//float deltaCurrentFromPeriodHighest = coinHistory->periodHighDeltaFromLast(alertsConfig->alertPriceDecreasePeriod, builduptime);
		float deltaCurrentFromLast = coinHistory->periodDeltaLastFromFirst(alertsConfig->alertPriceDecreasePeriod);
		if (deltaCurrentFromLast <= alertsConfig->alertPriceDecreaseDelta)// && builduptime > 12)
		{
			alertTriggered = true;
		}
		else
			return false;
	}

	//does alert require a volume increase?
	if (alertsConfig->volDeltaRequired && alertPossible)
	{
		if (sessionlen < (alertsConfig->alertVolumePeriod * 60))
			return false;

		float recentVolume = coinHistory->volumePerMin(5);
		float historicVolume = coinHistory->volumePerMin(alertsConfig->alertVolumePeriod);

		float deltaCurrentFromHIstoric = ((recentVolume / historicVolume) - 1) * 100;
		if (deltaCurrentFromHIstoric >= alertsConfig->alertVolumeDelta)
		{
			alertTriggered = true;
		}
		else
			return false;
	}

	if (alertsConfig->minBuyPcRequired && alertPossible)
	{
		if (sessionlen < (alertsConfig->minBuyPcPeriod * 60))
			return false;

		float buypc = coinHistory->getBuyPercentage(alertsConfig->minBuyPcPeriod);
		if (buypc >= alertsConfig->minBuyPcVal)
		{
			alertTriggered = true;
		}
		else
			return false;
	}

	if (alertsConfig->maxBuyPCAllowed && alertPossible)
	{
		if (sessionlen < (alertsConfig->maxBuyPCPeriod * 60))
			return false;

		float buypc = coinHistory->getBuyPercentage(alertsConfig->maxBuyPCPeriod);
		if (buypc <= alertsConfig->maxBuyPCVal)
		{
			alertTriggered = true;
		}
		else
			return false;
	}

	return alertTriggered;
}

void tradeEngine::processNewTradeFeedData(PAIR_TRADE_RECORD record)
{

	map<QString, marketHistory *> *exchangeHistory = &mktHistory[record.exchange];

	marketHistory *coinHistory;
	auto it = exchangeHistory->find(record.paircode);
	if (it == exchangeHistory->end()) {
		coinHistory = new marketHistory(record.paircode, paircodeToLongname(record.paircode), record.exchange, &autosave, tradeinterface, tradesConfig);
		mktHistory[record.exchange][record.paircode] = coinHistory;
	}
	else
		coinHistory = it->second;

	if (record.tradeType == pairTradeType::eINVALID)
	{
		if (!liveMode)
		{
			coinHistory->endSession();

			UI_FINISHED_REPLAY *msg = new UI_FINISHED_REPLAY;
			uiMsgQueue->addItem(msg);
		}
		else
		{
			cout << "Error invalid with live" << endl;
		}
		return;
	}

	coinHistory->add_trade_update(record, liveMode);

	time_t discard, builduptime;
	pumpRecord pumprec;
	//the trade trigger detector
	if (!coinHistory->watchedPump(&pumprec))
	{
		bool alertTriggered = testForTradeTrigger(coinHistory, &record, builduptime);
		if (alertTriggered)
		{
			pumprec.pumpID = pumpCount;
			pumprec.startTime = record.timecode;

			coinHistory->startPumpWatch(pumpCount, record, builduptime, alertsConfig);
			coinHistory->setActiveTrade(record.price*1.01, record.timecode);
			pumpCount++;
		}
	}

	//update stats in currencies window
	if (this->liveMode || this->updateCurrenciesOnReplay)
		sendCurrencyPaneUpdate(coinHistory, record.paircode, record.price);



	


}

void tradeEngine::main_loop()
{
	while (true)
	{
		UI_MESSAGE *update = feedDataQueue->waitItem();
		switch (update->msgType)
		{
		case uiMsgType::eTradeFeedData:
			TRADEFEED_UPDATE *tradeupdate = (TRADEFEED_UPDATE *)update;
			processNewTradeFeedData(tradeupdate->rec);

			
			break;

		}
		delete update;
	}
}


void tradeEngine::setLiveMode()
{
	liveMode = true;
}

void tradeEngine::setHistoryMode()
{
	for (auto excit = mktHistory.begin(); excit != mktHistory.end(); excit++)
		for (auto pairit = excit->second.begin(); pairit != excit->second.end(); pairit++)
			pairit->second->endSession();
}


bool tradeEngine::replayData(QString exchange, QString  pair, time_t sessionStart)
{
	while (replaythread)
	{
		if (!replaythread->is_alive())
			break;
		cout << "Waiting for replay thread thermination" << endl;
	}

	liveMode = false;

	marketHistory *relevantHistory = mktHistory.at(exchange).at(pair);
	std::vector<SHORT_PAIR_TRADE_RECORD> *records = relevantHistory->startReplaySession(sessionStart);

	replaythread = new dataReplayThread(uiMsgQueue, feedDataQueue, pair, exchange, records);
	std::thread coinigyFeedThread(&dataReplayThread::ThreadEntry, replaythread);
	coinigyFeedThread.detach();

	return true;
}