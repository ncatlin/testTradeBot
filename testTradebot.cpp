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
#include "testTradebot.h"
#include <thread>
#include <qtimer.h>
#include <qdatetime.h>
#include <qtreewidget.h>
#include <sstream>
#include <iomanip>
#include <numberFormatDelegate.h>
#include <qtextstream.h>

#include "tradesettings.h"

testTradebot::testTradebot(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	marketHistory::uiMsgQueue = &uiMsgQueue;

	logfstream.open("C:\\Users\\nia\\Documents\\Visual Studio 2017\\Projects\\testTradebot\\x64\\Release\\dumplog.txt", std::fstream::out);
	marketHistory::logfile = &logfstream;

	tradeinterface = new tradeInterface(&uiMsgQueue);
	std::thread tradeinterfacethread(&tradeInterface::ThreadEntry, tradeinterface);
	tradeinterfacethread.detach();

	statsEngine = new tradeEngine(&uiMsgQueue, &feedDataQueue);
	statsEngine->settradeInterface(tradeinterface);
	std::thread statsEngineThread(&tradeDataReader::ThreadEntry, statsEngine);
	statsEngineThread.detach();

	dataReader = new tradeDataReader(&uiMsgQueue, &feedDataQueue);
	std::thread coinigyFeedThread(&tradeDataReader::ThreadEntry, dataReader);
	coinigyFeedThread.detach();



	connect(ui.recordHistoryTree, &QTreeWidget::customContextMenuRequested, this, &testTradebot::prepareHistTreeMenu);
	connect(ui.favouritesTable, &QTableWidget::customContextMenuRequested, this, &testTradebot::prepareFavouritesMenu);
	connect(ui.pumpsTable, &QTableWidget::customContextMenuRequested, this, &testTradebot::preparePumpsMenu);
	connect(ui.currenciesTable, &QTableWidget::customContextMenuRequested, this, &testTradebot::prepareCurrenciesMenu);

	
	initialiseDeltaColours();

	QTimer *timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(readUIQ()));
	timer->start(20);

	ui.currenciesTable->setItemDelegateForColumn(1, new NumberFormatDelegate(this));
	applyAlertSettings();
	applyTradeSettings();
	resetTradeResults();
}

void testTradebot::resetTradeResults()
{
	tradeResults.profit = 0;
	tradeResults.wins = 0;
	tradeResults.pwins = 0;
	tradeResults.plosses = 0;
	tradeResults.losses = 0;
	tradeResults.trades = 0;
	tradeResults.misses = 0;
};


void testTradebot::prepareHistTreeMenu(const QPoint & pos)
{
	QTreeWidget *tree = ui.recordHistoryTree;

	QMenu menu(this);

	QAction *newAct = new QAction(QIcon(":/Resource/warning32.ico"), tr("&Load All"), this);
	newAct->setStatusTip(tr("Loads all exchange history data (slow!)"));
	connect(newAct, SIGNAL(triggered()), this, SLOT(doDataLoad()));
	menu.addAction(newAct);

	newAct = new QAction(QIcon(":/Resource/warning32.ico"), tr("&Load EBST-BTC"), this);
	newAct->setStatusTip(tr("Loads EBST-BTC pair data (for testing)"));
	connect(newAct, SIGNAL(triggered()), this, SLOT(doOneCoinLoad()));
	menu.addAction(newAct);

	QList<QTreeWidgetItem *> selecteditems = ui.recordHistoryTree->selectedItems();
	if (!selecteditems.empty())
	{
		newAct = new QAction(QIcon(":/Resource/warning32.ico"), tr("&Replay trades"), this);
		newAct->setStatusTip(tr("Feeds the trades through the trade engine as if they were happening in real time"));
		connect(newAct, SIGNAL(triggered()), this, SLOT(doHistoryReplay()));
		menu.addAction(newAct);
	}

	QPoint pt(pos);
	menu.exec(tree->mapToGlobal(pos));
}



void testTradebot::prepareFavouritesMenu(const QPoint & pos)
{
	QTableWidget *table = ui.favouritesTable;
	QTableWidgetItem *selecteditem = table->itemAt(pos);
	if (!selecteditem) return;

	QTableWidgetItem *widget = table->itemAt(pos);
	QMenu menu(this); 	
	
	QAction *newAct = new QAction(QIcon(":/Resource/warning32.ico"), tr("&Remove Favourites"), this);
	connect(newAct, SIGNAL(triggered()), this, SLOT(doUnfavourite()));
	menu.addAction(newAct);


	QPoint pt(pos);
	menu.exec(table->mapToGlobal(pos));
}

void testTradebot::preparePumpsMenu(const QPoint & pos)
{
	QTableWidget *table = ui.pumpsTable;
	QTableWidgetItem *selecteditem = table->itemAt(pos);
	if (!selecteditem) return;

	QMenu menu(this);

	QAction *newAct = new QAction(QIcon(":/Resource/warning32.ico"), tr("&Add to favourites"), this);
	connect(newAct, SIGNAL(triggered()), this, SLOT(doPumpFavourite()));
	menu.addAction(newAct);


	QPoint pt(pos);
	menu.exec(table->mapToGlobal(pos));
}

