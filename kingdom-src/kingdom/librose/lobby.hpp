/* $Id: multiplayer.hpp 47847 2010-12-05 21:12:23Z shadowmaster $ */
/*
   Copyright (C) 2005 - 2010 Philippe Plantier <ayin@anathas.org>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#ifndef LOBBY_HPP_INCLUDED
#define LOBBY_HPP_INCLUDED

#include "network.hpp"
#include "sdl_utils.hpp"
#include "events.hpp"
#include "config.hpp"
#include <time.h>

struct tchat_log {
	tchat_log(const std::string& msg, time_t t, bool other)
		: msg(msg)
		, t(t)
		, other(other)
	{}

	std::string msg;
	time_t t;
	bool other;
};
extern std::map<int, std::vector<tchat_log> > chat_logs;
void add_chat_log(int uid, const std::string& msg, bool other);

extern std::map<int, std::string> lookup_users;
void lookup_users_insert(int uid, const std::string& username);

class tlobby: public events::pump_monitor
{
public:
	static int reconnect_prohabit;
	static int heartbeat_threshold;

	enum tstate {s_none, s_create, s_consult, s_ready};
	enum ttype {t_none, t_data, t_connected, t_disconnected};
	class thandler
	{
	public:
		thandler();
		virtual ~thandler();
		virtual bool handle(ttype type, const config& data) = 0;
		void join();

	private:
		bool has_joined_;
	};

	class tlog_handler
	{
	public:
		tlog_handler();
		virtual ~tlog_handler();
		virtual void handle(tstate s, const std::string& msg) = 0;
		void join();

	private:
		bool has_joined_;
	};
	
	struct tlog
	{
		tlog(tstate state, const std::string& msg, time_t t)
			: state(state)
			, msg(msg)
			, t(t)
		{}
		
		tlobby::tstate state;
		std::string msg;
		time_t t;
	};

	tlobby();
	~tlobby();

	void process(events::pump_info&);
	void default_handle(const config& data);

	void set_host(const std::string& host, int port);
	const std::string host() const { return host_; }
	int port() const { return port_; }
	void reset_connect();
	bool check_time_overflow(Uint32 threshold);
	void process_error(const std::string& error_str);
	bool ready() const { return state_ == s_ready; }

	void add_log(tstate s, const std::string& msg);
	void clear_log() { logs.clear(); }
	std::string format_log_str() const;

public:
	network::connection sock;
	std::vector<thandler*> handlers;
	std::vector<tlog_handler*> log_handlers;
	std::vector<tlog> logs;

private:
	std::string host_;
	int port_;
	tstate state_;
	config data_;
	Uint32 last_active_time_;
	Uint32 next_create_time_;
};

extern tlobby lobby;

#endif
