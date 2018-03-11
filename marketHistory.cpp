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

#include "marketHistory.h"
#include <rapidjson\reader.h>
#include <rapidjson\filereadstream.h>
#include <rapidjson\filewritestream.h>
#include <rapidjson\writer.h>
#include <random>
#include <fstream>
#include <sstream>
#include <qdir.h>
#include <qdiriterator.h>
#include <iomanip> 

#define MINIMUM_PERIOD_SAVESIZE 500

SafeQueue<UI_MESSAGE> *marketHistory::uiMsgQueue = NULL;
ofstream *marketHistory::logfile = NULL;

marketHistory::marketHistory(QString pairCode, QString fullName, QString exchange, bool *autosaveflag, tradeInterface *tradeif, tradeSettingsStruct *tradesconf) :
	paircode(pairCode), longname(fullName), exchange(exchange), autosave(autosaveflag)
{
	tradeinterface = tradeif;
	tradesConfig = tradesconf;
	periodBins[2];
	periodBins[5];
	periodBins[10];
	periodBins[30];
	periodBins[60];
	resetBins();

	loadAll(QString::fromStdString(getSaveDir()));
}


marketHistory::~marketHistory()
{
}

float marketHistory::volumePerMin(int minutes)
{
	return float(periodBins.at(minutes).totalvolume) / float(minutes);
}

float marketHistory::pricePercentDelta(int minutes)
{
	return  periodBins.at(minutes).priceLowDelta;
}

float marketHistory::periodLowDeltaFromLast(int minutes, time_t &secsfromend)
{
	periodData *dat = &periodBins.at(minutes);
	if (dat->periodRecords.size() < 10)
	{
		secsfromend = 929292;
		return 0;
	}

	SHORT_PAIR_TRADE_RECORD latestRecord = dat->periodRecords.back();
	secsfromend = latestRecord.timecode - periodBins.at(minutes).priceLowTime;
	//secsfromstart = periodBins.at(minutes).priceLowTime - dat->periodRecords.front().timecode;
	return dat->priceLowDelta;
}

float marketHistory::periodHighDeltaFromLast(int minutes, time_t &secsfromend)
{
	periodData *dat = &periodBins.at(minutes);
	if (dat->periodRecords.size() < 10)
	{
		secsfromend = 929292;
		return 0;
	}

	SHORT_PAIR_TRADE_RECORD latestRecord = dat->periodRecords.back();
	secsfromend = latestRecord.timecode - periodBins.at(minutes).priceHighTime;
	//secsfromstart = periodBins.at(minutes).priceHighTime - dat->periodRecords.front().timecode;
	return dat->priceHighDelta;
}

float marketHistory::periodLowDeltaFromFirst(int minutes, time_t &secsfromstart)
{
	periodData *dat = &periodBins.at(minutes);
	if (dat->periodRecords.size() < 10)
	{
		secsfromstart = 929292;
		return 0;
	}

	SHORT_PAIR_TRADE_RECORD latestRecord = dat->periodRecords.back();
	secsfromstart = periodBins.at(minutes).priceLowTime - dat->periodRecords.front().timecode;
	return ((dat->periodRecords.front().price / dat->priceLowest) - 1) * 100 * -1;
}

float marketHistory::periodHighDeltaFromFirst(int minutes, time_t &secsfromstart)
{
	periodData *dat = &periodBins.at(minutes);
	if (dat->periodRecords.size() < 10)
	{
		secsfromstart = 929292;
		return 0;
	}

	SHORT_PAIR_TRADE_RECORD latestRecord = dat->periodRecords.back();
	secsfromstart = periodBins.at(minutes).priceHighTime - dat->periodRecords.front().timecode;
	return ((dat->periodRecords.front().price / dat->priceHighest) - 1) * 100 * -1;
}


float marketHistory::periodDeltaLastFromFirst(int minutes)
{
	periodData *dat = &periodBins.at(minutes);
	if (dat->periodRecords.size() <2 )
	{
		return 0;
	}
	return ((dat->periodRecords.front().price / dat->periodRecords.back().price) - 1) * 100 * -1;
}



float marketHistory::getBuyPercentage(int minutes)
{
	periodData *dat = &periodBins.at(minutes);
	double res = (dat->buyQty / (dat->totalvolume)) * 100;

	return res;
}