void testTradebot::prepareCurrenciesMenu(const QPoint & pos)
{
	QTableWidget *table = ui.currenciesTable;
	QTableWidgetItem *selecteditem = table->itemAt(pos);
	if (!selecteditem) return;

	QMenu menu(this);
	QAction *newAct = new QAction(QIcon(":/Resource/warning32.ico"), tr("&Add to favourites"), this);
	connect(newAct, SIGNAL(triggered()), this, SLOT(doCurrencyTableFavourite()));
	menu.addAction(newAct);


	QPoint pt(pos);
	menu.exec(table->mapToGlobal(pos));
}



void testTradebot::doHistoryReplay()
{

	QList<QTreeWidgetItem *> selecteditems = ui.recordHistoryTree->selectedItems();
	if (selecteditems.empty()) return;

	QTreeWidgetItem *hist = selecteditems.back();

	for (auto it = selecteditems.begin(); it != selecteditems.end(); it++)
	{
		QTreeWidgetItem *itam = *it;
		QVariant var = itam->data(1, Qt::UserRole);
		time_t sessionStart = var.toDouble();
		if (!sessionStart) continue;

		UI_QUEUE_REPLAY *msg = new UI_QUEUE_REPLAY;
		msg->itam = itam;
		msg->sessionstart = sessionStart;
		uiMsgQueue.addItem(msg);
	}

	UI_QUEUE_REPLAY *msg = new UI_QUEUE_REPLAY;
	msg->itam = 0;
	msg->sessionstart = 0;
	uiMsgQueue.addItem(msg);
}

bool testTradebot::isFavourite(QString paircode)
{
	favsmutex.lock();
	bool result = (favouritesDict.find(paircode) != favouritesDict.end());
	favsmutex.unlock();
	return result;
}

void testTradebot::doUnfavourite()
{
	QTableWidget *table = ui.favouritesTable;

	QList<QTableWidgetItem *> selecteditems = table->selectedItems();
	if (selecteditems.empty()) return;

	for (auto itemit = selecteditems.begin(); itemit != selecteditems.end(); itemit++)
	{
		QTableWidgetItem *itam = *itemit;
		int itemrow = itam->row();
		QString nametext = table->item(itemrow, 0)->data(Qt::UserRole).toString();
		if (isFavourite(nametext))
		{
			table->removeRow(itam->row());
			favouritesDict.erase(itam->text());
		}
	}

}
void testTradebot::doPumpFavourite()
{
	QTableWidget *table = ui.pumpsTable;
	QList<QTableWidgetItem *> selecteditems = table->selectedItems();
	if (selecteditems.empty()) return;

	for (auto itemit = selecteditems.begin(); itemit != selecteditems.end(); itemit++)
	{
		QTableWidgetItem *itam = *itemit;
		int itemrow = itam->row();
		QString nametext = table->item(itemrow, 0)->data(Qt::UserRole).toString();
		if (!isFavourite(nametext))
		{
			favsmutex.lock();
			favouritesDict[nametext] = true;
			favsmutex.unlock();
		}
	}
}
void testTradebot::doCurrencyTableFavourite()
{

	QTableWidget *table = ui.currenciesTable;
	QList<QTableWidgetItem *> selecteditems = table->selectedItems();
	if (selecteditems.empty()) return;

	for (auto itemit = selecteditems.begin(); itemit != selecteditems.end(); itemit++)
	{
		QTableWidgetItem *itam = *itemit;
		int itemrow = itam->row();
		QString nametext = table->item(itemrow, 0)->data(Qt::UserRole).toString();
		if (!isFavourite(nametext))
		{
			favsmutex.lock();
			favouritesDict[nametext] = true;
			favsmutex.unlock();
		}
	}
}


void testTradebot::handle_sessionRecordsLoaded(UI_SESSION_RECORDS_LOADED * msg)
{
	QTreeWidgetItem * exchangeItem;
	QList<QTreeWidgetItem *> results = ((QTreeWidget *)ui.recordHistoryTree)->findItems(msg->exchangeName, Qt::MatchExactly, 0);
	if (results.empty())
	{
		exchangeItem = new QTreeWidgetItem();
		exchangeItem->setText(0, msg->exchangeName);
		ui.recordHistoryTree->addTopLevelItem(exchangeItem);
		ui.recordHistoryTree->sortColumn();
	}
	else
	{
		assert(results.size() == 1);
		exchangeItem = results.front();
	}

	QTreeWidgetItem * currencyPairItem = NULL;
	int exchgPairCount = exchangeItem->childCount();
	for (int i = 0; i < exchgPairCount; i++)
		if (exchangeItem->child(i)->text(0) == msg->currencyPair)
			currencyPairItem = exchangeItem->child(i);

	if (currencyPairItem == NULL)
	{
		currencyPairItem = new QTreeWidgetItem();
		currencyPairItem->setText(0, msg->currencyPair);
		exchangeItem->addChild(currencyPairItem);
		exchangeItem->sortChildren(0, Qt::SortOrder::AscendingOrder);
	}

	int sessionCount = currencyPairItem->childCount();
	for (int i = 0; i < sessionCount; i++)
	{
		QVariant var = currencyPairItem->child(i)->data(1, Qt::UserRole);
		time_t sessionStart = var.toDouble();
		if (sessionStart == msg->startTime)
			return;
	}

	double duration = (msg->endTime - msg->startTime) / 3600.0;
	std::stringstream ss;
	ss.precision(3);
	char buffer[80];
	ss << msg->startTime << ".json ";
	strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", localtime(&msg->startTime));
	ss << buffer;
	ss << " - ";
	strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", localtime(&msg->endTime));
	ss << buffer;
	ss << " ( " << msg->recordCount << " records, " << duration << " hours)";

	QTreeWidgetItem* sessionItem = new QTreeWidgetItem();
	sessionItem->setText(0, QString::fromStdString(ss.str()));
	sessionItem->setData(1, Qt::UserRole, msg->startTime);
	currencyPairItem->addChild(sessionItem);

	currencyPairItem->sortChildren(0, Qt::SortOrder::AscendingOrder);
}


