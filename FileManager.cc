#include <unistd.h>
#include <stdlib.h>
#include <string>
#include <time.h>

#include <QDebug>
#include <QVariant>
#include <QFileInfo>
#include <QFile>
#include <QRegExp>
#include <QDir>


#include "main.hh"

#define MAX_FILE_SIZE 3348480
#define BLOCK_SIZE 8192

FileManager::FileManager(){
	qDebug() << "[FileManager::FileManager]Initiated";
}

void FileManager::files_selected_add(QStringList files){
	QStringList list = files;
	QStringList::Iterator it = list.begin();
	while(it != list.end()) {
	    add_file_entry(*it);
	    ++it;
	}
	dump_file_list();
}

QByteArray FileManager::hash_sha1(QByteArray data){
	if(!QCA::isSupported("sha1"))
	        qDebug() << "[FileManager::hash_sha1]SHA1 not supported!";
    else
    {	
    	QCA::Hash shaHash("sha1");
    	shaHash.update(data);
    	QByteArray result = shaHash.final().toByteArray();
    	// qDebug() << "[FileManager::hash_sha1]Hash result: " << QCA::arrayToHex(result);
    	// qDebug() << "[FileManager::hash_sha1]Hash size: " << result.size();
        return result;
    }
    return NULL;
}

void FileManager::dump_file_list(){
	for(int i=0; i<file_info_list.length(); i++){
		qDebug() << "[FileManager::dump_file_list]Entry " << i 
			<< " of the list is:"
			<< file_info_list.at(i).file_name << " with " <<
			file_info_list.at(i).block_count << " blocks";
	}
}

void FileManager::dump_option_list(){
	for(int i=0; i<file_option_list.length(); i++){
		qDebug() << "[FileManager::dump_option_list]Entry " << i 
			<< " of the list is:"
			<< file_option_list.at(i).file_name;
	}
}

void FileManager::received_query(QMap<QString, QVariant> query){

	qDebug() << "[FileManager::received_query]Processing query";

	QVariantList match_names;
	QVariantList match_ids;

	// parse search key
	QStringList terms = query[QString("Search")].toString().split(QRegExp("\\s+"), 
		QString::SkipEmptyParts);

	// look against info file_name
	for(int i=0; i<file_info_list.length(); i++){
		QString name = file_info_list.at(i).file_name;
		for(int j=0; j<terms.length(); j++){
			QRegExp rx(terms[j]);
			// qDebug() << "search term name: " << terms[j];
			if(rx.indexIn(name) != -1){
				// is a match
				qDebug() << "[FileManager::received_query]Match found!";
				match_names.append(QVariant(name));
				match_ids.append(QVariant(file_info_list.at(i).blocklist_hash));
				break;
			}
		}
	}

	if(match_names.length()>0){
		emit return_query(query[QString("Origin")].toString(), 
			query[QString("Search")].toString(), match_names, match_ids);
	}
}

void FileManager::file_option_add(QString file_name, QString source, QByteArray file_ID){
	// add this option to local option list
	file_info new_entry;
	new_entry.file_name = file_name;
	new_entry.source = source;
	new_entry.downloaded_block_count = 0;
	new_entry.blocklist_hash = file_ID;
	file_option_list.append(new_entry);
	dump_option_list();
}

void FileManager::block_received(QString source, QByteArray data, QByteArray hash){

	// if yes, store it and decompose it, then call download manager
	// else, fill it into the block spot
	// finally check whether the file is complete
	// if so, call method to write file

	QByteArray check_hash = hash_sha1(data);
	if(check_hash != hash){
		qDebug() << "[FileManager::block_received]Hash does not match!";
		return;
	}

	for(int i=0; i<file_option_list.length(); i++){
		if(file_option_list.at(i).source != source){
			continue;
		}
		if(file_option_list.at(i).blocklist_hash == hash){
			// check whether the data is a blocklist_hash
			file_option_list[i].blocklist = data;
			// decompose the blocklist call download manager
			download_manager(i);
			return;
		}else{
			// go through each file_block_hash
			for(int j=0; j < file_option_list.at(i).block_count; j++){
				if(file_option_list.at(i).file_block_hash[j]==hash){
					file_option_list[i].file_block[j] = data;
					file_option_list[i].downloaded_block_count++;
					// check whether file is complete
					// if so, export file
					if(file_option_list.at(i).downloaded_block_count == file_option_list.at(i).block_count){
						qDebug() << "[FileManager::block_received]File download complete";
						export_file(file_option_list.at(i));

					}
					return;
				}
			}
		}
	}
}

