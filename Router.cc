#include <unistd.h>
#include <stdlib.h>
#include <string>
#include <time.h>

#include <QDebug>
#include <QVariant>

#include "main.hh"

Router::Router(){
	qDebug() << "[Router::Router]Router initiated";
}

// QHash<QString,QHash<QString,QVariant>> routing_table;

void Router::update_table(QMap<QString, QVariant> message,QHostAddress ip,quint16 port){
	QString origin = message["Origin"].toString();
	quint32 sequence = message["SeqNo"].toInt();

	if(routing_table.contains(origin)){
		// we see if the info in DB is older
		if(routing_table[origin]["SeqNo"].toUInt() < sequence){
			// update info
			// here ip is first converted into string to be converted into qvariant
			// if the message contains shortcut

			if(message.contains("LastIP") && message.contains("LastPort")){
				qDebug() << "Shortcutting entry with bigger SeqNo";
				routing_table[origin]["IP"] = message["LastIP"];
				routing_table[origin]["port"] = message["LastPort"];
			}else{
				qDebug() << "Updating entry NORMAL";
				routing_table[origin]["IP"] = QVariant(ip.toString());
				routing_table[origin]["port"] = QVariant(port);
			}
			routing_table[origin]["SeqNo"] = QVariant(sequence);
		}else if(routing_table[origin]["SeqNo"].toUInt() == sequence){
			if(message.contains("LastIP") && message.contains("LastPort")){
				qDebug() << "Shortcutting entry with same SeqNo";
				routing_table[origin]["IP"] = message["LastIP"];
				routing_table[origin]["port"] = message["LastPort"];
			}
		}
	}else{
		// new entry
		qDebug() << "New router entry";
		QHash<QString,QVariant> entry;
		if(message.contains("LastIP") && message.contains("LastPort")){
			entry["IP"] = message["LastIP"];
			entry["port"] = message["LastPort"];
		}else{
			entry["IP"] = QVariant(ip.toString());
			entry["port"] = QVariant(port);
		}
		entry["SeqNo"] = QVariant(sequence);
		routing_table[origin] = entry;
	}
	qDebug() << "[Router::update_table]Updated router table" << routing_table;

}

bool Router::is_new_origin(QMap<QString, QVariant> message){
	QString origin = message["Origin"].toString();
	if(routing_table.contains(origin)){
		return false;
	}
	return true;
}

QHash<QString,QVariant> Router::retrieve_info(QString origin){
	if(routing_table.contains(origin)){
		return routing_table[origin];
	}else{
		// create a NULL entry
		QHash<QString,QVariant> empty;
		return empty;
	}
}