void testTradebot::doDataLoad()
{
	ui.statusBar->showMessage("Loading market history...");
	statsEngine->loadData();
	ui.statusBar->showMessage("Market history loaded!",8);
}

void testTradebot::doOneCoinLoad()
{
	ui.statusBar->showMessage("Loading EBST history...");
	statsEngine->loadOneCoinData("EBST-BTC");
	ui.statusBar->showMessage("Market history loaded!", 8);
}

QColor testTradebot::deltaToColour(float delta)
{
	if (delta == 0)
		return Qt::white;

	int magnitude;
	int absdelta = int(fabs(delta));

	if (absdelta >= 10)
		magnitude = 10;
	else
		magnitude = absdelta;

	if (magnitude < 0 || magnitude > 10)
		return Qt::white;

	if (delta > 0)
		return positiveColours.at(magnitude);
	else
		return negativeColours.at(magnitude);
}

void testTradebot::initialiseDeltaColours()
{
	negativeColours.resize(11);
	negativeColours[0] = QColor(255, 229, 229);
	negativeColours[1] = QColor(255, 221, 221);
	negativeColours[2] = QColor(255, 214, 214);
	negativeColours[3] = QColor(255, 196, 196);
	negativeColours[4] = QColor(255, 191, 191);
	negativeColours[5] = QColor(255, 186, 186);
	negativeColours[6] = QColor(255, 176, 176);
	negativeColours[7] = QColor(255, 172, 172);
	negativeColours[8] = QColor(255, 166, 166);
	negativeColours[9] = QColor(255, 160, 160);
	negativeColours[10] = QColor(255, 156, 156);

	positiveColours.resize(11);
	positiveColours[0] = QColor(237, 255, 239);
	positiveColours[1] = QColor(221, 255, 221);
	positiveColours[2] = QColor(214, 255, 214);
	positiveColours[3] = QColor(196, 255, 196);
	positiveColours[4] = QColor(191, 255, 191);
	positiveColours[5] = QColor(186, 255, 186);
	positiveColours[6] = QColor(176, 255, 176);
	positiveColours[7] = QColor(172, 255, 172);
	positiveColours[8] = QColor(166, 255, 166);
	positiveColours[9] = QColor(156, 255, 156);
	positiveColours[10] = QColor(145, 255, 145);

}

void testTradebot::handle_UI_string_msg(UI_STRING_MESSAGE *msg)
{
	QDateTime localtime(QDateTime::currentDateTime());
	QString dateTimeString = localtime.toString("ddd-d-MMM hh:mm:ss");
	this->ui.textLog->append(dateTimeString+":"+msg->stringData);
}


void testTradebot::setCurrencyCellDelta(float delta, int row, int col, bool favourite = false)
{
	NumericSortWidgetItem *item = new NumericSortWidgetItem();
	if (delta == -93337)
	{
		item->setText("-"); //todo- write a percentage progress value here?
	}
	else
	{
		item->setData(Qt::DisplayRole, delta);
		item->setNumericValue(delta);
		item->setBackgroundColor(deltaToColour(delta));
	}
	item->setTextAlignment(Qt::AlignCenter);
	
	if (!favourite)
		ui.currenciesTable->setItem(row, col, item);
	else
		ui.favouritesTable->setItem(row, col, item);
}

void testTradebot::setBuysCellDelta(QTableWidget *table, float delta, int row, int col, bool favourite = false)
{
	NumericSortWidgetItem *item = new NumericSortWidgetItem();
	if (delta == -93337)
	{
		item->setText("-"); //todo- write a percentage progress value here?
	}
	else
	{
		item->setData(Qt::DisplayRole, delta);
		item->setNumericValue(delta);
		float convpercent = (delta - 50) / 5.0;
		item->setBackgroundColor(deltaToColour(convpercent));
	}
	item->setTextAlignment(Qt::AlignCenter);

	table->setItem(row, col, item);

}

