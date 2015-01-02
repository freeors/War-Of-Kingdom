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

tlobby lobby;

tlobby::thandler::thandler()
	: has_joined_(false)
{
}

tlobby::thandler::~thandler()
{
	std::vector<tlobby::thandler*>& handlers = lobby.handlers;
	std::vector<tlobby::thandler*>::iterator it = std::find(handlers.begin(), handlers.end(), this);
	if (it != handlers.end()) {
		handlers.erase(it);
	}
}

void tlobby::thandler::join()
{
	if (has_joined_) {
		return;
	}
	lobby.handlers.push_back(this);
	has_joined_ = true;
}

tlobby::tlog_handler::tlog_handler()
	: has_joined_(false)
{
}

tlobby::tlog_handler::~tlog_handler()
{
	std::vector<tlobby::tlog_handler*>& handlers = lobby.log_handlers;
	handlers.erase(std::remove(handlers.begin(), handlers.end(), this));
}

void tlobby::tlog_handler::join()
{
	if (has_joined_) {
		return;
	}
	lobby.log_handlers.push_back(this);
	has_joined_ = true;
}

int tlobby::reconnect_prohabit = 20000;
int tlobby::heartbeat_threshold = 120000; // heartbeat maybe to 1 minute.

tlobby::tlobby()
	: pump_monitor(false)
	, host_()
	, port_(0)
	, sock(network::null_connection)
	, state_(s_none)
	, data_()
	, handlers()
	, log_handlers()
	, last_active_time_(0)
	, next_create_time_(0)
{}

tlobby::~tlobby() 
{
	// tlobby is global variable. it maybe destruct after other global netowrk variable.
	// don't call network::disconnect at here, disconnect maybe use network variable but has destructed.
	// sock is released at last network::~manager
}

void tlobby::set_host(const std::string& host, int port)
{
	if (host == host_ && port == port_) {
		return;
	}
	host_ = host;
	port_ = port;
}

class async_connect_waiter : public threading::waiter
{
public:
	async_connect_waiter() {}
	ACTION process(threading::async_operation_ptr op);
};

async_connect_waiter::ACTION async_connect_waiter::process(threading::async_operation_ptr op)
{
	// see remark#50
	while (op.use_count() < 5) {
		SDL_Delay(1);
	}

	return WAIT;
}

void tlobby::add_log(tlobby::tstate s, const std::string& msg)
{
	logs.push_back(tlog(s, msg, time(NULL)));
	if (!log_handlers.empty()) {
		tlog_handler& h = *log_handlers.back();
		h.handle(s, msg);
	}
}

std::string state_name(tlobby::tstate s)
{
	if (s == tlobby::s_none) {
		return dgettext("wesnoth-lib", "Wait");
	} else if (s == tlobby::s_create) {
		return dgettext("wesnoth-lib", "Create");
	} else if (s == tlobby::s_consult) {
		return dgettext("wesnoth-lib", "Consult");
	} else if (s == tlobby::s_ready) {
		return dgettext("wesnoth-lib", "Ready");
	}
	return null_str;
}

std::string tlobby::format_log_str() const
{
	std::stringstream ss;
	for (std::vector<tlog>::const_iterator it = logs.begin(); it != logs.end(); ++ it) {
		const tlog& log = *it;
		if (it != logs.begin()) {
			ss << "\n";
		}
		ss << tintegrate::generate_format(state_name(log.state), "yellow");
		ss << "    " << format_time_date(log.t) << "\n";
		ss << "--  " << log.msg;
	}
	return ss.str();
}

void tlobby::reset_connect()
{
	network::disconnect(sock);
	state_ = s_none;
	sock = network::null_connection;

	for (std::vector<thandler*>::const_reverse_iterator rit = handlers.rbegin(); rit != handlers.rend(); ++ rit) {
		thandler& h = **rit;
		h.handle(t_disconnected, null_cfg);
	}
}

