#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <stdlib.h>
#include "game.h"
#include "client.h"
#include "tablezone.h"
#include "handzone.h"
#include "carddatabase.h"
#include "dlg_startgame.h"
#include "counter.h"
#include "gamescene.h"

Game::Game(CardDatabase *_db, Client *_client, GameScene *_scene, QMenuBar *menuBar, QObject *parent)
	: QObject(parent), db(_db), client(_client), scene(_scene), started(false), currentPhase(-1)
{
	connect(client, SIGNAL(gameEvent(const ServerEventData &)), this, SLOT(gameEvent(const ServerEventData &)));

	aNextPhase = new QAction(this);
	connect(aNextPhase, SIGNAL(triggered()), this, SLOT(actNextPhase()));
	aNextTurn = new QAction(this);
	connect(aNextTurn, SIGNAL(triggered()), this, SLOT(actNextTurn()));
	aTap = new QAction(this);
	aUntap = new QAction(this);
	aDoesntUntap = new QAction(this);
	aFlip = new QAction(this);
	aAddCounter = new QAction(this);
	aRemoveCounter = new QAction(this);
	aSetCounters = new QAction(this);
	connect(aSetCounters, SIGNAL(triggered()), this, SLOT(actSetCounters()));
	aMoveToTopLibrary = new QAction(this);
	aMoveToBottomLibrary = new QAction(this);
	aMoveToGraveyard = new QAction(this);
	aMoveToExile = new QAction(this);

	gameMenu = menuBar->addMenu(QString());
	gameMenu->addAction(aNextPhase);
	gameMenu->addAction(aNextTurn);
	
	cardMenu = menuBar->addMenu(QString());
	cardMenu->addAction(aTap);
	cardMenu->addAction(aUntap);
	cardMenu->addAction(aDoesntUntap);
	cardMenu->addSeparator();
	cardMenu->addAction(aFlip);
	cardMenu->addSeparator();
	cardMenu->addAction(aAddCounter);
	cardMenu->addAction(aRemoveCounter);
	cardMenu->addAction(aSetCounters);
	cardMenu->addSeparator();
	moveMenu = cardMenu->addMenu(QString());
	
	moveMenu->addAction(aMoveToTopLibrary);
	moveMenu->addAction(aMoveToBottomLibrary);
	moveMenu->addAction(aMoveToGraveyard);
	moveMenu->addAction(aMoveToExile);
	
	cardMenuHandlers.insert(aTap, &Game::actTap);
	cardMenuHandlers.insert(aUntap, &Game::actUntap);
	cardMenuHandlers.insert(aDoesntUntap, &Game::actDoesntUntap);
	cardMenuHandlers.insert(aFlip, &Game::actFlip);
	cardMenuHandlers.insert(aAddCounter, &Game::actAddCounter);
	cardMenuHandlers.insert(aRemoveCounter, &Game::actRemoveCounter);
	cardMenuHandlers.insert(aMoveToTopLibrary, &Game::actMoveToTopLibrary);
	cardMenuHandlers.insert(aMoveToBottomLibrary, &Game::actMoveToBottomLibrary);
	cardMenuHandlers.insert(aMoveToGraveyard, &Game::actMoveToGraveyard);
	cardMenuHandlers.insert(aMoveToExile, &Game::actMoveToExile);
	
	QHashIterator<QAction *, CardMenuHandler> i(cardMenuHandlers);
	while (i.hasNext()) {
		i.next();
		connect(i.key(), SIGNAL(triggered()), this, SLOT(cardMenuAction()));
	}

	dlgStartGame = new DlgStartGame(db);
	connect(dlgStartGame, SIGNAL(newDeckLoaded(const QStringList &)), client, SLOT(submitDeck(const QStringList &)));
	connect(dlgStartGame, SIGNAL(finished(int)), this, SLOT(readyStart()));
	
	retranslateUi();
	
	PendingCommand_ListPlayers *pc = client->listPlayers();
	connect(pc, SIGNAL(playerListReceived(QList<ServerPlayer>)), this, SLOT(playerListReceived(QList<ServerPlayer>)));
}