void testTradebot::handle_currencyUpdate(UI_CURRENCY_PAIR_UPDATE *msg)
{
	QList<QTableWidgetItem *> results = ui.currenciesTable->findItems(msg->paircode, Qt::MatchStartsWith);
	int row;
	if (results.empty())
	{
		ui.currenciesTable->setRowCount(ui.currenciesTable->rowCount() + 1);
		row = ui.currenciesTable->rowCount() - 1;

		QString namestring = msg->paircode + " - " + statsEngine->paircodeToLongname(msg->paircode);
		QTableWidgetItem *item = new QTableWidgetItem(namestring);
		item->setData(Qt::UserRole, msg->paircode);
		item->setTextAlignment(Qt::AlignLeft);
		ui.currenciesTable->setItem(row, 0, item);
	}
	else
	{
		row = results.front()->row();
	}


	QTableWidgetItem *numberItem = new QTableWidgetItem();
	numberItem->setTextAlignment(Qt::AlignCenter);
	numberItem->setData(Qt::DisplayRole, msg->price);
	ui.currenciesTable->setItem(row, 1, numberItem);

	numberItem = new QTableWidgetItem();
	numberItem->setTextAlignment(Qt::AlignCenter);
	numberItem->setData(Qt::DisplayRole, msg->vol10m);
	ui.currenciesTable->setItem(row, 2, numberItem);

	setCurrencyCellDelta(msg->delta2, row, 3);
	setCurrencyCellDelta(msg->delta5, row, 4);
	setCurrencyCellDelta(msg->delta10, row, 5);
	setCurrencyCellDelta(msg->delta30, row, 6);
	setCurrencyCellDelta(msg->delta60, row, 7);

	setBuysCellDelta(ui.currenciesTable, msg->twominbuys, row, 8);
	setBuysCellDelta(ui.currenciesTable, msg->fiveminbuys, row, 9);
	setBuysCellDelta(ui.currenciesTable, msg->tenminbuys, row, 10);
	setBuysCellDelta(ui.currenciesTable, msg->thirtyminbuys, row,11);

	setCurrencyCellDelta(msg->delta5V, row, 12);
	setCurrencyCellDelta(msg->delta10V, row, 13);
	setCurrencyCellDelta(msg->delta30V, row, 14);

}

void testTradebot::handle_favouriteUpdate(UI_CURRENCY_PAIR_UPDATE *msg)
{
	QList<QTableWidgetItem *> results = ui.favouritesTable->findItems(msg->paircode, Qt::MatchStartsWith);
	int row;
	if (results.empty())
	{
		ui.favouritesTable->insertRow(ui.favouritesTable->rowCount());
		row = ui.favouritesTable->rowCount() - 1;

		QString namestring = msg->paircode + " - " + statsEngine->paircodeToLongname(msg->paircode);
		QTableWidgetItem *item = new QTableWidgetItem(namestring);
		item->setTextAlignment(Qt::AlignLeft);
		ui.favouritesTable->setItem(row, 0, item);
	}
	else
	{
		row = results.front()->row();
	}


	NumericSortWidgetItem *numberItem = new NumericSortWidgetItem();
	numberItem->setTextAlignment(Qt::AlignCenter);
	numberItem->setData(Qt::DisplayRole, msg->price);
	ui.favouritesTable->setItem(row, 1, numberItem);

	numberItem = new NumericSortWidgetItem();
	numberItem->setTextAlignment(Qt::AlignCenter);
	numberItem->setData(Qt::DisplayRole, msg->vol10m);
	ui.favouritesTable->setItem(row, 2, numberItem);


	setCurrencyCellDelta(msg->delta2, row, 3, true);
	setCurrencyCellDelta(msg->delta5, row, 4, true);
	setCurrencyCellDelta(msg->delta10, row, 5, true);
	setCurrencyCellDelta(msg->delta30, row, 6, true);
	setCurrencyCellDelta(msg->delta60, row, 7, true);


	setBuysCellDelta(ui.favouritesTable, msg->twominbuys, row, 8);
	setBuysCellDelta(ui.favouritesTable, msg->fiveminbuys, row, 9);
	setBuysCellDelta(ui.favouritesTable, msg->tenminbuys, row, 10);
	setBuysCellDelta(ui.favouritesTable, msg->thirtyminbuys, row, 11);

	setCurrencyCellDelta(msg->delta5V, row, 12, true);
	setCurrencyCellDelta(msg->delta10V, row, 13, true);
	setCurrencyCellDelta(msg->delta30V, row, 14, true);
}


void testTradebot::setPumpCellDelta(float delta, int row, int col)
{
	NumericSortWidgetItem *item = new NumericSortWidgetItem();

	item->setData(Qt::DisplayRole,delta);
	item->setNumericValue(delta);
	item->setBackgroundColor(deltaToColour(delta));

	item->setTextAlignment(Qt::AlignCenter);


	ui.pumpsTable->setItem(row, col, item);

}

void testTradebot::setPumpCellNone(int row, int col)
{
	NumericSortWidgetItem *item = new NumericSortWidgetItem();
	item->setText("-");
	item->setTextAlignment(Qt::AlignCenter);
	ui.pumpsTable->setItem(row, col, item);

}



