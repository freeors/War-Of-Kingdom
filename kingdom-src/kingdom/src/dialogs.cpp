/* $Id: dialogs.cpp 41192 2010-02-13 17:54:41Z silene $ */
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

/**
 * @file dialogs.cpp
 * Various dialogs: advance_unit, show_objectives, save+load game, network::connection.
 */

#include "global.hpp"

#include "dialogs.hpp"
#include "game_events.hpp"
#include "game_display.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "help.hpp"
#include "language.hpp"
#include "log.hpp"
#include "map.hpp"
#include "map_exception.hpp"
#include "marked-up_text.hpp"
#include "menu_events.hpp"
#include "mouse_handler_base.hpp"
#include "minimap.hpp"
#include "replay.hpp"
#include "resources.hpp"
#include "savegame.hpp"
#include "thread.hpp"
#include "wml_separators.hpp"
#include "wml_exception.hpp"
#include "formula_string_utils.hpp"
#include "gui/dialogs/game_save.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/dialogs/select_unit.hpp"
#include "artifical.hpp"
#include "unit_display.hpp"
#include "gui/dialogs/network_transmission.hpp"
#include "gui/widgets/window.hpp"
#include "multiplayer.hpp"
#include "lobby.hpp"
#include "integrate.hpp"

#include <boost/foreach.hpp>
#include <clocale>

static lg::log_domain log_engine("engine");
#define LOG_NG LOG_STREAM(info, log_engine)
#define ERR_NG LOG_STREAM(err, log_engine)

static lg::log_domain log_display("display");
#define LOG_DP LOG_STREAM(info, log_display)

#define ERR_G  LOG_STREAM(err, lg::general)

static lg::log_domain log_config("config");
#define ERR_CF LOG_STREAM(err, log_config)

namespace dialogs
{

bool advance_unit(unit& u, bool random_choice, bool choose_from_random)
{
	game_display& gui = *resources::screen;

	if (u.advances() == false) {
		return false;
	}

	const std::vector<std::string>& options = u.advances_to();
	std::vector<std::string> lang_options;

	std::vector<unit> sample_units;
	unit* sample_unit;
	for(std::vector<std::string>::const_iterator op = options.begin(); op != options.end(); ++op) {
		sample_unit = ::get_advanced_unit(&u, *op);
		sample_units.push_back(*sample_unit);
		delete sample_unit;
		const unit& type = sample_units.back();

		lang_options.push_back(IMAGE_PREFIX + type.absolute_image() + u.image_mods() + COLUMN_SEPARATOR + type.type_name());
		preferences::encountered_units().insert(*op);
	}

	bool always_display = false;
	BOOST_FOREACH (const config &mod, u.get_modification_advances())
	{
		if (utils::string_bool(mod["always_display"])) always_display = true;
		std::string to;
		if (u.packed()) {
			to = u.packee_type_id();
		} else {
			to = u.type_id();
		}
		sample_unit = ::get_advanced_unit(&u, to);
		sample_units.push_back(*sample_unit);
		delete sample_unit;
		sample_units.back().add_modification(mod);
		const unit& type = sample_units.back();
		if (!mod["image"].empty()) {
			lang_options.push_back(IMAGE_PREFIX + mod["image"].str() + COLUMN_SEPARATOR + mod["description"].str());
		} else {
			lang_options.push_back(IMAGE_PREFIX + type.absolute_image() + u.image_mods() + COLUMN_SEPARATOR + mod["description"].str());
		}
	}

	if (lang_options.empty()) {
		return false;
	}

	int res = 0;

	const config* ran_results = NULL;
	if (choose_from_random) {
		get_random();
		ran_results = get_random_results();
	}

	if (ran_results) {
		res = (*ran_results)["choose"].to_int();

	} else if (random_choice) {
		if (lang_options.size() > 1) {
			// select character if existed.
			size_t index = 0;
			for (std::vector<std::string>::const_iterator it = options.begin(); it != options.end(); ++ it, index ++) {
				const unit_type* ut = unit_types.find(*it);
				if (ut->especial() != -1) {
					break;
				}
			}
			if (index == sample_units.size()) {
				res = rand()%lang_options.size();
			} else {
				res = index;
			}
		} else {
			res = 0;
		}
	} else if (lang_options.size() > 1 || always_display) {

		std::vector<const unit*> sample_units_ptr;
		for (std::vector<unit>::const_iterator iter = sample_units.begin(); iter != sample_units.end(); ++ iter) {
			sample_units_ptr.push_back(&*iter);
		}
		gui2::tselect_unit dlg(gui, *resources::teams, *resources::units, *resources::heros, sample_units_ptr, gui2::tselect_unit::ADVANCE);
		try {
			dlg.show(gui.video());
		} catch(twml_exception& e) {
			e.show(gui);
		}
		res = dlg.troop_index();
	}

	if (choose_from_random) {
		if (!ran_results) {
			config cfg;
			cfg["choose"] = res;
			set_random_results(cfg);
		}
	} else {
		recorder.choose_option(res);
	}

	return animate_unit_advancement(u, size_t(res));

	// loc maybe is loc_ of advanced unit, so is invalid after advance,
	// don't call relatve loc_, else use copy of loc.
}

bool animate_unit_advancement(unit& u, size_t choice)
{
	const events::command_disabler cmd_disabler;

	if (u.advances() == false) {
		return false;
	}

	const std::vector<std::string>& options = u.advances_to();
	std::vector<config> mod_options = u.get_modification_advances();

	if (choice >= options.size() + mod_options.size()) {
		return false;
	}

	// When the unit advances, it fades to white, and then switches
	// to the new unit, then fades back to the normal colour

	game_display* disp = resources::screen;
	rect_of_hexes& draw_area = disp->draw_area();
	bool force_scroll = preferences::scroll_to_action();
	bool animate = force_scroll || point_in_rect_of_hexes(u.get_location().x, u.get_location().y, draw_area);
	if (!resources::screen->video().update_locked() && animate) {
		unit_animator animator;
		bool with_bars = true;
		animator.add_animation(&u, "levelout", u.get_location(), map_location(), 0, with_bars);
		animator.start_animations();
		animator.wait_for_end();
	}

	if(choice < options.size()) {
		// chosen_unit is not a reference, since the unit may disappear at any moment.
		std::string chosen_unit = options[choice];
		::advance_unit(u, chosen_unit);
	} else {
		unit* amla_unit = &u;
		const config &mod_option = mod_options[choice - options.size()];
		
		game_events::fire("advance", u.get_location());

		amla_unit->get_experience(increase_xp::attack_ublock(*amla_unit), -amla_unit->max_experience()); // subtract xp required
		// ALMA may want to change status, but add_modification in modify_according_to_hero cannot change state,
		// so it need call amla_unit->add_modification instead of amla_unit->modify_according_to_hero.
		amla_unit->add_modification(mod_option);

		game_events::fire("post_advance", u.get_location());
	}

	resources::screen->invalidate_unit();

	if (!resources::screen->video().update_locked()) {
		if (force_scroll || point_in_rect_of_hexes(u.get_location().x, u.get_location().y, draw_area)) {
			unit_animator animator;
			animator.add_animation(&u, "levelin", u.get_location(), map_location(), 0, true);
			animator.start_animations();
			animator.wait_for_end();
			animator.set_all_standing();
		
			resources::screen->invalidate(u.get_location());
			resources::screen->draw();
		}
		events::pump();
	}

	resources::screen->invalidate_all();
	if (force_scroll || point_in_rect_of_hexes(u.get_location().x, u.get_location().y, draw_area)) {
		resources::screen->draw();
	}

	return true;
}

void show_objectives(const config &level, const team& t)
{
	static const std::string no_objectives(_("No objectives available"));
	const std::string& objectives = t.objectives();

	gui2::show_transient_message(resources::screen->video(), level["name"],	
		t.form_results_of_battle_tip(objectives.empty() ? no_objectives : objectives), "", true);
}

} // end namespace dialogs

