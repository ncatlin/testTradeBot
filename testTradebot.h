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

#include <QtWidgets/QMainWindow>
#include "ui_testTradebot.h"
#include "tradeDataReader.h"
#include "uiMessage.h"
#include "tradeEngine.h"
#include "tradeInterface.h"
#include <fstream>

struct tradeResultStats{
	size_t wins, pwins, losses, plosses;
	size_t trades;
	double profit, misses;
};

class testTradebot : public QMainWindow
{
	Q_OBJECT

public:
	testTradebot(QWidget *parent = Q_NULLPTR);

private slots:
	void readUIQ();
	void doDataLoad();
	void doOneCoinLoad();
	void clearPumps();
	void pumpsExport();

	void historyModeToggled(bool newState);
	void liveModeToggled(bool newState); 
	void toggleReplayCurrencypaneUpdate(bool state) 
	{ statsEngine->setDisplayReplayInCurrencyPane(state); }
	void toggleReplayAutosave(bool state)	{
		statsEngine->setAutosave(state);
	}
	void doHistoryReplay();
	void doUnfavourite();
	void doPumpFavourite();
	void doCurrencyTableFavourite();
	void expandSelectAllHist();
	void applyAlertSettings();
	void applyTradeSettings();

protected:
	void closeEvent(QCloseEvent *event) override;

private:	
	void handle_UI_string_msg(UI_STRING_MESSAGE *msg);
	void handle_currencyUpdate(UI_CURRENCY_PAIR_UPDATE *msg);
	void handle_sessionRecordsLoaded(UI_SESSION_RECORDS_LOADED * msg);
	void handle_favouriteUpdate(UI_CURRENCY_PAIR_UPDATE *msg);
	void handle_pumpStart(UI_PUMP_START_NOTIFY * msg);
	void handle_pumpEnded(UI_PUMP_DONE_NOTIFY * msg);

	void startNextReplay();

	void prepareHistTreeMenu(const QPoint & pos);
	void prepareFavouritesMenu(const QPoint & pos);
	void preparePumpsMenu(const QPoint & pos);
	void prepareCurrenciesMenu(const QPoint & pos);

	bool isFavourite(QString paircode);
	void setCurrencyCellDelta(float delta, int row, int col, bool favourite);
	void setBuysCellDelta(QTableWidget *table, float delta, int row, int col, bool favourite);
	void setPumpCellDelta(float delta, int row, int col);
	void initialiseDeltaColours(); 
	void setPumpCellNone(int row, int col);
	QColor deltaToColour(float delta);
	void resetTradeResults();
	void setTradeResultStatsLabels();

private:

	Ui::testTradebotClass ui;
	tradeDataReader *dataReader; 
	tradeInterface *tradeinterface;
	tradeEngine *statsEngine;
	vector<QColor> negativeColours;
	vector<QColor> positiveColours;
	map<size_t, int> pumpRows;

	std::ofstream logfstream;

	SafeQueue<UI_MESSAGE> uiMsgQueue; //read by ui thread, written by all others
	SafeQueue<UI_MESSAGE> feedDataQueue; //read by data engine, written by feed piper reader

	list <UI_QUEUE_REPLAY *> scheduledReplays;
	map<QString, bool> favouritesDict;
	std::mutex favsmutex;
	tradeResultStats tradeResults;
};

class NumericSortWidgetItem : public QTableWidgetItem {
public:
	bool operator <(const NumericSortWidgetItem &other) const
	{
		return numericValue < other.getNumericValue();
	}
	void setNumericValue(double val) { numericValue = val; }
	double getNumericValue() const { return numericValue; }

private:
	double numericValue;
};