void testTradebot::handle_pumpStart(UI_PUMP_START_NOTIFY * msg)
{
	QTableWidgetItem *newPumpItem = new QTableWidgetItem();


	ui.pumpsTable->insertRow(ui.pumpsTable->rowCount());
	int row = ui.pumpsTable->rowCount() - 1;
	pumpRows[msg->pumpID] = row;

	//0 time
	char buffer[80];
	strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", localtime(&msg->timestart));
	QTableWidgetItem *sitem = new QTableWidgetItem(QString::fromStdString(string(buffer)));
	ui.pumpsTable->setItem(row, 0, sitem);

	//1 pair name
	QString namestring = msg->paircode + "/" + statsEngine->paircodeToLongname(msg->paircode);
	sitem = new QTableWidgetItem(namestring);
	sitem->setTextAlignment(Qt::AlignLeft);
	if(isFavourite(msg->paircode))
		sitem->setBackgroundColor(QColor(247, 236, 104));
	ui.pumpsTable->setItem(row, 1, sitem);

	//2 volume per min over last 10 mins, to see if coin is big enough
	NumericSortWidgetItem *item = new NumericSortWidgetItem();
	item->setData(Qt::DisplayRole, QString::number(msg->vol10, 'f', 2));
	item->setNumericValue(msg->vol10);
	item->setTextAlignment(Qt::AlignCenter);
	ui.pumpsTable->setItem(row, 2, item);

	//3 price we bought at
	item = new NumericSortWidgetItem();
	item->setData(Qt::DisplayRole, QString::number(msg->buyPrice, 'f', 8));
	item->setTextAlignment(Qt::AlignCenter);
	ui.pumpsTable->setItem(row, 3, item);

	//4 alert price delta over 60min low
	if (msg->sessionTime > 40)
		setPumpCellDelta(msg->delta60Low, row, 4);
	else
		setPumpCellNone(row, 4);

	//5 alert price delta over 10min low
	setPumpCellDelta(msg->delta10Low, row, 5);

	//6 alert price delta over 5min low
	setPumpCellDelta(msg->delta5Low, row, 6);

	//7 alert price delta over 2min low
	setPumpCellDelta(msg->delta2Low, row, 7);


	//8 2 min vol increase over hour average
	if (msg->sessionTime > 40)
	{
		float deltaVol5From60 = ((float(msg->vol5) / float(msg->vol60))-1)*100;
		setPumpCellDelta(deltaVol5From60, row, 8);
	}
	else
		setPumpCellNone(row, 8);

	//9 2 min vol increase over 10m average
	float deltaVol5From30 = ((float(msg->vol5) / float(msg->vol30))-1)*100;
	setPumpCellDelta(deltaVol5From30, row, 9);

	//10 2 min vol increase over 5m average
	float deltaVol5From10 = ((float(msg->vol5) / float(msg->vol10))-1)*100;
	setPumpCellDelta(deltaVol5From10, row, 10);


	if (msg->sessionTime > 2)
	{
		setBuysCellDelta(ui.pumpsTable, msg->buys2, row, 11);
	}
	if (msg->sessionTime > 5)
	{
		setBuysCellDelta(ui.pumpsTable, msg->buys5, row, 12);
	}	if (msg->sessionTime > 10)
	{
		setBuysCellDelta(ui.pumpsTable, msg->buys10, row, 13);
	}	if (msg->sessionTime > 30)
	{
		setBuysCellDelta(ui.pumpsTable, msg->buys30, row, 14);
	}	if (msg->sessionTime > 60)
	{
		setBuysCellDelta(ui.pumpsTable, msg->buys60, row, 15);
	}



	//increment pump counts table for this pair
	QList<QTableWidgetItem *> results = ui.pumpCountTable->findItems(msg->paircode, Qt::MatchStartsWith);
	if (results.empty())
	{
		ui.pumpCountTable->insertRow(ui.pumpCountTable->rowCount());
		row = ui.pumpCountTable->rowCount() - 1;

		QString namestring = msg->paircode + " - " + statsEngine->paircodeToLongname(msg->paircode);
		QTableWidgetItem *mitem = new QTableWidgetItem(namestring);
		mitem->setTextAlignment(Qt::AlignLeft);
		ui.pumpCountTable->setItem(row, 0, mitem);

		NumericSortWidgetItem *numberItem = new NumericSortWidgetItem();
		numberItem->setTextAlignment(Qt::AlignCenter);
		numberItem->setData(Qt::DisplayRole, 1);
		ui.pumpCountTable->setItem(row, 1, numberItem);
	}
	else
	{
		row = results.front()->row();
		QVariant curcount = ui.pumpCountTable->item(row, 1)->data(Qt::DisplayRole);
		ui.pumpCountTable->item(row, 1)->setData(Qt::DisplayRole, curcount.toInt() + 1);
	}




}