void marketHistory::save(time_t periodTime)
{
	save(getSavePath2(periodTime), periodTime);
}


float marketHistory::minutesSinceStarted(time_t sessionStart) 
{
	vector<SHORT_PAIR_TRADE_RECORD> *recvec = &historyPeriods.at(sessionStart);
	if (recvec->empty()) return 0.0;
	return (recvec->back().timecode - sessionStart) / 60.0;
};

void marketHistory::save(string filename, time_t periodTime)
{
	std::vector<SHORT_PAIR_TRADE_RECORD> *recs = &historyPeriods.at(periodTime);
	if (recs->size() < MINIMUM_PERIOD_SAVESIZE) return;

	QFileInfo targfile(QString::fromStdString(filename));

	FILE *savefile;
	errno_t err = fopen_s(&savefile, filename.c_str(), "wb");
	if (err)// non-Windows use "w"?
	{
		std::cerr << "fopen_s Failed to open " << filename << "for writing, err: " << err << std::endl;
		return;
	}


	//mush it all into json
	rapidjson::Document jsondoc;

	char buffer[65536];
	rapidjson::FileWriteStream outstream(savefile, buffer, sizeof(buffer));
	rapidjson::Writer<rapidjson::FileWriteStream> writer{ outstream };



	writer.StartObject();

	writer.Key("TimeStart");
	writer.Uint64(periodTime);

	writer.Key("TimeEnd");
	float timend = minutesSinceStarted(periodTime);
	writer.Double(timend);

	if (timend > 1000)
		cout << "Saved Suspicious length of minutes (" << timend << ") in file " << filename << endl;


	writer.Key("RecordCount");
	writer.Uint64(recs->size());

	writer.Key("Data");
	writer.StartArray();

	time_t lasttime = 0;
	for (auto pairit = recs->begin(); pairit != recs->end(); pairit++)
	{
		writer.StartObject();

		writer.Key("Type");
		writer.Int(pairit->tradeType);

		writer.Key("Price");
		writer.Double(pairit->price);

		writer.Key("Quantity");
		writer.Double(pairit->quantity);

		writer.Key("Time");
		writer.Uint64(pairit->timecode);

		/*
		time_t timediff = lasttime - pairit->timecode;
		if (timediff > 10)
		{
			cerr << "WARNING: file " << filename << " record time " << lasttime <<
				" comes before lower record " << pairit->timecode << ", " <<timediff<<" seconds before"<< endl;
		}
		*/

		lasttime = pairit->timecode;

		writer.EndObject();
	}

	writer.EndArray();

	writer.EndObject();


	fclose(savefile);
}

bool marketHistory::loadToJSON(QString filename, rapidjson::Document *jsondoc)
{
	char buffer[65536];

	QFileInfo targfile(filename);
	if (!targfile.exists())
	{
		std::cout << "error: cant load " << filename.toStdString() << std::endl;
		return false;
	}

	FILE* pFile;
	fopen_s(&pFile, filename.toStdString().c_str(), "rb");
	if (!pFile)
	{
		QFileInfo thisfile(filename);
		if (thisfile.isDir())
			return false;
		std::cerr << "Warning: Could not open file for reading. Abandoning Load." << std::endl;
		return false;
	}

	//load it all from json
	rapidjson::FileReadStream is(pFile, buffer, sizeof(buffer));
	jsondoc->ParseStream<0, rapidjson::UTF8<>, rapidjson::FileReadStream>(is);



	if (!jsondoc->IsObject())
	{
		std::cerr << "Warning: Corrupt file. Abandoning Load." << std::endl;
		if (jsondoc->HasParseError())
		{
			std::cerr << "\t rapidjson parse error " << jsondoc->GetParseError() << " at offset " << jsondoc->GetErrorOffset() << std::endl;
		}
		return false;
	}

	fclose(pFile);
	return true;
}

bool marketHistory::loadAll(QString dirname)
{
	QDir dir(dirname);
	if (!dir.exists())
	{
		dir.mkdir(dirname);
		return false;
	}

	QDirIterator it(dirname, QDir::Files, QDirIterator::NoIteratorFlags);
	int recs = 0;
	do
	{
		it.next();
		QString fullpath = dirname + it.fileName();
		load(fullpath.toStdString());

	} while (it.hasNext());
	return true;
}

