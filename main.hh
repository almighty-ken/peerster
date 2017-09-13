#ifndef PEERSTER_MAIN_HH
#define PEERSTER_MAIN_HH

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QUdpSocket>
#include <QString>
#include <QVariant>
#include <QPushButton>
#include <QMap>
#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <QTimer>
#include <QTime>
#include <QHostInfo>
#include <QQueue>
#include <QPair>
#include <QList>

class ChatDialog : public QDialog
{
	Q_OBJECT

public:
	ChatDialog();

public slots:
	void gotReturnPressed();
	void received_from_UDP(QString message);

signals:
	void send_message(QString message);

private:
	QTextEdit *textview;
	QTextEdit *textline;
	QPushButton *textsend;
};


class Peer : public QHostInfo
{
	Q_OBJECT

public:
	QString host_name;
	QHostAddress host_addr;
	quint16 host_port;

	void insert(QString);

public slots:
	void looked_up_ip(QHostInfo);
	void looked_up_name(QHostInfo);

};


class GNode : public QUdpSocket
{
	Q_OBJECT

public:
	GNode();

	// Bind this socket to a Peerster-specific default port.
	bool bind();
	void SerializeSend_message(QMap<QString, QVariant>, QHostAddress, quint16);
	// void SerializeSend_message(QMap<QString,QMap<QString, quint32>>,
	 // QHostAddress, quint16);


public slots:
	void received_message2send(QString message);
	void received_UDP_message();
	void check_waitlist();
	void entropy_message();

signals:
	void send_message2Dialog(QString message);

private:
	int myPortMin, myPortMax, myPort;
	// int neighbor1, neighbor2;
	QTimer entropy_timer;
	quint32 message_sequence;
	QString originID;
	QMap<QString,QMap<QString, QVariant>> messageDB;
	QMap<QString,QVariant> statusDB;
	QMap<QPair<QString,quint16>,QQueue<QPair<QTime,QByteArray>>> confirm_waitlist;
	QList<Peer> peer_list;

	bool inDB(QMap<QString, QVariant>);
	void random_send(QByteArray);
	void send_messageUDP(QByteArray, QHostAddress, quint16);
	void add2DB(QMap<QString, QVariant>);
	QMap<QString,QVariant> build_status();
	void process_status(QMap<QString,QVariant>, QHostAddress, quint16);
	void update_waitlist(QHostAddress, quint16);

};

#endif // PEERSTER_MAIN_HH