void testTradebot::handle_pumpEnded(UI_PUMP_DONE_NOTIFY * msg)
{
	int row = pumpRows[msg->pumpID];

	//11 lowest price in 10 mins since alert
	setPumpCellDelta(msg->deltaPeriodLow, row, 16);

	//12 seconds til that happened
	QTableWidgetItem *secsitem = new QTableWidgetItem();
	secsitem->setData(Qt::DisplayRole, msg->lowtime);
	secsitem->setTextAlignment(Qt::AlignCenter);
	ui.pumpsTable->setItem(row, 17, secsitem);



	//13 higest price in 10 mins since alert
	setPumpCellDelta(msg->deltaPEriodHigh, row, 18);

	//14 secs til highest price reached
	secsitem = new QTableWidgetItem();
	secsitem->setData(Qt::DisplayRole, msg->hightime);
	secsitem->setTextAlignment(Qt::AlignCenter);
	ui.pumpsTable->setItem(row, 19, secsitem);

	//15 difference between start and end price
	setPumpCellDelta(msg->deltaPeriod, row, 20);

	//16 btc per min in last 10 mins
	setPumpCellDelta(msg->volPeriod, row, 21);

	//17 time ended
	char buffer[80];
	strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", localtime(&msg->timeend));
	QTableWidgetItem *item = new QTableWidgetItem(QString::fromStdString(string(buffer)));
	ui.pumpsTable->setItem(row, 22, item);

	//18 price at the end
	item = new NumericSortWidgetItem();
	item->setData(Qt::DisplayRole, QString::number(msg->endPrice, 'f', 8));
	item->setTextAlignment(Qt::AlignCenter);
	ui.pumpsTable->setItem(row, 23, item);

	//19 trade result string
	QString valuetraded = "";
	if (msg->percentTraded < 100)
	{
		if (msg->tradeResult == eTradeResult::eFailBuyAny)
			tradeResults.misses += 1;
		else
		{
			valuetraded = " (" + QString::number(msg->percentTraded, 'f', 0) + "%)";
			tradeResults.misses += (1 - (msg->percentTraded*0.01));
		}
	}

	item = new QTableWidgetItem();
	switch (msg->tradeResult)
	{
	case eTradeResult::eTargetWin:
		{
		item->setText("WIN"+valuetraded);
		item->setBackgroundColor(QColor(156, 255, 156));
		tradeResults.wins += 1;
		break;
		}
	case eTradeResult::ePartialWin:
		{

		item->setText("Small Profit" + valuetraded);
		item->setBackgroundColor(QColor(221, 255, 221));
		tradeResults.pwins += 1;
		break;
		}
	case eTradeResult::ePartialLoss:
		{
		item->setText("Small Loss" + valuetraded);
		item->setBackgroundColor(QColor(255, 221, 221));
		tradeResults.plosses += 1;
		break;
		}
	case eTradeResult::eStopLoss:
		{

		item->setText("LOSS" + valuetraded);
		item->setBackgroundColor(QColor(255, 160, 160));
		tradeResults.losses += 1;
		break;
		}
	case eTradeResult::eFailBuyAny:
	{
		item->setText("No Buy");
		item->setBackgroundColor(QColor(195, 195, 195));

		break;
	}
	}
	item->setTextAlignment(Qt::AlignCenter);
	ui.pumpsTable->setItem(row, 24, item);
	


	//21 profit/loss
	setPumpCellDelta(msg->tradeProfit, row, 25);


	//22 time trade took
	NumericSortWidgetItem *nitem = new NumericSortWidgetItem();
	nitem->setData(Qt::DisplayRole, msg->tradeDuration);
	nitem->setNumericValue(msg->tradeDuration);
	nitem->setTextAlignment(Qt::AlignCenter);
	if (msg->tradeDuration < 6)
		nitem->setBackgroundColor(QColor(255, 221, 155));
	ui.pumpsTable->setItem(row, 26, nitem);

	tradeResults.trades += 1;
	tradeResults.profit += msg->tradeProfit;

	setTradeResultStatsLabels();
}

void testTradebot::setTradeResultStatsLabels()
{
	stringstream winlabeltext;
	winlabeltext << "Wins (Total: " << tradeResults.wins << " partial: " << tradeResults.pwins << ")";
	ui.winsLabel->setText(QString::fromStdString(winlabeltext.str()));

	stringstream losslabeltext;
	losslabeltext << "Losses (Total: " << tradeResults.losses << " partial: " << tradeResults.plosses << ")";
	ui.lossesLabel->setText(QString::fromStdString(losslabeltext.str()));

	float losses = tradeResults.losses + tradeResults.plosses;
	float wins = tradeResults.wins + tradeResults.pwins;
	if (losses == 0 && wins > 0)
		ui.winPercentLabel->setText("Win %: 100");
	else if (losses > 0 && wins == 0)
		ui.winPercentLabel->setText("Win %: 0");
	else
	{
		float winpc = (wins / (wins + losses)) * 100;
		ui.winPercentLabel->setText("Win %: " + QString::number(winpc, 'f', 2));

	}


	double avgprof = tradeResults.profit / float(tradeResults.trades);
	ui.avgProfitLabel->setText("Profit Avg: " + QString::number(avgprof, 'f', 2) + "%");
	ui.profitTotalLabel->setText("Profit Tot: " + QString::number(tradeResults.profit, 'f', 2) + "%");

	float misspc = (float(tradeResults.misses) / float(tradeResults.trades)) * 100;
	ui.nobuyLabel->setText("NoBuys: " + QString::number(misspc, 'f', 1) + "%");
}