Game::~Game()
{
	qDebug("Game destructor");
	for (int i = 0; i < players.size(); i++) {
		emit playerRemoved(players.at(i));
		delete players.at(i);
	}
	delete gameMenu;
	delete cardMenu;
}

void Game::retranslateUi()
{
	gameMenu->setTitle(tr("&Game"));
	aNextPhase->setText(tr("Next &phase"));
	aNextPhase->setShortcut(tr("Ctrl+Space"));
	aNextTurn->setText(tr("Next &turn"));
	aNextTurn->setShortcuts(QList<QKeySequence>() << QKeySequence(tr("Ctrl+Return")) << QKeySequence(tr("Ctrl+Enter")));
	
	cardMenu->setTitle(tr("C&ard"));
	aTap->setText(tr("&Tap"));
	aUntap->setText(tr("&Untap"));
	aDoesntUntap->setText(tr("Toggle &normal untapping"));
	aFlip->setText(tr("&Flip"));
	aAddCounter->setText(tr("&Add counter"));
	aRemoveCounter->setText(tr("&Remove counter"));
	aSetCounters->setText(tr("&Set counters..."));
	aMoveToTopLibrary->setText(tr("&top of library"));
	aMoveToBottomLibrary->setText(tr("&bottom of library"));
	aMoveToGraveyard->setText(tr("&graveyard"));
	aMoveToGraveyard->setShortcut(tr("Ctrl+Del"));
	aMoveToExile->setText(tr("&exile"));
	
	moveMenu->setTitle(tr("&Move to"));
	
	for (int i = 0; i < players.size(); ++i)
		players[i]->retranslateUi();
}

Player *Game::addPlayer(int playerId, const QString &playerName, bool local)
{
	Player *newPlayer = new Player(playerName, playerId, local, db, client, this);
	scene->addPlayer(newPlayer);

	connect(newPlayer, SIGNAL(sigShowCardMenu(QPoint)), this, SLOT(showCardMenu(QPoint)));
	connect(newPlayer, SIGNAL(logMoveCard(Player *, QString, CardZone *, int, CardZone *, int)), this, SIGNAL(logMoveCard(Player *, QString, CardZone *, int, CardZone *, int)));
	connect(newPlayer, SIGNAL(logCreateToken(Player *, QString)), this, SIGNAL(logCreateToken(Player *, QString)));
	connect(newPlayer, SIGNAL(logSetCardCounters(Player *, QString, int, int)), this, SIGNAL(logSetCardCounters(Player *, QString, int, int)));
	connect(newPlayer, SIGNAL(logSetTapped(Player *, QString, bool)), this, SIGNAL(logSetTapped(Player *, QString, bool)));
	connect(newPlayer, SIGNAL(logSetCounter(Player *, QString, int, int)), this, SIGNAL(logSetCounter(Player *, QString, int, int)));
	connect(newPlayer, SIGNAL(logSetDoesntUntap(Player *, QString, bool)), this, SIGNAL(logSetDoesntUntap(Player *, QString, bool)));

	players << newPlayer;
	emit playerAdded(newPlayer);
	
	return newPlayer;
}

void Game::playerListReceived(QList<ServerPlayer> playerList)
{
	QStringList nameList;
	for (int i = 0; i < playerList.size(); ++i) {
		nameList << playerList[i].getName();
		addPlayer(playerList[i].getPlayerId(), playerList[i].getName(), playerList[i].getLocal());
	}
	emit logPlayerListReceived(nameList);
	restartGameDialog();
}

void Game::readyStart()
{
	client->readyStart();
}

void Game::restartGameDialog()
{
	dlgStartGame->show();
}

