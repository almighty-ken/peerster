#include <unistd.h>
#include <stdlib.h>
#include <string>
#include <time.h>

#include <QVBoxLayout>
#include <QGridLayout>

#include <QDebug>
#include <QKeyEvent>
#include <QVariant>

#include "main.hh"

ChatDialog::ChatDialog()
{
	// Create the direct message chat dialog

	setWindowTitle("Peerster");
	resize(400,600);
	move(200,200);

	// Read-only text box where we display messages from everyone.
	// This widget expands both horizontally and vertically.
	textview = new QTextEdit(this);
	textview->setReadOnly(true);

	// Small text-entry box the user can enter messages.
	// This widget normally expands only horizontally,
	// leaving extra vertical space for the textview widget.
	//
	// You might change this into a read/write QTextEdit,
	// so that the user can easily enter multi-line messages.
	
	textline = new QTextEdit(this);

	// A button for sending out a message
	textsend = new QPushButton("Send Message",this);

	// A textline for adding peers
	peerline = new QLineEdit(this);

	// A button for adding peers
	peeradd = new QPushButton("Add peer",this);


	// lab2
	// A QListWidget for selecting direct messaging options
	dm_target = new QListWidget(this);

	// lab3
	// A button for selecting files to add to system
	select_file = new QPushButton("Select Files to share",this);

	file_target = new QListWidget(this);
	// search_input = new QLineEdit(this);
	search = new QPushButton("Search",this);

	// Lay out the widgets to appear in the main window.
	// For Qt widget and layout concepts see:
	// http://doc.qt.nokia.com/4.7-snapshot/widgets-and-layouts.html
	// QVBoxLayout *layout = new QVBoxLayout();
	QGridLayout *layout = new QGridLayout();
	layout->addWidget(peerline,1,0);
	layout->addWidget(peeradd,1,1);
	// lab2
	layout->addWidget(dm_target,2,0,1,2);
	

	layout->addWidget(textview,3,0);
	layout->addWidget(textline,4,0,4,1);
	layout->addWidget(textsend,4,1);

	// lab3
	layout->addWidget(select_file,5,1);
	layout->addWidget(search,6,1);
	layout->addWidget(file_target,3,1);


	

	textline->setFocus();
	setLayout(layout);

	// Register a callback on the textline's returnPressed signal
	// so that we can send the message entered by the user.
	connect(textsend, SIGNAL(clicked()),
		this, SLOT(gotReturnPressed()));

	// Register a callback on the peeradd's returnPressed signal
	// so that we can send the message entered by the user.
	connect(peeradd, SIGNAL(clicked()),
		this, SLOT(addPeerPressed()));

	// lab3
	// Register a callback on the sekect_file signal
	// so that we can open file dialog
	connect(select_file, SIGNAL(clicked()),
		this, SLOT(select_file_dialog()));

	connect(search, SIGNAL(clicked()),
		this, SLOT(searchPressed()));

	// lab2
	// send signal when
	connect(dm_target, SIGNAL(itemDoubleClicked(QListWidgetItem*)),
		&dm_dialog, SLOT(show_with_target(QListWidgetItem*)));
	connect(&dm_dialog, SIGNAL(send_dm(QString,QString)),
		this, SLOT(pass_dm_signal(QString,QString)));

	connect(file_target, SIGNAL(itemDoubleClicked(QListWidgetItem*)),
		this, SLOT(download_prepare(QListWidgetItem*)));
}

void ChatDialog::download_prepare(QListWidgetItem* item){
	emit send_download(item->text());
}

void ChatDialog::searchPressed()
{
	QString message = textline->toPlainText();
	emit send_query(message);
	textline->clear();
	file_target->clear();
}

void ChatDialog::select_file_dialog(){
	QStringList files = QFileDialog::getOpenFileNames(
                        this,
                        "Select one or more files to share",
                        "",
                        "");
	emit files_selected(files);
}

void ChatDialog::add_dm_target(QString origin){
	dm_target -> addItem(origin);
	qDebug() << "[ChatDialog::add_dm_target]Direct message target added";
}