void testTradebot::startNextReplay()
{
	if (scheduledReplays.empty())
		return;

	UI_QUEUE_REPLAY *msg = scheduledReplays.front();
	scheduledReplays.pop_front();

	QTreeWidgetItem *itam = (QTreeWidgetItem *)msg->itam;
	QString pair = itam->parent()->text(0);
	QString exchange = itam->parent()->parent()->text(0);


	stringstream statusMsg;
	statusMsg << "Replaying market data for " << exchange.toStdString() << ", pair " << pair.toStdString() << " session " << msg->sessionstart << endl;
	ui.statusBar->showMessage(QString::fromStdString(statusMsg.str()));

	if (!statsEngine->replayData(exchange, pair, msg->sessionstart))
		ui.statusBar->showMessage("Failed to start replay!", 8);

	delete msg;
}

void testTradebot::readUIQ()
{
	unsigned long updcount = 0;
	while (!uiMsgQueue.empty())
	{
		UI_MESSAGE *msg =  uiMsgQueue.waitItem();

		switch (msg->msgType)
		{
			case uiMsgType::eTextLog:
			{
				handle_UI_string_msg((UI_STRING_MESSAGE *)msg);
				break; 
			}
			case uiMsgType::eCurrencyUpdate:
			{
				handle_currencyUpdate((UI_CURRENCY_PAIR_UPDATE *)msg);
				bool isfav = isFavourite(((UI_CURRENCY_PAIR_UPDATE *)msg)->paircode);
				if (isfav)
					handle_favouriteUpdate((UI_CURRENCY_PAIR_UPDATE *)msg);

				break;
			}
			case uiMsgType::eSessionRecordsLoaded:
			{
				handle_sessionRecordsLoaded((UI_SESSION_RECORDS_LOADED *)msg);
				break;
			}

			case uiMsgType::ePumpStart:
			{
				ui.statusBar->showMessage("Pump Started "+((UI_PUMP_START_NOTIFY *)msg)->paircode);
				handle_pumpStart((UI_PUMP_START_NOTIFY *)msg);
				break;
			}
			case uiMsgType::ePumpEnd:
			{
				ui.statusBar->showMessage("Pump Ended");
				handle_pumpEnded((UI_PUMP_DONE_NOTIFY *)msg);
				break;
			}
			case uiMsgType::eReplaySchedule:
			{
				time_t sessstart = ((UI_QUEUE_REPLAY *)msg)->sessionstart;
				UI_QUEUE_REPLAY *cppy = new UI_QUEUE_REPLAY;
				cppy->itam = ((UI_QUEUE_REPLAY *)msg)->itam;
				cppy->sessionstart = sessstart;

				if (sessstart == 0 && cppy->itam == 0)
				{
					ui.statusBar->showMessage("All replays finished!");
					//if brute forcing settings would be good time to signal next round
				}
				else
				{
					scheduledReplays.push_back(cppy);
					bool replayactive = statsEngine->replayActive();
					if (!replayactive)
					{

						startNextReplay();
					}
				}
				break;
			}
			case uiMsgType::eReplayDone:
			{
				ui.statusBar->showMessage("Replay Finished");

				if (!scheduledReplays.empty())
					startNextReplay();
			}
			case uiMsgType::eFeedConnect:
			{
				if (((UI_FEED_CONNECTION *)msg)->tradeFeed)
				{
					if (((UI_FEED_CONNECTION *)msg)->connected)
					{
						ui.statusBar->showMessage("Feed Pipe: Connected!");
						ui.feedConLabel->setText("Feed Pipe: Connected");
					}
					else
					{
						ui.statusBar->showMessage("Feed Pipe: Disconnected!");
						ui.feedConLabel->setText("Feed Pipe: Disconnected");
					}
				}
				else if (((UI_FEED_CONNECTION *)msg)->tradeInterface)
				{
					if (((UI_FEED_CONNECTION *)msg)->connected)
					{
						ui.statusBar->showMessage("Trade Interface: Connected!");
						ui.interfaceConLabel->setText("Trade Interface: Connected");
					}
					else
					{
						ui.statusBar->showMessage("Trade Interface: Disconnected!");
						ui.interfaceConLabel->setText("Trade Interface: Disconnected");
					}
				}
			}

		}

		delete msg;
	}
}

void testTradebot::historyModeToggled(bool newState)
{
	if (newState)
	{
		dataReader->stopFeedPipe();
		statsEngine->setHistoryMode();
		ui.liveModeToggle->setChecked(false);
	}
}

void testTradebot::liveModeToggled(bool newState)
{
	if (newState)
	{
		if (!dataReader->startFeedPipe())
			return;
		statsEngine->setLiveMode();
		ui.historyModeToggle->setChecked(false);
	}
}

void testTradebot::closeEvent(QCloseEvent *event)
{
	dataReader->stopFeedPipe();
	//force save of any active sessions
	statsEngine->setHistoryMode();
}

