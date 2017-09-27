#include <unistd.h>
#include <stdlib.h>
#include <string>
#include <time.h>

#include <QVBoxLayout>
#include <QDebug>
#include "main.hh"

DmDialog::DmDialog()
{
	setWindowTitle("Send DM");

	textline = new QTextEdit(this);

	// A button for sending out a message
	textsend = new QPushButton("Send Direct Message",this);

	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(textline);
	layout->addWidget(textsend);
	textline->setFocus();
	setLayout(layout);

	// Register a callback on the textline's returnPressed signal
	// so that we can send the message entered by the user.
	connect(textsend, SIGNAL(clicked()),
		this, SLOT(gotReturnPressed()));
}

void DmDialog::gotReturnPressed()
{
	// Initially, just echo the string locally.
	// Insert some networking code here...
	// should emit a message containing the message to GNode class
	QString message = textline->toPlainText();
	// qDebug() << "[ChatDialog::gotReturnPressed]emitted message: " << message;
	// textview->append(message);
	emit send_dm(target,message);

	// Clear the textline to get ready for the next input message.
	textline->clear();

	close();
}

void DmDialog::show_with_target(QListWidgetItem* message_target){
	// qDebug() << message_target->text();
	target = message_target->text();
	open();
}