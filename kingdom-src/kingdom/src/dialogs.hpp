/* $Id: dialogs.hpp 40489 2010-01-01 13:16:49Z mordante $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#ifndef DIALOGS_H_INCLUDED
#define DIALOGS_H_INCLUDED

class config;
class unit;
class unit_type;
class hero;
class team;
class display;

#include "map_location.hpp"
#include "events.hpp"
#include "network.hpp"

namespace dialogs {

/**
 * Function to handle an advancing unit. If there is only one choice to advance
 * to, the unit will be automatically advanced. If there is a choice, and
 * 'random_choice' is true, then a unit will be selected at random. Otherwise,
 * a dialog will be displayed asking the user what to advance to.
 *
 * Note that 'loc' is not a reference, because deleting an item from the units
 * map (when replacing the unit that is being advanced) will possibly
 * invalidate the reference.
 *
 * The game only expects an advancement to be triggered by a fight, if the
 * cause for advancement is different (eg unstore_unit) the add_replay_event
 * should be set.
 */
bool advance_unit(unit& u, bool random_choice = false, bool choose_from_random = false);

bool animate_unit_advancement(unit& u, size_t choice);

void show_objectives(const config &level, const team& t);

network::connection network_connect_dialog(display& disp, const std::string& msg, const std::string& hostname, int port, bool quiet);
void network_receive_dialog(display& disp, const std::string& msg, std::vector<char>& buf, network::connection connection_num, int hidden_ms = 4);
network::connection network_receive_dialog(display& disp, const std::string& msg, config& cfg, network::connection connection_num = 0, int hidden_ms = 4);
void network_send_dialog(display& disp, const std::string& msg, const char* buf, int len, network::connection connection_num, int hidden_ms = 4);

} //end namespace dialogs

namespace network_asio {

class connection
{
public:
	connection(network::connection connection_num);

	virtual void poll() = 0;
	void cancel() { done_ = true; }

	// upload
	size_t bytes_written();
	size_t bytes_to_write();
	// download
	size_t bytes_read();
	size_t bytes_to_read();

	bool done() const { return done_; }
protected:
	network::connection connection_num_;
	bool done_;
};

}

#endif