void testTradebot::expandSelectAllHist()
{
	for (auto exi = 0; exi < ui.recordHistoryTree->topLevelItemCount(); exi++)
	{
		QTreeWidgetItem *excg = ui.recordHistoryTree->topLevelItem(exi);
		excg->setExpanded(true);
		for (auto coini = 0; coini < excg->childCount(); coini++)
		{
			QTreeWidgetItem *coin = excg->child(coini);
			coin->setExpanded(true);
			for (auto sessi = 0; sessi < coin->childCount(); sessi++)
			{
				QTreeWidgetItem *sess = coin->child(sessi);
				sess->setSelected(true);
			}
		}
	}
}

void testTradebot::clearPumps()
{

	ui.pumpsTable->setRowCount(0);
	pumpRows.clear();
	statsEngine->resetPumps();
	resetTradeResults();
	setTradeResultStatsLabels();
}


void testTradebot::pumpsExport()
{
	QString filename = "C:\\Users\\nia\\PycharmProjects\\autotrader\\trainingdata\\bittrex\\pumpsExport.csv";
	QFile f(filename);

	int reccount = 0;
	if (f.open(QFile::WriteOnly | QFile::Truncate))
	{
		QTextStream data(&f);
		QStringList strList;

		for (int c = 0; c < ui.pumpsTable->columnCount(); ++c)
		{
			strList <<
				"\" " +
				ui.pumpsTable->horizontalHeaderItem(c)->data(Qt::DisplayRole).toString() +
				"\" ";
		}
		data << strList.join(";") + "\n";

		for (int r = 0; r < ui.pumpsTable->rowCount(); ++r)
		{
			strList.clear();
			for (int c = 0; c < ui.pumpsTable->columnCount(); ++c)
			{
				QTableWidgetItem *itam = ui.pumpsTable->item(r, c);
				if (!itam)
					strList << "\" ??  \" ";
				else
					strList << itam->text();
			}
			data << strList.join(";") + "\n";
			reccount++;
		}
		f.close();
	}

	cout << "Exported " << reccount << " pumps as .csv to " << filename.toStdString() << endl;
}

void testTradebot::applyAlertSettings()
{
	alertSettingsStruct *newstruc = new alertSettingsStruct;

	newstruc->priceIncreaseRequired = ui.alertPriceIncreaseEnable->isChecked();
	newstruc->alertPriceIncreaseDelta = ui.alertPriceDeltaText->text().toFloat();
	newstruc->alertPriceIncreasePeriod = ui.alertPriceDeltaPeriodComb->currentText().toInt();

	newstruc->priceDecreaseRequired = ui.alertPriceDecreaseEnable->isChecked();
	newstruc->alertPriceDecreaseDelta = ui.priceDecEdit->text().toFloat();
	newstruc->alertPriceDecreasePeriod = ui.alertPriceDecPeriodComb->currentText().toInt();

	newstruc->volDeltaRequired = ui.alertVolDeltaEnable->isChecked();
	newstruc->alertVolumeDelta = ui.altertVolumeDeltaText->text().toFloat();
	newstruc->alertVolumePeriod = ui.alertVolDeltaPeriodComb->currentText().toInt();

	newstruc->minVolRequired = ui.minCoinVolEnable->isChecked();
	newstruc->minimumBtcVolumeCoin = ui.alertMinVolumeText->text().toFloat();
	newstruc->minimumBtcVolPeriod = ui.alertMinVolPeriodComb->currentText().toInt();

	newstruc->maxBuyPCAllowed = ui.alertBuyPcAtLeastEnable->isChecked();
	newstruc->maxBuyPCVal = ui.alertBuyPcAtLeastText->text().toFloat();
	newstruc->maxBuyPCPeriod = ui.alertBuyPcAtLeastCombo->currentText().toInt();

	newstruc->minBuyPcRequired = ui.alertBuyPcEnable->isChecked();
	newstruc->minBuyPcVal = ui.alertBuyPcText->text().toFloat();
	newstruc->minBuyPcPeriod = ui.alertBuyPcCombo->currentText().toInt();

	statsEngine->setAlertSettings(newstruc);
}


void testTradebot::applyTradeSettings()
{
	tradeSettingsStruct *newstruc = new tradeSettingsStruct;
	newstruc->targetSellHigh = ui.targSellEdit->text().toFloat();
	cout << "Target sell percentage set to " << newstruc->targetSellHigh << endl;
	newstruc->stopLossLow = ui.stopLossSellEdit->text().toFloat();
	newstruc->lateExitPercent = ui.lateTradeExitPct->text().toFloat();
	newstruc->lateExitStart = ui.lateTradeStart->text().toInt();
	newstruc->buyValue = ui.buyValEdit->text().toFloat();
	newstruc->haltBuyAnyTime = ui.haltbuyanyedit->text().toInt();
	newstruc->haltBuyAllTime = ui.haltbuyalledit->text().toInt();
	newstruc->hardTimeLimit = ui.timeLimitEdit->text().toInt();

	cout << "Buy amount set to " << newstruc->buyValue << endl;
	statsEngine->setTradeSettings(newstruc);
}