namespace network_asio {

connection::connection(network::connection connection_num)
	: connection_num_(connection_num)
	, done_(false)
{}

size_t connection::bytes_written()
{
	if (connection_num_ != network::null_connection) {
		network::statistics stat = network::get_send_stats(connection_num_);
		return stat.current;
	} else {
		return 5;
	}
}

size_t connection::bytes_to_write()
{
	if (connection_num_ != network::null_connection) {
		network::statistics stat = network::get_send_stats(connection_num_);
		return stat.current_max;
	} else {
		return 100;
	}
}

size_t connection::bytes_read()
{
	if (connection_num_ != network::null_connection) {
		network::statistics stat = network::get_receive_stats(connection_num_);
		return stat.current;
	} else {
		return 5;
	}
}

size_t connection::bytes_to_read()
{
	if (connection_num_ != network::null_connection) {
		network::statistics stat = network::get_receive_stats(connection_num_);
		return stat.current_max;
	} else {
		return 100;
	}
}

class connection_recv_buf : public connection
{
public:
	connection_recv_buf(network::connection connection_num, std::vector<char>& buf)
		: connection(connection_num)
		, buf_(buf)
	{}

	void poll();
private:
	std::vector<char>& buf_;
};

void connection_recv_buf::poll()
{
	network::connection sock = network::receive_data(buf_);
	if (sock != network::null_connection) {
		cancel();
	}
}

class connection_recv_cfg : public connection, public tlobby::thandler
{
public:
	connection_recv_cfg(network::connection connection_num, config& data)
		: connection(connection_num)
		, data_(data)
		, res_(network::null_connection)
	{
		join();
	}

	bool handle(tlobby::ttype type, const config& data);
	void poll();
	network::connection res() const { return res_; }
private:
	config& data_;
	network::connection res_;
};

bool connection_recv_cfg::handle(tlobby::ttype type, const config& data)
{
	if (type == tlobby::t_data) {
		data_ = data;
		res_ = lobby.sock;
		return true;
	}

	return false;
}

void connection_recv_cfg::poll()
{
	if (res_ != network::null_connection) {
		cancel();
	}
}

class connection_send_buf : public connection
{
public:
	connection_send_buf(network::connection connection_num, int len)
		: connection(connection_num)
		, len_(len)
	{}

