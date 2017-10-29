#include <unistd.h>
#include <stdlib.h>
#include <string>
#include <time.h>

#include <QDebug>
#include <QVariant>
#include <vector>
#include <random>
#include <algorithm>
#include <iterator>
#include <chrono>

#include "main.hh"

#define MAX_HOPS 10
#define MAX_BUDGET 3

GNode::GNode(){
	// Pick a range of four UDP ports to try to allocate by default,
	// computed based on my Unix user ID.
	// This makes it trivial for up to four Peerster instances per user
	// to find each other on the same host,
	// barring UDP port conflicts with other applications
	// (which are quite possible).
	// We use the range from 32768 to 49151 for this purpose.
	myPortMin = 32768 + (getuid() % 4096)*4;
	myPortMax = myPortMin + 3;
	message_sequence = 0;
	noForward = false;
	require_confirm = false;

	// generate a unique originID here
	srand (time(NULL)+getpid());
	originID = QHostInfo::localHostName();
	originID.append(QString::number(rand()%1000));


	// set up entropy timer
	entropy_timer.start(10000);
	route_timer.start(6000);

	qDebug() << "[GNode::GNode]originID is: " << originID;

	connect(this,SIGNAL(readyRead()),
		this,SLOT(received_UDP_message()));
	connect(&entropy_timer,SIGNAL(timeout()),this,SLOT(entropy_message()));
	connect(&entropy_timer,SIGNAL(timeout()),this,SLOT(check_waitlist()));

	// lab2
	connect(&route_timer,SIGNAL(timeout()),this,SLOT(send_route_rumour()));
	send_route_rumour();

	// lab3
	connect(&search_timer,SIGNAL(timeout()),this,SLOT(exec_search()));
}

void GNode::learn_peer(QHostAddress addr, quint16 port){
	// go through list
	for(int i=0; i<peer_list.length(); i++){
		if(peer_list[i]->host_addr == addr && peer_list[i]->host_port == port)
			return;
	}
	// add peer
	Peer *new_peer = new Peer();
	new_peer->insert(addr,port);
	peer_list.append(new_peer);
	qDebug() << "[GNode::learn_peer]New peer auto added, now we have "<< peer_list.length();
}

