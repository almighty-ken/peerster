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
#include <QStringList>
#include <QFileDialog>
#include <QtCrypto/QtCrypto>

#define MAX_BLOCKS 410

typedef struct file_info{
	QString file_name;
	QString source;
	quint32 file_size;
	quint16 block_count;
	quint16 downloaded_block_count;
	QByteArray blocklist;
	QByteArray blocklist_hash;
	QByteArray file_block_hash[MAX_BLOCKS];
	QByteArray file_block[MAX_BLOCKS];
}file_info;

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
		void pass_dm_signal(QString,QString);
		void set_title(QString);
		void select_file_dialog();
		void searchPressed();

		// lab3
		void add_file_target(QString file_name);
		void download_prepare(QListWidgetItem* item);

	signals:
		void send_message(QString message);
		void add_peer(QString source);
		void send_dm(QString target, QString message, quint32 hop);
		void files_selected(QStringList files);
		void send_query(QString query);
		void send_download(QString file_name);


	private:
		QTextEdit *textview;
		QTextEdit *textline;
		QLineEdit *peerline;
		QPushButton *textsend;
		QPushButton *peeradd;
		QListWidget *dm_target;
		DmDialog dm_dialog;

		// lab3
		QPushButton *select_file;
		QListWidget *file_target;
		
		// instead, overload message for search terms
		// QLineEdit *search_input;
		QPushButton *search;
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
		QHash<QString,QHash<QString,QVariant> > routing_table;
};

class FileManager : public QObject
{
	Q_OBJECT

	public:
		FileManager();

	public slots:
		// lab3
		void files_selected_add(QStringList files);
		void received_query(QMap<QString, QVariant> query);
		void file_option_add(QString file_name, QString source, QByteArray file_ID);
		void file_download(QString file_name);
		void block_received(QString source, QByteArray data, QByteArray hash);
		void block_requested(QString dest, QByteArray hash);

	signals:
		void return_query(QString dest, 
			QString search_reply, QVariantList match_names, QVariantList match_ids);
		void send_block_req(QString dest, QByteArray hash);
		void send_block_reply(QString dest, QByteArray hash, QByteArray data);


	private:
		QList<file_info> file_info_list;
		QList<file_info> file_option_list;
		void add_file_entry(QString file_name);
		QByteArray hash_sha1(QByteArray data);
		void dump_file_list();
		void dump_option_list();
		void download_manager(int i);
		void export_file(file_info file);
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
		
		// lab3
		void start_search(QString keywords);
		void send_search_reply(QString dest, 
			QString search_reply, QVariantList match_names, QVariantList match_ids);
		void exec_search();
		void send_block_req(QString dest, QByteArray hash);
		void send_block_reply(QString dest, QByteArray hash, QByteArray data);


	signals:
		void send_message2Dialog(QString message);
		void add_dm_target(QString target);
		void send_originID(QString ID);

		void file_query(QMap<QString, QVariant> query);
		void send_file2Dialog(QString file_name);
		void send_file2Manager(QString file_name, QString source, QByteArray file_ID);
		void block_received(QString source, QByteArray data, QByteArray hash);
		void block_requested(QString dest, QByteArray hash);

	private:
		int myPortMin, myPortMax, myPort;
		bool noForward;
		bool require_confirm;
		QTimer entropy_timer;
		quint32 message_sequence;
		// quint32 hop_limit;
		QString originID;
		QMap<QString,QMap<QString, QVariant> > messageDB;
		QMap<QString,QVariant> statusDB;
		QMap<QPair<QString,quint16>,QQueue<QPair<QTime,QByteArray> > > confirm_waitlist;
		QList<Peer*> peer_list;
		
		// lab2
		Router router;
		QTimer route_timer;

		// lab3
		QTimer search_timer;
		quint16 search_cnt;
		QMap<QString,QVariant> query_cache;
		

		bool inDB(QMap<QString, QVariant>);
		void random_send(QByteArray);
		void all_send(QByteArray);
		int check_message_type(QMap<QString,QVariant>);
		void send_messageUDP(QByteArray, QHostAddress, quint16);
		void add2DB(QMap<QString, QVariant>);
		QMap<QString,QVariant> build_status();
		void process_status(QMap<QString,QVariant>, QHostAddress, quint16);
		void update_waitlist(QHostAddress, quint16);
		void learn_peer(QHostAddress, quint16);
		QByteArray add_shortcut_info(QMap<QString,QVariant>, QHostAddress, quint16);
		void broadcast_search(QMap<QString,QVariant>);
		void search_reply_proc(QMap<QString,QVariant>);
		void block_reply_proc(QMap<QString,QVariant>);
		void block_request_proc(QMap<QString,QVariant>);
};

#endif // PEERSTER_MAIN_HH