bool marketHistory::load(string filename)
{


	rapidjson::Document jsondoc;
	if (!loadToJSON(QString::fromStdString(filename), &jsondoc))
		return false;


	unsigned long periodsloaded = 0;

	rapidjson::Value::ConstMemberIterator periodTimeIt = jsondoc.FindMember("TimeStart");
	time_t periodTime = periodTimeIt->value.GetUint64();
	if (historyPeriods.find(periodTime) != historyPeriods.end()) return false;

	periodTimeIt = jsondoc.FindMember("TimeEnd");
	time_t minsLength = periodTimeIt->value.GetDouble();
	if (minsLength > 1000)
		cout << "Suspicious length of minutes (" << minsLength << ") in file " << filename << endl;

	rapidjson::Value::ConstMemberIterator records = jsondoc.FindMember("Data");
	unsigned long numrecords = records->value.Capacity();
	if (numrecords < MINIMUM_PERIOD_SAVESIZE) //skip low record count history period - not useful for learning
	{
		return false;
	}

	activePeriod = &historyPeriods[periodTime];
	activePeriod->resize(numrecords);
	size_t index = 0;

	time_t lasttime = 0;

	rapidjson::Value::ConstValueIterator recordsIterator = records->value.Begin();
	for (; recordsIterator != records->value.End(); recordsIterator++)
	{

		activePeriod->at(index).timecode = recordsIterator->FindMember("Time")->value.GetUint64();
		activePeriod->at(index).tradeType = (pairTradeType)recordsIterator->FindMember("Type")->value.GetInt();
		activePeriod->at(index).price = recordsIterator->FindMember("Price")->value.GetDouble();
		activePeriod->at(index).quantity = recordsIterator->FindMember("Quantity")->value.GetDouble();
		/*
		time_t timediff = lasttime - activePeriod->at(index).timecode;
		if (timediff > 10)
		{
			cerr << "WARNING: file " << filename << " record time " << lasttime << " comes before lower record " 
				<< activePeriod->at(index).timecode  << " timediff: "<<timediff<< endl;
		}
		*/
		lasttime = activePeriod->at(index).timecode;
		index++;
	}


	return true;
}

string marketHistory::getSaveDir()
{
	string path = "C:\\Users\\nia\\PycharmProjects\\autotrader\\trainingdata\\bittrex\\pairHistories\\";
	QString fname(this->paircode);
	path.append(fname.toStdString());
	path.append("\\");
	return path;
}

string marketHistory::getSavePath2(time_t sessiontime)
{
	stringstream pathstring;
	pathstring << "C:\\Users\\nia\\PycharmProjects\\autotrader\\trainingdata\\bittrex\\pairHistories\\" << paircode.toStdString() << "\\";


	QString dirname= QString::fromStdString(pathstring.str());
	
	QDir dir(dirname);
	if (!dir.exists())
	{
		dir.mkdir(dirname);
	}

	pathstring << sessiontime;
	pathstring << ".json";
	return pathstring.str();
}


void marketHistory::recalculateHighLows(periodData *dat)
{

	double lowestPrice = dat->periodRecords.front().price;
	time_t lowestTime = dat->periodRecords.front().timecode;
	double highestPrice = lowestPrice;
	time_t highestTime = lowestTime;

	//redo these regularly as out of order timestamps can fuck things up
	dat->buyQty = 0;
	dat->sellQty = 0;
	dat->totalvolume = 0;

	for (auto it = dat->periodRecords.begin(); it != dat->periodRecords.end(); it++)
	{
		double recvol = it->quantity*it->price;
		dat->totalvolume += recvol;

		if (it->tradeType == pairTradeType::eBuy)
			dat->buyQty += recvol;
		else if(it->tradeType == pairTradeType::eSell)
			dat->sellQty += recvol;

		

		if (it->price > highestPrice)
		{
			highestPrice = it->price;
			highestTime = it->timecode;
			continue;
		}
		if (it->price < lowestPrice)
		{
			lowestPrice = it->price;
			lowestTime = it->timecode;
			continue;
		}


	}



	dat->priceLowest = lowestPrice;
	dat->priceHighest = highestPrice;
	dat->priceLowTime = lowestTime;
	dat->priceHighTime = highestTime;
	dat->priceLowDelta = ((dat->periodRecords.back().price / dat->priceLowest) - 1) * 100;
	dat->priceHighDelta = ((dat->periodRecords.back().price / dat->priceHighest) - 1) * 100;
}