void GNode::received_UDP_message(){
	// qDebug() << "Message from UDP waiting:";
	QByteArray datagram;
	QMap<QString, QVariant> map_message;
	QHostAddress address;
	quint16 port;
	datagram.resize(QUdpSocket::pendingDatagramSize());
	// if we wish to record down the source of the message we can also do it here
	QUdpSocket::readDatagram(datagram.data(),datagram.size(),&address,&port);

	// learn peer here
	learn_peer(address,port);
	// now we should deserialize the message
	{
		QDataStream * stream = new QDataStream(&datagram, QIODevice::ReadOnly);
		(*stream) >> map_message;
	}
	// check if the message is a rumor or a status
	int message_type = check_message_type(map_message);
	if(message_type == 0){
		// this is a status message, which is a confirmation
		// qDebug() << "[GNode::received_UDP_message]Received status message:" << map_message;

		// now we should update waitlist
		update_waitlist(address,port);

		QMap<QString,QVariant> status = map_message["Want"].toMap();
		process_status(status,address,port);
	}else if(message_type == 1){
		// this is a chat rumor message
		// now we should see if we already have the message
		// if have seen, ignore, else start rumoring
		// qDebug() << "[GNode::received_UDP_message]Received rumor message:" << map_message;
		if(router.is_new_origin(map_message)){
			emit add_dm_target(map_message["Origin"].toString());
		}
		router.update_table(map_message,address,port);
		if(!inDB(map_message)){
			datagram = add_shortcut_info(map_message,address,port);
			if(!noForward)
				random_send(datagram);
			QVariant message_text = map_message["ChatText"];
			emit send_message2Dialog(message_text.toString());
		}
		// no matter in DB or not, we want to make sure statusDB is updated
		add2DB(map_message);
		// now we should reply a confirmation that we have received the message
		qDebug() << "[GNode::received_UDP_message]Sending Confirmation...";
		SerializeSend_message(build_status(),address,port);
	}else if(message_type==2){
		// this is a route rumor message
		if(router.is_new_origin(map_message)){
			emit add_dm_target(map_message["Origin"].toString());
		}
		router.update_table(map_message,address,port);
		datagram = add_shortcut_info(map_message,address,port);
		// qDebug() << "[GNode::received_UDP_message]Received route message:" << map_message;
		// enabling random_send here can cause big traffic between two nodes
		random_send(datagram);
	}else if(message_type==3){
		// this is a DM
		// qDebug() << "[GNode::received_UDP_message]Received direct message:" << map_message;
		if(map_message["Dest"].toString()==originID){
			// this is our message
			emit send_message2Dialog(map_message["ChatText"].toString());
		}else{
			if(map_message["HopLimit"].toUInt()>0){
				// forward the message
				QString message_text = map_message["ChatText"].toString();
				QString target = map_message["Dest"].toString();
				quint32 limit = map_message["HopLimit"].toUInt()-1;
				if(!noForward)
					received_dm2send(target,message_text,limit);
			}
		}
	}else if(message_type==4){
		// this is search request, first locally process, then broadcast

		// emit a signal with message to FileManager
		// FileManager returns a signal with info to send search reply
		qDebug() << "[GNode::received_UDP_message]Received search request:" << map_message;
		emit file_query(map_message);
		broadcast_search(map_message);
	}else if(message_type==5){
		// this is a search reply
		// treat similarly to DM, forward or receive if dest
		qDebug() << "[GNode::received_UDP_message]Received search reply:" << map_message;
		if(map_message["Dest"].toString()==originID){
			// this is our message
			// lab3: differentiate between file sending messages and DM here
			search_reply_proc(map_message);
		}else{
			if(map_message["HopLimit"].toUInt()>0){
				// forward the message
				QString target = map_message["Dest"].toString();
				map_message["HopLimit"] = QVariant(map_message["HopLimit"].toUInt()-1);

				QHash<QString,QVariant> entry;
				entry = router.retrieve_info(target);
				QHostAddress ip = QHostAddress(entry["IP"].toString());
				quint16 port = entry["port"].toUInt();

				SerializeSend_message(map_message,ip,port);
			}
		}
	}else if(message_type==6){
		qDebug() << "[GNode::received_UDP_message]Received block reply:" << map_message;
		if(map_message["Dest"].toString()==originID){
			// this is our message
			// lab3: differentiate between file sending messages and DM here
			block_reply_proc(map_message);
		}else{
			if(map_message["HopLimit"].toUInt()>0){
				// forward the message
				QString target = map_message["Dest"].toString();
				map_message["HopLimit"] = QVariant(map_message["HopLimit"].toUInt()-1);

				QHash<QString,QVariant> entry;
				entry = router.retrieve_info(target);
				QHostAddress ip = QHostAddress(entry["IP"].toString());
				quint16 port = entry["port"].toUInt();

				SerializeSend_message(map_message,ip,port);
			}
		}
	}else if(message_type==7){
		qDebug() << "[GNode::received_UDP_message]Received block request:" << map_message;
		if(map_message["Dest"].toString()==originID){
			// this is our message
			// lab3: differentiate between file sending messages and DM here
			block_request_proc(map_message);
		}else{
			if(map_message["HopLimit"].toUInt()>0){
				// forward the message
				QString target = map_message["Dest"].toString();
				map_message["HopLimit"] = QVariant(map_message["HopLimit"].toUInt()-1);
				QHash<QString,QVariant> entry;
				entry = router.retrieve_info(target);
				QHostAddress ip = QHostAddress(entry["IP"].toString());
				quint16 port = entry["port"].toUInt();
				SerializeSend_message(map_message,ip,port);
			}
		}
	}else{
		qDebug() << "[GNode::received_UDP_message]Received invalid UDP message";
	}
}

