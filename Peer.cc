#include <QStringList>
#include "main.hh"

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
			QHostInfo::lookupHost(host_addr,this,SLOT(looked_up_name(QHostInfo)));
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
	QHostInfo::lookupHost(host_addr,this,SLOT(looked_up_name(QHostInfo)));
}

void Peer::looked_up_ip(QHostInfo info){
	// get ip from info
	if (host.error() != QHostInfo::NoError){
        qDebug() << "[Peer::looked_up_ip]Lookup failed:" << info.errorString();
        return;
	}
    foreach (const QHostAddress &address, host.addresses()){
    	host_addr = address;
		qDebug() << "[Peer::looked_up_ip]Found address:" << address.toString();
		active = true;
    }	
}

void Peer::looked_up_name(QHostInfo info){
	// get name from info
	if (host.error() != QHostInfo::NoError){
        qDebug() << "[Peer::looked_up_name]Lookup failed:" << info.errorString();
        return;
	}
	host_name = host.hostName();
	qDebug() << "[Peer::looked_up_name]Found name:" << host_name;
	active = true;
}