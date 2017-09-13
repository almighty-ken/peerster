#include <QStringList>
#include "main.hh"

Peer::Peer(){
	active = false;
}

void Peer::insert(QString source){
	active = false;
	QStringList parsed = source.split(":");
	if(parsed.length() == 2){
		QString first = parsed[0];
		QString second = parsed[1];

		QHostAddress ip = QHostAddress(first);
		if(ip.isNull()){
			// we have to perform lookup for hostname->ip
			host_name = first;
			QHostInfo::lookupHost(host_name,this,SLOT(looked_up_ip(QHostInfo)));
		}else{
			// we have to perform lookup for ip->hostname
			host_addr = first;
			QHostInfo::lookupHost(host_addr.toString(),this,SLOT(looked_up_name(QHostInfo)));
		}

		host_port = second.toUInt();
		return;
	}
	qDebug() << "[Peer::insert] Bad address";
}

void Peer::insert(QHostAddress addr, quint16 port){
	active = false;
	host_addr = addr;
	host_port = port;
	QHostInfo::lookupHost(host_addr.toString(),this,SLOT(looked_up_name(QHostInfo)));
}

void Peer::looked_up_ip(QHostInfo info){
	// get ip from info
	if (info.error() != QHostInfo::NoError){
        qDebug() << "[Peer::looked_up_ip]Lookup failed:" << info.errorString();
        return;
	}
	host_addr = info.addresses().at(0);
	qDebug() << "[Peer::looked_up_ip]Found address:" << host_addr.toString();
	active = true;
}

void Peer::looked_up_name(QHostInfo info){
	// get name from info
	if (info.error() != QHostInfo::NoError){
        qDebug() << "[Peer::looked_up_name]Lookup failed:" << info.errorString();
        return;
	}
	host_name = info.hostName();
	qDebug() << "[Peer::looked_up_name]Found name:" << host_name;
	active = true;
}