void GNode::block_request_proc(QMap<QString,QVariant> map_message){
	// first get the origin so we can return the message later
	// get the hash, and emit a message with these information
	QString dest = map_message["Origin"].toString();
	QByteArray hash = map_message["BlockRequest"].toByteArray();
	// qDebug() << ""
	emit block_requested(dest,hash);
}

void GNode::send_block_reply(QString dest, QByteArray hash, QByteArray data){
	qDebug() << "[GNode::send_block_reply]Sending block reply";
	QMap<QString, QVariant> map_message;
	map_message[QString("Dest")] = QVariant(dest);
	map_message[QString("Origin")] = QVariant(originID);
	map_message[QString("HopLimit")] = QVariant(MAX_HOPS);
	map_message[QString("BlockReply")] = QVariant(hash);
	map_message[QString("Data")] = QVariant(data);

	QHash<QString,QVariant> entry;
	entry = router.retrieve_info(dest);
	QHostAddress ip = QHostAddress(entry["IP"].toString());
	quint16 port = entry["port"].toUInt();
	SerializeSend_message(map_message,ip,port);
}

void GNode::block_reply_proc(QMap<QString,QVariant> reply){
	// get the source from origin in message
	// get the data and hash
	// send to manager
	QString source = reply["Origin"].toString();
	QByteArray data = reply["Data"].toByteArray();
	QByteArray hash = reply["BlockReply"].toByteArray();

	emit block_received(source, data, hash);
}

void GNode::search_reply_proc(QMap<QString,QVariant> reply){
	QVariantList filenames = reply["MatchNames"].toList();
	QVariantList fileIDs = reply["MatchIDs"].toList();
	QString source = reply["Origin"].toString();

	for(int i=0; i<filenames.length(); i++){
		// emit send_file2Dialog(filenames.at(i).toString());
		emit send_file2Manager(filenames.at(i).toString(), source, fileIDs.at(i).toByteArray());
	}
}

void GNode::broadcast_search(QMap<QString,QVariant> message){
	quint16 budget = message[QString("Budget")].toUInt() - 1;

	if(budget <= 0)
		return;

	quint16 base = budget/peer_list.length();
	quint16 leftover = budget - (base*peer_list.length());

	std::vector<int> idx;
	for(int i=0; i<peer_list.length(); i++){
		idx.push_back(i);
	}
	// std::random_device rd;
	// std:mt19937 g(rd());
	unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
	std::shuffle(idx.begin(),idx.end(),std::default_random_engine(seed));

	for(int i=0; i<peer_list.length(); i++){
		int dest = idx.back();
		idx.pop_back();
		if(i < leftover){
			// can have extra budget
			message[QString("Budget")] = QVariant(base+1);
			SerializeSend_message(message,peer_list[dest]->host_addr,peer_list[dest]->host_port);
		}
		if(base>0){
			message[QString("Budget")] = QVariant(base);
			SerializeSend_message(message,peer_list[dest]->host_addr,peer_list[dest]->host_port);
		}
	}
}

void GNode::start_search(QString keywords){
	// reset params
	search_cnt = 1;

	// we first parse the keywords, then
	QMap<QString, QVariant> map_message;
	map_message[QString("Search")] = QVariant(keywords);
	map_message[QString("Origin")] = QVariant(originID);
	query_cache = map_message;

	search_timer.start(1000);
}

