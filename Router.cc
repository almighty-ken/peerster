#include <unistd.h>
#include <stdlib.h>
#include <string>
#include <time.h>

#include <QDebug>
#include <QVariant>

#include "main.hh"

Router::Router(){
	QDebug() << "Router initiated";
}

// QHash<QString,QHash<QString,QVariant>> routing_table;

void Router::update_table(QMap<QString, QVariant> message,QHostAddress ip,quint16 port){
	QString origin = message["Origin"].toString();
	quint32 sequence = message["SeqNo"].toInt();

	if(routing_table.contains(origin)){
		// we see if the info in DB is older
		if(routing_table[origin]["SeqNo"].toInt() < sequence){
			// update info
			routing_table[origin]["IP"] = QVariant(ip);
			routing_table[origin]["port"] = QVariant(port);
			routing_table[origin]["SeqNo"] = QVariant(sequence);
		}
	}else{
		// new entry
		QHash<QString,QVariant> entry;
		entry["IP"] = QVariant(ip);
		entry["port"] = QVariant(port);
		entry["SeqNo"] = QVariant(sequence);
		routing_table[origin] = entry;
	}
	QDebug() << "[Router::update_table]Updated router table";

}

QHash<QString,QVariant> Router::retrieve_info(QString origin){
	if(routing_table.contains(origin)){
		return routing_table[origin];
	}else{
		return NULL;
	}
}