void Game::gameEvent(const ServerEventData &msg)
{
	qDebug(QString("game::gameEvent: public=%1, player=%2, name=%3, type=%4, data=%5").arg(msg.getPublic()).arg(msg.getPlayerId()).arg(msg.getPlayerName()).arg(msg.getEventType()).arg(msg.getEventData().join("/")).toLatin1());
	Player *p = players.findPlayer(msg.getPlayerId());
	if (!msg.getPublic()) {
		if (!p)
			return;
		p->gameEvent(msg);
	} else {
		if ((!p) && (msg.getEventType() != eventJoin)) {
			// XXX
		}

		switch(msg.getEventType()) {
		case eventSay:
			emit logSay(p, msg.getEventData()[0]);
			break;
		case eventJoin: {
			const QStringList &data = msg.getEventData();
			if (data.size() != 1)
				return;
			bool spectator = data[0].toInt();
			if (spectator) {
				spectatorList << msg.getPlayerName();
				emit logJoinSpectator(msg.getPlayerName());
			} else {
				Player *newPlayer = addPlayer(msg.getPlayerId(), msg.getPlayerName(), false);
				emit logJoin(newPlayer);
			}
			break;
		}
		case eventLeave: {
			if (p)
				emit logLeave(p);
				// XXX Spieler natürlich noch rauswerfen
			else {
				int spectatorIndex = spectatorList.indexOf(msg.getPlayerName());
				if (spectatorIndex != -1) {
					spectatorList.removeAt(spectatorIndex);
					emit logLeaveSpectator(msg.getPlayerName());
				}
			}
			break;
		}
		case eventReadyStart:
			if (started) {
				started = false;
				emit logReadyStart(p);
				if (!p->getLocal())
					restartGameDialog();
			}
			break;
		case eventGameStart:
			started = true;
			emit logGameStart();
			break;
		case eventShuffle:
			emit logShuffle(p);
			break;
		case eventRollDie: {
			QStringList data = msg.getEventData();
			int sides = data[0].toInt();
			int roll = data[1].toInt();
			emit logRollDie(p, sides, roll);
			break;
		}
		case eventSetActivePlayer: {
			QStringList data = msg.getEventData();
			int playerId = data[0].toInt();
			Player *player = players.findPlayer(playerId);
			if (!player) {
				qDebug(QString("setActivePlayer: invalid player: %1").arg(playerId).toLatin1());
				break;
			}
			for (int i = 0; i < players.size(); ++i)
				players[i]->setActive(players[i] == player);
			emit logSetActivePlayer(player);
			break;
		}
		case eventSetActivePhase: {
			QStringList data = msg.getEventData();
			int phase = data[0].toInt();
			if (currentPhase != phase) {
				currentPhase = phase;
				emit setActivePhase(phase);
			}
			break;
		}

		case eventName:
		case eventCreateToken:
		case eventSetupZones:
		case eventSetCardAttr:
		case eventAddCounter:
		case eventSetCounter:
		case eventDelCounter:
		case eventPlayerId: {
			p->gameEvent(msg);
			break;
		}
		case eventDumpZone: {
			QStringList data = msg.getEventData();
			Player *zoneOwner = players.findPlayer(data[0].toInt());
			if (!zoneOwner)
				break;
			CardZone *zone = zoneOwner->getZones()->findZone(data[1]);
			if (!zone)
				break;
			emit logDumpZone(p, zone, data[2].toInt());
			break;
		}
		case eventStopDumpZone: {
			QStringList data = msg.getEventData();
			Player *zoneOwner = players.findPlayer(data[0].toInt());
			if (!zoneOwner)
				break;
			CardZone *zone = zoneOwner->getZones()->findZone(data[1]);
			if (!zone)
				break;
			emit logStopDumpZone(p, zone);
			break;
		}
		case eventMoveCard: {
			if (!p->getLocal())
				p->gameEvent(msg);
			break;
		}
		case eventDraw: {
			emit logDraw(p, msg.getEventData()[0].toInt());
			if (!p->getLocal())
				p->gameEvent(msg);
			break;
		}
		case eventInvalid: {
			qDebug("Unhandled global event");
		}
		default: {
		}
		}
	}
}

void Game::actNextPhase()
{
	int phase = currentPhase;
	if (++phase >= phaseCount)
		phase = 0;
	client->setActivePhase(phase);
}

void Game::actNextTurn()
{
	client->nextTurn();
}