void GNode::exec_search(){
	search_cnt = search_cnt*2;
	if(search_cnt >= MAX_BUDGET){
		qDebug() << "[GNode::exec_search]Search ended";
		// turn off timer
		search_timer.stop();
		return;
	}
	qDebug() << "[GNode::exec_search]Search with budget " << search_cnt;
	quint16 search_budget = search_cnt;
	query_cache[QString("Budget")] = QVariant(search_budget);

	QByteArray data;
	{
		QDataStream * stream = new QDataStream(&data, QIODevice::WriteOnly);
		(*stream) << query_cache;
	}

	all_send(data);

	// std::vector<int> idx;
	// for(int i=0; i<peer_list.length(); i++){
	// 	idx.push_back(i);
	// }
	// std::random_device rd;
	// std:mt19937 g(rd());
	// std::shuffle(v.begin(),v.end(),g);


	// for(int i=0; i<peer_list.length(); i++){
	// 	int dest = idx.back();
	// 	idx.pop_back();
	// 	if(i < leftover){
	// 		// can have extra budget
	// 		SerializeSend_message(map_message,peer_list[dest]->host_addr,peer_list[dest]->host_port);
	// 	}
	// 	SerializeSend_message(map_message,peer_list[dest]->host_addr,peer_list[dest]->host_port);
	// }
}

void GNode::send_block_req(QString dest, QByteArray hash){
	qDebug() << "[GNode::send_block_req]Sending block request";
	QMap<QString, QVariant> map_message;
	map_message[QString("Dest")] = QVariant(dest);
	map_message[QString("Origin")] = QVariant(originID);
	map_message[QString("HopLimit")] = QVariant(MAX_HOPS);
	map_message[QString("BlockRequest")] = QVariant(hash);

	QHash<QString,QVariant> entry;
	entry = router.retrieve_info(dest);
	QHostAddress ip = QHostAddress(entry["IP"].toString());
	quint16 port = entry["port"].toUInt();

	qDebug() << "[GNode::send_block_req]Resquest: " << map_message;
	qDebug() << ip << port;

	SerializeSend_message(map_message,ip,port);
}

int GNode::check_message_type(QMap<QString,QVariant> message){
	if(message.contains("Want"))
		return 0;
	else if((message.contains("ChatText") && message.contains("Origin")) && message.contains("SeqNo"))
		return 1;
	else if(message.contains("Origin") && message.contains("SeqNo"))
		return 2;
	else if((message.contains("Origin") && message.contains("Dest")) && message.contains("HopLimit") 
		&& !message.contains("MatchIDs") && !message.contains("BlockReply") && !message.contains("BlockRequest"))
		return 3;
	else if((message.contains("Origin") && message.contains("Search")) && message.contains("Budget"))
		return 4;
	else if((message.contains("Dest") && message.contains("SearchReply")) && message.contains("MatchIDs"))
		return 5;
	else if((message.contains("Dest") && message.contains("BlockReply")) && message.contains("Data"))
		return 6;
	else if((message.contains("Dest") && message.contains("BlockRequest")) && message.contains("Origin"))
		return 7;
	return -1;
}

void GNode::send_search_reply(QString dest, 
	QString search_reply, QVariantList match_names, QVariantList match_ids){
	// generate a search reply message using the DM protocol
	qDebug() << "[GNode::send_search_reply]Found target locally, send reply";
	QMap<QString, QVariant> map_message;
	map_message[QString("Dest")] = QVariant(dest);
	map_message[QString("Origin")] = QVariant(originID);
	map_message[QString("HopLimit")] = QVariant(MAX_HOPS);
	map_message[QString("SearchReply")] = QVariant(search_reply);
	map_message[QString("MatchNames")] = QVariant(match_names);
	map_message[QString("MatchIDs")] = QVariant(match_ids);

	QHash<QString,QVariant> entry;
	entry = router.retrieve_info(dest);
	QHostAddress ip = QHostAddress(entry["IP"].toString());
	quint16 port = entry["port"].toUInt();

	// qDebug() << "[GNode::send_search_reply]Response: " << map_message;
	// qDebug() << ip << port;

	SerializeSend_message(map_message,ip,port);
}

