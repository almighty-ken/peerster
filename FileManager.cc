#include <unistd.h>
#include <stdlib.h>
#include <string>
#include <time.h>

#include <QDebug>
#include <QVariant>
#include <QFileInfo>
#include <QFile>
#include <QRegExp>


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

QString FileManager::hash_sha1(QByteArray data){
	if(!QCA::isSupported("sha1"))
	        qDebug() << "[FileManager::hash_sha1]SHA1 not supported!";
    else
    {	
    	QCA::Hash shaHash("sha1");
    	shaHash.update(data);
    	QByteArray result = shaHash.final().toByteArray();
    	qDebug() << "[FileManager::hash_sha1]Hash result: " << QCA::arrayToHex(result);
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

void FileManager::received_query(QMap<QString, QVariant> query){

	QList<file_info> file_info_list;
	QVariantList match_names;
	QVariantList match_ids;

	// parse search key
	QStringList terms = query[QString("Search")].toString().split(QRegExp("\\s+"), 
		QString::SkipEmptyParts);

	// look against info file_name
	for(int i=0; i<file_info_list.length(); i++){
		QString name = file_info_list.at(i).file_name;
		for(int j=0; j<terms.length(); j++){
			if(name.toStdString().find(terms[j].toStdString()) != std::string::npos){
				// is a match
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
	new_entry.blocklist_hash = file_ID;
	file_option_list.append(new_entry);
}

void FileManager::file_download(QString file_name){
	for(int i=0; i<file_option_list.length(); i++){
		if(file_option_list.at(i).file_name==file_name){
			// TODO:emit some signal to initiate download
			qDebug() << "[FileManager::file_download]Init download";
			break;
		}
	}
	qDebug() << "[FileManager::file_download]Bad download file name";
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
		new_file.file_block[block_count].append(temp,length);
		current_size += read_len;
		// calculate the hash of this and append to block_list
		new_file.blocklist.append(hash_sha1(new_file.file_block[block_count]));
		block_count++;
	}
	new_file.block_count = block_count;

	// calculate blocklist_hash
	// insert into file_info_list
	new_file.blocklist_hash.append(hash_sha1(new_file.blocklist));
	file_info_list.append(new_file);
	qDebug() << "[FileManager::add_file_entry]File entry added";
}
