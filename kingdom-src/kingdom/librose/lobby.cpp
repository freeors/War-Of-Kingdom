/* $Id: multiplayer.cpp 47847 2010-12-05 21:12:23Z shadowmaster $ */
/*
   Copyright (C) 2007 - 2010
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#define GETTEXT_DOMAIN "wesnoth-lib"

#include "global.hpp"

#include "gettext.hpp"
#include "lobby.hpp"
#include "thread.hpp"
#include "help.hpp"
#include "filesystem.hpp"
#include "serialization/string_utils.hpp"
#include "formula_string_utils.hpp"
#include "wml_exception.hpp"
#include "preferences.hpp"
#include "integrate.hpp"
#include "proto_irc.hpp"
#include "posix.h"

struct tfopen_lock
{
	tfopen_lock(const std::string& file, bool read_only)
		: fp(INVALID_FILE)
		, data(NULL)
		, data_size(0)
	{
		if (read_only) {
			posix_fopen(file.c_str(), GENERIC_READ, OPEN_EXISTING, fp);
		} else {
			posix_fopen(file.c_str(), GENERIC_WRITE, CREATE_ALWAYS, fp);
		}
	}

	~tfopen_lock()
	{
		close();
	}

	bool valid() const { return fp != INVALID_FILE; }

	void resize_data(int size, int vsize = 0)
	{
		if (size > data_size) {
			char* tmp = (char*)malloc(size);
			if (data) {
				if (vsize) {
					memcpy(tmp, data, vsize);
				}
				free(data);
			}
			data = tmp;
			data_size = size;
		}
	}

	void close()
	{
		if (data) {
			free(data);
			data = NULL;
			data_size = 0;
		}
		if (fp != INVALID_FILE) {
			posix_fclose(fp);
			fp = INVALID_FILE;
		}
	}

public:
	posix_file_t fp;
	char* data;
	int data_size;
};

tlobby* lobby = NULL;

tlobby::thandler::thandler()
	: has_joined_(false)
{
}

tlobby::thandler::~thandler()
{
	std::vector<thandler*>& handlers = lobby->handlers_;
	std::vector<thandler*>::iterator it = std::find(handlers.begin(), handlers.end(), this);
	if (it != handlers.end()) {
		handlers.erase(it);
	}
}

void tlobby::thandler::join()
{
	if (has_joined_) {
		return;
	}
	lobby->handlers_.push_back(this);
	has_joined_ = true;
}

tlobby::tlog_handler::tlog_handler()
	: has_joined_(false)
{
}

tlobby::tlog_handler::~tlog_handler()
{
	std::vector<tlog_handler*>& handlers = lobby->log_handlers_;
	handlers.erase(std::remove(handlers.begin(), handlers.end(), this));
}

void tlobby::tlog_handler::join()
{
	if (has_joined_) {
		return;
	}
	lobby->log_handlers_.push_back(this);
	has_joined_ = true;
}

network::connection connection_id = 1;

tsock null_sock(-1);

#define INVALID_PORT	-1
tsock::tsock(int at)
	: at_(at)
	, tag_()
	, conn_(network::null_connection)
	, host_()
	, port_(INVALID_PORT)
	, state_(s_none)
	, last_active_time_(0)
	, next_create_time_(0)
	, sock2_(NULL)
	, accept_(false)
	, reconnect_prohabit_(DEFAULT_RECONNECT_PROHABIT)
	, heartbeat_threshold_(DEFAULT_HEARTBEAT_THRESHOLD)
	, max_noresponse_msgs_(2)
	, noresponse_threshold_(20000)
	, data_()
	, raw_data_(NULL)
	, raw_data_size_(0)
	, raw_data_vsize_(0)
	, raw_data_only(true)
	, require_stats(true)
	, connected_at_(0)
	, msg_send_time(0)
	, msg_send_gap(0)
	, error_()
{}

tsock::tsock(const tsock& that)
	: at_(that.at_)
	, tag_(that.tag_)
	, conn_(that.conn_)
	, host_(that.host_)
	, port_(that.port_)
	, state_(that.state_)
	, last_active_time_(that.last_active_time_)
	, next_create_time_(that.next_create_time_)
	, sock2_(that.sock2_)
	, accept_(that.accept_)
	, reconnect_prohabit_(that.reconnect_prohabit_)
	, heartbeat_threshold_(that.heartbeat_threshold_)
	, data_(that.data_)
	, raw_data_only(that.raw_data_only)
	, require_stats(that.require_stats)
	, connected_at_(that.connected_at_)
	, error_(that.error_)
{
	if (raw_data_size_ != that.raw_data_size_) {
		if (raw_data_) {
			free(raw_data_);
		}
		raw_data_ = (char*)malloc(that.raw_data_size_);
		memcpy(raw_data_, that.raw_data_, that.raw_data_vsize_);

	} else if (that.raw_data_) {
		memcpy(raw_data_, that.raw_data_, that.raw_data_vsize_);

	} else {
		VALIDATE(!raw_data_, null_str);
	}

	raw_data_size_ = that.raw_data_size_;
	raw_data_vsize_ = that.raw_data_vsize_;
}

tsock::~tsock()
{
	if (raw_data_) {
		free(raw_data_);
	}
}

// true: this msg can inserted, next can send it.
// false: 1)buffed is full. so next cann't send.
// remark: if this msg has buffed, will update its timestamp.
bool tsock::insert_noresponse_msg(int major, const std::string& minor)
{
	const std::string cookie = tnoresponse_msg::form_cookie(major, minor);
	Uint32 now = SDL_GetTicks();
	
	for (std::vector<tnoresponse_msg>::iterator it = noresponse_msgs_.begin(); it != noresponse_msgs_.end(); ) {
		tnoresponse_msg& msg = *it;
		if (now >= msg.t + noresponse_threshold_) {
			it = noresponse_msgs_.erase(it);
			continue;
		}
		if (msg.cookie == cookie) {
			it = noresponse_msgs_.erase(it);
			continue;
		}
		++ it;
	}

	if ((int)noresponse_msgs_.size() >= max_noresponse_msgs_) {
		return false;
	}
	noresponse_msgs_.push_back(tnoresponse_msg(now, cookie));

	return true;
}

bool tsock::erase_noresponse_msg(int major, const std::string& minor)
{
	const std::string cookie = tnoresponse_msg::form_cookie(major, minor);

	std::vector<tnoresponse_msg>::iterator it = std::find(noresponse_msgs_.begin(), noresponse_msgs_.end(), cookie);
	if (it == noresponse_msgs_.end()) {
		return false;
	}
	noresponse_msgs_.erase(it);
	return true;
}

void tsock::resize_raw_data(int size)
{
	if (size > raw_data_size_) {
		char* tmp = (char*)malloc(size);
		if (raw_data_) {
			if (raw_data_vsize_) {
				memcpy(tmp, raw_data_, raw_data_vsize_);
			}
			free(raw_data_);
		}
		raw_data_ = tmp;
		raw_data_size_ = size;
	}
}

bool tsock::connect(TCPsocket sock, const std::string& host, int port)
{
	sock2_ = sock;
	host_ = host;
	port_ = port;
	conn_ = connection_id ++;

	return true;
}

void tsock::post_disconnect()
{
	sock2_ = NULL;
	connected_at_ = 0;
	conn_ = network::null_connection;
}

void tsock::set_connect_result(const std::string& error)
{
	connected_at_ = SDL_GetTicks();
	error_ = error;
}

void tsock::set_host(const std::string& host, int port)
{
	if (host == host_ && port == port_) {
		return;
	}
	host_ = host;
	port_ = port;
}

void tsock::reset_connect()
{
	// create sock thread must be successfully. it is different from create sock successfully.
	// connected_at_ != 0 mean thread successfully.
	// conn_ != network::null_connection mean sock successfully.
	// if reset_connect should disconenct, it must called after create sock thread run! or result to app and namespace::network out of sync.
	// namespace::network has a bonus sock that app no. create sock thread created it(sock).
	// see remark#50
	VALIDATE(connected_at_, "thread that create sock must has run!");

	network::disconnect(conn_);
	state_ = s_none;
	conn_ = network::null_connection;

	lobby->broadcast_handle_raw(at_, t_disconnected, NULL);
}

bool tsock::check_time_overflow(Uint32 threshold)
{
	bool should_reconnect = false;
	int distance = 0;
	if (state_ == s_create || state_ == s_consult || state_ == s_ready) {
		Uint32 now = SDL_GetTicks();
		if (now - last_active_time_ > threshold) {
			if (now < last_active_time_ + reconnect_prohabit_) {
				next_create_time_ = last_active_time_ + reconnect_prohabit_;
				distance = next_create_time_ - now;
			} else {
				next_create_time_ = now;
			}
			should_reconnect = true;
		}
	}

	if (should_reconnect) {
		std::string err;

		utils::string_map symbols;
		symbols["threshold"] = str_cast(threshold / 1000);
		std::string str = vgettext("wesnoth-lib", "Have not received data in $threshold sec!", symbols);

		symbols["error"] = tintegrate::generate_format(str, "red");
		if (distance > 1000) {
			symbols["distance"] = str_cast(distance / 1000); 
			err = vgettext("wesnoth-lib", "$error will reconnect after $distance sec.", symbols);
		} else {
			err = vgettext("wesnoth-lib", "$error will Reconnect immediately!", symbols);
		}
		lobby->add_log(*this, err);
		reset_connect();
	}

	return should_reconnect;
}

void tsock::process_error(const std::string& err_str)
{
	std::string err_str2 = tintegrate::stuff_escape(err_str);

	next_create_time_ = last_active_time_ + reconnect_prohabit_;
	Uint32 now = SDL_GetTicks();
	if (now > next_create_time_) {
		next_create_time_ = now;
	}
	int distance = next_create_time_ > now? next_create_time_ - now: 0;

	std::string err;
	utils::string_map symbols;
	symbols["error"] = tintegrate::generate_format(err_str2, "red");
	if (distance > 1000) {
		symbols["distance"] = str_cast(distance / 1000); 
		err = vgettext("wesnoth-lib", "$error will reconnect after $distance sec.", symbols);
	} else {
		err = vgettext("wesnoth-lib", "$error will Reconnect immediately!", symbols);
	}

	lobby->add_log(*this, err);
	reset_connect();
}

size_t tsock::queue_raw_data(const char* buf, int len)
{
	network::buffer* queued_buf = new network::buffer(sock2_);
	queued_buf->raw_buffer.resize(len);
	memcpy(&queued_buf->raw_buffer[0], buf, len);
	network::queue_buffer(sock2_, queued_buf);
	return len;
}

size_t tsock::queue_data(const config& buf, const std::string& packet_type)
{
	network::buffer* queued_buf = new network::buffer(sock2_);
	network::output_to_buffer(sock2_, buf, queued_buf->stream);
	const size_t size = queued_buf->stream.str().size();

	network::add_bandwidth_out(packet_type, size);
	network::queue_buffer(sock2_, queued_buf);
	return size;
}

std::map<std::string, int> tlobby_user::uids;
int tlobby_user::get_uid(const std::string& nick2, bool must_exist)
{
	static int id = 1;

	std::string nick = nick2;
	
	if (lobby->chat.serv()) {
		char* nick_prefixes = lobby->chat.serv()->nick_prefixes;
		if (strchr(nick_prefixes, nick[0])) {
			VALIDATE(false, "nick has invalid prefix!");
		}
	}
	std::map<std::string, int>::const_iterator it = uids.find(nick);
	if (it != uids.end()) {
		return it->second;
	}
	if (must_exist) {
		std::stringstream err;
		err << tintegrate::generate_format(nick, "yellow");
		err << " nick isn't in lobby!";
		VALIDATE(false, err.str());
	}
	uids.insert(std::make_pair(nick, id));
	return id ++;
}

const std::string& tlobby_user::get_nick(int uid)
{
	for (std::map<std::string, int>::const_iterator it = uids.begin(); it != uids.end(); ++ it) {
		if (it->second == uid) {
			return it->first;
		}
	}
	VALIDATE(false, "Cannot find uid!");
	return null_str;
}

#define TLOBBY_USER_NPOS		0
const int tlobby_user::npos = TLOBBY_USER_NPOS;

#define TLOBBY_CHANNEL_NPOS		0
const int tlobby_channel::npos = TLOBBY_CHANNEL_NPOS;
const int tlobby_channel::t_me = 1;
const int tlobby_channel::t_friend = 2;
std::map<std::string, int> tlobby_channel::cids;

tlobby_user null_user(TLOBBY_USER_NPOS, "");
tlobby_channel null_channel(TLOBBY_CHANNEL_NPOS, "", "");

int tlobby_channel::get_cid(const std::string& chan, bool must_exist)
{
	static int id = min_allocatable;
	if (cids.empty()) {
		cids.insert(std::make_pair("friend", t_friend));
	}
	std::map<std::string, int>::const_iterator it = cids.find(chan);
	if (it != cids.end()) {
		return it->second;
	}
	if (must_exist) {
		std::stringstream err;
		err << tintegrate::generate_format(chan, "yellow");
		err << " channel isn't in lobby!";
		VALIDATE(false, err.str());
	}
	cids.insert(std::make_pair(chan, id));
	return id ++;
}

const std::string& tlobby_channel::get_nick(int cid)
{
	for (std::map<std::string, int>::const_iterator it = cids.begin(); it != cids.end(); ++ it) {
		if (it->second == cid) {
			return it->first;
		}
	}
	VALIDATE(false, "Cannot find cid!");
	return null_str;
}

tlobby_channel::~tlobby_channel()
{
	// lobby is static variable.
	// don't destruct users.	
}

std::string tlobby_channel::name() const
{
	if (is_allocatable(cid)) {
		return nick;
	}
	if (cid == t_me) {
		return _("Me");
	} else if (cid == t_friend) {
		return _("Friend");
	}
	return null_str;
}

tlobby_user& tlobby_channel::get_user(int uid) const
{
	for (std::vector<tlobby_user*>::const_iterator it = users.begin(); it != users.end(); ++ it) {
		tlobby_user& user = **it;
		if (user.uid == uid) {
			return user;
		}
	}
	return null_user;
}

tlobby_user& tlobby_channel::insert_user(int uid, const std::string& nick)
{
	tlobby_user& user = lobby->chat.insert_user(uid, nick, cid);
	users.push_back(&user);
	return user;
}

void tlobby_channel::erase_user(int uid)
{
	int current_id;
	std::vector<tlobby_user*>::iterator it = users.begin();
	for (; it != users.end(); ++ it) {
		const tlobby_user& u = **it;
		if (uid == npos || u.uid == uid) {
			current_id = u.uid;
			lobby->chat.erase_user(current_id, cid); // after it, u.uid became invalid!
			if (current_id == uid) {
				users.erase(it);
				return;
			}
		}
	}
	VALIDATE(uid == npos, "uid must be npos!");
	users.clear();
}

namespace chat_logs {

std::map<int, treceiver> receivers;
treceiver null_receiver;

treceiver& find_receiver(int id, bool channel, bool allow_create)
{
	for (std::map<int, treceiver>::iterator it = receivers.begin(); it != receivers.end(); ++ it) {
		treceiver& receiver = it->second;
		if (receiver.id == id && receiver.channel == channel) {
			return receiver;
		}
	}

	VALIDATE(allow_create, "Cannot find receiver!");
	std::pair<std::map<int, treceiver>::iterator, bool> ins = receivers.insert(std::make_pair(id, treceiver(id, channel)));
	return ins.first->second;
}

void add(int id, bool channel, const tlobby_user& sender, const std::string& msg)
{
	treceiver& receiver = find_receiver(id, channel, true);

	time_t t = time(NULL);
	if (!receiver.logs.empty()) {
		tlog& log = receiver.logs.back();
		int diff = 0;
		if (log.t > t) {
			diff = log.t - t;
		} else if (log.t < t) {
			diff = t - log.t;
		}
		if (diff <= 2 && sender.nick == log.nick) {
			std::stringstream ss;
			ss << log.msg << "\n" << msg;
			log.msg = ss.str();
			return;
		}
	}
	receiver.insert_log(sender.uid, sender.nick, msg, t);
}

const std::string history_log = "history.log";
const std::string temp_log = "__temp.log";
#define LOGFILE_HEADER_SIZE		48
#define LOGFILE_INDEX_SIZE		56
#define LOGFILE_DATA_PREFIX_SIZE	16

struct tlogfile_header {
	uint32_t fourcc;
	uint32_t reserve0;
	int index_offset;
	int index_size;
	char nick[LOGFILE_HEADER_SIZE - 16];
};

struct tlogfile_data {
	uint64_t t;
	int nick_size;
	int msg_size;
};

struct tlogfile_index {
	uint64_t from;
	uint64_t to;
	int offset;
	int size;
	uint32_t flag;
	char nick[LOGFILE_INDEX_SIZE - 28];
};

std::set<thistory_log> history_logs;

bool valid_logfile(tfopen_lock& lock, int* index_offset, int* index_size)
{
	uint32_t fsizelow, fsizehigh, bytertd;
		
	if (!lock.valid()) {
		return false;
	}
	posix_fsize(lock.fp, fsizelow, fsizehigh);
	if (fsizelow <= LOGFILE_HEADER_SIZE) {
		return false;
	}
	posix_fseek(lock.fp, 0, 0);
	lock.resize_data(LOGFILE_HEADER_SIZE);
	posix_fread(lock.fp, lock.data, LOGFILE_HEADER_SIZE, bytertd);

	tlogfile_header* header = (tlogfile_header*)lock.data;
	if (header->fourcc != mmioFOURCC('L', 'O', 'G', '0')) {
		return false;
	}
	if (header->reserve0) {
		return false;
	}
	if (!header->index_size || header->index_offset + header->index_size != (int)fsizelow) {
		return false;
	}
	if (index_offset) {
		*index_offset = header->index_offset;
	}
	if (index_size) {
		*index_size = header->index_size;
	}
	return true;
}

void indexs_from_mem(const char* data, int data_size, std::set<thistory_log>& logs)
{
	logs.clear();
	if (data_size <= 0) {
		return;
	}
	tlogfile_index* index;
	int pos = 0;
	do {
		index = (tlogfile_index*)(data + pos);
		logs.insert(thistory_log(tlobby_user::npos, index->nick, index->from, index->to, index->offset, index->size));
		pos += LOGFILE_INDEX_SIZE;
		
	} while (pos < data_size);
}

void verify_logfile_data(const std::string& ansifile)
{
	uint32_t bytertd;
	int index_offset, index_size;

	tfopen_lock lock(ansifile, true);
	bool ret = valid_logfile(lock, &index_offset, &index_size);
	if (!ret) {
		return;
	}

	lock.resize_data(index_size);
	posix_fseek(lock.fp, index_offset, 0);
	posix_fread(lock.fp, lock.data, index_size, bytertd);

	std::set<thistory_log> history2_logs;
	indexs_from_mem(lock.data, index_size, history2_logs);

	std::string nick, msg;
	tlogfile_data data;
	for (std::set<thistory_log>::const_iterator it = history2_logs.begin(); it != history2_logs.end(); ++ it) {
		const thistory_log& user = *it;

		lock.resize_data(user.size);
		posix_fseek(lock.fp, user.offset, 0);
		posix_fread(lock.fp, lock.data, user.size, bytertd);

		int pos = 0;
		do {
			memcpy(&data, lock.data + pos, sizeof(data));
			VALIDATE(data.nick_size > 0 && data.nick_size < user.size, "nick size!");
			VALIDATE(data.msg_size > 0 && data.msg_size < user.size, "msg size!");

			nick.assign(lock.data + pos + LOGFILE_DATA_PREFIX_SIZE, data.nick_size);
			msg.assign(lock.data + pos + LOGFILE_DATA_PREFIX_SIZE + data.nick_size, data.msg_size);
			VALIDATE(!msg.empty() && utils::is_utf8str(msg), "msg string!");
			pos += LOGFILE_DATA_PREFIX_SIZE + data.nick_size + data.msg_size;
			
		} while (pos < user.size);
	}
}

void restore_from_logfile()
{
	uint32_t bytertd;
	int index_offset, index_size;

	std::string file = get_user_data_dir_utf8() + "/data/" + history_log;
	conv_ansi_utf8(file, false);

	tfopen_lock lock(file, true);
	if (!valid_logfile(lock, &index_offset, &index_size)) {
		return;
	}

	lock.resize_data(index_size);
	posix_fseek(lock.fp, index_offset, 0);
	posix_fread(lock.fp, lock.data, index_size, bytertd);

	indexs_from_mem(lock.data, index_size, history_logs);
}

void user_from_logfile(const thistory_log& user, std::vector<tlog>& logs)
{
	uint32_t bytertd;

	std::string file = get_user_data_dir_utf8() + "/data/" + history_log;
	conv_ansi_utf8(file, false);

	tfopen_lock lock(file, true);
	if (!valid_logfile(lock, NULL, NULL)) {
		return;
	}

	lock.resize_data(user.size);
	posix_fseek(lock.fp, user.offset, 0);
	posix_fread(lock.fp, lock.data, user.size, bytertd);

	std::string nick, msg;
	tlogfile_data data;
	int pos = 0;
	do {
		memcpy(&data, lock.data + pos, sizeof(data));
		if (data.nick_size < 0 || data.nick_size >= user.size) {
			return;
		}
		if (data.msg_size < 0 || data.msg_size >= user.size) {
			return;
		}
		nick.assign(lock.data + pos + LOGFILE_DATA_PREFIX_SIZE, data.nick_size);
		msg.assign(lock.data + pos + LOGFILE_DATA_PREFIX_SIZE + data.nick_size, data.msg_size);
		logs.push_back(tlog(tlobby_user::npos, nick, msg, data.t));
		pos += LOGFILE_DATA_PREFIX_SIZE + data.nick_size + data.msg_size;
		
	} while (pos < user.size);
}

void save_temp_logfile()
{
	uint32_t bytertd;

	// if no chat, do nothing;
	int has_log_receivers = 0;
	for (std::map<int, treceiver>::const_iterator it = receivers.begin(); it != receivers.end(); ++ it) {
		const treceiver& receiver = it->second;
		if (!receiver.logs.empty()) {
			has_log_receivers ++;
		}
	}
	if (!has_log_receivers) {
		return;
	}

	std::string file = get_user_data_dir_utf8() + "/data/" + temp_log;
	conv_ansi_utf8(file, false);

	tfopen_lock lock(file, false);
	if (!lock.valid()) {
		return;
	}

	tlogfile_header header;
	tlogfile_data data;

	memset(&header, 0, sizeof(header));
	header.fourcc = mmioFOURCC('L', 'O', 'G', '0');
	
	int index_size = has_log_receivers * LOGFILE_INDEX_SIZE;
	header.index_size = index_size;
	// last will update it. here is move file pointer.
	posix_fwrite(lock.fp, &header, sizeof(header), bytertd);
	
	lock.resize_data(128 * 1024);
	VALIDATE(lock.data_size > index_size, "more users!");
	memset(lock.data, 0, index_size);

	int data_size = 0;
	char* ptr;
	int n = 0;
	int fpos = LOGFILE_HEADER_SIZE;


	for (std::map<int, treceiver>::const_iterator it = receivers.begin(); it != receivers.end(); ++ it) {
		const treceiver& receiver = it->second;
		if (receiver.logs.empty()) {
			continue;
		}

		tlogfile_index* index = (tlogfile_index*)(lock.data + n * LOGFILE_INDEX_SIZE);
		index->from = receiver.logs.front().t;
		index->to = receiver.logs.back().t;
		index->offset = fpos;
		memcpy(index->nick, receiver.nick.c_str(), receiver.nick.size());
		data_size = 0;
		for (std::vector<tlog>::const_iterator it2 = receiver.logs.begin(); it2 != receiver.logs.end(); ++ it2) {
			const tlog& log = *it2;
			data_size += 8 + 4 + 4 + log.nick.size() + log.msg.size();
		}
		index->size = data_size;
		index->flag = 0;

		lock.resize_data(index_size + data_size);
		ptr = lock.data + index_size;
		for (std::vector<tlog>::const_iterator it2 = receiver.logs.begin(); it2 != receiver.logs.end(); ++ it2) {
			const tlog& log = *it2;
			data.t = log.t;
			data.nick_size = log.nick.size();
			data.msg_size = log.msg.size();
			
			memcpy(ptr, &data, sizeof(data));
			ptr += sizeof(data);
			memcpy(ptr, log.nick.c_str(), data.nick_size);
			ptr += data.nick_size;
			memcpy(ptr, log.msg.c_str(), data.msg_size);
			ptr += data.msg_size;
		}
		posix_fwrite(lock.fp, lock.data + index_size, data_size, bytertd);
		fpos += data_size;
		n ++;
	}

	// align 4
	int fpos2 = (fpos + 3) & ~3;
	if (fpos2 != fpos) {
		posix_fwrite(lock.fp, lock.data + index_size, fpos2 - fpos, bytertd);
	}

	// sequence wirte.
	posix_fwrite(lock.fp, lock.data, index_size, bytertd);

	header.index_offset = fpos2;
	posix_fseek(lock.fp, 0, 0);
	posix_fwrite(lock.fp, &header, sizeof(header), bytertd);
}

bool can_combine_logfile(const std::set<thistory_log>& temps, const std::set<thistory_log>& historys)
{
	std::set<thistory_log>::const_iterator temp_it;
	for (std::set<thistory_log>::const_iterator it = historys.begin(); it != historys.end(); ++ it) {
		const thistory_log& history = *it;
		temp_it = temps.find(history);
		if (temp_it != temps.end()) {
			return history.to < temp_it->from;
		}
	}
	return true;
}

int skip_log(uint64_t min_log_time, const char* data, int size)
{
	int pos = 0;
	tlogfile_data log;

	do {
		memcpy(&log, data + pos, sizeof(log));
		if (log.t >= min_log_time) {
			break;
		}
		pos += LOGFILE_DATA_PREFIX_SIZE + log.nick_size + log.msg_size;
		
	} while (pos < size);
	return pos;
}

// combine temp_log and history_log to history_log.
void combine_logfile()
{
	const size_t log_days = 30;
	std::string temp_file = get_user_data_dir_utf8() + "/data/" + temp_log;
	std::string history_file = get_user_data_dir_utf8() + "/data/" + history_log;
	std::string result_file = get_user_data_dir_utf8() + "/data/" + "result.log";

	uint32_t bytertd;
	int temp_index_offset, temp_index_size, history_index_offset, history_index_size;

	conv_ansi_utf8(temp_file, false);
	tfopen_lock temp(temp_file, true);
	if (!valid_logfile(temp, &temp_index_offset, &temp_index_size)) {
		return;
	}

	std::set<thistory_log> temp_logs;
	temp.resize_data(temp_index_size);
	posix_fseek(temp.fp, temp_index_offset, 0);
	posix_fread(temp.fp, temp.data, temp_index_size, bytertd);
	indexs_from_mem(temp.data, temp_index_size, temp_logs);
	
	// open previous history.log for read
	std::set<thistory_log> history_logs2;
	conv_ansi_utf8(history_file, false);
	tfopen_lock history(history_file, true);
	if (valid_logfile(history, &history_index_offset, &history_index_size)) {
		history.resize_data(history_index_size);
		posix_fseek(history.fp, history_index_offset, 0);
		posix_fread(history.fp, history.data, history_index_size, bytertd);

		indexs_from_mem(history.data, history_index_size, history_logs2);

		if (!can_combine_logfile(temp_logs, history_logs2)) {
			temp.close();
			remove(temp_file.c_str());
			return;
		}
		// re-position history to data area.
		posix_fseek(history.fp, LOGFILE_HEADER_SIZE, 0);
	} else {
		// history_log is invalid. simple comform delete it and rename temp_log to it.
		temp.close();
		history.close();

		remove(history_file.c_str());
		rename(temp_file.c_str(), history_file.c_str());
		return;
	}

	// create result.log for write
	conv_ansi_utf8(result_file, false);
	tfopen_lock result(result_file, false);
	if (!result.valid()) {
		return;
	}
	// last will update it. here is move file pointer.
	posix_fwrite(result.fp, temp.data, LOGFILE_HEADER_SIZE, bytertd);
	int fpos = LOGFILE_HEADER_SIZE;

	// of course, history_logs2 maybe overlap temp_logs, must a think memory is little, needn't allocat more time.
	result.resize_data((history_logs2.size() + temp_logs.size()) * LOGFILE_INDEX_SIZE);
	memset(result.data, 0, (history_logs2.size() + temp_logs.size()) * LOGFILE_INDEX_SIZE);

	int skip_size;
	time_t min_log_time = time(NULL) - log_days * 24 * 3600;
	min_log_time -= min_log_time % (24 * 3600);
	std::set<thistory_log>::iterator temp_it;
	int n = 0;
	// users order in history_logs2 is different from history.log's order!
	// history_logs2 order is order by nick.
	// history.log's order is no rule. In general, it is decided by receivers(std::map<int, treceiver>).
	for (std::set<thistory_log>::const_iterator it = history_logs2.begin(); it != history_logs2.end(); ++ it) {
		const thistory_log& history_log = *it;

		tlogfile_index* index = (tlogfile_index*)(result.data + n * LOGFILE_INDEX_SIZE);
		index->from = history_log.from;
		index->to = history_log.to;
		index->offset = fpos;
		index->size = history_log.size;
		index->flag = 0;
		memcpy(index->nick, history_log.nick.c_str(), history_log.nick.size());

		// users order in history_logs2 is different from history.log's order. re-seek!
		posix_fseek(history.fp, history_log.offset, 0);
		history.resize_data(history_log.size);
		posix_fread(history.fp, history.data, history_log.size, bytertd);
		skip_size = skip_log(min_log_time, history.data, history_log.size);
		if (skip_size < history_log.size) {
			index->size = history_log.size - skip_size;
			posix_fwrite(result.fp, history.data + skip_size, index->size, bytertd);
			fpos += index->size;
		} else {
			index->size = 0;
		}

		temp_it = temp_logs.find(history_log);
		if (temp_it != temp_logs.end()) {
			const thistory_log& temp_log = *temp_it;
			posix_fseek(temp.fp, temp_log.offset, 0);
			temp.resize_data(temp_log.size);
			posix_fread(temp.fp, temp.data, temp_log.size, bytertd);
			posix_fwrite(result.fp, temp.data, temp_log.size, bytertd);
			fpos += temp_log.size;

			// update to time.
			index->to = temp_log.to;
			index->size += temp_log.size;

			temp_logs.erase(temp_it);
		}
		if (index->size) {
			n ++;
		}
	}
	for (std::set<thistory_log>::const_iterator it = temp_logs.begin(); it != temp_logs.end(); ++ it, n ++) {
		const thistory_log& temp_log = *it;

		tlogfile_index* index = (tlogfile_index*)(result.data + n * LOGFILE_INDEX_SIZE);
		index->from = temp_log.from;
		index->to = temp_log.to;
		index->offset = fpos;
		index->size = temp_log.size;
		index->flag = 0;
		memcpy(index->nick, temp_log.nick.c_str(), temp_log.nick.size());

		posix_fseek(temp.fp, temp_log.offset, 0);
		temp.resize_data(temp_log.size);
		posix_fread(temp.fp, temp.data, temp_log.size, bytertd);
		posix_fwrite(result.fp, temp.data, temp_log.size, bytertd);

		fpos += temp_log.size;
	}

	// align 4
	int fpos2 = (fpos + 3) & ~3;
	if (fpos2 != fpos) {
		// temp.data must allocatable, use it as memory buffer.
		posix_fwrite(result.fp, temp.data, fpos2 - fpos, bytertd);
	}

	// sequence wirte.
	posix_fwrite(result.fp, result.data, n * LOGFILE_INDEX_SIZE, bytertd);

	tlogfile_header* header = (tlogfile_header*)result.data;
	memset(header, 0, sizeof(tlogfile_header));
	header->fourcc = mmioFOURCC('L', 'O', 'G', '0');
	header->index_offset = fpos2;
	header->index_size = n * LOGFILE_INDEX_SIZE;
	posix_fseek(result.fp, 0, 0);
	posix_fwrite(result.fp, header, sizeof(tlogfile_header), bytertd);

	temp.close();
	history.close();
	result.close();

	remove(temp_file.c_str());
	remove(history_file.c_str());
	rename(result_file.c_str(), history_file.c_str());
}

}

tlobby::tchat_sock::tchat_sock()
	: tsock(tag_chat)
	, line_(NULL)
	, line_size_(0)
	, serv_(NULL)
	, pong_receiving_(false)
	, last_task_time_(0)
	, online_offline_received_(false)
{
	task_threshold_ = noresponse_threshold_;
	msg_send_gap = 200;

	// once use chat, chat alway in use.
	// shinken reconnect delay to 5 sec.
	reconnect_prohabit_ = 5000;

	chat_logs::restore_from_logfile();
}

tlobby::tchat_sock::tchat_sock(const tlobby::tchat_sock& that)
	: tsock(that)
{
	// because irc::server variable, don't support copy-evalue.
	VALIDATE(false, "don't support!");
}

tlobby::tchat_sock::~tchat_sock()
{
	if (serv_) {
		delete serv_;
	}
	if (line_) {
		free(line_);
	}

	chat_logs::save_temp_logfile();
	chat_logs::combine_logfile();
	save_preferences();
}

void tlobby::tchat_sock::save_preferences()
{
	std::stringstream person_ss, channel_ss;
	for (std::map<int, tlobby_channel>::iterator it = lobby->chat.channels.begin(); it != lobby->chat.channels.end(); ++ it) {
		bool person = !tlobby_channel::is_allocatable(it->first);
		tlobby_channel& channel = it->second;
		if (person) {
			for (std::vector<tlobby_user*>::iterator it2 = channel.users.begin(); it2 != channel.users.end(); ++ it2) {
				tlobby_user& user = **it2;
				if (user.me) {
					continue;
				}
				if (!person_ss.str().empty()) {
					person_ss << ",";
				}
				person_ss << user.nick;
			}
		} else {
			if (!channel_ss.str().empty()) {
				channel_ss << ",";
			}
			channel_ss << channel.nick;
		}
	}
	preferences::set_chat_person(person_ss.str());
	preferences::set_chat_channel(channel_ss.str());
}

void tlobby::tchat_sock::set_host(const std::string& host, int port)
{
	tsock::set_host(host, port);
}

void tlobby::tchat_sock::reset_connect()
{
	for (std::map<int, tlobby_channel>::iterator it = lobby->chat.channels.begin(); it != lobby->chat.channels.end(); ++ it) {
		bool person = !tlobby_channel::is_allocatable(it->first);
		tlobby_channel& channel = it->second;
		if (person) {
			for (std::vector<tlobby_user*>::iterator it2 = channel.users.begin(); it2 != channel.users.end(); ++ it2) {
				tlobby_user& user = **it2;
				user.online = false;
			}
		} else {
			channel.erase_user(tlobby_user::npos);
		}
	}
	tsock::reset_connect();
}

irc::ircnet tlobby::tchat_sock::generate_ircnet() const
{
	irc::ircnet net;
	net.nick = nick_;
	net.real = "realname";
	net.logintype = LOGIN_DEFAULT_REAL;

	return net;
}

void tlobby::tchat_sock::process()
{
	const Uint32 now = SDL_GetTicks();

	if (state_ == s_none) {
		if (now <= next_create_time_) {
			return;
		}
		if (!utils::isvalid_nick(lobby->nick_)) {
			return;
		}

		nick_ = lobby->nick_;
		if (!serv_) {
			tag_ = _("Chat");

			serv_ = irc::server_new(generate_ircnet());
			serv_->sock = this;
			strcpy(serv_->servername, host_.c_str());
			if (strcmp(host_.c_str(), serv_->hostname)) {
				strcpy(serv_->hostname, host_.c_str());
			}

			VALIDATE(irc::is_channel(serv_, game_config::app_channel.c_str()), "Must define app channel!");
			if (channels.empty()) {
				std::vector<std::string> vstr = utils::split(preferences::chat_channel());
				vstr.insert(vstr.begin(), game_config::app_channel);

				for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
					const std::string& chan = *it;
					int cid = tlobby_channel::get_cid(chan, false);
					const tlobby_channel& channel = get_channel(cid);
					if (channel.valid()) {
						continue;
					}
					channels.insert(std::make_pair(cid, tlobby_channel(cid, chan)));
				}

				vstr = utils::split(preferences::chat_person());

				int type = tlobby_channel::t_friend;
				const std::string& chan = tlobby_channel::get_nick(type);
				channels.insert(std::make_pair(type, tlobby_channel(type, chan)));

				for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
					const std::string& nick = *it;

					int uid = tlobby_user::get_uid(nick, false);
					const tlobby_user& user = get_user(uid);
					if (user.valid()) {
						continue;
					}

					std::map<int, tlobby_channel>::iterator it2 = channels.find(type);
					tlobby_channel& channel = it2->second;
					channel.insert_user(uid, nick);	
				}
			}
		}

		me = NULL;
		connected_at_ = 0;
		conn_ = network::null_connection;
		last_active_time_ = now;
		last_task_time_ = 0; // make sure first run one.
		msg_send_time = 0;
		pong_receiving_ = false;
		online_offline_received_ = false;
		noresponse_msgs_.clear();


		VALIDATE(conn_ == network::null_connection, "s_none, must be null_connection");
		threading::waiter waiter;
		network::connect(*this, host_, port_, waiter);
		
		std::stringstream ss;
		ss << "Connecting to " << tintegrate::generate_format(host_, "green") << ":" << tintegrate::generate_format(port_, "green");
		ss << " Nick: " << tintegrate::generate_format(nick_, "green");
		lobby->add_log(*this, ss.str());
		state_ = s_create;

	} else if (state_ == s_create) {
		// create sock thread must be successfully. it is different from create sock successfully.
		// connected_at_ != 0 mean thread successfully.
		// conn_ != network::null_connection mean sock successfully.
		if (connected_at_) {
			if (conn_ != network::null_connection) {
				lobby->add_log(*this, "Create success! Enter consult.");
				state_ = s_consult;

				serv_->p_login(serv_, serv_->network.nick.c_str(), serv_->network.real.c_str());

			} else {
				// it is not connect to network or to internet. prohabit to 40 sec.
				const int network_error_prohabit = 40000;

				std::stringstream err;
				next_create_time_ = SDL_GetTicks() + network_error_prohabit;
				err << tintegrate::generate_format(error_, "red");
				err << " will reconnect after " << network_error_prohabit / 1000 << " s.";
				lobby->add_log(*this, err.str());
				state_ = s_none;
			}
		}

	} else if (state_ == s_consult) {
		std::vector<char> buf;
		const network::connection ret = network::receive_data(buf, conn_);
		if (ret != conn_) {
			// first response maybe cost more time.
			check_time_overflow(120000);
			return;
		}
		last_active_time_ = now;
		process_data(&buf[0], buf.size());
		
	} else if (state_ == s_ready) {
		std::vector<char> buf;
		network::connection ret = network::receive_data(buf, conn_);
		if (ret == network::null_connection) {
			if (pong_receiving_) {
				check_time_overflow(ping_interval * 2);
				return;
			}

			if (now - last_active_time_ >= ping_interval) {
				send_ping();

			} else if (!last_task_time_ || now - last_task_time_ >= task_threshold_) {
				do_task();
			}

		} else {
			last_active_time_ = now;
			// since has receive data, i think this conenction is active.
			pong_receiving_ = false;

			process_data(&buf[0], buf.size());

		}
	}
}

void tlobby::tchat_sock::send_ping()
{
	const Uint32 now = SDL_GetTicks();
	std::stringstream ss;

	unsigned long tim = irc::make_ping_time();

	ss << "LAG" << tim;
	serv_->p_ping(serv_, "", ss.str().c_str());

	last_active_time_ = now;
	pong_receiving_ = true;
}

tlobby_channel& tlobby::tchat_sock::get_channel(int cid)
{
	std::map<int, tlobby_channel>::iterator it = channels.find(cid);
	if (it != channels.end()) {
		return it->second;
	}
	return null_channel;
}

const tlobby_channel& tlobby::tchat_sock::get_channel(int cid) const
{
	std::map<int, tlobby_channel>::const_iterator it = channels.find(cid);
	if (it != channels.end()) {
		return it->second;
	}
	return null_channel;
}

tlobby_channel& tlobby::tchat_sock::insert_channel(int cid, const std::string& nick)
{
	VALIDATE(channels.find(cid) == channels.end(), "channel must not exist!");

	std::pair<std::map<int, tlobby_channel>::iterator, bool> ins = channels.insert(std::make_pair(cid, tlobby_channel(cid, nick)));
	return ins.first->second;
}

void tlobby::tchat_sock::erase_channel(int cid)
{
	std::map<int, tlobby_channel>::iterator it = channels.find(cid);
	VALIDATE(it != channels.end(), "channel must be valid!");

	tlobby_channel& channel = it->second;

	irc::part_channel(serv_, channel.nick.c_str());
	channel.erase_user(tlobby_user::npos);
	channels.erase(it);
}

bool tlobby::tchat_sock::is_favor_user(int uid) const
{
	for (std::map<int, tlobby_channel>::const_iterator it = channels.begin(); it != channels.end(); ++ it) {
		if (tlobby_channel::is_allocatable(it->first)) {
			continue;
		}
		const tlobby_channel& channel = it->second;
		const tlobby_user& user = channel.get_user(uid);
		if (user.valid()) {
			return true;
		}
	}
	return false;
}

tlobby_user& tlobby::tchat_sock::favor_user(int uid) const
{
	for (std::map<int, tlobby_channel>::const_iterator it = channels.begin(); it != channels.end(); ++ it) {
		if (tlobby_channel::is_allocatable(it->first)) {
			continue;
		}
		const tlobby_channel& channel = it->second;
		tlobby_user& user = channel.get_user(uid);
		if (user.valid()) {
			return user;
		}
	}
	return null_user;
}

tlobby_user& tlobby::tchat_sock::get_user(int uid)
{
	std::map<int, tlobby_user>::iterator it = users_.find(uid);
	if (it == users_.end()) {
		return null_user;
	}

	return it->second;
}

tlobby_user& tlobby::tchat_sock::insert_user(int uid, const std::string& nick, int cid)
{
	std::map<int, tlobby_user>::iterator it = users_.find(uid);
	if (it == users_.end()) {
		it = users_.insert(std::make_pair(uid, tlobby_user(uid, nick))).first;
	}

	tlobby_user& user = it->second;
	VALIDATE(!user.cids.count(cid), "must not exist cid!");
	user.cids.insert(cid);

	return user;
}

void tlobby::tchat_sock::erase_user(int uid, int cid)
{
	std::map<int, tlobby_user>::iterator it = users_.find(uid);
	VALIDATE(it != users_.end(), "lobby no this uid!");

	tlobby_user& user = it->second;
	user.cids.erase(cid);
	if (user.cids.empty()) {
		users_.erase(it);
	}
}

bool tlobby::tchat_sock::join_channel(tlobby_channel& channel, bool force)
{
	if (!force) {
		if (!insert_noresponse_msg(msg_join, channel.nick)) {
			return false;
		}
	}
	std::stringstream ss;
	ss << "Want join channel: " << channel.nick;
	lobby->add_log(lobby->chat, ss.str());

	serv_->p_join(serv_, channel.nick.c_str(), channel.key.c_str());
	if (force) {
		insert_noresponse_msg(msg_join, channel.nick);
	}
	return true;
}

void tlobby::tchat_sock::find_channel(const std::string& chan, int min_users)
{
	serv_->p_list_channels(serv_, chan.c_str(), min_users);
	insert_noresponse_msg(msg_chanlist, "*");
}

bool tlobby::tchat_sock::erase_noresponse_msg(int major, const std::string& minor)
{
	if (!tsock::erase_noresponse_msg(major, minor)) {
		return false;
	}

	// shedule do task next pool.
	last_task_time_ = 0;
	return true;
}

void tlobby::tchat_sock::do_task()
{
	Uint32 now = SDL_GetTicks();
	std::stringstream ss;
	
	bool buffed_valid = true;
	const int who_reqeusting_threshold = online_offline_received_? task_threshold_ * 4 : task_threshold_ / 2;
	for (std::map<int, tlobby_channel>::iterator it = channels.begin(); it != channels.end(); ++ it) {
		tlobby_channel& channel = it->second;
		if (tlobby_channel::is_allocatable(it->first)) {
			if (buffed_valid) {
				if (!channel.joined() && !channel.err && !channel.users_receiving) {
					buffed_valid = join_channel(channel, false);
					
				}
			}
		} else if (buffed_valid && (!channel.who_reqeusting || now >= channel.who_reqeusting + who_reqeusting_threshold)) {
			// MONITOR + ,ancientcc,shuouw,rose
			ss.str("");
			for (std::vector<tlobby_user*>::const_iterator it2 = channel.users.begin(); it2 != channel.users.end(); ++ it2) {
				tlobby_user& user = **it2;
				ss << ",";
				ss << user.nick;
			}
			if (!ss.str().empty()) {
				buffed_valid = insert_noresponse_msg(msg_monitor, "*");
				if (buffed_valid) {
					serv_->p_monitor(serv_, ss.str().c_str(), true);
					// p_monitor maybe no response.
					channel.who_reqeusting = now;
				}
			}
		}
	}

	last_task_time_ = now;
}

// 1.mask must be integer of like xxx1000
// 2.if val is 0, align_ceil will return 0. even if mask is any value.
#ifndef align_ceil
#define align_ceil(val, mask)     (((val) + (mask) - 1) & ~((mask) - 1)) 
#endif

void tlobby::tchat_sock::process_data(const char* data, int len)
{
	if (line_size_ < len) {
		resize_line_buf(align_ceil(len, 0x400), 0);
	}

	int i = 0, pos = 0;
	while (i < len) {
		switch (data[i]) {
		case '\r':
			break;

		case '\n':
			line_[pos] = 0;
			serv_->p_inline(serv_, line_, pos);
			pos = 0;
			break;

		default:
			line_[pos] = data[i];
			pos ++;
		}
		i++;
	}
}

void tlobby::tchat_sock::resize_line_buf(int size, int vsize)
{
	if (size > line_size_) {
		char* tmp = (char*)malloc(size);
		if (line_) {
			if (vsize) {
				memcpy(tmp, line_, vsize);
			}
			free(line_);
		}
		line_ = tmp;
		line_size_ = size;
	}
}

SOCKET_STATE tlobby::tchat_sock::receive_buf(std::vector<char>& buf)
{
	const int min_raw_data_size = 1024;
	const int chunk_size = 1024;
	int ret_size = 0;

	{
		const threading::lock lock(*network::stats_mutex);
		network::transfer_stats[sock2_].second.fresh_current(chunk_size * 100);
	}

	do {
		if (raw_data_size_ < raw_data_vsize_ + chunk_size) {
			resize_raw_data(raw_data_size_ + chunk_size);
		}
		bool res = network::receive_with_timeout(*this, raw_data_ + raw_data_vsize_, chunk_size, true, 30000, 300000, &ret_size);
		if (!res) {
			return SOCKET_ERRORED;
		}
		raw_data_vsize_ += ret_size;
	} while (ret_size == chunk_size);

	if (!raw_data_vsize_) {
		return SOCKET_ERRORED;
	}

	int i = 0, last_lf = 0;
	while (i < raw_data_vsize_) {
		if (raw_data_[i] == '\n') {
			last_lf = i;
		}
		i ++;
	}

	last_lf ++;
	buf.resize(last_lf);
	memcpy(&buf[0], raw_data_, last_lf);

	if (last_lf != raw_data_vsize_) {
		memmove(raw_data_, raw_data_ + last_lf, raw_data_vsize_ - last_lf);
	}
	raw_data_vsize_ -= last_lf;

	return SOCKET_READY;
}

bool tlobby::tchat_sock::connect(TCPsocket sock, const std::string& host, int port)
{
	tsock::connect(sock, host, port);
	return true;
}

void tlobby::tchat_sock::pre_disconnect()
{
	if (state_ == s_ready) {
		// immdate call close sock, this command cann't reach to server!
		serv_->p_quit(serv_, "Leaving");
	}
}

void tlobby::tchat_sock::tcp_send_len(char* buf, int len)
{
	network::send_raw_data(buf, len, conn_);
}

void tlobby::tchat_sock::handle_command(const char* param[])
{
	std::stringstream ss;

	if (state_ == s_consult) {
		if (!strcasecmp(param[0], "NICKERR")) {
			ss.str("");

			VALIDATE(utils::to_int(param[2]) != 1, "Invalid nick!");

			nick_ = nick_ + "_";
			ss << tintegrate::generate_format(param[1], "red") << " is already in use. Retrying with ";
			ss << tintegrate::generate_format(nick_, "yellow");
			lobby->add_log(*this, ss.str());

			serv_->p_change_nick(serv_, nick_.c_str());

		} else if (!strcasecmp(param[0], "LOGIN")) {
			ss.str("");
			ss << "Consult success. Enter ready!";
			lobby->add_log(*this, ss.str());

			tlobby_channel& channel = get_channel(tlobby_channel::t_friend);
			int uid = tlobby_user::get_uid(nick_, false);
			me = &channel.get_user(uid);
			if (!me->valid()) {
				me = &channel.insert_user(uid, nick_);
			}
			me->me = true;

			state_ = s_ready;
			lobby->broadcast_handle_raw(at_, t_connected, param);
			return;
		}

	}

	if (state_ != s_ready) {
		return;
	}
	
	if (!strcasecmp(param[0], "PONG")) {
		pong_receiving_ = false;
		lobby->add_log(*this, "Receive pong from server!");
		return;

	} else if (!strcasecmp(param[0], "NAMREPLAY")) {
		if (!process_userlist_th(param[1], param[2])) {
			return;
		}

	} else if (!strcasecmp(param[0], "PART")) {
		if (!process_part_th(param[1], param[2], param[3])) {
			return;
		}

	} else if (!strcasecmp(param[0], "ENDOFNAMES")) {
		if (!process_userlist_end(param[1])) {
			return;
		}

	} else if (!strcasecmp(param[0], "UJOINED")) {
		process_ujoined(param[1]);

	} else if (!strcasecmp(param[0], "UPARTED")) {
		process_uparted(param[1]);

	} else if (!strcasecmp(param[0], "JOIN")) {
		if (!process_join(param[1], param[2])) {
			return;
		}

	} else if (!strcasecmp(param[0], "WHOREPLAY")) {
		if (!process_whois(param[1], param[2], utils::to_bool(param[3]), utils::to_bool(param[4]))) {
			return;
		}

	} else if (!strcasecmp(param[0], "QUIT")) {
		if (!process_quit_th(param[1])) {
			return;
		}
	} else if (!strcasecmp(param[0], "PRIVMSG")) {
		if (!process_message(param[1], param[2], param[3])) {
			return;
		}

	} else if (!strcasecmp(param[0], "LISTEND")) {
		process_chanlist_end();

	} else if (!strcasecmp(param[0], "OFFLINE")) {
		process_offline(param[1]);

	} else if (!strcasecmp(param[0], "ONLINE")) {
		process_online(param[1]);

	} else if (!strcasecmp(param[0], "FORBIDJOIN")) {
		process_forbid_join(param[1], param[2]);

	}

	lobby->broadcast_handle_raw(at_, t_data, param);

	if (!strcasecmp(param[0], "NAMREPLAY")) {
		process_userlist_bh(param[1]);

	} else if (!strcasecmp(param[0], "PART")) {
		process_part_bh(param[1], param[2], param[3]);

	} else if (!strcasecmp(param[0], "QUIT")) {
		process_quit_bh(param[1]);

	}
}

void tlobby::tchat_sock::process_chanlist_end()
{
	erase_noresponse_msg(msg_chanlist, "*");
}

void tlobby::tchat_sock::split_offline_online(const char* nicks, std::vector<std::string>& result)
{
	const char* chr;
	const char* p = nicks;
	const char* p1 = strchr(nicks, ',');
	size_t size;

	result.clear();
	do {
		p1 = strchr(p, ',');
		chr = strchr(p, '!');
		
		if (p1) {
			if (chr && chr < p1) {
				size = chr - p;
			} else {
				size = p1 - p;
			}
		} else {
			if (chr) {
				size = chr - p;
			} else {
				size = std::string::npos;
			}
		}
		if (size == std::string::npos) {
			result.push_back(p);
		} else {
			result.push_back(std::string(p, size));
		}
		
		if (p1) {
			p = p1 + 1;
		} else {
			p = p1;
		}

	} while (p);
}

void tlobby::tchat_sock::process_online(const char* nicks)
{
	std::vector<std::string> vstr;
	split_offline_online(nicks, vstr);

	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::string& nick = *it;
		int uid = tlobby_user::get_uid(nick);
		tlobby_user& user = get_user(uid);
		if (user.valid()) {
			user.online = true;
		}
	}

	tlobby_channel& channel = get_channel(tlobby_channel::t_friend);
	channel.who_reqeusting = SDL_GetTicks();

	erase_noresponse_msg(msg_monitor, "*");
	online_offline_received_ = true;
}

void tlobby::tchat_sock::process_offline(const char* nicks)
{
	std::vector<std::string> vstr;
	split_offline_online(nicks, vstr);

	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::string& nick = *it;
		int uid = tlobby_user::get_uid(nick);
		tlobby_user& user = get_user(uid);
		if (user.valid()) {
			user.online = false;
		}
	}

	tlobby_channel& channel = get_channel(tlobby_channel::t_friend);
	channel.who_reqeusting = SDL_GetTicks();

	erase_noresponse_msg(msg_monitor, "*");
	online_offline_received_ = true;
}

void tlobby::tchat_sock::process_forbid_join(const std::string& chan, const std::string& reason)
{
	tlobby_channel& channel = get_channel(tlobby_channel::get_cid(chan));
	channel.err = true;

	erase_noresponse_msg(msg_join, chan);

	{
		std::stringstream ss;
		ss << noresponse_msgs_.size() << "  " << chan << tintegrate::generate_format(reason, "red");
		lobby->add_log(*this, ss.str());
	}
}

bool tlobby::tchat_sock::process_message(const std::string& chan, const std::string& from, const std::string& text)
{
	tlobby_user& user = get_user(tlobby_user::get_uid(from, false));
	return user.valid();
}

bool tlobby::tchat_sock::process_userlist_th(const std::string& chan, const std::string& names)
{
	std::vector<std::string> users = utils::split(names, ' ');
	tlobby_channel& channel = get_channel(tlobby_channel::get_cid(chan, false));
	if (!channel.valid()) {
		// I has been received channel: ##fix_your_connection
		return false;
	}

	if (!channel.users_receiving) {
		// first clear all user in this channel
		channel.erase_user(tlobby_channel::npos);
		channel.users_receiving = true;
	}

	char* nick_prefixes = lobby->chat.serv()->nick_prefixes;
	std::string nopre_nick;
	for (std::vector<std::string>::const_iterator it = users.begin(); it != users.end(); ++ it) {
		nopre_nick = *it;
		// Ignore prefixes so '!' won't cause issues
		if (strchr(nick_prefixes, nopre_nick[0])) {
			nopre_nick = nopre_nick.substr(1);
		}

		channel.insert_user(tlobby_user::get_uid(nopre_nick, false), nopre_nick);
	}
	return true;
}

void tlobby::tchat_sock::process_userlist_bh(const std::string& chan)
{
	tlobby_channel& channel = get_channel(tlobby_channel::get_cid(chan));
	if (!channel.users_receiving) {
		channel.users_receiving = true;
	}
}

bool tlobby::tchat_sock::process_userlist_end(const std::string& chan)
{
	erase_noresponse_msg(msg_userlist, chan);

	tlobby_channel& channel = get_channel(tlobby_channel::get_cid(chan));
	if (!channel.valid()) {
		// see process_userlist_th, maybe unfavor channel: ##fix_your_connection etc.
		return false;
	}
	channel.users_receiving = false;

	return true;
}

void tlobby::tchat_sock::process_ujoined(const std::string& chan)
{
	std::stringstream ss;

	ss << "You have joined into " << chan;
	ss << " success!";
	lobby->add_log(*this, ss.str());

	erase_noresponse_msg(msg_join, chan);

	// sends a MODE
	serv_->p_join_info(serv_, chan.c_str());
	// serv_->p_user_list(serv_, chan.c_str());

	Uint32 now = SDL_GetTicks();
	insert_noresponse_msg(msg_userlist, chan);
}

void tlobby::tchat_sock::process_uparted(const std::string& chan)
{
	// channel has erased from chat_sock.
	// don't validate chan.
	std::stringstream ss;

	ss << "You have parted from " << chan;
	ss << " success!";
	lobby->add_log(*this, ss.str());
}

bool tlobby::tchat_sock::process_join(const std::string& chan, const std::string& nick)
{
	tlobby_channel& channel = get_channel(tlobby_channel::get_cid(chan));
	if (!channel.valid()) {
		// 1. I part this channel.
		// 2. at the same time(server hasn't receive part request), one join this channel.
		return false;
	}
	int uid = tlobby_user::get_uid(nick, false);
	tlobby_user& user = channel.get_user(uid);
	if (user.valid()) {
		// normal, part msg is lose, it re-join channel.
		user.online = true;
		return false;
	}

	tlobby_user& user2 = channel.insert_user(uid, nick);
	user2.online = true;

	return true;
}

bool tlobby::tchat_sock::process_part_th(const std::string& chan, const std::string& nick, const std::string& reason)
{
	tlobby_channel& channel = get_channel(tlobby_channel::get_cid(chan));
	if (!channel.valid()) {
		// see process_userlist_th, maybe unfavor channel: ##fix_your_connection etc.
		return false;
	}
	int uid = tlobby_user::get_uid(nick, false);
	tlobby_user& user = channel.get_user(uid);
	if (!user.valid()) {
		return false;
	}
	return true;
}

void tlobby::tchat_sock::process_part_bh(const std::string& chan, const std::string& nick, const std::string& reason)
{
	tlobby_channel& channel = get_channel(tlobby_channel::get_cid(chan));
	VALIDATE(channel.valid(), "Hasn't establish channel!");

	channel.erase_user(tlobby_user::get_uid(nick));
}

bool tlobby::tchat_sock::process_whois(const std::string& chan, const std::string& nick, bool online, bool away)
{
	// :wolfe.freenode.net 354 kingdom_ 152 ##linux bryan triton.friertech.net wilhelm.freenode.net bfrizzl:wolfe.freenode.net NOTICE * :*** Looking up your hostname...
	// when client quit, but stil in channel, will receive it. base irc, nick is *
	// of course, it should resolve and remark it.
	if (nick == "*") {
		return false;
	}

	// BUG!
	// i received non-exist nick. i guess, if one change nick running.
	// unitl support change nick, get ride of false parameter.
	tlobby_user& user = get_user(tlobby_user::get_uid(nick, false));
	if (!user.valid()) {
		return false;
	}

	user.online = online;
	user.away = away;

	return true;
}

bool tlobby::tchat_sock::process_quit_th(const std::string& nick)
{
	// I think nick cannot ecount unknown nick, but I ecouned. for example Guest87083
	tlobby_user& user = get_user(tlobby_user::get_uid(nick, false));
	if (!user.valid()) {
		// 1. I part this channel. all user in channel is erase.
		// 2. at the same time(server hasn't receive part request), one that in this channel quit.
		return false;
	}
	return true;
}

void tlobby::tchat_sock::process_quit_bh(const std::string& nick)
{
	tlobby_user& user = get_user(tlobby_user::get_uid(nick));
	VALIDATE(user.valid(), "user must be valid!");

	user.online = false;

	// channel.erase_user will modify cids.
	std::set<int> cids = user.cids;
	for (std::set<int>::const_iterator it = cids.begin(); it != cids.end(); ++ it) {
		int cid = *it;
		if (tlobby_channel::is_allocatable(cid)) {
			tlobby_channel& channel = get_channel(cid);
			channel.erase_user(user.uid);
		}
	}
}

void tlobby::thttp_sock::process()
{
}

SOCKET_STATE tlobby::thttp_sock::receive_buf(std::vector<char>& buf)
{
	const int support_max_header_size = 1024;
	int read_size, total_size, ret_size;

	size_t end_header;
	std::string header;
	header.resize(support_max_header_size);
	read_size = 0;
	{
		const threading::lock lock(*network::stats_mutex);
		network::transfer_stats[sock2_].second.fresh_current(support_max_header_size * 100);
	}

	do {
		bool res = network::receive_with_timeout(*this, &header[read_size], support_max_header_size - read_size, true, 30000, 300000, &ret_size);
		if (!res) {
			return SOCKET_ERRORED;
		}
		if (!ret_size) {
			// resumme can read header with continue.
			return SOCKET_ERRORED;
		}
		read_size += ret_size;
		end_header = header.find("\r\n\r\n");
	} while (end_header == std::string::npos);

	int content_length = 0;
	size_t start = header.find("Content-Length:", 0);
	if (start != std::string::npos && start < end_header) {
		start += 15;
		size_t end = header.find("\r\n", start);
		if (end != std::string::npos && end <= end_header) {
			std::string str = header.substr(start, end - start);
			content_length = lexical_cast<int>(str);
		}
	}
	
	total_size = end_header + 4 + content_length;
	if (total_size < read_size) {
		// invalid http response
		// return SOCKET_ERRORED;

		buf.resize(read_size);
		memcpy(&buf[0], &header[0], read_size);
		return SOCKET_READY;
	}
	buf.resize(total_size);
	memcpy(&buf[0], &header[0], read_size);
	if  (total_size - read_size > 0) {
		{
			const threading::lock lock(*network::stats_mutex);
			network::transfer_stats[sock2_].second.fresh_current(total_size);
			network::transfer_stats[sock2_].second.transfer(read_size);
		}

		bool res = network::receive_with_timeout(*this, &buf[read_size], total_size - read_size, true);
		if (!res) {
			return SOCKET_ERRORED;
		}
	}

	return SOCKET_READY;
}

void tlobby::ttransit_sock::process()
{
	if (state_ == s_none) {
		if (SDL_GetTicks() <= next_create_time_) {
			return;
		}
		if (host_.empty() || port_ == INVALID_PORT) {
			return;
		}
		if (tag_.empty()) {
			tag_ = _("Transit");
		}

		connected_at_ = 0;
		conn_ = network::null_connection;
		last_active_time_ = SDL_GetTicks();

		VALIDATE(conn_ == network::null_connection, "s_none, must be null_connection");
		threading::waiter waiter;
		network::connect(*this, host_, port_, waiter);
		
		std::stringstream strstr;
		strstr << "Start connecting to " << tintegrate::generate_format(host_, "green") << ":" << tintegrate::generate_format(port_, "green");
		lobby->add_log(*this, strstr.str());
		state_ = s_create;

	} else if (state_ == s_create) {
		if (connected_at_) {
			if (conn_ != network::null_connection) {
				lobby->add_log(*this, "Create success! Enter consult.");
				state_ = s_consult;
			} else {
				std::stringstream err;
				next_create_time_ = SDL_GetTicks() + reconnect_prohabit_;
				err << tintegrate::generate_format(error_, "red");
				err << " will reconnect after " << reconnect_prohabit_ / 1000 << " s.";
				lobby->add_log(*this, err.str());
				state_ = s_none;
			}
		} else {
			check_time_overflow(reconnect_prohabit_);
		}
	} else if (state_ == s_consult) {
		const network::connection ret = network::receive_data(data_, conn_);
		if (ret != conn_) {
			check_time_overflow(reconnect_prohabit_);
			return;
		}

		// step1: server--->client, version block, no data
		//       client--->server, version block, has data
		if (data_.child("version")) {
			lobby->add_log(*this, "Receive [version], response [version].");

			config cfg;
			config res;
			// cfg["version"] = game_config::version;
			// fake version, in order to login in wesnoth server
			// cfg["version"] = "1.9.10";
			cfg["version"] = "test";
			res.add_child("version", cfg);
			network::send_data(*this, res);

		} else if (data_.child("mustlogin")) {
			std::stringstream ss;
			ss << "Receive [mustlogin], response [login]: " << tintegrate::generate_format(preferences::login(), "green");
			lobby->add_log(*this, ss.str());

			config response ;
			config& sp = response.add_child("login") ;
			sp["username"] = preferences::login();

			// Login and enable selective pings -- saves server bandwidth
			// If ping_timeout has a non-zero value, do not enable
			// selective pings as this will cause clients to falsely
			// believe the server has died and disconnect.
			// if (preferences::get_ping_timeout()) {
			if (false) {
				// Pings required so disable selective pings
				sp["selective_ping"] = false;
			} else {
				// Client is bandwidth friendly so allow
				// server to optimize ping frequency as needed.
				sp["selective_ping"] = true;
			}
			network::send_data(*this, response);

		} else if (data_.child("join_lobby")) {
			lobby->add_log(*this, "Receive [join_lobby], Consult success. Enter ready.!");

			state_ = s_ready;
			for (std::vector<tlobby::thandler*>::const_reverse_iterator rit = lobby->handlers_.rbegin(); rit != lobby->handlers_.rend(); ++ rit) {
				tlobby::thandler& h = **rit;
				h.handle(at_, t_connected, null_cfg);
			}

		} else if (const config& cfg = data_.child("error")) {
			process_error(cfg["message"].str());
			return;
		}

	} else if (state_ == s_ready) {
		ttype type = t_data;
		network::connection ret = network::receive_data(data_, conn_);
		if (ret == network::null_connection) {
			check_time_overflow(heartbeat_threshold_);

		} else {
			last_active_time_ = SDL_GetTicks();
		
			bool halt = false;
			for (std::vector<tlobby::thandler*>::const_reverse_iterator rit = lobby->handlers_.rbegin(); rit != lobby->handlers_.rend(); ++ rit) {
				tlobby::thandler& h = **rit;
				halt = h.handle(at_, type, data_);
				if (halt && type == t_data) {
					break;
				}
			}
			if (!halt) {
				default_handle(data_);
			}
		}
	}
}

void tlobby::ttransit_sock::default_handle(const config& data)
{
}

bool tlobby::ttransit_sock::connect(TCPsocket sock, const std::string& host, int port)
{
	tsock::connect(sock, host, port);

	// Send data telling the remote host that this is a new connection
	char buf[4] ALIGN_4;
	SDLNet_Write32(0, reinterpret_cast<void*>(buf));
	const int nbytes = SDLNet_TCP_Send(sock, buf, 4);
	if (nbytes != 4) {
		return false;
	}
	return true;
}

void tlobby::ttransit_sock::pre_disconnect()
{
	tsock::pre_disconnect();
}

void tlobby::ttransit_sock::post_disconnect()
{
	tsock::post_disconnect();
	remote_handle_ = 0;
}

size_t tlobby::ttransit_sock::queue_raw_data(const char* buf, int len)
{
	network::buffer* queued_buf = new network::buffer(sock2_);
	assert(*buf == 31);
	network::make_network_buffer(buf, len, queued_buf->raw_buffer);
	network::queue_buffer(sock2_, queued_buf);
	return 4 + len;
}

SOCKET_STATE tlobby::ttransit_sock::receive_buf(std::vector<char>& buf)
{
	char num_buf[4] ALIGN_4;
	bool res = network::receive_with_timeout(*this, num_buf, 4, false);

	if (!res) {
		return SOCKET_ERRORED;
	}

	const int len = SDLNet_Read32(reinterpret_cast<void*>(num_buf));

	if (len < 1 || len > 100000000) {
		return SOCKET_ERRORED;
	}

	buf.resize(len);
	char* beg = &buf[0];
	const char* const end = beg + len;

	if (require_stats) {
		const threading::lock lock(*network::stats_mutex);
		network::transfer_stats[sock2_].second.fresh_current(len);
	}

	res = network::receive_with_timeout(*this, beg, end - beg, true);
	if (!res) {
		return SOCKET_ERRORED;
	}

	return SOCKET_READY;
}

bool tlobby::ttransit_sock::receive_probed()
{
	// See if this socket is still waiting for it to be assigned its remote handle.
	// If it is, then the first 4 bytes must be the remote handle.
	if (is_pending_remote_handle()) {
		char buf[4] ALIGN_4;
		int len = SDLNet_TCP_Recv(sock2_, buf, 4);
		if (len != 4) {
			throw network::error("Remote host disconnected", conn_);
		}
		const int remote_handle = SDLNet_Read32(reinterpret_cast<void*>(buf));
		remote_handle_ = remote_handle;

		return false;
	}
	return true;
}

bool tlobby::ttransit_sock::is_pending_remote_handle() const
{
	return !host_.empty() && !remote_handle_;
}

tlobby::tlobby()
	: pump_monitor(false)
	, handlers_()
	, log_handlers_()
	, logs_()
{
	socks_.push_back(&chat);
	socks_.push_back(&http);
	socks_.push_back(&transit);
}

tlobby::~tlobby() 
{
	// sock is released at last network::~manager
	for (std::vector<tsock*>::const_iterator it = socks_.begin(); it != socks_.end(); ++ it) {
		tsock* info = *it;
		if (info->accept_) {
			delete info;
		}
	}
}


void tlobby::set_nick2(const std::string& leader)
{
	std::string nick = preferences::nick();
	if (nick.empty()) {
		nick = leader;
	}
	set_nick(nick);
}

void tlobby::set_nick(const std::string& nick)
{
	bool dirty = nick_ != nick;

	nick_ = nick;
	if (dirty) {
		if (chat.connected_at_) {
			chat.reset_connect();
		}
	}
}

tsock& tlobby::get_connection_details(network::connection conn) const
{
	VALIDATE(conn != network::null_connection, null_str);

	for (std::vector<tsock*>::const_iterator it = socks_.begin(); it != socks_.end(); ++ it) {
		tsock* s = *it;
		if (s->conn_ == conn) {
			return *s;
		}
	}
	VALIDATE(false, _("invalid network handle"));
	return null_sock;
}

tsock& tlobby::get_connection_details2(TCPsocket sock) const
{
	VALIDATE(sock, null_str);

	for (std::vector<tsock*>::const_iterator it = socks_.begin(); it != socks_.end(); ++ it) {
		tsock* s = *it;
		if (s->sock2_ == sock) {
			return *s;
		}
	}
	VALIDATE(false, _("invalid network TCPsocket"));
	return null_sock;
}

void tlobby::pre_disconnect(network::connection conn)
{
	tsock& sock = get_connection_details(conn);
	sock.pre_disconnect();
}

void tlobby::post_disconnect(network::connection conn)
{
	tsock& sock = get_connection_details(conn);
	sock.post_disconnect();

	if (sock.accept_) {
		int at = sock.at_;
		std::vector<tsock*>::iterator it = socks_.begin();
		std::advance(it, sock.at_);
		socks_.erase(it);

		int s = (int)socks_.size();
		for (int n = at; n < s; n ++) {
			socks_[n]->at_ --;
		}
	}
}

bool tlobby::insert_accept_sock(TCPsocket sock)
{
	tsock* info = get_accept_sock();
	info->at_ = socks_.size();
	info->accept_ = true;
	if (!info->connect(sock, "", 0)) {
		delete info;
		return false;
	}

	socks_.push_back(info);
	return true;
}

tsock* tlobby::get_accept_sock()
{
	return new tsock(socks_.size());
}

void tlobby::add_log(const tsock& sock, const std::string& msg)
{
	// if not connected to netowrk, log will flush. attempt to reduce it.
	const size_t max_logs = 120;

	std::stringstream ss;
	ss << tintegrate::generate_format(sock.tag(), "yellow") << " " << msg;
	if (logs_.size() >= max_logs) {
		std::vector<tlog>::iterator end = logs_.begin();
		std::advance(end, max_logs / 4);
		logs_.erase(logs_.begin(), end);
	}
	logs_.push_back(tlog(sock.state(), ss.str(), time(NULL)));
	if (!log_handlers_.empty()) {
		tlog_handler& h = *log_handlers_.back();
		h.handle(sock, msg);
	}
}

std::string state_name(tsock::tstate s)
{
	if (s == tsock::s_none) {
		return _("Wait");
	} else if (s == tsock::s_create) {
		return _("Create");
	} else if (s == tsock::s_consult) {
		return _("Consult");
	} else if (s == tsock::s_ready) {
		return _("Ready");
	}
	return null_str;
}

std::string tlobby::format_log_str() const
{
	std::stringstream ss;
	for (std::vector<tlog>::const_iterator it = logs_.begin(); it != logs_.end(); ++ it) {
		const tlog& log = *it;
		if (it != logs_.begin()) {
			ss << "\n";
		}
		ss << tintegrate::generate_format(state_name(log.state), "yellow");
		ss << "    " << format_time_date(log.t) << "\n";
		ss << "--  " << log.msg;
	}
	return ss.str();
}

void tlobby::process(events::pump_info&)
{
	for (std::vector<tsock*>::const_iterator it = socks_.begin(); it != socks_.end(); ++ it) {
		tsock* sock = *it;
		try {
			sock->process();
		} catch (network::error& e) {
			std::string err_str = e.message;
			if (e.message.empty()) {
				err_str = "Unspecial error", "red";
			}
			sock->process_error(err_str);
		}
	}
}

void tlobby::broadcast_handle_raw(int at, tsock::ttype type, const char* param[])
{
	bool halt = false;
	for (std::vector<tlobby::thandler*>::const_reverse_iterator rit = handlers_.rbegin(); rit != handlers_.rend(); ++ rit) {
		tlobby::thandler& h = **rit;
		halt = h.handle_raw(at, type, param);
		if (type == tsock::t_data && halt) {
			break;
		}
	}
}