void marketHistory::clearOldBinRecords()
{
	//clear records older than period
	clock_t timenow = clock();
	for (auto binit = periodBins.begin(); binit != periodBins.end(); binit++)
	{
		int period = binit->first;
		periodData *dat = &binit->second;

		clock_t targetTime = dat->periodRecords.back().timecode - period*60;
		
		int popcount = 0;
		bool needRecalculateHighLows = false;
		for (auto recordit = dat->periodRecords.begin(); recordit != dat->periodRecords.end(); recordit++)
		{
			if (recordit->timecode >= targetTime)
				break;
			
			popcount++;

			if (recordit->price == dat->priceHighest || recordit->price == dat->priceLowest)
				needRecalculateHighLows = true;
			if (!needRecalculateHighLows)
			{
				double recordVolume = (recordit->quantity * recordit->price);
				if (recordit->tradeType == pairTradeType::eBuy)
				{
					binit->second.buyQty -= recordVolume;
				}
				else if (recordit->tradeType == pairTradeType::eSell)
					{

					binit->second.sellQty -= recordVolume;
				}

				binit->second.totalvolume -= recordVolume;
			}
		}
		
		//if ((binit->second.buyQty + binit->second.sellQty) != binit->second.totalvolume)
		//	needRecalculateHighLows = true;
	
		for (int pi = 0; pi < popcount; pi++)
			dat->periodRecords.pop_front();

		if (needRecalculateHighLows || dat->buyQty < 0 || dat->sellQty < 0) //happens with very small doubles sometimes 
		{
			recalculateHighLows(dat);
		}


	}
}