void FileManager::export_file(file_info file){
	// concat the blocks into 1 QByteArray
	// modify file name
	// export file with data
	QByteArray data;
	for(int i=0; i<file.block_count; i++){
		data.append(file.file_block[i]);
	}

	QString file_name = file.file_name;
	QStringList terms = file_name.split('/');
	file_name = terms.last();
	QString file_path = QDir::currentPath().append(file_name);

	qDebug() << "[FileManager::export_file]Exporting to " << file_path;

	QFile file_pt(file_path);
	file_pt.open(QIODevice::WriteOnly);
	file_pt.write(data);
	file_pt.close();
}

void FileManager::block_requested(QString dest, QByteArray hash){
	// go through all the blocks and send a block reply
	QByteArray data;
	for(int i=0; i<file_info_list.length(); i++){
		file_info entry = file_info_list.at(i);
		if(entry.blocklist_hash == hash){
			data = entry.blocklist;
			break;
		}
		for(int j=0; j<entry.block_count; j++){
			if(entry.file_block_hash[j] == hash){
				data = entry.file_block[j];
				break;
			}
		}
	}
	if(data.isEmpty()){
		qDebug() << "[FileManager::block_requested]Couldn't find requested block";
		return;
	}

	emit send_block_reply(dest,hash,data);
	return;
}

void FileManager::file_download(QString file_name){
	for(int i=0; i<file_option_list.length(); i++){
		if(file_option_list.at(i).file_name==file_name){
			// download_manager(file_option_list.at(i));
			// start download for block file
			// emit signal for block request
			// the block reply signal will trigger a slot to fill that block in
			// the slot checks whether hash is the blocklist_hash
			// if yes, trigger download manger
			emit send_block_req(file_option_list.at(i).source,file_option_list.at(i).blocklist_hash);
			return;
		}
	}
	qDebug() << "[FileManager::file_download]Bad download file name";
}

void FileManager::download_manager(int i){
	qDebug() << "[FileManager::download_manager]Init download";
	// i is the index in the option list where blocklist is just acquired
	// use the blocklist to fill in file_block_hash
	// set the total block_count
	// for each entry in file_block_hash
	// emit a download request

	QByteArray blocklist = file_option_list.at(i).blocklist;
	quint16 block_count = 0;

	while(!blocklist.isEmpty()){
		QByteArray block_hash = blocklist.mid(0,20);
		file_option_list[i].blocklist.remove(0,20);
		file_option_list[i].file_block_hash[block_count] = block_hash;
		block_count++;
	}

	file_option_list[i].block_count = block_count;

	for(int j=0; j<block_count; j++){
		emit send_block_req(file_option_list[i].source,file_option_list[i].file_block_hash[j]);
		sleep(2);
	}
}

void FileManager::add_file_entry(QString file_name){
	// qDebug() << file_name;
	// we build the entry of files here
	// use internal qlist to store these files

	// does list perform deep copy?
	file_info new_file;
	new_file.file_name = file_name;
	QFileInfo info(file_name);
	if(info.size() > MAX_FILE_SIZE){
		qDebug() << "[FileManager::add_file_entry]File oversize!";
		return;
	}
	new_file.file_size = info.size();

	// start parsing file into blocks
	QFile file(file_name);
	if(!file.open(QIODevice::ReadOnly)){
		qDebug() << "[FileManager::add_file_entry]File cannot be opened!";
	}
	QDataStream in(&file);
	quint32 current_size = 0;
	quint16 block_count = 0;

	while(current_size < new_file.file_size){
		char temp[BLOCK_SIZE];
		quint32 length = BLOCK_SIZE;
		int read_len = in.readRawData(temp,length);
		new_file.file_block[block_count].append(temp);
		current_size += read_len;
		// calculate the hash of this and append to block_list
		QByteArray hash = hash_sha1(new_file.file_block[block_count]);
		new_file.blocklist.append(hash);
		new_file.file_block_hash[block_count] = hash;
		block_count++;
	}
	new_file.block_count = block_count;

	// calculate blocklist_hash
	// insert into file_info_list
	new_file.blocklist_hash.append(hash_sha1(new_file.blocklist));
	file_info_list.append(new_file);
	qDebug() << "[FileManager::add_file_entry]File entry added";
}
