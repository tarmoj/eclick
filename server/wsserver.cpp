#include "wsserver.h"
#include "QtWebSockets/qwebsocketserver.h"
#include "QtWebSockets/qwebsocket.h"
#include <QtCore/QDebug>
#include <QDir>
#include <QNetworkInterface>


QT_USE_NAMESPACE



WsServer::WsServer(quint16 port, QObject *parent) :
    QObject(parent),
	m_pWebSocketServer(new QWebSocketServer(QStringLiteral("eClickServer"),
                                            QWebSocketServer::NonSecureMode, this)),
	m_clients(), m_oscClients()
{
    if (m_pWebSocketServer->listen(QHostAddress::Any, port)) {
        qDebug() << "WsServer listening on port" << port;
        connect(m_pWebSocketServer, &QWebSocketServer::newConnection,
                this, &WsServer::onNewConnection);
        connect(m_pWebSocketServer, &QWebSocketServer::closed, this, &WsServer::closed);
	}

	sendOsc = true;
	sendWs = false;
	settings = new QSettings("eclick","server"); // TODO platform independent
	createOscClientsList(getOscAddresses());
}



WsServer::~WsServer()
{
    m_pWebSocketServer->close();
    qDeleteAll(m_clients.begin(), m_clients.end());
}


void WsServer::onNewConnection()
{
    QWebSocket *pSocket = m_pWebSocketServer->nextPendingConnection();

    connect(pSocket, &QWebSocket::textMessageReceived, this, &WsServer::processTextMessage);
    //connect(pSocket, &QWebSocket::binaryMessageReceived, this, &WsServer::processBinaryMessage);
    connect(pSocket, &QWebSocket::disconnected, this, &WsServer::socketDisconnected);

    m_clients << pSocket;
	emit newConnection(m_clients.count()); //TODO: kui enamus OSC-ga, siis näita pigem, mitu OSC aadressi reas
}



void WsServer::processTextMessage(QString message)
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (!pClient) {
        return;
    }
    qDebug()<<message;

    QStringList messageParts = message.split(" ");

	// python serverist:

//	if 'channels' in mess_array:
//			channels = int(mess_array[mess_array.index("channels")+1]) #TODO: catch exception if not int
//		else:
//			channels = TUTTI

	if (messageParts[0]=="hello") { // comes as "hello <instrument>"
		if (messageParts.length()>1) {
			QString instrument = messageParts[1]; // for future
		}
		QString senderUrl = pClient->peerAddress().toString();
		qDebug()<<"Hello from: "<<senderUrl;
		if (!senderUrl.isEmpty()) { // append to oscAddresses and send confirmation
			QOscClient * target = new QOscClient(pClient->peerAddress(),8008,this); // what if localhost, does it work then?
			if (target) {
				if (!oscAddresses.contains(senderUrl)) {
					oscAddresses<<senderUrl;
					emit updateOscAddresses(oscAddresses.join(","));
					m_oscClients << target;
					target->sendData("/metronome/notification", "Got you!" );
				} else {
					qDebug()<<"Adress already registered: "<<senderUrl;
					target->sendData("/metronome/notification", "All fine" );
				}
			} else {
				qDebug()<<"Could not create OSC address to "<<senderUrl;
			}
		}
	}

	int bar = -1, beat = -1, led = -1;
	float duration = 0;
	if (messageParts.contains("bar")) {
		bar = messageParts[messageParts.indexOf("bar")+1].toInt();
	}

	if (messageParts.contains("beat")) {
		beat = messageParts[messageParts.indexOf("beat")+1].toInt();
	}

	if (messageParts.contains("duration")) {
		duration = messageParts[messageParts.indexOf("duration")+1].toFloat();
	}

	if (messageParts.contains("led")) {
		led = messageParts[messageParts.indexOf("led")+1].toInt();
	}

	if (messageParts.contains("notification")) { //NB! maybe there should be also a slot and later csound string channel
		QString notification = message.split("notification")[1]; // get the message
		handleNotification(notification.simplified());
	}

	if (messageParts.contains("tempo")) {
		handleTempo( messageParts[messageParts.indexOf("tempo")+1].toDouble());
	}

	if (bar>=0 && beat>=0)  // format message shorter for javascript
		handleBeatBar(bar,beat);

	if (led >= 0)
		handleLed(led,duration);


}

//void WsServer::processBinaryMessage(QByteArray message)
//{
//    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
//    if (pClient) {
//        pClient->sendBinaryMessage(message);
//    }
//}

void WsServer::socketDisconnected()
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient) {
        m_clients.removeAll(pClient);
        emit newConnection(m_clients.count());
        pClient->deleteLater();
	}
}

void WsServer::handleBeatBar(int bar, int beat)
{
	qDebug()<<"Bar: "<<bar<<" Beat: "<<beat;
	emit newBeatBar(bar, beat); // necessary, since QML reads only signals from wsServer
	if (sendOsc) {
		foreach(QOscClient * target, m_oscClients) {
			QList<QVariant> data;
			data << bar << beat;
			target->sendData("/metronome/beatbar", data);
		}
	}
	if (sendWs) {
		QString message;
		message.sprintf("b %d %d", bar, beat); // short form to send out for javascript and app
		send2all(message);
	}


}