void marketHistory::add_trade_update(PAIR_TRADE_RECORD tradeUpdate, bool livedata)
{

	//give a few seconds head start to take into account that feed may be delayed and so may our order
	if (isActiveTrade() && (tradeUpdate.timecode > (pumprec.startTime + TRADE_START_DELAY)))
	{
		//still need to buy more
		if (activeTrade.buyCapitalRemaining > 0)
		{
			//try buying more a
			if (tradeUpdate.price <= activeTrade.startPrice)
			{
				double valueOffered = tradeUpdate.price * tradeUpdate.quantity;
				double capitalSpent = fmin(activeTrade.buyCapitalRemaining, valueOffered);
				double amountBought = capitalSpent / tradeUpdate.price;

				activeTrade.buyCapitalSpent += capitalSpent;
				activeTrade.buyCapitalRemaining -= capitalSpent;
				activeTrade.sellTargetRemaining += amountBought;
			}

			if (activeTrade.buyCapitalRemaining > 0)
			{
				if (activeTrade.buyCapitalRemaining == activeTrade.initialBuyCapital)
				{
					time_t endTime = pumprec.startTime + tradesConfig->haltBuyAnyTime;
					if (tradeUpdate.timecode >= endTime)
					{
						setTradeComplete(0, tradeUpdate.timecode);

						UI_PUMP_DONE_NOTIFY *msg = new UI_PUMP_DONE_NOTIFY;
						msg->pumpID = pumprec.pumpID;
						msg->timeend = tradeUpdate.timecode;
						msg->tradeResult = eFailBuyAny;
						msg->tradeProfit = 0;
						msg->tradeDuration = tradeUpdate.timecode - activeTrade.secondsStarted;
						msg->deltaPeriodLow = msg->deltaPeriodLow = 0;
						msg->deltaPeriod = msg->volPeriod = 0;
						msg->hightime = msg->lowtime = 0;
						msg->percentTraded = 0;
						msg->endPrice = tradeUpdate.price;
						uiMsgQueue->addItem(msg);

						stopPumpWatch();
					}
				}

				else
				{
					time_t endTime = pumprec.startTime + tradesConfig->haltBuyAllTime;
					if (tradeUpdate.timecode >= endTime)
					{
						activeTrade.buyCapitalRemaining = 0;
					}
				}
			}

		}

		//bought what we need, trying to sell it
		if (activeTrade.buyCapitalRemaining == 0)
		{
			double targdiv = (tradeUpdate.price / getTargetPrice()) * 100;
			double lossdiv = (tradeUpdate.price / getStoplossPrice()) * 100;
			//cout << "Trying to sell. Price " << tradeUpdate.price << " %targ: " << targdiv << " targloss: " << lossdiv << endl;
			//price high, sell at target price
			if (tradeUpdate.price >= getTargetPrice())
			{
				double amountToSell = fmin(activeTrade.sellTargetRemaining, tradeUpdate.quantity);
				double valueSold = amountToSell * tradeUpdate.price;

				activeTrade.sellTargetRemaining -= amountToSell;
				activeTrade.capitalReturned += valueSold;
				//cout << "Selling at loss of " << (activeTrade.startPrice /tradeUpdate.price) << endl;
			}//price low, sell at current price
			else if (tradeUpdate.price <= getStoplossPrice())
			{
				double amountToSell = fmin(activeTrade.sellTargetRemaining, tradeUpdate.quantity);
				double valueSold = amountToSell * tradeUpdate.price;

				activeTrade.sellTargetRemaining -= amountToSell;
				activeTrade.capitalReturned += valueSold;

				//cout << "Selling at profit of " << lossdiv << endl;
			}
			else
			{	//time getting on
				time_t lateExitStart = pumprec.startTime + tradesConfig->lateExitStart;
				if (tradeUpdate.timecode >= lateExitStart) 
				{
					//exit early if after threshold (soft time cap) and price high enough
					if (tradeUpdate.price >= activeTrade.earlyExitPrice)
					{
						double amountToSell = fmin(activeTrade.sellTargetRemaining, tradeUpdate.quantity);
						double valueSold = amountToSell * tradeUpdate.price;

						activeTrade.sellTargetRemaining -= amountToSell;
						activeTrade.capitalReturned += valueSold;
					}
					else
					{

						//sell at current price if reached very end of our time (hard time cap)
						time_t endTime = pumprec.startTime + tradesConfig->hardTimeLimit;
						if (tradeUpdate.timecode > endTime)
						{
							double amountToSell = fmin(activeTrade.sellTargetRemaining, tradeUpdate.quantity);
							double valueSold = amountToSell * tradeUpdate.price;

							activeTrade.sellTargetRemaining -= amountToSell;
							activeTrade.capitalReturned += valueSold; 
						}

					}
				}
			}
		}


		assert(activeTrade.sellTargetRemaining >= 0);

		if (activeTrade.buyCapitalRemaining == 0 && activeTrade.sellTargetRemaining == 0)
		{

			double profit = ((activeTrade.capitalReturned / activeTrade.buyCapitalSpent) - 1) * 100;
			profit -= (TRADE_FEE * 2);
			setTradeComplete(profit, tradeUpdate.timecode);

			UI_PUMP_DONE_NOTIFY *msg = new UI_PUMP_DONE_NOTIFY;
			msg->pumpID = pumprec.pumpID;
			msg->timeend = tradeUpdate.timecode;
			msg->deltaPEriodHigh = periodHighDeltaFromFirst(30, msg->hightime);
			msg->deltaPeriod = periodDeltaLastFromFirst(30);
			msg->deltaPeriodLow = periodLowDeltaFromFirst(30, msg->lowtime);
			msg->volPeriod = volumePerMin(30);
			getTradeResults(msg->tradeResult, msg->tradeProfit, msg->tradeDuration);
			msg->percentTraded = (activeTrade.buyCapitalSpent / tradesConfig->buyValue) *100;
			msg->endPrice = tradeUpdate.price;
			uiMsgQueue->addItem(msg);

			stopPumpWatch();
		}


	}

	if (inLiveSession){
		add_live_trade_update(tradeUpdate);
		return;
	}

	if (inReplaySession) {
		add_replay_trade_update(tradeUpdate);
		return;
	}



	if (livedata)
	{
		activityMutex.lock();
		startLiveSession(tradeUpdate.timecode, livedata);
		activityMutex.unlock();
		add_live_trade_update(tradeUpdate);
	}
	else
	{
		activityMutex.lock();
		startReplaySession(tradeUpdate.timecode);
		activityMutex.unlock();
		add_replay_trade_update(tradeUpdate);
	}


}

