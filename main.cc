#include <unistd.h>
#include <stdlib.h>
#include <string>
#include <time.h>

#include <QVBoxLayout>

#include <QDebug>
#include <QKeyEvent>
#include <QVariant>

#include "main.hh"

ChatDialog::ChatDialog()
{
	setWindowTitle("Peerster");

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

	// Lay out the widgets to appear in the main window.
	// For Qt widget and layout concepts see:
	// http://doc.qt.nokia.com/4.7-snapshot/widgets-and-layouts.html
	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(peerline);
	layout->addWidget(peeradd);
	layout->addWidget(textview);
	layout->addWidget(textline);
	layout->addWidget(textsend);
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
}

void ChatDialog::addPeerPressed()
{
	// Initially, just echo the string locally.
	// Insert some networking code here...
	// should emit a message containing the message to GNode class
	QString p = peerline->text();
	qDebug() << "[ChatDialog::addPeerPressed]" << p;

	emit add_peer(p);

	// Clear the textline to get ready for the next input message.
	peerline->clear();
}

void ChatDialog::gotReturnPressed()
{
	// Initially, just echo the string locally.
	// Insert some networking code here...
	// should emit a message containing the message to GNode class
	QString message = textline->toPlainText();
	qDebug() << "[ChatDialog::gotReturnPressed]emitted message: " << message;
	// textview->append(message);
	emit send_message(message);

	// Clear the textline to get ready for the next input message.
	textline->clear();
}

void ChatDialog::received_from_UDP(QString message){
	textview->append(message);
	qDebug() << "[ChatDialog::received_from_UDP]received message and added to chatlog: " << message;
}

int main(int argc, char **argv)
{
	// Initialize Qt toolkit
	QApplication app(argc,argv);

	// Create an initial chat dialog window
	ChatDialog dialog;
	dialog.show();

	// Create a UDP network socket
	GNode gnode;
	if (!gnode.bind())
		exit(1);

	// connect the send_message signal to the received_message2send slot
	QObject::connect(&dialog, SIGNAL(send_message(QString)),&gnode, 
		SLOT(received_message2send(QString)));

	// connect the received_message signal from GNode to slot in ChatDialog
	QObject::connect(&gnode, SIGNAL(send_message2Dialog(QString)),
		&dialog,SLOT(received_from_UDP(QString)));

	// connect the received_message signal from GNode to slot in ChatDialog
	QObject::connect(&dialog, SIGNAL(add_peer(QString)),
		&gnode,SLOT(add_peer_from_dialog(QString)));

	// Enter the Qt main loop; everything else is event driven
	return app.exec();
}