void WsServer::handleLed(int ledNumber, float duration) {
	qDebug()<<"Led: "<<ledNumber<<" duration: "<<duration;
	emit newLed(ledNumber,duration);
	if (sendOsc) {
		foreach(QOscClient * target, m_oscClients) {
			QList<QVariant> data;
			data << ledNumber << (double)duration; // QOsc types does not recognise float...
			target->sendData("/metronome/led", data);
		}
	}

	if (sendWs) {
		QString message;
		message.sprintf("l %d %f", ledNumber, duration); // short form to send out for javascript and app
		send2all(message);
	}


}

void WsServer::handleNotification(QString message, float duration)
{
	qDebug()<<"Notification: "<<message <<" for " << duration << "seconds.";
	if (sendOsc) {
		foreach(QOscClient * target, m_oscClients) {
			QList<QVariant> data;
			data << message << (double)duration; // QOsc types does not recognise float...
			target->sendData("/metronome/notification", data);
		}
	}
	//joke for ending
//	QStringList endmessages = QStringList()<<"Uhhh..."<<"Hästi tehtud!"<< "Läbi sai" << "OK!" << "Nu-nuu..." << "Tsss!" << "Löpp" << "Pole hullu!";
//	if (message=="end") {
//		message = endmessages[qrand()%(endmessages.length()-1)];
//	}
	if (sendWs) {
		message = "n "+message;
		send2all(message);
	}

}

void WsServer::handleTempo(double tempo) // TODO: change to double, not string
{
	//tempo = tempo.left((tempo.indexOf("."))+3); // cut to 2 decimals
	qDebug()<<"Tempo: "<<tempo;
	if (sendOsc) {
		foreach(QOscClient * target, m_oscClients) {
			//QList<QVariant> data;
			//data << ledNumber << (double)duration; // QOsc types does not recognise float...
			target->sendData("/metronome/tempo", tempo);
		}

	}
	QString tempoString = QString::number(tempo, 'f',2);
	emit newTempo(tempoString); // construct string here!
	if (sendWs) {
		send2all("t "+tempoString);

	}
}

void WsServer::createOscClientsList(QString addresses)
{
	oscAddresses.clear();
	m_oscClients.clear();
	foreach (QString address, addresses.split(",")) {
		address = address.simplified();
		address = (address=="localhost") ? "127.0.0.1" : address; // does not like "localhost" as string
		if (!address.isEmpty()) { // for any case
			QOscClient * client = new QOscClient(QHostAddress(address), 8008, this);
			if (client) {
				m_oscClients<<client; // muuda target oscClient vms
				oscAddresses<<address;
				//testing
				client->sendData("/test", address);
			} else {
				qDebug()<<"Could not create OSC address to "<<address;
			}
		}
	}
	emit updateOscAddresses(oscAddresses.join(","));
	qDebug()<<"OSC targets count: " << m_oscClients.count();

}


void WsServer::sendMessage(QWebSocket *socket, QString message )
{
    if (socket == 0)
    {
        return;
    }
    socket->sendTextMessage(message);

}


void WsServer::send2all(QString message)
{
	foreach (QWebSocket *socket, m_clients) {
		socket->sendTextMessage(message);
	}
}


void WsServer::setSendOsc(bool onOff)
{
	sendOsc  = onOff;
}

void WsServer::setSendWs(bool onOff)
{
	sendWs = onOff;
}

void WsServer::setOscAddresses(QString addresses)
{
	bool oldvalue = sendOsc;
	sendOsc = false; // as kind of mutex not to send any osc messages during that time
	createOscClientsList(addresses);

	if (settings) { // store in settings
		if (addresses=="none" ||  oscAddresses.isEmpty()  ) // allow also stroring empty addresses line, but avoid invalid value.
			settings->setValue("oscaddresses", "");
		else
			settings->setValue("oscaddresses", oscAddresses);
	}
	sendOsc = oldvalue;
}

QString WsServer::getOscAddresses()
{
	if (settings) {
		QVariant value = settings->value("oscaddresses");
		if (value.type()==QMetaType::QString)
			return value.toString();
		else if (value.type()==QMetaType::QStringList)
			return value.toStringList().join(",");
		else
			return QString();
	}
}

QString WsServer::getLocalAddress()
{
	QString address = QString();
	QList <QHostAddress> localAddresses = QNetworkInterface::allAddresses();
	for(int i = 0; i < localAddresses.count(); i++) {

		if(!localAddresses[i].isLoopback())
			if (localAddresses[i].protocol() == QAbstractSocket::IPv4Protocol ) {
				address = localAddresses[i].toString();
				qDebug() << "YOUR IP: " << address;
				break; // get the first address
		}

	}
	return address;
}