void marketHistory::update_bins_with_transaction(SHORT_PAIR_TRADE_RECORD rec)
{


	for (auto binit = periodBins.begin(); binit != periodBins.end(); binit++)
	{
		binit->second.periodRecords.push_back(rec);

		if (binit->second.periodRecords.size() == 1) //first record
		{
			binit->second.priceHighest = rec.price;
			binit->second.priceLowest = rec.price;
			binit->second.priceHighTime = rec.timecode;
			binit->second.priceLowTime = rec.timecode;
			binit->second.priceHighDelta = 0;
			binit->second.priceLowDelta = 0;
		}
		else if (rec.price > binit->second.priceHighest) //new high added
		{
			binit->second.priceHighest = rec.price;
			binit->second.priceHighTime = rec.timecode;
			binit->second.priceHighDelta = 0;
			binit->second.priceLowDelta = ((rec.price / binit->second.priceLowest) - 1) * 100;
		}
		else if (rec.price < binit->second.priceLowest) //new low added
		{
			binit->second.priceLowest = rec.price;
			binit->second.priceLowTime = rec.timecode;
			binit->second.priceLowDelta = 0;
			binit->second.priceHighDelta = ((rec.price / binit->second.priceHighest) - 1) * 100;
		}
		else
		{
			binit->second.priceLowDelta = ((rec.price / binit->second.priceLowest) - 1) * 100;
			binit->second.priceHighDelta = ((rec.price / binit->second.priceHighest) - 1) * 100;
		}
		//cout << binit->second.priceLowDelta << endl;
		double recordVolume = (rec.quantity * rec.price);
		binit->second.totalvolume += recordVolume;

		if (rec.tradeType == pairTradeType::eBuy)
		{			
			binit->second.buyQty += recordVolume;
		}
		else
		{
			binit->second.sellQty += recordVolume;
		}
	}

	clearOldBinRecords();

//#define DORECORDSTRINGS
#ifdef DORECORDSTRINGS
	//delete old recordstrings
	clock_t targetTime = this->periodBins.at(30).periodRecords.back().timecode - (30 * 60);
	int popcount = 0;
	for (auto recordit = this->period30RecordStrings.begin(); recordit != period30RecordStrings.end(); recordit++)
	{
		if (recordit->first < targetTime)
			popcount++;
	}
	for (int i = 0; i < popcount; i++)
		period30RecordStrings.pop_front();

#define DUMP_PRISE_RISE_PC 6
	stringstream recstring;
	recstring.precision(2);
	recstring << "2m$%: " << std::fixed << std::setw(6) << setfill(' ') << periodDeltaLastFromFirst(2) << ", ";
	recstring << "5m$%: " << std::fixed << std::setw(6) << setfill(' ') << periodDeltaLastFromFirst(5) << ", ";
	recstring << "10m$%: " << std::fixed << std::setw(6) << setfill(' ') << periodDeltaLastFromFirst(10) << ", ";
	recstring << "30m$%: " << std::fixed << std::setw(6) << setfill(' ') << periodDeltaLastFromFirst(30) << " | ";
	recstring << "2mBy%: " << std::fixed << std::setw(6) << setfill(' ') << getBuyPercentage(2) << ", ";
	recstring << "5mBy%: " << std::fixed << std::setw(6) << setfill(' ') << getBuyPercentage(5) << ", ";
	recstring << "10mBy%: " << std::fixed << std::setw(6) << setfill(' ') << getBuyPercentage(10) << ", ";
	recstring << "30mBy%: " << std::fixed << std::setw(6) << setfill(' ') << getBuyPercentage(30) << " | ";
	float vol2min = volumePerMin(2);
	recstring << "5mVd%: " << std::fixed << std::setw(6) << setfill(' ') << ((vol2min / volumePerMin(5)) - 1) * 100 << ", ";
	recstring << "10mVd%: " << std::fixed << std::setw(6) << setfill(' ') << ((vol2min / volumePerMin(10)) - 1) * 100 << ", ";
	recstring << "30mVd%: " << std::fixed << std::setw(6) << setfill(' ') << ((vol2min / volumePerMin(30)) - 1) * 100 << ", ";

	string recs = recstring.str();
	period30RecordStrings.push_back(make_pair(rec.timecode, recs));

	if (periodDeltaLastFromFirst(30) > DUMP_PRISE_RISE_PC && rec.timecode > lastPeriodDump + (15*60))
	{
		float vol10m = volumePerMin(10); //ensure high volume over last 10 mins so its not a flash pump by one buyer
		if (vol10m > 2)
		{
			lastPeriodDump = rec.timecode;
			stringstream firstline;
			firstline << "\nCurrency " << this->paircode.toStdString() << " price rose above "
				<< DUMP_PRISE_RISE_PC << "% in 30 mins. 30m vol: " << volumePerMin(30) << endl;

			cout << firstline.str();
			*logfile << firstline.str();

			*logfile << "\t Setup stats " << period30RecordStrings.front().second << endl;

			for (auto it = period30RecordStrings.begin(); it != period30RecordStrings.end(); it++)
			{
				int minsFromStart = (it->first - lastPeriodDump) / 60;
				*logfile << "\t" << minsFromStart << ": " << it->second << endl;
			}
			*logfile << "Dump complete" << endl << endl;
			*logfile << std::flush;
		}
	}
#endif

}