QByteArray GNode::add_shortcut_info(QMap<QString,QVariant> message, QHostAddress ip, quint16 port){
	message["LastIP"] = QVariant(ip.toString());
	message["LastPort"] = QVariant(port);
	QByteArray data;
	{
		QDataStream * stream = new QDataStream(&data, QIODevice::WriteOnly);
		(*stream) << message;
	}
	return data;
}


QMap<QString,QVariant> GNode::build_status(){
	QMap<QString,QVariant> status;
	
	status[QString("Want")] = QVariant(statusDB);
	// qDebug() << "[GNode::build_status]Current statusDB is :"<<status;
	return status;
}

void GNode::entropy_message(){
	QByteArray data;
	{
		QDataStream * stream = new QDataStream(&data, QIODevice::WriteOnly);
		(*stream) << build_status();
	}
	random_send(data);
	// qDebug() << "[GNode::entropy_message]Generated anti entropy message";
}

void GNode::process_status(QMap<QString,QVariant> status, QHostAddress addr, quint16 port){
	// we should check if status message equals to our local status
	if(status==statusDB){
		// randomly decide whether we should stop rumoring
		if(rand()%2 == 0){
			return;
		}else{
			QByteArray data;
			{
				QDataStream * stream = new QDataStream(&data, QIODevice::WriteOnly);
				(*stream) << build_status();
			}
			random_send(data);
		}
	}else{
		// if we have what others don't, send rumor
		// iterate through our own statusDB, if for a user our number is higher
		// send seqNO message to the source
		QMap<QString, QVariant>::const_iterator j = statusDB.constBegin();
		while (j != statusDB.constEnd()) {
		    QString userID = j.key();
		    quint32 seq = j.value().toUInt();
		    if(seq > status.value(userID).toUInt()){
		    	// construct key here
		    	QString message_key = userID;
		    	message_key.append(QString::number(status.value(userID).toUInt()));
		    	// qDebug() << "[GNode::process_status]message_key is: " << message_key;
		    	QMap<QString, QVariant> message = messageDB.value(message_key);
		    	if(noForward)
		    		break;
		    	// qDebug() << "[GNode::process_status]Sharing message with peer: " << message;
		    	SerializeSend_message(message,addr,port);
		    	break;
		    }
		    ++j;
		}
		// if others have what we don't, send status
		// iterate through their status, if for a user their number is higher
		// send our status over
		QMap<QString, QVariant>::const_iterator i = status.constBegin();
		while (i != status.constEnd()) {
		    QString userID = i.key();
		    quint32 seq = i.value().toUInt();
		    if(seq > statusDB.value(userID).toUInt()){
		    	SerializeSend_message(build_status(),addr,port);
		    	break;
		    }
		    ++i;
		}
	}
}

bool GNode::inDB(QMap<QString, QVariant> message){
	// the DB uses QMap<origin,seqno> as ID
	QString key = message.value("Origin").toString();
	key.append(message.value("SeqNo").toString());
	// qDebug() << "[GNode::inDB]Key used in DB search:" << key;
	if(messageDB.contains(key))
		return true;
	else
		return false;
}


void GNode::add2DB(QMap<QString, QVariant> message){
	// we add the message into message DB
	// the key is origin+seqno
	// statusDB keeps the lowest seqno of unseen messages
	QString origin = message["Origin"].toString();
	QString key = message["Origin"].toString();
	key.append(message["SeqNo"].toString());
	messageDB[key] = message;

	if(statusDB.contains(origin)){
		if(statusDB[origin].toInt()==message["SeqNo"].toInt()){
			statusDB.insert(origin,QVariant(statusDB[origin].toInt()+1));
			// qDebug() << "[GNode::add2DB]Can increase status";
		}else{
			// qDebug() << "[GNode::add2DB]Not sequential message";
		}
	}
	else if(message["SeqNo"].toInt()==0){
		statusDB.insert(origin,QVariant(1));
		// qDebug() << "[GNode::add2DB]Write 1 for DB";
	}else{
		statusDB.insert(origin,QVariant(0));
		// qDebug() << "[GNode::add2DB]New Message Source";
	}
	// qDebug() << "[GNode::add2DB]Added to DB:" << key;
}