void ChatDialog::add_file_target(QString file_name){
	file_target -> addItem(file_name);
	qDebug() << "[ChatDialog::add_dm_target]Direct message target added";
}

void ChatDialog::set_title(QString ID){
	setWindowTitle(ID);
}

void ChatDialog::addPeerPressed()
{
	// Initially, just echo the string locally.
	// Insert some networking code here...
	// should emit a message containing the message to GNode class
	QString p = peerline->text();
	// qDebug() << "[ChatDialog::addPeerPressed]" << p;

	emit add_peer(p);

	// Clear the textline to get ready for the next input message.
	peerline->clear();
}

void ChatDialog::pass_dm_signal(QString target,QString message){
	// we pass this signal to GNode
	emit send_dm(target,message,10);
}

void ChatDialog::gotReturnPressed()
{
	// Initially, just echo the string locally.
	// Insert some networking code here...
	// should emit a message containing the message to GNode class
	QString message = textline->toPlainText();
	// qDebug() << "[ChatDialog::gotReturnPressed]emitted message: " << message;
	// textview->append(message);
	emit send_message(message);

	// Clear the textline to get ready for the next input message.
	textline->clear();
}

void ChatDialog::received_from_UDP(QString message){
	textview->append(message);
	// qDebug() << "[ChatDialog::received_from_UDP]received message and added to chatlog: " << message;
}

int main(int argc, char **argv)
{
	// Initialize Qt toolkit
	QApplication app(argc,argv);

	// Init crypto lib
	QCA::Initializer qcainit;

	// Create an initial chat dialog window
	ChatDialog dialog;
	dialog.show();

	// Create a UDP network socket
	GNode gnode;

	QObject::connect(&gnode, SIGNAL(send_originID(QString)),
		&dialog,SLOT(set_title(QString)));

	if (!gnode.bind())
		exit(1);

	// lab3
	// Create a file manager for file sharing
	FileManager fmanager;

	// connect the send_message signal to the received_message2send slot
	QObject::connect(&dialog, SIGNAL(send_message(QString)),&gnode, 
		SLOT(received_message2send(QString)));

	// connect the received_message signal from GNode to slot in ChatDialog
	QObject::connect(&gnode, SIGNAL(send_message2Dialog(QString)),
		&dialog,SLOT(received_from_UDP(QString)));

	QObject::connect(&dialog, SIGNAL(add_peer(QString)),
		&gnode,SLOT(add_peer_from_dialog(QString)));

	QObject::connect(&dialog, SIGNAL(send_dm(QString,QString,quint32)),
		&gnode,SLOT(received_dm2send(QString,QString,quint32)));

	QObject::connect(&gnode, SIGNAL(add_dm_target(QString)),
		&dialog,SLOT(add_dm_target(QString)));

	// lab3
	// connect files selected to FileManager
	QObject::connect(&dialog, SIGNAL(files_selected(QStringList)),
		&fmanager,SLOT(files_selected_add(QStringList)));

	QObject::connect(&dialog, SIGNAL(send_download(QString)),
		&fmanager,SLOT(file_download(QString)));

	QObject::connect(&dialog, SIGNAL(send_query(QString)),&gnode, 
		SLOT(start_search(QString)));

	QObject::connect(&gnode, SIGNAL(file_query(QMap<QString, QVariant>)),
		&fmanager,SLOT(received_query(QMap<QString, QVariant>)));

	QObject::connect(&fmanager, SIGNAL(return_query(QString, 
			QString, QVariantList, QVariantList)),
		&gnode,SLOT(send_search_reply(QString, 
			QString, QVariantList, QVariantList)));

	QObject::connect(&gnode, SIGNAL(send_file2Dialog(QString)),
		&dialog,SLOT(add_file_target(QString)));

	QObject::connect(&gnode, SIGNAL(send_file2Manager(QString, QString, QByteArray)),
		&fmanager,SLOT(file_option_add(QString, QString, QByteArray)));
	// Enter the Qt main loop; everything else is event driven
	return app.exec();
}