void marketHistory::add_replay_trade_update(PAIR_TRADE_RECORD tradeUpdate)
{
	assert(inReplaySession);

	SHORT_PAIR_TRADE_RECORD rec;
	rec.price = tradeUpdate.price;
	rec.quantity = tradeUpdate.quantity;
	rec.timecode = tradeUpdate.timecode;
	rec.tradeType = tradeUpdate.tradeType;


	update_bins_with_transaction(rec);

}

void marketHistory::add_live_trade_update(PAIR_TRADE_RECORD tradeUpdate)
{
	assert(inLiveSession);

	if (isWatchedPump || hasBeenActive) //send to past trades incase it didnt finish after 10 min watch period
	{
		if (tradeUpdate.timecode >= lastMsgTime)
		{
			tradeinterface->sendPrice(this->paircode, tradeUpdate.price);
			lastMsgTime = tradeUpdate.timecode;
		}

		
	}

	SHORT_PAIR_TRADE_RECORD rec;
	rec.price = tradeUpdate.price;
	rec.quantity = tradeUpdate.quantity;
	rec.timecode = tradeUpdate.timecode;
	rec.tradeType = tradeUpdate.tradeType;
	activePeriod->push_back(rec);

	update_bins_with_transaction(rec);

	//save each coin every 5-9 mins
	if (*autosave)
	{
		time_t timenow;
		time(&timenow);
		if (timenow > lastSave + 300)
		{
			save(activePeriodStart);
			lastSave = timenow;
			int timevary = rand() & 0xff;
			lastSave += timevary;
		}
	}
}

void marketHistory::generateNewRecordAlerts(QString exchgname)
{
	for (auto periodit = historyPeriods.begin(); periodit != historyPeriods.end(); periodit++)
	{
		UI_SESSION_RECORDS_LOADED *recLoadMsg = new UI_SESSION_RECORDS_LOADED;
		recLoadMsg->exchangeName = exchgname;
		recLoadMsg->currencyPair = paircode;
		recLoadMsg->recordCount = periodit->second.size();
		recLoadMsg->startTime = periodit->first;
		if (!periodit->second.empty())
			recLoadMsg->endTime = periodit->second.back().timecode;
		else
			recLoadMsg->endTime = periodit->first;

		uiMsgQueue->addItem(recLoadMsg);
	}
}

void marketHistory::resetBin(periodData *dat)
{
	dat->periodRecords.clear();
	dat->priceHighest = 0;
	dat->priceLowest = 0;
	dat->totalvolume = 0;
	dat->volumedelta = 0;
	dat->priceLowDelta = 0;
	dat->priceHighDelta = 0;
	dat->priceHighTime = 0;
	dat->priceLowTime = 0;
	dat->buyQty = 0;
	dat->sellQty = 0;
}

void marketHistory::resetBins()
{
	resetBin(&periodBins.at(2));
	resetBin(&periodBins.at(5));
	resetBin(&periodBins.at(10));
	resetBin(&periodBins.at(30));
	resetBin(&periodBins.at(60));
}

void marketHistory::endSession()
{


	if (inLiveSession)
	{
		cout << "Coin " << paircode.toStdString() << " ending live session " << endl;
		activityMutex.lock();

	save(activePeriodStart);
	inLiveSession = false;
	activePeriod = NULL;

	isWatchedPump = false;
	resetActiveTrade();

	resetBins();
	activityMutex.unlock(); 
	}
	else if (inReplaySession)
	{

		cout << "Coin " << paircode.toStdString() << " ending replay session " << endl;
		activePeriod = NULL;
		resetBins();
		inReplaySession = false;
		resetActiveTrade();
		isWatchedPump = false;

	}
	else
	{
		cout << "Error no session" << endl;
	}
	period30RecordStrings.clear();
	lastPeriodDump = 0;
}