void GNode::received_dm2send(QString target,QString message,quint32 hop){
	// this supports sending dm through route table
	// qDebug() << target << message;
	QMap<QString, QVariant> map_message;
	map_message[QString("Dest")] = QVariant(target);
	map_message[QString("Origin")] = QVariant(originID);
	map_message[QString("HopLimit")] = QVariant(hop);
	map_message[QString("ChatText")] = QVariant(message);

	QHash<QString,QVariant> entry;
	entry = router.retrieve_info(target);
	QHostAddress ip = QHostAddress(entry["IP"].toString());
	quint16 port = entry["port"].toUInt();

	SerializeSend_message(map_message,ip,port);
}

void GNode::send_route_rumour(){
	// send out our own route rumour
	// this will utilize the GNode::random_send function
	// we will build the route_rumour message and serialize it
	// then send it out through random_send
	QMap<QString, QVariant> map_message;
	map_message[QString("Origin")] = QVariant(originID);
	map_message[QString("SeqNo")] = QVariant(message_sequence);
	QByteArray data;
	{
		QDataStream * stream = new QDataStream(&data, QIODevice::WriteOnly);
		(*stream) << map_message;
	}
	// qDebug() << "[GNode::send_route_rumour]Route Rumor Generated";
	all_send(data);
}

void GNode::all_send(QByteArray data){
	int peer_cnt = peer_list.length();
	if(peer_cnt==0)
		return;
	for(int i=0; i<peer_cnt; i++){
		send_messageUDP(data,peer_list[i]->host_addr,peer_list[i]->host_port);
	}
	// qDebug() << "[GNode::all_send]Message sent to all peers";
}

void GNode::random_send(QByteArray data){
	// randomly choose a peer from peer list
	if(peer_list.length()==0)
		return;
	int idx = rand() % peer_list.length();
	if((rand()%3)==1){
		// stop forwarding randomly
		return;
	}
	send_messageUDP(data,peer_list[idx]->host_addr,peer_list[idx]->host_port);

	// qDebug() << "[GNode::random_send]Message sent to " << peer_list[idx]->host_addr 
		// << ":" << peer_list[idx]->host_port;
}

void GNode::send_messageUDP(QByteArray data, QHostAddress addr, quint16 port){

	QUdpSocket::writeDatagram(data,addr,port);
	
	QPair<QString,quint16> source;
	source.first = addr.toString();
	source.second = port;
	if(!confirm_waitlist.contains(source)){
		QQueue<QPair<QTime,QByteArray> > queue;
		confirm_waitlist[source] = queue;
	}
	QPair<QTime,QByteArray> p;
	p.first = QTime::currentTime();
	p.second = data;
	confirm_waitlist[source].enqueue(p);
	// set timer to trigger waitlist manager
	// QTimer::singleShot(5000,this,SLOT(check_waitlist()));
}

void GNode::update_waitlist(QHostAddress addr, quint16 port){
	QPair<QString,quint16> source;
	source.first = addr.toString();
	source.second = port;
	while(confirm_waitlist.contains(source) && !confirm_waitlist[source].isEmpty()){
		confirm_waitlist[source].dequeue();
	}
}

void GNode::check_waitlist(){
	if(!require_confirm)
		return;
	// iterate through head of every queue
	// if time out, resend that QByteArray
	// qDebug() << "[GNode::check_waitlist]Checking waitlist...";
	QMap<QPair<QString,quint16>, QQueue<QPair<QTime,QByteArray> > >::const_iterator i \
		= confirm_waitlist.constBegin();

	while (i != confirm_waitlist.constEnd()){
		QPair<QString,quint16> source = i.key();
		QQueue<QPair<QTime,QByteArray> > queue = i.value();
		if(!queue.isEmpty()){
			QTime send_time = queue.head().first;
			QTime due_time = send_time.addSecs(4);
			if(due_time < QTime::currentTime()){
				// message is due, resend
				QByteArray data = queue.dequeue().second;
				qDebug() << "[GNode::check_waitlist]Message to " << source.first << ":" \
				<< source.second << " overdue and is being resent";
				send_messageUDP(data,QHostAddress(source.first),source.second);
			}
			// clear queue
		}
		i++;
	}
}