bool tlobby::check_time_overflow(Uint32 threshold)
{
	bool should_reconnect = false;
	int distance = 0;
	if (state_ == s_create || state_ == s_consult || state_ == s_ready) {
		Uint32 now = SDL_GetTicks();
		if (now - last_active_time_ > threshold) {
			if (now < last_active_time_ + reconnect_prohabit) {
				next_create_time_ = last_active_time_ + reconnect_prohabit;
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
		std::string str = vgettext("wesnoth-lib", "No response in $threshold sec!", symbols);

		symbols["error"] = tintegrate::generate_format(str, "red");
		if (distance > 1000) {
			symbols["distance"] = str_cast(distance / 1000); 
			err = vgettext("wesnoth-lib", "$error will reconnect after $distance sec.", symbols);
		} else {
			err = vgettext("wesnoth-lib", "$error will Reconnect immediately!", symbols);
		}
		add_log(state_, err);
		reset_connect();
	}

	return should_reconnect;
}

void tlobby::process_error(const std::string& err_str)
{
	std::string err_str2 = tintegrate::stuff_escape(err_str);

	next_create_time_ = last_active_time_ + reconnect_prohabit;
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

	add_log(state_, err);
	reset_connect();
}

void tlobby::process(events::pump_info&)
{
	try {
		if (state_ == s_none) {
			if (SDL_GetTicks() <= next_create_time_) {
				return;
			}

			network::lobby_sock = -1;
			last_active_time_ = SDL_GetTicks();

			VALIDATE(sock == network::null_connection, "s_none, must be null_connection");
			async_connect_waiter waiter;
			network::connect(host_, port_, false, true, waiter);
			
			std::stringstream strstr;
			strstr << "Start connecting to " << tintegrate::generate_format(host_, "green") << ":" << tintegrate::generate_format(port_, "green");
			add_log(state_, strstr.str());
			state_ = s_create;

		} else if (state_ == s_create) {
			if (network::lobby_sock != -1) {
				sock = network::lobby_sock;
				if (sock != network::null_connection) {
					add_log(state_, "Create success! Enter consult.");
					state_ = s_consult;
				} else {
					std::stringstream err;
					next_create_time_ = SDL_GetTicks() + reconnect_prohabit;
					err << tintegrate::generate_format(network::lobby_sock_error, "red");
					err << " will reconnect after " << reconnect_prohabit / 1000 << " s.";
					add_log(state_, err.str());
					state_ = s_none;
				}
			} else {
				check_time_overflow(reconnect_prohabit);
			}
		} else if (state_ == s_consult) {
			const network::connection ret = network::receive_data(data_, sock);
			if (ret != sock) {
				check_time_overflow(reconnect_prohabit);
				return;
			}

			// step1: server--->client, version block, no data
			//       client--->server, version block, has data
			if (data_.child("version")) {
				add_log(state_, "Receive [version], response [version].");

				config cfg;
				config res;
				// cfg["version"] = game_config::version;
				// fake version, in order to login in wesnoth server
				// cfg["version"] = "1.9.10";
				cfg["version"] = "test";
				res.add_child("version", cfg);
				network::send_data(res, sock);

			} else if (data_.child("mustlogin")) {
				std::stringstream ss;
				ss << "Receive [mustlogin], response [login]: " << tintegrate::generate_format(preferences::login(), "green");
				add_log(state_, ss.str());

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
				network::send_data(response, sock);

			} else if (data_.child("join_lobby")) {
				add_log(state_, "Receive [join_lobby], Consult success. Enter ready.!");

				state_ = s_ready;
				for (std::vector<thandler*>::const_reverse_iterator rit = handlers.rbegin(); rit != handlers.rend(); ++ rit) {
					thandler& h = **rit;
					h.handle(t_connected, null_cfg);
				}

			} else if (const config& cfg = data_.child("error")) {
				process_error(cfg["message"].str());
				return;
			}

		} else if (state_ == s_ready) {
			ttype type = t_none;
			network::connection ret = network::receive_data(data_, sock);
			if (ret != sock) {
				if (check_time_overflow(heartbeat_threshold)) {
					return;
				}
				ret = network::null_connection;
			} else {
				last_active_time_ = SDL_GetTicks();
				type = t_data;
				// if (data_.has_attribute("ping")) {
				//	add_log(state_, "Receive ping package.");
				// }
			}

			bool halt = false;
			for (std::vector<thandler*>::const_reverse_iterator rit = handlers.rbegin(); rit != handlers.rend(); ++ rit) {
				thandler& h = **rit;
				halt = h.handle(type, data_);
				if (halt && type == t_data) {
					break;
				}
			}
			if (!halt && type == t_data) {
				default_handle(data_);
			}
		}

	} catch (network::error& e) {
		std::string err_str = e.message;
		if (e.message.empty()) {
			err_str = "Unspecial error", "red";
		}
		process_error(err_str);
	}
}

std::map<int, std::string> lookup_users;
int rfind_user(const std::string& username)
{
	for (std::map<int, std::string>::const_iterator it = lookup_users.begin(); it != lookup_users.end(); ++ it) {
		if (it->second == username) {
			return it->first;
		}
	}
	return HTTP_INVALID_UID;
}

void lookup_users_insert(int uid, const std::string& username)
{
	std::map<int, std::string>::const_iterator it = lookup_users.find(uid);
	if (it == lookup_users.end()) {
		lookup_users.insert(std::make_pair(uid, username));
	}
}

void tlobby::default_handle(const config& data)
{
	if (const config& c = data.child("whisper")) {
		std::string sender = c["sender"].str();
		const std::string& message = c["message"].str();
		
		int uid = rfind_user(sender);
		if (uid != HTTP_INVALID_UID) {
			add_chat_log(uid, message, true);
		}
	}
}