void marketHistory::resetActiveTrade()
{
	activeTrade.isPending = false;
}

void  marketHistory::startPumpWatch(size_t pumpID, PAIR_TRADE_RECORD record, time_t buildupTime, alertSettingsStruct *alertsconf)
{
	time_t discard;

	tradeinterface->tradeAlert(this->paircode, record.price);

	pumprec.startTime = record.timecode;
	pumprec.pumpID = pumpID;

	UI_PUMP_START_NOTIFY *msg = new UI_PUMP_START_NOTIFY;
	msg->pumpID = pumprec.pumpID;
	msg->paircode = record.paircode;
	msg->sessionTime = minutesSinceStarted();
	msg->timestart = pumprec.startTime;
	msg->buyPrice = record.price;
	/*
	if (alertsconf->priceIncreaseRequired)
	{
		msg->delta2Low = periodLowDeltaFromLast(2, discard);
		msg->delta5Low = periodLowDeltaFromLast(5, discard);
		msg->delta10Low = periodLowDeltaFromLast(10, discard);
		msg->delta60Low = periodLowDeltaFromLast(60, discard);

	}
	else
	{
		msg->delta2Low = periodHighDeltaFromLast(2, discard);
		msg->delta5Low = periodHighDeltaFromLast(5, discard);
		msg->delta10Low = periodHighDeltaFromLast(10, discard);
		msg->delta60Low = periodHighDeltaFromLast(60, discard);
	}
	*/
	msg->delta2Low = periodDeltaLastFromFirst(2);
	msg->delta5Low = periodDeltaLastFromFirst(5);
	msg->delta10Low = periodDeltaLastFromFirst(10);
	msg->delta60Low = periodDeltaLastFromFirst(60);

	msg->vol30 = volumePerMin(30);
	msg->vol5 = volumePerMin(5);
	msg->vol10 = volumePerMin(10);
	msg->vol60 = volumePerMin(60);

	msg->buys2 = getBuyPercentage(2);
	msg->buys5 = getBuyPercentage(5);
	msg->buys10 = getBuyPercentage(10);
	msg->buys30 = getBuyPercentage(30);
	msg->buys60 = getBuyPercentage(60);

	uiMsgQueue->addItem(msg);

	isWatchedPump = true;
	hasBeenActive = true;


}


void marketHistory::setActiveTrade(double buyprice, time_t buytime)
{
	activeTrade.isPending = true;

	activeTrade.startPrice = buyprice;
	if (tradesConfig->stopLossLow >= tradesConfig->targetSellHigh)
		cerr << "Warning, target percent " << tradesConfig->targetSellHigh << 
		" is not bigger than loss percent " << tradesConfig->stopLossLow << endl;

	activeTrade.targetPrice = activeTrade.startPrice + activeTrade.startPrice*tradesConfig->targetSellHigh*0.01;
	activeTrade.lossPrice = activeTrade.startPrice + activeTrade.startPrice*tradesConfig->stopLossLow*0.01;
	activeTrade.earlyExitPrice = activeTrade.startPrice + activeTrade.startPrice * tradesConfig->lateExitPercent*0.01;

	activeTrade.secondsStarted = buytime;
	activeTrade.initialBuyCapital = tradesConfig->buyValue;
	activeTrade.buyCapitalRemaining = tradesConfig->buyValue;

	activeTrade.sellTargetRemaining = 0;
	activeTrade.capitalReturned = 0;
	activeTrade.buyCapitalSpent = 0;
}


void marketHistory::setTradeComplete(double profit, time_t timestamp)
{
	activeTrade.isPending = false;
	if (profit >= tradesConfig->targetSellHigh)
		activeTrade.result = eTradeResult::eTargetWin;
	else if (profit <= tradesConfig->stopLossLow)
		activeTrade.result = eTradeResult::eStopLoss;
	else if (profit >= 0)
		activeTrade.result = eTradeResult::ePartialWin;
	else
		activeTrade.result = eTradeResult::ePartialLoss;

	activeTrade.duration = timestamp - activeTrade.secondsStarted;
	activeTrade.profit = profit;



}