	void poll();

private:
	int len_;
};

void connection_send_buf::poll()
{
	network::statistics stat = network::get_send_stats(connection_num_);
	if (stat.current == len_) {
		cancel();
	}
}

class connection_open : public connection
{
public:
	connection_open(threading::async_operation_ptr op)
		: connection(network::null_connection)
		, op_(op)
	{}

	void poll();
private:
	threading::async_operation_ptr op_;
};

void connection_open::poll()
{
	const threading::condition::WAIT_TIMEOUT_RESULT res = op_->finished_.wait_timeout(op_->get_mutex(), 20);
	if (res == threading::condition::WAIT_OK || op_->finishedVar_) {
		cancel();
	}
}

}

namespace dialogs
{

class connect_waiter : public threading::waiter
{
public:
	connect_waiter(display& disp, const std::string& msg) : disp_(disp), msg_(msg)
	{}
	ACTION process(threading::async_operation_ptr op);

private:
	display& disp_;
	std::string msg_;
};

connect_waiter::ACTION connect_waiter::process(threading::async_operation_ptr op)
{
	try {
		network_asio::connection_open conn(op);
		gui2::tnetwork_transmission dlg(conn, msg_, "");
		dlg.show(disp_.video());
		if (dlg.get_retval() == gui2::twindow::OK) {
			return WAIT;
		}

	} catch (network::error& e) {
		std::string err = e.message;
		if (e.message.empty()) {
			err = "Unspecial error";
		}
		gui2::show_transient_message(disp_.video(), "", gettext(err.c_str()));
	}
	return ABORT;
}

std::string form_connect_to_title()
{
	std::stringstream strstr;
	utils::string_map i18n_symbols;
	strstr << tintegrate::generate_format(game_config::bbs_server.name, "green");
	i18n_symbols["server"] = strstr.str();

	return vgettext("Connecting to $server...", i18n_symbols);
}

std::string form_send_to_title()
{
	std::stringstream strstr;
	utils::string_map i18n_symbols;
	strstr << tintegrate::generate_format(game_config::bbs_server.name, "green");
	i18n_symbols["server"] = strstr.str();

	return vgettext("Sending to $server...", i18n_symbols);
}

std::string form_receive_from_title()
{
	std::stringstream strstr;
	utils::string_map i18n_symbols;
	strstr << tintegrate::generate_format(game_config::bbs_server.name, "green");
	i18n_symbols["server"] = strstr.str();

	return vgettext("Reading from $server...", i18n_symbols);
}

network::connection network_connect_dialog(display& disp, const std::string&, const std::string& hostname, int port, bool xmit_http_data, bool quiet)
{
	network::connection conn = network::null_connection;

	connect_waiter waiter(disp, form_connect_to_title());
	try {
		conn = network::connect(hostname, port, xmit_http_data, false, waiter);
	} catch (network::error& e) {
		std::string err = e.message;
		if (e.message.empty()) {
			err = "Unspecial error";
		}
		if (!quiet) {
			gui2::show_transient_message(disp.video(), "", gettext(err.c_str()));
		}
	}
	return conn;
}

void network_receive_dialog(display& disp, const std::string& msg, std::vector<char>& buf, network::connection connection_num, int hidden_ms)
{
	try {
		network_asio::connection_recv_buf conn(connection_num, buf);
		gui2::tnetwork_transmission dlg(conn, msg.empty()? form_receive_from_title(): msg, "", hidden_ms);
		dlg.show(disp.video());

	} catch(network::error& e) {
		std::string err = e.message;
		if (e.message.empty()) {
			err = "Unspecial error";
		}
		gui2::show_transient_message(disp.video(), "", gettext(err.c_str()));
	}
}

network::connection network_receive_dialog(display& disp, const std::string& msg, config& cfg, network::connection connection_num, int hidden_ms)
{
	try {
		network_asio::connection_recv_cfg conn(connection_num, cfg);
		gui2::tnetwork_transmission dlg(conn, msg.empty()? form_receive_from_title(): msg, "", hidden_ms);
		dlg.show(disp.video());

		return conn.res();

	} catch(network::error& e) {
		std::string err = e.message;
		if (e.message.empty()) {
			err = "Unspecial error";
		}
		gui2::show_transient_message(disp.video(), "", gettext(err.c_str()));
	}

	return network::null_connection;
}

void network_send_dialog(display& disp, const std::string& msg, const char* buf, int len, network::connection connection_num, int hidden_ms)
{
	try {
		network::send_raw_data(buf, len, connection_num);

		network_asio::connection_send_buf conn(connection_num, len);
		gui2::tnetwork_transmission dlg(conn, msg.empty()? form_send_to_title(): msg, "", hidden_ms);
		dlg.set_track_upload(true);
		dlg.show(disp.video());

	} catch(network::error& e) {
		std::string err = e.message;
		if (e.message.empty()) {
			err = "Unspecial error";
		}
		gui2::show_transient_message(disp.video(), "", gettext(err.c_str()));
	}
}

} // end namespace dialogs
