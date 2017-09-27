#ifndef PEERSTER_MAIN_HH
#define PEERSTER_MAIN_HH

#include <QDialog>
#include <QApplication>
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
#include <QHash>
#include <QList>
#include <QListWidget>
#include <QListWidgetItem>

class DmDialog : public QDialog
{
	Q_OBJECT
public:
	DmDialog();

public slots:
	void show_with_target(QListWidgetItem*);
	void gotReturnPressed();

signals:
	void send_dm(QString target, QString message);

private:
	QString target;
	QTextEdit *textline;
	QPushButton *textsend;
};

class ChatDialog : public QDialog
{
	Q_OBJECT

public:
	ChatDialog();

public slots:
	void gotReturnPressed();
	void received_from_UDP(QString message);
	void addPeerPressed();
	void add_dm_target(QString origin);
	// void test(QListWidgetItem*);
	void pass_dm_signal(QString,QString);

signals:
	void send_message(QString message);
	void add_peer(QString source);
	void send_dm(QString target, QString message, quint32 hop);


private:
	QTextEdit *textview;
	QTextEdit *textline;
	QLineEdit *peerline;
	QPushButton *textsend;
	QPushButton *peeradd;
	QListWidget *dm_target;
	DmDialog dm_dialog;
};


class Peer : public QObject
{
	Q_OBJECT

public:
	Peer();
	QString host_name;
	QHostAddress host_addr;
	quint16 host_port;
	bool active;

	void insert(QString);
	void insert(QHostAddress,quint16);

public slots:
	void looked_up_ip(QHostInfo);
	void looked_up_name(QHostInfo);

};


class Router : public QObject
{
	Q_OBJECT
public:
	Router();
	void update_table(QMap<QString, QVariant>,QHostAddress,quint16);
	QHash<QString,QVariant> retrieve_info(QString);
	bool is_new_origin(QMap<QString, QVariant>);

private:
	QHash<QString,QHash<QString,QVariant>> routing_table;
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
	void add_peer_from_dialog(QString source);

	// lab2
	void send_route_rumour();
	void received_dm2send(QString target,QString message,quint32);

signals:
	void send_message2Dialog(QString message);
	void add_dm_target(QString target);

private:
	int myPortMin, myPortMax, myPort;
	QTimer entropy_timer;
	quint32 message_sequence;
	// quint32 hop_limit;
	QString originID;
	QMap<QString,QMap<QString, QVariant>> messageDB;
	QMap<QString,QVariant> statusDB;
	QMap<QPair<QString,quint16>,QQueue<QPair<QTime,QByteArray>>> confirm_waitlist;
	QList<Peer*> peer_list;
	
	// lab2
	Router router;
	QTimer route_timer;

	bool inDB(QMap<QString, QVariant>);
	void random_send(QByteArray);
	int check_message_type(QMap<QString,QVariant>);
	void send_messageUDP(QByteArray, QHostAddress, quint16);
	void add2DB(QMap<QString, QVariant>);
	QMap<QString,QVariant> build_status();
	void process_status(QMap<QString,QVariant>, QHostAddress, quint16);
	void update_waitlist(QHostAddress, quint16);
	void learn_peer(QHostAddress, quint16);




};

#endif // PEERSTER_MAIN_HH
