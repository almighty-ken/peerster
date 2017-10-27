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
			qDebug() << "[Router::update_table]Overwriting with newer SeqNo";

			if(message.contains("LastIP") && message.contains("LastPort")){
				qDebug() << "Shortcutting entry with bigger SeqNo";
				routing_table[origin]["IP"] = message["LastIP"];
				routing_table[origin]["port"] = message["LastPort"];
			}else{
				qDebug() << "Updating entry DIRECT";
				routing_table[origin]["IP"] = QVariant(ip.toString());
				routing_table[origin]["port"] = QVariant(port);
				routing_table[origin]["direct"] = QVariant(1);
			}
			routing_table[origin]["SeqNo"] = QVariant(sequence);
		}else if(routing_table[origin]["SeqNo"].toUInt() == sequence){
			// if original entry is direct, then ignore
			// else overwrite
			qDebug() << "[Router::update_table]checking with same SeqNo";
			if(message.contains("LastIP") && message.contains("LastPort")){
				qDebug() << "Shortcutting entry with same SeqNo";
				if(routing_table[origin]["direct"].toInt()==0){
					routing_table[origin]["IP"] = message["LastIP"];
					routing_table[origin]["port"] = message["LastPort"];
				}
			}else{
				routing_table[origin]["IP"] = QVariant(ip.toString());
				routing_table[origin]["port"] = QVariant(port);
				routing_table[origin]["direct"] = QVariant(1);
			}
		}
	}else{
		// new entry
		qDebug() << "[Router::update_table]New router entry";
		QHash<QString,QVariant> entry;
		if(message.contains("LastIP") && message.contains("LastPort")){
			entry["IP"] = message["LastIP"];
			entry["port"] = message["LastPort"];
			entry["direct"] = QVariant(0);
		}else{
			// is direct
			entry["IP"] = QVariant(ip.toString());
			entry["port"] = QVariant(port);
			entry["direct"] = QVariant(1);
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
		qDebug() << "[Router::retrieve_info]Retrieve info fail!";
		QHash<QString,QVariant> empty;
		return empty;
	}
}