void Game::showCardMenu(QPoint p)
{
	cardMenu->exec(p);
}

void Game::cardMenuAction()
{
	// Determine the appropriate handler function.
	CardMenuHandler handler = cardMenuHandlers.value(static_cast<QAction *>(sender()));
			
	// The list of selected items is randomly shuffled.
	QList<QGraphicsItem *> sel = scene->selectedItems();
	while (!sel.isEmpty()) {
		unsigned int i = (unsigned int) (((double) sel.size()) * qrand() / (RAND_MAX + 1.0));
		qDebug(QString("%1 items left, i=%2").arg(sel.size()).arg(i).toLatin1());
		CardItem *card = qgraphicsitem_cast<CardItem *>(sel.takeAt(i));
		// For each item, the handler function is called.
		(this->*handler)(card);
	}
}

void Game::actTap(CardItem *card)
{
	if (!card->getTapped())
		client->setCardAttr(qgraphicsitem_cast<CardZone *>(card->parentItem())->getName(), card->getId(), "tapped", "1");
}

void Game::actUntap(CardItem *card)
{
	if (card->getTapped())
		client->setCardAttr(qgraphicsitem_cast<CardZone *>(card->parentItem())->getName(), card->getId(), "tapped", "0");
}

void Game::actDoesntUntap(CardItem *card)
{
	client->setCardAttr(qgraphicsitem_cast<CardZone *>(card->parentItem())->getName(), card->getId(), "doesnt_untap", QString::number(!card->getDoesntUntap()));
}

void Game::actFlip(CardItem *card)
{
	QString zone = qgraphicsitem_cast<CardZone *>(card->parentItem())->getName();
	client->moveCard(card->getId(), zone, zone, card->getGridPoint().x(), card->getGridPoint().y(), !card->getFaceDown());
}

void Game::actAddCounter(CardItem *card)
{
	if (card->getCounters() < MAX_COUNTERS_ON_CARD)
		client->setCardAttr(qgraphicsitem_cast<CardZone *>(card->parentItem())->getName(), card->getId(), "counters", QString::number(card->getCounters() + 1));
}

void Game::actRemoveCounter(CardItem *card)
{
	if (card->getCounters())
		client->setCardAttr(qgraphicsitem_cast<CardZone *>(card->parentItem())->getName(), card->getId(), "counters", QString::number(card->getCounters() - 1));
}

void Game::actSetCounters()
{
	bool ok;
	int number = QInputDialog::getInteger(0, tr("Set counters"), tr("Number:"), 0, 0, MAX_COUNTERS_ON_CARD, 1, &ok);
	if (!ok)
		return;

	QListIterator<QGraphicsItem *> i(scene->selectedItems());
	while (i.hasNext()) {
		CardItem *temp = (CardItem *) i.next();
		client->setCardAttr(qgraphicsitem_cast<CardZone *>(temp->parentItem())->getName(), temp->getId(), "counters", QString::number(number));
	}
}

void Game::actMoveToTopLibrary(CardItem *card)
{
	CardZone *startZone = qgraphicsitem_cast<CardZone *>(card->parentItem());
	client->moveCard(card->getId(), startZone->getName(), "deck", 0, 0, false);
}

void Game::actMoveToBottomLibrary(CardItem *card)
{
	CardZone *startZone = qgraphicsitem_cast<CardZone *>(card->parentItem());
	client->moveCard(card->getId(), startZone->getName(), "deck", -1, 0, false);
}

void Game::actMoveToGraveyard(CardItem *card)
{
	CardZone *startZone = qgraphicsitem_cast<CardZone *>(card->parentItem());
	client->moveCard(card->getId(), startZone->getName(), "grave", 0, 0, false);
}

void Game::actMoveToExile(CardItem *card)
{
	CardZone *startZone = qgraphicsitem_cast<CardZone *>(card->parentItem());
	client->moveCard(card->getId(), startZone->getName(), "rfg", 0, 0, false);
}

void Game::hoverCardEvent(CardItem *card)
{
	emit hoverCard(card->getName());
}