void GNode::SerializeSend_message(QMap<QString, QVariant> map,
	QHostAddress addr, quint16 port){
	QByteArray data;
	{
		QDataStream * stream = new QDataStream(&data, QIODevice::WriteOnly);
		(*stream) << map;
	}
	send_messageUDP(data,addr,port);
	// qDebug() << "Message sent to addr: " << addr << port;
}

void GNode::received_message2send(QString message){
	// this slot receives signals containing message from GUI
	// this slot serializes QString to form QVariantmap object

	// QVariant variant_message = QVariant(message);
	QMap<QString, QVariant> map_message;
	map_message[QString("ChatText")] = QVariant(message);
	map_message[QString("Origin")] = QVariant(originID);
	map_message[QString("SeqNo")] = QVariant(message_sequence);

	if(peer_list.length()==0)
		return;
	int idx = rand() % peer_list.length();
	SerializeSend_message(map_message,peer_list[idx]->host_addr,peer_list[idx]->host_port);
	
	message_sequence++;
}

void GNode::add_peer_from_dialog(QString source){
	Peer *new_peer = new Peer();
	new_peer->insert(source);
	peer_list.append(new_peer);
}

bool GNode::bind(){
	// add peers from command line
	bool noDefaultPeer = false;
	bool default2peer = false;
	QStringList args = QCoreApplication::arguments();
	for(int i=1; i<args.length(); i++){
		if(args.at(i)=="-nopeer"){
			noDefaultPeer = true;
			continue;
		}
		if(args.at(i)=="-noforward"){
			noForward = true;
			continue;
		}
		if(args.at(i)=="-requireconfirm"){
			require_confirm = true;
			continue;
		}
		if(args.at(i)=="-default2peer"){
			default2peer = true;
			noDefaultPeer = true;
			continue;
		}
		Peer *new_peer = new Peer();
		new_peer->insert(args.at(i));
		peer_list.append(new_peer);
	}

	// emit a signal here to show origin ID
	emit send_originID(originID);

	// Try to bind to each of the range myPortMin..myPortMax in turn.
	for (int p = myPortMin; p <= myPortMax; p++) {
		if (QUdpSocket::bind(p)) {
			qDebug() << "[GNode::bind]bound to UDP port " << p;
			myPort = p;
			// build initial peer list
			for (int p = myPortMin; p <= myPortMax; p++){
				if(p==myPort)
					continue;
				// add this port to peer
				if(noDefaultPeer)
					continue;
				Peer *new_peer = new Peer();
				new_peer->insert(QHostAddress::LocalHost,p);
				peer_list.append(new_peer);
			}
			// lab3 update: -default2peer option
			if(default2peer){
				// TODO
				// qDebug() << "Using default 2 node option";
				if(myPort==myPortMin){
					Peer *new_peer = new Peer();
					new_peer->insert(QHostAddress::LocalHost,myPort+1);
					peer_list.append(new_peer);
				}else{
					Peer *new_peer = new Peer();
					new_peer->insert(QHostAddress::LocalHost,myPortMin);
					peer_list.append(new_peer);
				}
			}
			// lab3 update: send message to self
			Peer *new_peer = new Peer();
			new_peer->insert(QHostAddress::LocalHost,myPort);
			peer_list.append(new_peer);

			return true;
		}
	}

	qDebug() << "[GNode::bind]Oops, no ports in my default range " << myPortMin
		<< "-" << myPortMax << " available";
	return false;
}