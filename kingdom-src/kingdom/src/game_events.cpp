/* $Id: game_events.cpp 47568 2010-11-15 15:08:29Z anonymissimus $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file
 * Processing of WML-events.
 */

#include "global.hpp"

#include "actions.hpp"
#include "ai/manager.hpp"
#include "dialogs.hpp"
#include "game_display.hpp"
#include "game_events.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "gui/dialogs/gamestate_inspector.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/widgets/window.hpp"
#include "help.hpp"
#include "log.hpp"
#include "map.hpp"
#include "map_label.hpp"
#include "map_exception.hpp"
#include "replay.hpp"
#include "resources.hpp"
#include "scripting/lua.hpp"
#include "sound.hpp"
#include "soundsource.hpp"
#include "terrain_filter.hpp"
#include "unit_display.hpp"
#include "unit_helper.hpp"
#include "wml_exception.hpp"
#include "play_controller.hpp"
#include "persist_var.hpp"
#include "formula_string_utils.hpp"
#include "artifical.hpp"
#include "side_filter.hpp"

#include <boost/foreach.hpp>
#include <boost/scoped_array.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <algorithm>
#include <iomanip>
#include <iostream>

static lg::log_domain log_engine("engine");
#define DBG_NG LOG_STREAM(debug, log_engine)
#define LOG_NG LOG_STREAM(info, log_engine)
#define WRN_NG LOG_STREAM(warn, log_engine)
#define ERR_NG LOG_STREAM(err, log_engine)

static lg::log_domain log_display("display");
#define DBG_DP LOG_STREAM(debug, log_display)
#define LOG_DP LOG_STREAM(info, log_display)

static lg::log_domain log_wml("wml");
#define DBG_WML LOG_STREAM(debug, log_wml)
#define LOG_WML LOG_STREAM(info, log_wml)
#define WRN_WML LOG_STREAM(warn, log_wml)
#define ERR_WML LOG_STREAM(err, log_wml)

static lg::log_domain log_config("config");
#define ERR_CF LOG_STREAM(err, log_config)

/**
 * State when processing a flight of events or commands.
 */
struct event_context
{
	bool mutated;
	bool skip_messages;
	event_context(bool s): mutated(true), skip_messages(s) {}
};

static event_context default_context(false);
static event_context *current_context = &default_context;

/**
 * Context state with automatic lifetime handling.
 */
struct scoped_context
{
	event_context *old_context;
	event_context new_context;

	scoped_context()
		: old_context(current_context)
		, new_context(old_context != &default_context && old_context->skip_messages)
	{
		current_context = &new_context;
	}

	~scoped_context()
	{
		old_context->mutated |= new_context.mutated;
		current_context = old_context;
	}
};

static bool screen_needs_rebuild;

namespace {

	std::stringstream wml_messages_stream;

	bool manager_running = false;
	int floating_label = 0;

	std::vector< game_events::event_handler > new_handlers;
	typedef std::pair< std::string, config* > wmi_command_change;
	std::vector< wmi_command_change > wmi_command_changes;

	const gui::msecs prevent_misclick_duration = 10;
	const gui::msecs average_frame_time = 30;

	class pump_manager {
		public:
		pump_manager() :
			x1_(resources::state_of_game->get_variable("x1")),
			x2_(resources::state_of_game->get_variable("x2")),
			y1_(resources::state_of_game->get_variable("y1")),
			y2_(resources::state_of_game->get_variable("y2"))
		{
			++instance_count;
		}
		~pump_manager() {
			resources::state_of_game->get_variable("x1") = x1_;
			resources::state_of_game->get_variable("x2") = x2_;
			resources::state_of_game->get_variable("y1") = y1_;
			resources::state_of_game->get_variable("y2") = y2_;
			--instance_count;
		}
		static unsigned count() {
			return instance_count;
		}
		private:
		static unsigned instance_count;
		int x1_, x2_, y1_, y2_;
	};
	unsigned pump_manager::instance_count=0;

} // end anonymous namespace (1)

#ifdef _MSC_VER
// std::getline might be broken in Visual Studio so show a warning
#if _MSC_VER < 1300
#ifndef GETLINE_PATCHED
#pragma message("warning: the std::getline implementation in your compiler might be broken.")
#pragma message(" http://support.microsoft.com/default.aspx?scid=kb;EN-US;q240015")
#endif
#endif
#endif

/**
 * Helper function which determines whether a wml_message text can
 * really be pushed into the wml_messages_stream, and does it.
 */
static void put_wml_message(const std::string& logger, const std::string& message)
{
	if (logger == "err" || logger == "error") {
		ERR_WML << message << "\n";
		wml_messages_stream << _("Error: ") << message << "\n";
	} else if (logger == "warn" || logger == "wrn" || logger == "warning") {
		WRN_WML << message << "\n";
		wml_messages_stream << _("Warning: ") << message << "\n";
	} else if ((logger == "debug" || logger == "dbg") && !lg::debug.dont_log(log_wml)) {
		DBG_WML << message << "\n";
		wml_messages_stream << _("Debug: ") << message << "\n";
	} else if (!lg::info.dont_log(log_wml)) {
		LOG_WML << message << "\n";
		wml_messages_stream << _("Info: ") << message << "\n";
	}
}

/**
 * Helper function for show_wml_errors(), which gathers
 * the messages from a stringstream.
 */
static void fill_wml_messages_map(std::map<std::string, int>& msg_map, std::stringstream& source)
{
	while(true) {
		std::string msg;
		std::getline(source, msg);

		if(source.eof()) {
			break;
		}

		if(msg == "") {
			continue;
		}

		if(msg_map.find(msg) == msg_map.end()) {
			msg_map[msg] = 1;
		} else {
			msg_map[msg]++;
		}
	}
	// Make sure the eof flag is cleared otherwise no new messages are shown
	source.clear();
}

/**
 * Shows a summary of the errors encountered in WML thusfar,
 * to avoid a lot of the same messages to be shown.
 * Identical messages are shown once, with (between braces)
 * the number of times that message was encountered.
 * The order in which the messages are shown does not need
 * to be the order in which these messages are encountered.
 * Messages are always written to std::cerr.
 */
static void show_wml_errors()
{
	// Get all unique messages in messages,
	// with the number of encounters for these messages
	std::map<std::string, int> messages;
	fill_wml_messages_map(messages, lg::wml_error);

	// Show the messages collected
	const std::string caption = "Invalid WML found";
	for(std::map<std::string, int>::const_iterator itor = messages.begin();
			itor != messages.end(); ++itor) {

		std::stringstream msg;
		msg << itor->first;
		if(itor->second > 1) {
			msg << " (" << itor->second << ")";
		}

		resources::screen->add_chat_message(time(NULL), caption, 0, msg.str(),
				events::chat_handler::MESSAGE_PUBLIC, false);
		std::cerr << caption << ": " << msg.str() << '\n';
	}
}

static void show_wml_messages()
{
	// Get all unique messages in messages,
	// with the number of encounters for these messages
	std::map<std::string, int> messages;
	fill_wml_messages_map(messages, wml_messages_stream);

	// Show the messages collected
	const std::string caption = "WML";
	for(std::map<std::string, int>::const_iterator itor = messages.begin();
			itor != messages.end(); ++itor) {

		std::stringstream msg;
		msg << itor->first;
		if(itor->second > 1) {
			msg << " (" << itor->second << ")";
		}

		resources::screen->add_chat_message(time(NULL), caption, 0, msg.str(),
				events::chat_handler::MESSAGE_PUBLIC, false);
	}
}

typedef void (*wml_handler_function)(
	const game_events::queued_event &event_info, const vconfig &cfg);

typedef std::map<std::string, wml_handler_function> static_wml_action_map;
/** Map of the default action handlers known of the engine. */
static static_wml_action_map static_wml_actions;

/**
 * WML_HANDLER_FUNCTION macro handles auto registeration for wml handlers
 *
 * @param pname wml tag name
 * @param pei the variable name of game_events::queued_event object inside function
 * @param pcfg the variable name of config object inside function
 *
 * You are warned! This is evil macro magic!
 *
 * The following code registers a [foo] tag:
 * \code
 * // comment out unused parameters to prevent compiler warnings
 * WML_HANDLER_FUNCTION(foo, event_info, cfg)
 * {
 *    // code for foo
 * }
 * \endcode
 *
 * Generated code looks like this:
 * \code
 * void wml_action_foo(...);
 * struct wml_func_register_foo {
 *   wml_func_register_foo() {
 *     static_wml_actions["foo"] = &wml_func_foo;
 *   } wml_func_register_foo;
 * void wml_func_foo(...)
 * {
 *    // code for foo
 * }
 * \endcode
 */
#define WML_HANDLER_FUNCTION(pname, pei, pcfg) \
	static void wml_func_##pname(const game_events::queued_event &pei, const vconfig &pcfg); \
	struct wml_func_register_##pname \
	{ \
		wml_func_register_##pname() \
		{ static_wml_actions[#pname] = &wml_func_##pname; } \
	}; \
	static wml_func_register_##pname wml_func_register_##pname##_aux;  \
	static void wml_func_##pname(const game_events::queued_event& pei, const vconfig& pcfg)

namespace game_events {

	static bool matches_special_filter(const config &cfg, const vconfig& filter);

	static bool internal_conditional_passed(const vconfig& cond, bool& backwards_compat)
	{
		static std::vector<std::pair<int,int> > default_counts = utils::parse_ranges("1-99999");

		// If the if statement requires we have a certain unit,
		// then check for that.
		const vconfig::child_list& have_unit = cond.get_children("have_unit");
		backwards_compat = backwards_compat && have_unit.empty();
		for(vconfig::child_list::const_iterator u = have_unit.begin(); u != have_unit.end(); ++u) {
			if(resources::units == NULL)
				return false;
			std::vector<std::pair<int,int> > counts = (*u).has_attribute("count")
				? utils::parse_ranges((*u)["count"]) : default_counts;
			int match_count = 0;
			for (unit_map::const_iterator itor = resources::units->begin(); itor != resources::units->end(); ++ itor)
			{
				unit& i = *itor;
				if(i.hitpoints() > 0 && unit_matches_filter(i, *u)) {
					++match_count;
					if(counts == default_counts) {
						// by default a single match is enough, so avoid extra work
						break;
					}
				}
			}
			if(!in_ranges(match_count, counts)) {
				return false;
			}
		}

		// If the if statement requires we have a certain location,
		// then check for that.
		const vconfig::child_list& have_location = cond.get_children("have_location");
		backwards_compat = backwards_compat && have_location.empty();
		for(vconfig::child_list::const_iterator v = have_location.begin(); v != have_location.end(); ++v) {
			std::set<map_location> res;
			terrain_filter(*v, *resources::units).get_locations(res);

			std::vector<std::pair<int,int> > counts = (*v).has_attribute("count")
				? utils::parse_ranges((*v)["count"]) : default_counts;
			if(!in_ranges<int>(res.size(), counts)) {
				return false;
			}
		}

		// Check against each variable statement,
		// to see if the variable matches the conditions or not.
		const vconfig::child_list& variables = cond.get_children("variable");
		backwards_compat = backwards_compat && variables.empty();

		BOOST_FOREACH (const vconfig &values, variables)
		{
			const std::string name = values["name"];
			config::attribute_value value;
			if (name != "random") {
				value = resources::state_of_game->get_variable_const(name);
			} else {
				int ran_num = get_random();
				ran_num = ran_num % 100;
				value = ran_num;
			}
			std::string str_value = value.str();
			double num_value = value.to_double();

#define TEST_ATTR(name, test) do { \
			if (values.has_attribute(name)) { \
				config::attribute_value attr = values[name]; \
				if (!(test)) return false; \
			} \
			} while (0)

#define TEST_STR_ATTR(name, test) do { \
			if (values.has_attribute(name)) { \
				std::string attr_str = values[name]; \
				if (!(test)) return false; \
			} \
			} while (0)

#define TEST_NUM_ATTR(name, test) do { \
			if (values.has_attribute(name)) { \
				double attr_num = values[name].to_double(); \
				if (!(test)) return false; \
			} \
			} while (0)

			TEST_STR_ATTR("equals",                str_value == attr_str);
			TEST_STR_ATTR("not_equals",            str_value != attr_str);
			TEST_NUM_ATTR("numerical_equals",      num_value == attr_num);
			TEST_NUM_ATTR("numerical_not_equals",  num_value != attr_num);
			TEST_NUM_ATTR("greater_than",          num_value >  attr_num);
			TEST_NUM_ATTR("less_than",             num_value <  attr_num);
			TEST_NUM_ATTR("greater_than_equal_to", num_value >= attr_num);
			TEST_NUM_ATTR("less_than_equal_to",    num_value <= attr_num);
			TEST_ATTR("boolean_equals",     value.to_bool() == attr.to_bool());
			TEST_ATTR("boolean_not_equals", value.to_bool() != attr.to_bool());
			TEST_STR_ATTR("contains", value.str().find(attr_str) != std::string::npos);

#undef TEST_ATTR
#undef TEST_STR_ATTR
#undef TEST_NUM_ATTR
		}
		return true;
	}

	bool conditional_passed(const vconfig& cond, bool backwards_compat)
	{
		bool allow_backwards_compat = backwards_compat = backwards_compat &&
			cond["backwards_compat"].to_bool(true);
		bool matches = internal_conditional_passed(cond, allow_backwards_compat);

		// Handle [and], [or], and [not] with in-order precedence
		int or_count = 0;
		vconfig::all_children_iterator cond_i = cond.ordered_begin();
		vconfig::all_children_iterator cond_end = cond.ordered_end();
		while(cond_i != cond_end)
		{
			const std::string& cond_name = cond_i.get_key();
			const vconfig& cond_filter = cond_i.get_child();

			// Handle [and]
			if(cond_name == "and")
			{
				matches = matches && conditional_passed(cond_filter, backwards_compat);
				backwards_compat = false;
			}
			// Handle [or]
			else if(cond_name == "or")
			{
				matches = matches || conditional_passed(cond_filter, backwards_compat);
				++or_count;
			}
			// Handle [not]
			else if(cond_name == "not")
			{
				matches = matches && !conditional_passed(cond_filter, backwards_compat);
				backwards_compat = false;
			}
			++cond_i;
		}
		// Check for deprecated [or] syntax
		if(matches && or_count > 1 && allow_backwards_compat)
		{
			///@deprecated r18803 [or] syntax
			lg::wml_error << "possible deprecated [or] syntax: now forcing re-interpretation\n";
			/**
			 * @todo For now we will re-interpret it according to the old
			 * rules, but this should be later to prevent re-interpretation
			 * errors.
			 */
			const vconfig::child_list& orcfgs = cond.get_children("or");
			for(unsigned int i=0; i < orcfgs.size(); ++i) {
				if(conditional_passed(orcfgs[i])) {
					return true;
				}
			}
			return false;
		}
		return matches;
	}

	void wml_expand_if(config& cfg)
	{
		// [side]下可能有多个[if]
		BOOST_FOREACH (config &if_cfg, cfg.child_range("if")) { 
			const std::string type = conditional_passed(vconfig(if_cfg)) ? "then" : "else";

			// If the if statement passed, then execute all 'then' statements,
			// otherwise execute 'else' statements
			BOOST_FOREACH (config& cmd, if_cfg.child_range(type)) { 
				// [then]/[else]下可能还有[if]
				if (cmd.child_count("if")) {
					wml_expand_if(cmd);
				}
				cfg.append(cmd);
			}
		}
		cfg.clear_children("if");
	}

	void handle_wml_log_message(const config& cfg)
	{
		const std::string& logger = cfg["logger"];
		const std::string& msg = cfg["message"];

		put_wml_message(logger,msg);
	}

	void handle_deprecated_message(const config& cfg)
	{
		// Note: no need to translate the string, since only used for deprecated things.
		const std::string& message = cfg["message"];
		lg::wml_error << message << '\n';
	}

	std::vector<int> get_sides_vector(const vconfig& cfg, const bool only_ssf, const bool only_side)
	{
		if(only_ssf) {
			side_filter filter(cfg);
			return filter.get_teams();
		}

		const config::attribute_value sides = cfg["side"];
		const vconfig &ssf = cfg.child("filter_side");

		if (!ssf.null() && !only_side) {
			if(!sides.empty()) { WRN_NG << "ignoring duplicate side filter information (inline side=)\n"; }
			side_filter filter(ssf);
			return filter.get_teams();
		}

		if (sides.blank()) {
			if(only_side) put_wml_message("error", "empty side= is deprecated, use side=1");
			//To deprecate the current default (side=1), require one of the currently two ways
			//of specifying a side - putting inline side= or [filter_side].
			else put_wml_message("error", "empty side= and no [filter_side] is deprecated, use either side=1 or [filter_side]");
			std::vector<int> result;
			result.push_back(1); // we make sure the current default is maintained
			return result;
		}
		// uncomment if the decision will be made to make [filter_side] the only & final syntax for specifying sides
		// put_wml_message("error","specifying side via inline side= is deprecated, use [filter_side] ");
		side_filter filter(sides.str());
		return filter.get_teams();
	}

} // end namespace game_events (1)

namespace {

	std::set<std::string> used_items;

} // end anonymous namespace (2)

static bool events_init() { return resources::screen != 0; }

namespace {

	std::deque<game_events::queued_event> events_queue;


} // end anonymous namespace (3)

static map_location cfg_to_loc(const vconfig& cfg,int defaultx = 0, int defaulty = 0)
{
	int x = cfg["x"].to_int(defaultx) - 1;
	int y = cfg["y"].to_int(defaulty) - 1;

	return map_location(x, y);
}

namespace {

#define MAX_EVENT_HANLDERS	1024

	// std::vector<game_events::event_handler> event_handlers;
	game_events::event_handler* event_handlers[MAX_EVENT_HANLDERS];
	size_t event_vsize = 0;

} // end anonymous namespace (4)

static void toggle_shroud(const bool remove, const vconfig& cfg)
{
	int side_num = cfg["side"].to_int(1);
	const size_t index = side_num-1;

	if (index < resources::teams->size())
	{
		team &t = (*resources::teams)[index];
		std::set<map_location> locs;
		terrain_filter filter(cfg, *resources::units);
		filter.restrict_size(game_config::max_loop);
		filter.get_locations(locs, true);

		BOOST_FOREACH (map_location const &loc, locs)
		{
			if (remove) {
				t.clear_shroud(loc);
			} else {
				t.place_shroud(loc);
			}
		}
	}

	resources::screen->labels().recalculate_shroud();
	resources::screen->recalculate_minimap();
	resources::screen->invalidate_all();
}

WML_HANDLER_FUNCTION(remove_shroud, /*event_info*/, cfg)
{
	toggle_shroud(true,cfg);
}

WML_HANDLER_FUNCTION(place_shroud, /*event_info*/,cfg)
{
	toggle_shroud(false,cfg );
}

WML_HANDLER_FUNCTION(teleport, event_info, cfg)
{
	unit_map::iterator u = resources::units->find(event_info.loc1);

	// Search for a valid unit filter, and if we have one, look for the matching unit
	const vconfig filter = cfg.child("filter");
	if(!filter.null()) {
		for (u = resources::units->begin(); u != resources::units->end(); ++u){
			if(game_events::unit_matches_filter(*u, filter))
				break;
		}
	}

	if (u == resources::units->end()) return;

	// We have found a unit that matches the filter
	const map_location dst = cfg_to_loc(cfg);
	if (dst == u->get_location() || !resources::game_map->on_board(dst)) return;

	const unit *pass_check = &*u;
	if (cfg["ignore_passability"].to_bool())
		pass_check = NULL;
	const map_location vacant_dst = pathfind::find_vacant_tile(*resources::game_map, *resources::teams, *resources::units, dst, pass_check);
	if (!resources::game_map->on_board(vacant_dst)) return;

	int side = u->side();
	if (cfg["clear_shroud"].to_bool(true)) {
		clear_shroud(side);
	}

	map_location src_loc = u->get_location();

	std::vector<map_location> teleport_path;
	teleport_path.push_back(src_loc);
	teleport_path.push_back(vacant_dst);
	bool animate = cfg["animate"].to_bool();
	unit_display::move_unit(teleport_path, *u, *resources::teams, animate);

	resources::units->move(src_loc, vacant_dst);
	unit::clear_status_caches();

	u = resources::units->find(vacant_dst);
	u->set_standing();

	if (resources::game_map->is_village(vacant_dst)) {
		get_village(vacant_dst, side);
	}

	resources::screen->invalidate_unit_after_move(src_loc, dst);

	resources::screen->draw();
}

WML_HANDLER_FUNCTION(volume, /*event_info*/, cfg)
{

	int vol;
	double rel;
	std::string music = cfg["music"];
	std::string sound = cfg["sound"];

	if(!music.empty()) {
		vol = preferences::music_volume();
		rel = atof(music.c_str());
		if (rel >= 0.0 && rel < 100.0) {
			vol = static_cast<int>(rel*vol/100.0);
		}
		sound::set_music_volume(vol);
	}

	if(!sound.empty()) {
		vol = preferences::sound_volume();
		rel = atof(sound.c_str());
		if (rel >= 0.0 && rel < 100.0) {
			vol = static_cast<int>(rel*vol/100.0);
		}
		sound::set_sound_volume(vol);
	}

}

static void color_adjust(const vconfig& cfg)
{
	game_display &screen = *resources::screen;
	screen.adjust_colors(cfg["red"], cfg["green"], cfg["blue"]);
	screen.invalidate_all();
	screen.draw(true,true);
}

WML_HANDLER_FUNCTION(color_adjust, /*event_info*/, cfg)
{
	color_adjust(cfg);
}

//Function handling old name
///@deprecated 1.9.2 'colour_adjust' instead of 'color_adjust'
WML_HANDLER_FUNCTION(colour_adjust, /*event_info*/, cfg)
{
	color_adjust(cfg);
}

WML_HANDLER_FUNCTION(scroll, /*event_info*/, cfg)
{
	game_display &screen = *resources::screen;
	screen.scroll(cfg["x"], cfg["y"]);
	screen.draw(true,true);
}

// store time of day config in a WML variable; useful for those who
// are too lazy to calculate the corresponding time of day for a given turn,
// or if the turn / time-of-day sequence mutates in a scenario.
WML_HANDLER_FUNCTION(store_time_of_day, /*event_info*/, cfg)
{
	const map_location loc = cfg_to_loc(cfg, -999, -999);
	int turn = cfg["turn"];
	// using 0 will use the current turn
	const time_of_day& tod = resources::tod_manager->get_time_of_day(loc,turn);

	std::string variable = cfg["variable"];
	if(variable.empty()) {
		variable = "time_of_day";
	}

	variable_info store(variable, true, variable_info::TYPE_CONTAINER);

	config tod_cfg;
	tod.write(tod_cfg);

	(*store.vars).add_child(store.key, tod_cfg);
}

WML_HANDLER_FUNCTION(inspect, /*event_info*/, cfg)
{
	if (game_config::debug) {
		gui2::tgamestate_inspector inspect_dialog(cfg);
		inspect_dialog.show(resources::screen->video());
	}
}

WML_HANDLER_FUNCTION(modify_unit2, event_info, cfg)
{
	unit_map& units = *resources::units;
	hero_map& heros = *resources::heros;

	int number = cfg["hero"].to_int();
	hero& h = heros[number];
	unit* holder = units.find_unit(h);
	if (!holder) {
		return;
	}

	BOOST_FOREACH (const config &effect, cfg.get_config().child_range("effect")) {
		int tag = apply_to_tag::find(effect["apply_to"].str());
		if (tag == apply_to_tag::NONE) continue;
		holder->add_modification_internal(tag, effect, true);
	}
	return;
}

WML_HANDLER_FUNCTION(modify_ai, /*event_info*/, cfg)
{
	int side_num = cfg["side"].to_int(1);
	if (side_num==0) {
		return;
	}
	// ai::manager::modify_active_ai_for_side(side_num,cfg.get_parsed_config());
}

WML_HANDLER_FUNCTION(modify_side, /*event_info*/, cfg)
{
	std::vector<team>& teams = *resources::teams;
	hero_map& heros = *resources::heros;

	int side_num = cfg["side"].to_int();
	const size_t team_index = side_num - 1;
	if (team_index < 0 || team_index >= teams.size()) {
		return;
	}
	team& current_team = teams[team_index];

	std::string agree = cfg["agree"];
	std::string terminate = cfg["terminate"];
	std::string controller = cfg["controller"];
	std::string recruit_str = cfg["not_recruit"];
	std::string shroud_data = cfg["shroud_data"];
	const config& parsed = cfg.get_parsed_config();
	const config::const_child_itors &ai = parsed.child_range("ai");

	std::vector<std::string> vstr;

	/**
	 * @todo also allow client to modify a side's color if it is possible
	 * to change it on the fly without causing visual glitches
	 */
	if (!agree.empty()) {
		vstr = utils::split(agree);
		for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
			int side = lexical_cast_default<int>(*i);
			current_team.set_ally(side, true, true);
			teams[side - 1].set_ally(side_num, true, true);
		}
	}
	if (!terminate.empty()) {
		vstr = utils::split(terminate);
		for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
			int side = lexical_cast_default<int>(*i);
			current_team.set_ally(side, false, true, true);
			teams[side - 1].set_ally(side_num, false, true, true);
		}
	}

	// leader
	int leader = cfg["leader"].to_int(HEROS_INVALID_NUMBER);
	if (leader != HEROS_INVALID_NUMBER && leader != current_team.leader()->number_) {
		current_team.set_leader(&heros[leader]);
		current_team.readjust_all_unit();
	}

	// income
	int income = cfg["income"].to_int(-1);
	if (income != -1) {
		current_team.set_base_income(income + game_config::base_income);
	}
	// Modify total gold
	int gold = cfg["gold"].to_int(-1);
	if (gold) {
		current_team.set_gold(gold);
	}
	// technology
	std::string str = cfg["technology"].str();
	if (!str.empty()) {
		vstr = utils::split(str);
		std::vector<const ttechnology*>& holden = current_team.holded_technologies();
		bool adjust_unit = false;
		for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
			const ttechnology& t = unit_types.technology(*it);
			if (std::find(holden.begin(), holden.end(), &t) != holden.end()) {
				continue;
			}
			holden.push_back(&t);
			// apply this technology
			const std::vector<const ttechnology*>& parts = t.parts();
			for (std::vector<const ttechnology*>::const_iterator it = parts.begin(); it != parts.end(); ++ it) {
				const ttechnology& t2 = **it;
				if (t2.occasion() == ttechnology::FINISH) {
					current_team.add_modification_internal(t2.apply_to(), t2.effect_cfg());
				} else {
					adjust_unit = true;
				}
			}
		}
		if (adjust_unit) {
			current_team.readjust_all_unit();
		}
		if (std::find(holden.begin(), holden.end(), current_team.ing_technology()) != holden.end()) {
			current_team.reselect_ing_technology();
		}
	}

	// Set controller
	if (!controller.empty()) {
		teams[team_index].change_controller(controller);
	}
	// Set shroud
	config::attribute_value shroud = cfg["shroud"];
	if (!shroud.empty()) {
		teams[team_index].set_shroud(shroud.to_bool(true));
	}
	// Merge shroud data
	if (!shroud_data.empty()) {
		teams[team_index].merge_shroud_map_data(shroud_data);
	}
	// Set fog
	config::attribute_value fog = cfg["fog"];
	if (!fog.empty()) {
		teams[team_index].set_fog(fog.to_bool(true));
	}
	// Set income per village
	config::attribute_value village_gold = cfg["village_gold"];
	if (!village_gold.empty()) {
		teams[team_index].set_village_gold(village_gold);
	}
	// Override AI parameters
	if (ai.first != ai.second) {
		ai::manager::modify_active_ai_config_old_for_side(side_num,ai);
	}
	// Add shared view to current team
	config::attribute_value share_view = cfg["share_view"];
	if (!share_view.empty()){
		teams[team_index].set_share_view(share_view.to_bool(true));
		team::clear_caches();
		resources::screen->recalculate_minimap();
		resources::screen->invalidate_all();
	}
	// Add shared maps to current team
	// IMPORTANT: this MUST happen *after* share_view is changed
	config::attribute_value share_maps = cfg["share_maps"];
	if (!share_maps.empty()){
		teams[team_index].set_share_maps(share_maps.to_bool(true));
		team::clear_caches();
		resources::screen->recalculate_minimap();
		resources::screen->invalidate_all();
	}
}

WML_HANDLER_FUNCTION(modify_city, /*event_info*/, cfg)
{
	std::vector<team>& teams = *resources::teams;
	unit_map& units = *resources::units;
	hero_map& heros = *resources::heros;

	int city_number = cfg["city"].to_int(HEROS_INVALID_NUMBER);
	unit* u = units.find_unit(heros[city_number]);
	if (!u || !u->is_city()) {
		return;
	}
	artifical& city = *unit_2_artifical(u);
	team& current_team = teams[city.side() - 1];
	std::stringstream err;

	std::vector<std::string> vstr;
	std::string service = cfg["service"];
	if (!service.empty()) {
		vstr = utils::split(service);
		for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
			int number = lexical_cast_default<int>(*i);
			hero& h = heros[number];
			unit* u = units.find_unit(h);
			if (u) {
				err << "wml_func_modify_city, " << h.name() << " has been serviced, must not service again!";
				VALIDATE(!u, err.str());
			}
			city.finish_into(h, hero_status_moving);
		}
	}
	int soldiers = cfg["soldiers"].to_int(-1);
	if (soldiers > 0) {
		city.set_soldiers(soldiers);
	}
}

WML_HANDLER_FUNCTION(store_side, /*event_info*/, cfg)
{
	game_state *state_of_game = resources::state_of_game;
	std::vector<team> &teams = *resources::teams;

	std::string var_name = cfg["variable"];
	if (var_name.empty()) var_name = "side";

	int side_num = cfg["side"].to_int(1);
	size_t team_index = side_num - 1;
	if (team_index >= teams.size()) return;

	config side_data;
	teams[team_index].write(side_data);
	state_of_game->get_variable(var_name+".controller") = side_data["controller"];
	state_of_game->get_variable(var_name+".not_recruit") = side_data["not_recruit"];
	state_of_game->get_variable(var_name+".fog") = side_data["fog"];
	state_of_game->get_variable(var_name+".shroud") = side_data["shroud"];
	state_of_game->get_variable(var_name+".hidden") = side_data["hidden"];

	state_of_game->get_variable(var_name+".income") = teams[team_index].total_income();
	state_of_game->get_variable(var_name+".village_gold") = teams[team_index].village_gold();
	state_of_game->get_variable(var_name+".name") = teams[team_index].name();
	state_of_game->get_variable(var_name+".team_name") = teams[team_index].team_name();
	state_of_game->get_variable(var_name+".color") = teams[team_index].map_color_to();

	state_of_game->get_variable(var_name+".gold") = teams[team_index].gold();
}

WML_HANDLER_FUNCTION(modify_turns, /*event_info*/, cfg)
{
	config::attribute_value value = cfg["value"];
	std::string add = cfg["add"];
	config::attribute_value current = cfg["current"];
	tod_manager& tod_man = *resources::tod_manager;
	if(!add.empty()) {
		tod_man.modify_turns(add);
	} else if(!value.empty()) {
		tod_man.set_number_of_turns(value.to_int(-1));
	}
	// change current turn only after applying mods
	if(!current.empty()) {
		const unsigned int current_turn_number = tod_man.turn();
		int new_turn_number = current.to_int(current_turn_number);
		const unsigned int new_turn_number_u = static_cast<unsigned int>(new_turn_number);
		if(new_turn_number_u < current_turn_number || (new_turn_number > tod_man.number_of_turns() && tod_man.number_of_turns() != -1)) {
			ERR_NG << "attempted to change current turn number to one out of range (" << new_turn_number << ") or less than current turn\n";
		} else if(new_turn_number_u != current_turn_number) {
			tod_man.set_turn(new_turn_number_u);
			resources::state_of_game->get_variable("turn_number") = new_turn_number;
			resources::screen->new_turn();
		}
	}
}

WML_HANDLER_FUNCTION(set_variable, /*event_info*/, cfg)
{
	game_state *state_of_game = resources::state_of_game;

	const std::string name = cfg["name"];
	config::attribute_value &var = state_of_game->get_variable(name);

	config::attribute_value literal = cfg.get_config()["literal"]; // no $var substitution
	if (!literal.blank()) {
		var = literal;
	}

	config::attribute_value value = cfg["value"];
	if (!value.blank()) {
		var = value;
	}

	config::attribute_value format = cfg["format"];
	if (!format.blank()) {
		///@deprecated 1.9.2 Usage of 'format' instead of 'value'
		lg::wml_error << "Usage of 'format' is deprecated, use 'value' instead, "
			"support will be removed in 1.9.2.\n";
		var = format;
	}

	const std::string to_variable = cfg["to_variable"];
	if(to_variable.empty() == false) {
		var = state_of_game->get_variable(to_variable);
	}

	config::attribute_value add = cfg["add"];
	if (!add.empty()) {
		var = var.to_double() + add.to_double();
	}

	config::attribute_value sub = cfg["sub"];
	if (!sub.empty()) {
		var = var.to_double() - sub.to_double();
	}

	config::attribute_value multiply = cfg["multiply"];
	if (!multiply.empty()) {
		var = var.to_double() * multiply.to_double();
	}

	config::attribute_value divide = cfg["divide"];
	if (!divide.empty()) {
		if (divide.to_double() == 0) {
			ERR_NG << "division by zero on variable " << name << "\n";
			return;
		}
		var = var.to_double() / divide.to_double();
	}

	config::attribute_value modulo = cfg["modulo"];
	if (!modulo.empty()) {
		if (modulo.to_double() == 0) {
			ERR_NG << "division by zero on variable " << name << "\n";
			return;
		}
		var = std::fmod(var.to_double(), modulo.to_double());
	}

	config::attribute_value round_val = cfg["round"];
	if (!round_val.empty()) {
		double value = var.to_double();
		if (round_val == "ceil") {
			var = int(std::ceil(value));
		} else if (round_val == "floor") {
			var = int(std::floor(value));
		} else {
			// We assume the value is an integer.
			// Any non-numerical values will be interpreted as 0
			// Which is probably what was intended anyway
			int decimals = round_val.to_int();
			value *= std::pow(10.0, decimals); //add $decimals zeroes
			value = round_portable(value); // round() isn't implemented everywhere
			value *= std::pow(10.0, -decimals); //and remove them
			var = value;
		}
	}

	config::attribute_value ipart = cfg["ipart"];
	if (!ipart.empty()) {
		double result;
		std::modf(ipart.to_double(), &result);
		var = int(result);
	}

	config::attribute_value fpart = cfg["fpart"];
	if (!fpart.empty()) {
		double ignore;
		var = std::modf(fpart.to_double(), &ignore);
	}

	config::attribute_value string_length_target = cfg["string_length"];
	if (!string_length_target.blank()) {
		var = int(string_length_target.str().size());
	}

	// Note: maybe we add more options later, eg. strftime formatting.
	// For now make the stamp mandatory.
	const std::string time = cfg["time"];
	if(time == "stamp") {
		var = int(SDL_GetTicks());
	}

	// Random generation works as follows:
	// rand=[comma delimited list]
	// Each element in the list will be considered a separate choice,
	// unless it contains "..". In this case, it must be a numerical
	// range (i.e. -1..-10, 0..100, -10..10, etc).
	const std::string random = cfg["random"];
	std::string rand = cfg["rand"];
	if(random.empty() == false) {
		///@deprecated 1.9.2 Usage of 'random' instead or 'rand'
		lg::wml_error << "Usage of 'random' is deprecated, use 'rand' instead, "
			"support will be removed in 1.9.2.\n";
		if(rand.empty()) {
			rand = random;
		}
	}

	// The new random generator, the logic is a copy paste of the old random.
	if(rand.empty() == false) {
		assert(state_of_game);

		std::string random_value;

		std::string word;
		std::vector<std::string> words;
		std::vector<std::pair<long,long> > ranges;
		int num_choices = 0;
		std::string::size_type pos = 0, pos2 = std::string::npos;
		std::stringstream ss(std::stringstream::in|std::stringstream::out);
		while (pos2 != rand.length()) {
			pos = pos2+1;
			pos2 = rand.find(",", pos);

			if (pos2 == std::string::npos)
				pos2 = rand.length();

			word = rand.substr(pos, pos2-pos);
			words.push_back(word);
			std::string::size_type tmp = word.find("..");


			if (tmp == std::string::npos) {
				// Treat this element as a string
				ranges.push_back(std::pair<int, int>(0,0));
				num_choices += 1;
			}
			else {
				// Treat as a numerical range
				const std::string first = word.substr(0, tmp);
				const std::string second = word.substr(tmp+2,
						rand.length());

				long low, high;
				ss << first + " " + second;
				ss >> low;
				ss >> high;
				ss.clear();

				if (low > high) {
					std::swap(low, high);
				}
				ranges.push_back(std::pair<long, long>(low,high));
				num_choices += (high - low) + 1;

				// Make 0..0 ranges work
				if (high == 0 && low == 0) {
					words.pop_back();
					words.push_back("0");
				}
			}
		}

		int choice = state_of_game->rng().get_next_random() % num_choices;
		int tmp = 0;
		for(size_t i = 0; i < ranges.size(); ++i) {
			tmp += (ranges[i].second - ranges[i].first) + 1;
			if (tmp > choice) {
				if (ranges[i].first == 0 && ranges[i].second == 0) {
					random_value = words[i];
				}
				else {
					tmp = (ranges[i].second - (tmp - choice)) + 1;
					ss << tmp;
					ss >> random_value;
				}
				break;
			}
		}

		var = random_value;
	}


	const vconfig::child_list join_elements = cfg.get_children("join");
	if(!join_elements.empty())
	{
		const vconfig join_element=join_elements.front();

		std::string array_name=join_element["variable"];
		std::string separator=join_element["separator"];
		std::string key_name=join_element["key"];

		if(key_name.empty())
		{
			key_name="value";
		}

		bool remove_empty = join_element["remove_empty"].to_bool();

		std::string joined_string;

		variable_info vi(array_name, true, variable_info::TYPE_ARRAY);
		bool first = true;
		BOOST_FOREACH (const config &cfg, vi.as_array())
		{
			std::string current_string = cfg[key_name];
			if (remove_empty && current_string.empty()) continue;
			if (first) first = false;
			else joined_string += separator;
			joined_string += current_string;
		}

		var=joined_string;
	}

}


WML_HANDLER_FUNCTION(set_variables, /*event_info*/, cfg)
{
	const t_string& name = cfg["name"];
	variable_info dest(name, true, variable_info::TYPE_CONTAINER);

	std::string mode = cfg["mode"]; // replace, append, merge, or insert
	if(mode == "extend") {
		mode = "append";
	} else if(mode != "append" && mode != "merge") {
		if(mode == "insert") {
			size_t child_count = dest.vars->child_count(dest.key);
			if(dest.index >= child_count) {
				while(dest.index >= ++child_count) {
					//inserting past the end requires empty data
					dest.vars->append(config(dest.key));
				}
				//inserting at the end is handled by an append
				mode = "append";
			}
		} else {
			mode = "replace";
		}
	}

	const vconfig::child_list values = cfg.get_children("value");
	const vconfig::child_list literals = cfg.get_children("literal");
	const vconfig::child_list split_elements = cfg.get_children("split");

	config data;

	if(cfg.has_attribute("to_variable"))
	{
		variable_info tovar(cfg["to_variable"], false, variable_info::TYPE_CONTAINER);
		if(tovar.is_valid) {
			if(tovar.explicit_index) {
				data.add_child(dest.key, tovar.as_container());
			} else {
				variable_info::array_range range = tovar.as_array();
				while(range.first != range.second)
				{
					data.add_child(dest.key, *range.first++);
				}
			}
		}
	} else if(!values.empty()) {
		for(vconfig::child_list::const_iterator i=values.begin(); i!=values.end(); ++i)
		{
			data.add_child(dest.key, (*i).get_parsed_config());
		}
	} else if(!literals.empty()) {
		for(vconfig::child_list::const_iterator i=literals.begin(); i!=literals.end(); ++i)
		{
			data.add_child(dest.key, i->get_config());
		}
	} else if(!split_elements.empty()) {
		const vconfig split_element=split_elements.front();

		std::string split_string=split_element["list"];
		std::string separator_string=split_element["separator"];
		std::string key_name=split_element["key"];
		if(key_name.empty())
		{
			key_name="value";
		}

		bool remove_empty = split_element["remove_empty"].to_bool();

		char* separator = separator_string.empty() ? NULL : &separator_string[0];

		std::vector<std::string> split_vector;

		//if no separator is specified, explode the string
		if(separator == NULL)
		{
			for(std::string::iterator i=split_string.begin(); i!=split_string.end(); ++i)
			{
				split_vector.push_back(std::string(1, *i));
			}
		}
		else {
			split_vector=utils::split(split_string, *separator, remove_empty ? utils::REMOVE_EMPTY | utils::STRIP_SPACES : utils::STRIP_SPACES);
		}

		for(std::vector<std::string>::iterator i=split_vector.begin(); i!=split_vector.end(); ++i)
		{
			data.add_child(dest.key)[key_name]=*i;
		}
	}
	if(mode == "replace")
	{
		if(dest.explicit_index) {
			dest.vars->remove_child(dest.key, dest.index);
		} else {
			dest.vars->clear_children(dest.key);
		}
	}
	if(!data.empty())
	{
		if(mode == "merge")
		{
			if(dest.explicit_index) {
				// merging multiple children into a single explicit index
				// requires that they first be merged with each other
				data.merge_children(dest.key);
				dest.as_container().merge_with(data.child(dest.key));
			} else {
				dest.vars->merge_with(data);
			}
		} else if(mode == "insert" || dest.explicit_index) {
			BOOST_FOREACH (const config &child, data.child_range(dest.key))
			{
				dest.vars->add_child_at(dest.key, child, dest.index++);
			}
		} else {
			dest.vars->append(data);
		}
	}
}

WML_HANDLER_FUNCTION(sound_source, /*event_info*/, cfg)
{
	soundsource::sourcespec spec(cfg.get_parsed_config());
	resources::soundsources->add(spec);
}

WML_HANDLER_FUNCTION(remove_sound_source, /*event_info*/, cfg)
{
	resources::soundsources->remove(cfg["id"]);
}

void change_terrain(const map_location &loc, const t_translation::t_terrain &t,
	gamemap::tmerge_mode mode, bool replace_if_failed)
{
	gamemap *game_map = resources::game_map;

	t_translation::t_terrain
		old_t = game_map->get_terrain(loc),
		new_t = game_map->merge_terrains(old_t, t, mode, replace_if_failed);
	if (new_t == t_translation::NONE_TERRAIN) return;
	preferences::encountered_terrains().insert(new_t);

	if (game_map->is_village(old_t) && !game_map->is_village(new_t)) {
		int owner = village_owner(loc, *resources::teams);
	}

	game_map->set_terrain(loc, new_t);
	screen_needs_rebuild = true;

	BOOST_FOREACH (const t_translation::t_terrain &ut, game_map->underlying_union_terrain(loc)) {
		preferences::encountered_terrains().insert(ut);
	}
}

// Creating a mask of the terrain
WML_HANDLER_FUNCTION(terrain_mask, /*event_info*/, cfg)
{
	map_location loc = cfg_to_loc(cfg, 1, 1);

	gamemap mask(*resources::game_map);

	try {
		mask.read(cfg["mask"]);
	} catch(incorrect_map_format_error&) {
		ERR_NG << "terrain mask is in the incorrect format, and couldn't be applied\n";
		return;
	} catch(twml_exception& e) {
		e.show(*resources::screen);
		return;
	}
	bool border = cfg["border"].to_bool();
	resources::game_map->overlay(mask, cfg.get_parsed_config(), loc.x, loc.y, border);
	screen_needs_rebuild = true;
}

static bool try_add_unit_to_recall_list(const map_location& loc, const unit& u)
{
	return true;
}

// If we should spawn a new unit on the map somewhere
WML_HANDLER_FUNCTION(unit, /*event_info*/, cfg)
{
	unit_map& units = *resources::units;
	
	config parsed_cfg = cfg.get_parsed_config();

	unit new_unit(units, *resources::heros, *resources::teams, parsed_cfg, true, resources::state_of_game);

	// set human/menual
	if (new_unit.side() == rpg::h->side_ + 1) {
		if (rpg::stratum == hero_stratum_leader) {
			new_unit.set_human(true);
		} else if (rpg::stratum == hero_stratum_mayor) {
			if (new_unit.cityno() == rpg::h->city_) {
				new_unit.set_human(true);
			}
		}
	}

	config::attribute_value to_variable = cfg["to_variable"];
	if (!to_variable.blank())
	{
		parsed_cfg.remove_attribute("to_variable");
		config &var = resources::state_of_game->get_variable_cfg(to_variable);
		var.clear();
		new_unit.write(var);
		if (const config::attribute_value *v = parsed_cfg.get("x")) var["x"] = *v;
		if (const config::attribute_value *v = parsed_cfg.get("y")) var["y"] = *v;
		return;
	}
	preferences::encountered_units().insert(new_unit.type_id());
	map_location loc = cfg_to_loc(cfg);

	if (loc.x < 0) {
		loc.x = 0;
	} else if (loc.x >= resources::game_map->w()) {
		loc.x = resources::game_map->w() - 1;
	}
	if (loc.y < 0) {
		loc.y = 0;
	} else if (loc.y >= resources::game_map->h()) {
		loc.y = resources::game_map->h() - 1;
	}

	loc = pathfind::find_vacant_tile(*resources::game_map, *resources::teams, *resources::units, loc, &new_unit);

	const bool show = resources::screen && !resources::screen->fogged(loc);
	const bool animate = show && utils::string_bool(parsed_cfg["animate"], false);

	unit_map::iterator u_itor = units.find(loc);
	if (u_itor.valid()) {
		units.erase(&*u_itor);
	}
	units.add(loc, &new_unit);
	if (resources::game_map->is_village(loc)) {
		get_village(loc, new_unit.side());
	}

	resources::screen->invalidate(loc);

	rect_of_hexes& draw_area = resources::screen->draw_area();
	if (animate) {
		unit_display::unit_recruited(loc);
	} else if (show && point_in_rect_of_hexes(loc.x, loc.y, draw_area)) {
		resources::screen->draw();
	}
}

WML_HANDLER_FUNCTION(join, /*event_info*/, cfg)
{
	unit_map& units = *resources::units;
	hero_map& heros = *resources::heros;
	game_display& gui = *resources::screen;
	const config& parsed_cfg = cfg.get_parsed_config();

	std::string master_hero = parsed_cfg["to"].str();
	std::string join_hero = parsed_cfg["join"].str();

	if (master_hero.empty() || join_hero.empty()) {
		return;
	}
	hero& master = heros[atoi(master_hero.c_str())];
	unit* master_troop = units.find_unit(master);
	if (!master_troop || master_troop->third().valid()) {
		return;
	}
	std::vector<hero*> captains;
	captains.push_back(&master);
	if (master_troop->second().valid()) {
		captains.push_back(&master_troop->second());
	}
	captains.push_back(&heros[atoi(join_hero.c_str())]);

	master_troop->replace_captains(captains);

	gui.invalidate(master_troop->get_location());
	gui.draw();
}

WML_HANDLER_FUNCTION(sideheros, /*event_info*/, cfg)
{
	std::vector<team>& teams = *resources::teams;
	hero_map& heros = *resources::heros;
	unit_map& units = *resources::units;
	play_controller& controller = *resources::controller;
	const config& parsed_cfg = cfg.get_parsed_config();
	int side = parsed_cfg["side"].to_int(0);

	if (side <=0 || side > (int)teams.size()) {
		return;
	}
	team& selected_team = teams[side - 1];
	std::string message;
	artifical* selected_city = NULL;

	const std::vector<std::string> parsed_heros = utils::split(parsed_cfg["heros"]);
	std::set<int> dealing_heros;
	std::set<int>::iterator dealing_heros_itor;
	for (std::vector<std::string>::const_iterator itor = parsed_heros.begin(); itor != parsed_heros.end(); ++ itor) {
		int h = lexical_cast_default<int>(*itor);
		if (h >= 0 && h < (int)heros.size()) {
			dealing_heros.insert(h);
		}
	}
	const std::vector<artifical*>& holded_cities = selected_team.holded_cities();
	for (std::vector<artifical*>::const_iterator itor = holded_cities.begin(); itor != holded_cities.end(); ++ itor) {
		artifical& city = **itor;
		std::vector<hero*> leave_heros;
		std::vector<hero*> captains;

		// field troops
		std::vector<unit*> field_troops = city.field_troops();
		for (std::vector<unit*>::iterator i2 = field_troops.begin(); i2 != field_troops.end(); ++ i2) {
			unit& u = **i2;
			captains.clear();
			leave_heros.clear();
			
				if ((dealing_heros_itor = dealing_heros.find(u.master().number_)) == dealing_heros.end()) {
					leave_heros.push_back(&u.master());
				} else {
					dealing_heros.erase(dealing_heros_itor);
					captains.push_back(&u.master());
				}
				if (u.second().valid()) {
					if ((dealing_heros_itor = dealing_heros.find(u.second().number_)) == dealing_heros.end()) {
						leave_heros.push_back(&u.second());
					} else {
						dealing_heros.erase(dealing_heros_itor);
						captains.push_back(&u.second());
					}
				}
				if (u.third().valid()) {
					if ((dealing_heros_itor = dealing_heros.find(u.third().number_)) == dealing_heros.end()) {
						leave_heros.push_back(&u.third());
					} else {
						dealing_heros.erase(dealing_heros_itor);
						captains.push_back(&u.third());
					}
				}

			if (captains.empty()) {
				units.erase(&u);
			} else {
				if (leave_heros.size()) {
					u.replace_captains(captains);
				}
			}
			for (std::vector<hero*>::iterator i3 = leave_heros.begin(); i3 != leave_heros.end(); ++ i3) {
				hero& h = **i3;
				h.to_unstage();
				message = _("I will leave for a while, and be back in future.");
				show_hero_message(&h, &city, message, game_events::INCIDENT_LEAVE);
			}
		}

		// reside troops
		std::vector<unit*>& reside_troops = city.reside_troops();
		for (std::vector<unit*>::iterator i2 = reside_troops.begin(); i2 != reside_troops.end();) {
			unit& u = **i2;
			captains.clear();
			leave_heros.clear();

				if ((dealing_heros_itor = dealing_heros.find(u.master().number_)) == dealing_heros.end()) {
					leave_heros.push_back(&u.master());
				} else {
					dealing_heros.erase(dealing_heros_itor);
					captains.push_back(&u.master());
				}
				if (u.second().valid()) {
					if ((dealing_heros_itor = dealing_heros.find(u.second().number_)) == dealing_heros.end()) {
						leave_heros.push_back(&u.second());
					} else {
						dealing_heros.erase(dealing_heros_itor);
						captains.push_back(&u.second());
					}
				}
				if (u.third().valid()) {
					if ((dealing_heros_itor = dealing_heros.find(u.third().number_)) == dealing_heros.end()) {
						leave_heros.push_back(&u.third());
					} else {
						dealing_heros.erase(dealing_heros_itor);
						captains.push_back(&u.third());
					}
				}

			if (captains.empty()) {
				delete &u;
				i2 = reside_troops.erase(i2);
			} else {
				if (leave_heros.size()) {
					u.replace_captains(captains);
				}
				++ i2;
			}
			for (std::vector<hero*>::iterator i3 = leave_heros.begin(); i3 != leave_heros.end(); ++ i3) {
				hero& h = **i3;
				h.to_unstage();
				message = _("I will leave for a while, and be back in future.");
				show_hero_message(&h, &city, message, game_events::INCIDENT_LEAVE);
			}
		}
		
		leave_heros.clear();
		std::copy(city.finish_heros().begin(), city.finish_heros().end(), std::back_inserter(leave_heros));
		std::copy(city.fresh_heros().begin(), city.fresh_heros().end(), std::back_inserter(leave_heros));

		for (int type = 0; type < 3; type ++) { 
			std::vector<hero*>* list;
			if (type == 0) {
				list = &city.fresh_heros();
			} else if (type == 1) {
				list = &city.finish_heros();
			} else {
				list = &city.wander_heros();
			}
			for (std::vector<hero*>::iterator i3 = list->begin(); i3 != list->end();) {
				hero& h = **i3;
				if ((dealing_heros_itor = dealing_heros.find(h.number_)) == dealing_heros.end()) {
					h.to_unstage();
					message = _("I will leave for a while, and be back in future.");
					show_hero_message(&h, &city, message, game_events::INCIDENT_LEAVE);
					i3 = list->erase(i3);
				} else {
					dealing_heros.erase(dealing_heros_itor);
					++ i3;
				}
			}
		}
	}

	for (dealing_heros_itor = dealing_heros.begin(); dealing_heros_itor != dealing_heros.end(); ++ dealing_heros_itor) {
		hero& h = heros[*dealing_heros_itor];

		std::vector<artifical*>& side_cities = selected_team.holded_cities();
		selected_city = side_cities[0];

		extract_hero(units, h);
		h.status_ = hero_status_idle;
		h.side_ = selected_city->side() - 1;
		h.city_ = selected_city->cityno();
		selected_city->fresh_heros().push_back(&h);

		message = _("Let me join in. I will do my best to maintenance our honor.");
		join_anim(&h, selected_city, message);
	}
}

// it is called in new turn
WML_HANDLER_FUNCTION(ai, /*event_info*/, cfg)
{
	play_controller* controller = resources::controller;
	unit_map& units = *resources::units;
	hero_map& heros = *resources::heros;

	if (cfg["selfish"].to_bool()) {
		return;
	}

	if (controller->ally_all_ai()) {
		game_events::show_hero_message(&heros[hero::number_scout], NULL, _("lips death and teeth cold, all ai sides concluded treaty of alliance."), game_events::INCIDENT_ALLY);

		if (rpg::stratum != hero_stratum_leader) {
			// enter to final battle automaticly
			if (!controller->final_capital().first) {
				artifical* capital_of_human = units.city_from_cityno(rpg::h->city_);
				artifical* capital_of_ai = controller->decide_ai_capital();

				controller->set_final_capital(capital_of_human, capital_of_ai);
				controller->final_battle(capital_of_human->side(), -1, -1);
			}
		}
	}
}

// If we should recall units that match a certain description
WML_HANDLER_FUNCTION(recall, /*event_info*/, cfg)
{
	LOG_NG << "recalling unit...\n";
	bool unit_recalled = false;
	config temp_config(cfg.get_config());
	// Prevent the recall unit filter from using the location as a criterion

	/**
	 * @todo FIXME: we should design the WML to avoid these types of
	 * collisions; filters should be named consistently and always have a
	 * distinct scope.
	 */
	temp_config["x"] = "recall";
	temp_config["y"] = "recall";
	vconfig unit_filter(temp_config);
	for(int index = 0; !unit_recalled && index < int(resources::teams->size()); ++index) {
		LOG_NG << "for side " << index + 1 << "...\n";
	}
}

WML_HANDLER_FUNCTION(object, event_info, cfg)
{
	const vconfig filter = cfg.child("filter");

	std::string id = cfg["id"];

	// If this item has already been used
	if(id != "" && used_items.count(id))
		return;

	std::string image = cfg["image"];
	std::string caption = cfg["name"];
	std::string text;

	map_location loc;
	if(!filter.null()) {
		for (unit_map::const_iterator u = resources::units->begin(); u != resources::units->end(); ++ u) {
			if (game_events::unit_matches_filter(*u, filter)) {
				loc = u->get_location();
				break;
			}
		}
	}

	if(loc.valid() == false) {
		loc = event_info.loc1;
	}

	const unit_map::iterator u = resources::units->find(loc);

	std::string command_type = "then";

	if (u != resources::units->end() && (filter.null() || game_events::unit_matches_filter(*u, filter)))
	{
		text = cfg["description"].str();

		u->add_modification(cfg.get_parsed_config());

		resources::screen->select_hex(event_info.loc1);
		resources::screen->invalidate_unit();

		// Mark this item as used up.
		used_items.insert(id);
	} else {
		text = cfg["cannot_use_message"].str();
		command_type = "else";
	}

	if (!cfg["silent"].to_bool())
	{
		// Redraw the unit, with its new stats
		resources::screen->draw();

		try {
			gui2::show_transient_message(resources::screen->video(), caption, text, image, true);
		} catch(utils::invalid_utf8_exception&) {
			// we already had a warning so do nothing.
		}
	}

	BOOST_FOREACH (const vconfig &cmd, cfg.get_children(command_type)) {
		handle_event_commands(event_info, cmd);
	}
}

WML_HANDLER_FUNCTION(print, /*event_info*/, cfg)
{
	bool clear = false;
	// Remove any old message.
	if (floating_label) {
		font::remove_floating_label(floating_label);
		clear = true;
	}

	// Display a message on-screen
	std::string text = cfg["text"];
	if (text.empty()) {
		if (clear) {
			resources::screen->invalidate_all();
			resources::screen->draw();
		}
		return;
	}

	int size = cfg["size"].to_int(font::SIZE_SMALL);
	int lifetime = cfg["duration"].to_int(50);
	SDL_Color color = create_color(cfg["red"], cfg["green"], cfg["blue"]);

	const SDL_Rect& rect = resources::screen->map_outside_area();

	font::floating_label flabel(text);
	flabel.set_font_size(size);
	flabel.set_color(color);
	flabel.set_position(rect.w/2,rect.h/2);
	flabel.set_lifetime(lifetime);
	flabel.set_clip_rect(rect);

	floating_label = font::add_floating_label(flabel);
}

WML_HANDLER_FUNCTION(deprecated_message, /*event_info*/, cfg)
{
	game_events::handle_deprecated_message( cfg.get_parsed_config() );
}

WML_HANDLER_FUNCTION(wml_message, /*event_info*/, cfg)
{
	game_events::handle_wml_log_message( cfg.get_parsed_config() );
}


typedef std::map<map_location, int> recursion_counter;

class recursion_preventer {
	static recursion_counter counter_;
	static const int max_recursion = 10;
	map_location loc_;
	bool too_many_recursions_;

	public:
	recursion_preventer(map_location& loc) :
		loc_(loc),
		too_many_recursions_(false)
	{
		recursion_counter::iterator inserted = counter_.insert(std::make_pair(loc_, 0)).first;
		++inserted->second;
		too_many_recursions_ = inserted->second >= max_recursion;
	}
	~recursion_preventer()
	{
		recursion_counter::iterator itor = counter_.find(loc_);
		if (--itor->second == 0)
		{
			counter_.erase(itor);
		}
	}
	bool too_many_recursions() const
	{
		return too_many_recursions_;
	}
};

WML_HANDLER_FUNCTION(kill, event_info, cfg)
{
	unit_map& units = *resources::units;
	hero_map& heros = *resources::heros;

	int a_side = cfg["a_side"].to_int(HEROS_INVALID_SIDE);
	const std::vector<std::string> master_heros = utils::split(cfg["hero"]);

	for (std::vector<std::string>::const_iterator tmp = master_heros.begin(); tmp != master_heros.end(); ++ tmp) {
		unit* holder = NULL;
		hero& h = heros[lexical_cast_default<int>(*tmp)];
		if (h.status_ != hero_status_military) {
			continue;
		}

		holder = units.find_unit(h);
		if (!holder) {
			continue;
		}
		const map_location& loc = holder->get_location();

		if (!holder->is_artifical() && units.city_from_loc(loc)) {
			// reside troop
			artifical* city = units.city_from_loc(loc);
			std::vector<unit*>& reside_troops = city->reside_troops();
			int index = 0;
			for (std::vector<unit*>::iterator i = reside_troops.begin(); i != reside_troops.end(); ++ i, index ++) {
				if (*i == holder) {
					city->fresh_into(holder);
					city->troop_go_out(index);
					break;
				}
			}
		} else {
			if (cfg["animate"].to_bool()) {
				resources::screen->scroll_to_tile(loc);
				unit_display::unit_die(loc, *holder);
			}
			// city/artitifical/field troop
			unit_die(units, *holder, NULL, 0, a_side);

			// some unit killed, clear status caches.
			unit::clear_status_caches();
		}
	}
	return;
}

WML_HANDLER_FUNCTION(rename, event_info, cfg)
{
	hero_map& heros = *resources::heros;
	unit_map& units = *resources::units;
	play_controller& controller = *resources::controller;

	int number = cfg["number"].to_int(HEROS_INVALID_NUMBER);
	std::string name = cfg["name"].str();

	if (number < 0 || number >= (int)heros.size()) {
		return;
	}
	controller.insert_rename(number, name);
}

// Setting of menu items
WML_HANDLER_FUNCTION(set_menu_item, /*event_info*/, cfg)
{
	/*
	   [set_menu_item]
	   id=test1
	   image="buttons/group_all.png"
	   description="Summon Troll"
	   [show_if]
	   [not]
	   [have_unit]
	   x,y=$x1,$y1
	   [/have_unit]
	   [/not]
	   [/show_if]
	   [filter_location]
	   [/filter_location]
	   [command]
	   {LOYAL_UNIT $side_number (Troll) $x1 $y1 (Myname) ( _ "Myname")}
	   [/command]
	   [/set_menu_item]
	   */
	std::string id = cfg["id"];
	wml_menu_item*& mref = resources::state_of_game->wml_menu_items[id];
	if(mref == NULL) {
		mref = new wml_menu_item(id);
	}
	if(cfg.has_attribute("image")) {
		mref->image = cfg["image"].str();
	}
	if(cfg.has_attribute("description")) {
		mref->description = cfg["description"];
	}
	if(cfg.has_attribute("needs_select")) {
		mref->needs_select = cfg["needs_select"].to_bool();
	}
	if(cfg.has_child("show_if")) {
		mref->show_if = cfg.child("show_if").get_config();
	}
	if(cfg.has_child("filter_location")) {
		mref->filter_location = cfg.child("filter_location").get_config();
	}
	if(cfg.has_child("command")) {
		config* new_command = new config(cfg.child("command").get_config());
		wmi_command_changes.push_back(wmi_command_change(id, new_command));
	}
}

WML_HANDLER_FUNCTION(store_starting_location, /*event_info*/, cfg)
{
	std::string variable = cfg["variable"];
	if (variable.empty()) {
		variable="location";
	}
	int side_num = cfg["side"].to_int(1);

	const map_location& loc = resources::game_map->starting_position(side_num);
	config &loc_store = resources::state_of_game->get_variable_cfg(variable);
	loc_store.clear();
	loc.write(loc_store);
	resources::game_map->write_terrain(loc, loc_store);
	if (resources::game_map->is_village(loc)) {
		int side = village_owner(loc, *resources::teams) + 1;
		loc_store["owner_side"] = side;
	}
}

/* [store_villages] : store villages into an array
 * Keys:
 * - variable (mandatory): variable to store in
 * - side: if present, the village should be owned by this side (0=unowned villages)
 * - terrain: if present, filter the village types against this list of terrain types
 */
WML_HANDLER_FUNCTION(store_villages, /*event_info*/, cfg)
{
	log_scope("store_villages");
	std::string variable = cfg["variable"];
	if (variable.empty()) {
		variable="location";
	}
	config to_store;
	variable_info varinfo(variable, true, variable_info::TYPE_ARRAY);

	std::vector<map_location> locs = resources::game_map->villages();

	for(std::vector<map_location>::const_iterator j = locs.begin(); j != locs.end(); ++j) {
		bool matches = terrain_filter(cfg, *resources::units).match(*j);
		if(matches) {
			config &loc_store = to_store.add_child(varinfo.key);
			j->write(loc_store);
			resources::game_map->write_terrain(*j, loc_store);
			int side = village_owner(*j, *resources::teams) + 1;
			loc_store["owner_side"] = side;
		}
	}
	varinfo.vars->clear_children(varinfo.key);
	varinfo.vars->append(to_store);
}

WML_HANDLER_FUNCTION(end_turn, /*event_info*/, /*cfg*/)
{
	resources::controller->force_end_turn();
}

WML_HANDLER_FUNCTION(endlevel, /*event_info*/, cfg)
{
	game_state *state_of_game = resources::state_of_game;
	unit_map *units = resources::units;

	// Remove 0-hp units from the unit map to avoid the following problem:
	// In case a die event triggers an endlevel the dead unit is still as a
	// 'ghost' in linger mode. After save loading in linger mode the unit
	// is fully visible again.
	unit_map::iterator it = units->begin();
	while (it != units->end()) {
		unit& u = *it;
		if (u.hitpoints() <= 0) {
			if (!u.is_city()) {
				units->erase(&u);
				it = units->begin();
			} else {
				u.heal(u.max_hitpoints() / 2);
				++ it;
			}
		} else {
			++ it;
		}
	}

	std::string next_scenario = cfg["next_scenario"];
	if (!next_scenario.empty()) {
		state_of_game->classification().next_scenario = next_scenario;
	}

	std::string end_of_campaign_text = cfg["end_text"];
	if (!end_of_campaign_text.empty()) {
		state_of_game->classification().end_text = end_of_campaign_text;
	}

	config::attribute_value end_of_campaign_text_delay = cfg["end_text_duration"];
	if (!end_of_campaign_text_delay.empty()) {
		state_of_game->classification().end_text_duration =
			end_of_campaign_text_delay.to_int(state_of_game->classification().end_text_duration);
	}

	end_level_data &data = resources::controller->get_end_level_data();

	std::string result = cfg["result"];
	data.custom_endlevel_music = cfg["music"].str();
	data.carryover_report = cfg["carryover_report"].to_bool(true);
	data.prescenario_save = cfg["save"].to_bool(true);
	data.linger_mode = cfg["linger_mode"].to_bool(true)
		&& !resources::teams->empty();
	data.reveal_map = cfg["reveal_map"].to_bool(true);
	data.gold_bonus = cfg["bonus"].to_bool(true);
	data.carryover_percentage = cfg["carryover_percentage"].to_int(game_config::gold_carryover_percentage);
	data.carryover_add = cfg["carryover_add"].to_bool();

	if (result.empty() || result == "victory") {
		resources::controller->force_end_level(VICTORY);
	} else if (result == "continue") {
		///@deprecated 1.9.2 continue as result in [endlevel]
		lg::wml_error << "continue is deprecated as result in [endlevel]"
			" and will be removed in 1.9.2,"
			" use the new attributes instead.\n";
		data.carryover_percentage = 100;
		data.carryover_add = false;
		data.carryover_report = false;
		data.linger_mode = false;
		resources::controller->force_end_level(VICTORY);
	} else if (result == "continue_no_save") {
		///@deprecated 1.9.2 continue as result in [endlevel]
		lg::wml_error << "continue_no_save is deprecated as result in [endlevel]"
			" and will be removed in 1.9.2,"
			" use the new attributes instead.\n";
		data.carryover_percentage = 100;
		data.carryover_add = false;
		data.carryover_report = false;
		data.prescenario_save = false;
		data.linger_mode = false;
		resources::controller->force_end_level(VICTORY);
	} else {
		data.carryover_add = false;
		resources::controller->force_end_level(DEFEAT);
	}
}

WML_HANDLER_FUNCTION(redraw, /*event_info*/, cfg)
{
	game_display &screen = *resources::screen;

	config::attribute_value side = cfg["side"];
	if (!side.empty()) {
		clear_shroud(side);
		screen.recalculate_minimap();
	}
	if (screen_needs_rebuild) {
		screen_needs_rebuild = false;
		screen.recalculate_minimap();
		screen.rebuild_all();
	}
	screen.invalidate_all();
	screen.draw(true,true);
}

WML_HANDLER_FUNCTION(animate_unit, event_info, cfg)
{
	const events::command_disabler disable_commands;
	unit_display::wml_animation(cfg, event_info.loc1);
}

WML_HANDLER_FUNCTION(label, /*event_info*/, cfg)
{
	game_display &screen = *resources::screen;

	terrain_label label(screen.labels(), cfg.get_config());

	screen.labels().set_label(label.location(), label.text(),
		label.team_name(), label.color(), label.visible_in_fog(), label.visible_in_shroud(), label.immutable());
}

WML_HANDLER_FUNCTION(artifical, /*event_info*/, cfg)
{
	config parsed_cfg = cfg.get_config();
	const unit_type* ut = unit_types.find(parsed_cfg["type"]);
	if (ut->master() != HEROS_INVALID_NUMBER) {
		parsed_cfg["heros_army"] = ut->master();
	}
	artifical new_unit(parsed_cfg);
	map_location loc = cfg_to_loc(cfg);
	resources::units->add(loc, &new_unit);
}

WML_HANDLER_FUNCTION(heal_unit, event_info, cfg)
{
	unit_map *units = resources::units;

	bool animated = cfg["animate"].to_bool();

	const vconfig healed_filter = cfg.child("filter");
	unit_map::iterator u;

	if (healed_filter.null()) {
		// Try to take the unit at loc1
		u = units->find(event_info.loc1);
	}
	else {
		for(u  = units->begin(); u != units->end(); ++u) {
			if (game_events::unit_matches_filter(*u, healed_filter))
				break;
		}
	}

	if (!u.valid()) return;

	const vconfig healers_filter = cfg.child("filter_second");
	std::vector<unit *> healers;

	if (!healers_filter.null()) {
		for (unit_map::iterator v = units->begin(); v != units->end(); v ++) {
			if (game_events::unit_matches_filter(*v, healers_filter) &&
			    v->has_ability_type("heals")) {
				healers.push_back(&*v);
			}
		}
	}

	int amount = cfg["amount"];
	int real_amount = u->hitpoints();
	u->heal(amount);
	real_amount = u->hitpoints() - real_amount;

	if (animated) {
		unit_display::unit_healing(*u, u->get_location(),
			healers, real_amount);
	}

	resources::state_of_game->get_variable("heal_amount") = real_amount;
}

// Allow undo sets the flag saying whether the event has mutated the game to false
WML_HANDLER_FUNCTION(allow_undo,/*event_info*/,/*cfg*/)
{
	current_context->mutated = false;
}

// Conditional statements
static void if_while_handler(bool is_if, const game_events::queued_event &event_info, const vconfig &cfg)
{
	const size_t max_iterations = (is_if ? 1 : game_config::max_loop);
	const std::string pass = (is_if ? "then" : "do");
	const std::string fail = (is_if ? "else" : "");
	for (size_t i = 0; i != max_iterations; ++i) {
		const std::string type = game_events::conditional_passed(cfg) ? pass : fail;

		if (type == "") {
			break;
		}

		// If the if statement passed, then execute all 'then' statements,
		// otherwise execute 'else' statements
		BOOST_FOREACH (const vconfig &cmd, cfg.get_children(type)) {
			handle_event_commands(event_info, cmd);
		}
	}
}

WML_HANDLER_FUNCTION(if, event_info, cfg)
{
	log_scope("if");
	if_while_handler(true, event_info, cfg);
}

WML_HANDLER_FUNCTION(while, event_info, cfg)
{
	log_scope("while");
	if_while_handler(false, event_info, cfg);
}

WML_HANDLER_FUNCTION(open_help,  /*event_info*/, cfg)
{
	game_display &screen = *resources::screen;
	t_string topic_id = cfg["topic"];
	help::show_help(screen, topic_id.to_serialized());
}
// Helper namespace to do some subparts for message function
namespace {

/**
 * Helper to handle the speaker part of the message.
 *
 * @param event_info              event_info of message.
 * @param cfg                     cfg of message.
 *
 * @returns                       The unit who's the speaker or units->end().
 */
hero& handle_speaker(
		const game_events::queued_event& event_info,
		const vconfig& cfg,
		bool scroll)
{
	unit_map& units = *resources::units;
	game_display& screen = *resources::screen;
	hero_map& heros = *resources::heros;
	std::vector<team>& teams = *resources::teams;

	bool pop_in_fog = cfg["pop_in_fog"].to_bool();

	hero& h = heros[cfg["hero"].to_int()];
	const unit* speaker = units.find_unit(h);
	if (!speaker) {
		return hero_invalid;
	}

	if (!pop_in_fog) {
		// 多人对战时play.side是无效的，所以用这种方法得到side有问题
		// 用另一种方法代替，找到第一个controller是玩家的阵营（这种方法潜在危险就是选择了多个玩家）
		// team& player_team = (*resources::teams)[lexical_cast<int>(resources::state_of_game->get_variable("player.side")) - 1];
		team* player_team = NULL;
		for (size_t i = 0; i < teams.size(); i ++) {
			if (teams[i].is_human()) {
				player_team = &teams[i];
				break;
			}
		}
		if (player_team && player_team->fogged(speaker->get_location())) { 
			speaker = NULL;
		}
	}
	if (speaker) {
		const map_location& spl = speaker->get_location();
		if (scroll) {
			const int offset_from_center = std::max<int>(0, spl.y - 1);
			screen.scroll_to_tile(map_location(spl.x, offset_from_center));
		}
	} else {
		return hero_invalid;
	}

	screen.draw(false);
	return h;
}

/**
 * Helper to handle the image part of the message.
 *
 * @param cfg                     cfg of message.
 * @param speaker                 The speaker of the message.
 *
 * @returns                       The image to show.
 */
std::string get_image(const vconfig& cfg, unit_map::iterator speaker)
{
	std::string image = cfg["image"];
	if (image.empty() && speaker != resources::units->end())
	{
		image = speaker->profile();
		std::string::size_type offset = image.find('~');
		offset = image.find_last_of('/', offset);
		if (offset != std::string::npos) {
			image.insert(offset, "/transparent");
		} else {
			image = "transparent/" + image;
		}

		image::locator locator(image);
		if(!locator.file_exists()) {
			image = speaker->profile();

			if(image == speaker->absolute_image()) {
				image += speaker->image_mods();
			}
		}
	}
	else if (!image.empty())
	{
		std::string::size_type offset = image.find('~');
		offset = image.find_last_of('/', offset);
		if (offset != std::string::npos) {
			image.insert(offset, "/transparent");
		} else {
			image = "transparent/" + image;
		}

		image::locator locator(image);
		if(!locator.file_exists()) {
			image = cfg["image"].str();
		}
	}
	return image;
}

/**
 * Helper to handle the caption part of the message.
 *
 * @param cfg                     cfg of message.
 * @param speaker                 The speaker of the message.
 *
 * @returns                       The caption to show.
 */
std::string get_caption(const vconfig& cfg, unit_map::iterator speaker)
{
	std::string caption = cfg["caption"];
	if (caption.empty() && speaker != resources::units->end()) {
		caption = speaker->name();
		if(caption.empty()) {
			caption = speaker->type_name();
		}
	}
	return caption;
}

} // namespace

struct message_user_choice : mp_sync::user_choice
{
	vconfig cfg;
	hero& speaker;
	vconfig text_input_element;
	bool has_text_input;
	bool yesno;

	message_user_choice(const vconfig &c, hero& s,
		const vconfig &t, bool ht, bool _yesno)
		: cfg(c), speaker(s), text_input_element(t)
		, has_text_input(ht), yesno(_yesno)
	{}

	virtual config query_user() const
	{
		std::string image = speaker.image(true);
		std::string caption = speaker.name();

		size_t right_offset = image.find("~RIGHT()");
		bool left_side = right_offset == std::string::npos;
		if (!left_side) {
			image.erase(right_offset);
		}

		// Parse input text, if not available all fields are empty
		std::string text_input_label = text_input_element["label"];
		std::string text_input_content = text_input_element["text"];
		unsigned input_max_size = text_input_element["max_length"].to_int(256);
		if (input_max_size > 1024 || input_max_size < 1) {
			lg::wml_error << "invalid maximum size for input "
				<< input_max_size << '\n';
			input_max_size = 256;
		}

		std::stringstream incident_str;
		int incident = cfg["incident"].to_int(-1);
		if (incident >= 0) {
			incident_str << "incident/" << incident << ".png";	
		}
		int option_chosen;
		if (yesno) {
			const int res = gui2::show_message(resources::screen->video(), caption, cfg["message"], gui2::tmessage::yes_no_buttons, image, incident_str.str());
			option_chosen = (res == gui2::twindow::OK)? 0: 1;
		} else {
			const int res = gui2::show_message(resources::screen->video(), caption, cfg["message"], gui2::tmessage::auto_close, image, incident_str.str());
		}

		/* Since gui2::show_message needs to do undrawing the
		   chatlines can get garbled and look dirty on screen. Force a
		   redraw to fix it. */
		/** @todo This hack can be removed once gui2 is finished. */
		resources::screen->invalidate_all();
		resources::screen->draw(true,true);

		config ret;
		if (yesno) ret["value"] = option_chosen;
		if (has_text_input) ret["text"] = text_input_content;
		return ret;
	}

	virtual config random_choice(rand_rng::simple_rng &) const
	{
		return config();
	}
};

// Display a message dialog
WML_HANDLER_FUNCTION(message, event_info, cfg)
{
	game_events::handle_message(event_info, cfg);
}

namespace game_events {
int handle_message(const game_events::queued_event& event_info, const vconfig& cfg)
{
	// Check if there is any input to be made, if not the message may be skipped
	bool yesno = cfg["yesno"].to_bool();

	const vconfig::child_list text_input_elements = cfg.get_children("text_input");
	const bool has_text_input = (text_input_elements.size() == 1);

	bool has_input = has_text_input || yesno;

	// skip messages during quick replay
	play_controller *controller = resources::controller;
	if(!has_input && (
			 controller->is_skipping_replay() ||
			 current_context->skip_messages
			 ))
	{
		return -1;
	}

	// Check if this message is for this side
	std::string side_for_raw = cfg["side_for"];
	if (!side_for_raw.empty()) {
		/* Always ignore side_for when the message has some input
		   boxes, but display the error message only if side_for is
		   used for an inactive side. */
		bool side_for_show = has_input;
		if (has_input && side_for_raw != str_cast(resources::controller->current_side()))
			lg::wml_error << "[message]side_for= cannot query any user input out of turn.\n";

		std::vector<std::string> side_for =
			utils::split(side_for_raw, ',', utils::STRIP_SPACES | utils::REMOVE_EMPTY);
		std::vector<std::string>::iterator itSide;
		size_t side;

		// Check if any of side numbers are human controlled
		for (itSide = side_for.begin(); itSide != side_for.end(); ++itSide)
		{
			side = lexical_cast_default<size_t>(*itSide);
			// Make sanity check that side number is good
			// then check if this side is human controlled.
			if (side > 0 && side <= resources::teams->size() &&
				(*resources::teams)[side-1].is_human())
			{
				side_for_show = true;
				break;
			}
		}
		if (!side_for_show)
		{
			DBG_NG << "player isn't controlling side which should get message\n";
			return -1;
		}
	}

	hero& speaker = handle_speaker(event_info, cfg, cfg["scroll"].to_bool(true));
	if (!speaker.valid()) {
		// No matching unit found, so the dialog can't come up.
		// Continue onto the next message.
		WRN_NG << "cannot show message\n";
		return -1;
	}

	has_input = yesno || has_text_input;
	if (!has_input && get_replay_source().is_skipping()) {
		// No input to get and the user is not interested either.
		return -1;
	}

	if (cfg.has_attribute("sound")) {
		sound::play_sound(cfg["sound"]);
	}

	if (text_input_elements.size()>1) {
		lg::wml_error << "too many text_input tags, only one accepted\n";
	}

	const vconfig text_input_element = has_text_input ?
		text_input_elements.front() : vconfig::empty_vconfig();

	int option_chosen = 0;
	std::string text_input_result;

	DBG_DP << "showing dialog...\n";

	message_user_choice msg(cfg, speaker, text_input_element, has_text_input, yesno);
	if (!has_input) {
		/* Always show the dialog if it has no input, whether we are
		   replaying or not. */
		msg.query_user();
	} else {
		config choice = mp_sync::get_user_choice("input", msg, 0, true);
		option_chosen = choice["value"];	
		text_input_result = choice["text"].str();

		// save yesno choice to variable: choice
		config::attribute_value& var = resources::state_of_game->get_variable("choice");
		var = option_chosen? false: true;
	}

	if (has_text_input) {
		std::string variable_name=text_input_element["variable"];
		if (variable_name.empty()) {
			variable_name="input";
		}
		resources::state_of_game->set_variable(variable_name, text_input_result);
	}
	return option_chosen;
}
}

// Adding/removing new time_areas dynamically with Standard Location Filters.
WML_HANDLER_FUNCTION(time_area, /*event_info*/, cfg)
{
	log_scope("time_area");

	bool remove = cfg["remove"].to_bool();
	std::string ids = cfg["id"];

	if(remove) {
		const std::vector<std::string> id_list =
			utils::split(ids, ',', utils::STRIP_SPACES | utils::REMOVE_EMPTY);
		BOOST_FOREACH (const std::string& id, id_list) {
			resources::tod_manager->remove_time_area(id);
			LOG_NG << "event WML removed time_area '" << id << "'\n";
		}
	}
	else {
		std::string id;
		if(ids.find(',') != std::string::npos) {
			id = utils::split(ids,',',utils::STRIP_SPACES | utils::REMOVE_EMPTY).front();
			ERR_NG << "multiple ids for inserting a new time_area; will use only the first\n";
		} else {
			id = ids;
		}
		std::set<map_location> locs;
		terrain_filter filter(cfg, *resources::units);
		filter.restrict_size(game_config::max_loop);
		filter.get_locations(locs, true);
		config parsed_cfg = cfg.get_parsed_config();
		resources::tod_manager->add_time_area(id, locs, parsed_cfg);
		LOG_NG << "event WML inserted time_area '" << id << "'\n";
	}
}

//Replacing the current time of day schedule
WML_HANDLER_FUNCTION(replace_schedule, /*event_info*/, cfg)
{
	if(cfg.get_children("time").empty()) {
		ERR_NG << "attempted to to replace ToD schedule with empty schedule\n";
	} else {
		resources::tod_manager->replace_schedule(cfg.get_parsed_config());
		LOG_NG << "replaced ToD schedule\n";
	}
}

// Adding new events
WML_HANDLER_FUNCTION(event, /*event_info*/, cfg)
{
	if (!cfg["delayed_variable_substitution"].to_bool(true)) {
		new_handlers.push_back(game_events::event_handler(cfg.get_parsed_config()));
	} else {
		new_handlers.push_back(game_events::event_handler(cfg.get_config()));
	}
}

// Experimental map replace
WML_HANDLER_FUNCTION(replace_map, /*event_info*/, cfg)
{
	gamemap *game_map = resources::game_map;

	gamemap map(*game_map);
	try {
		map.read(cfg["map"]);
	} catch(incorrect_map_format_error&) {
		lg::wml_error << "replace_map: Unable to load map " << cfg["map"] << "\n";
		return;
	} catch(twml_exception& e) {
		e.show(*resources::screen);
		return;
	}
	if (map.total_width() > game_map->total_width()
	|| map.total_height() > game_map->total_height()) {
		if (!cfg["expand"].to_bool()) {
			lg::wml_error << "replace_map: Map dimension(s) increase but expand is not set\n";
			return;
		}
	}
	if (map.total_width() < game_map->total_width()
	|| map.total_height() < game_map->total_height()) {
		if (!cfg["shrink"].to_bool()) {
			lg::wml_error << "replace_map: Map dimension(s) decrease but shrink is not set\n";
			return;
		}
		unit_map *units = resources::units;
		unit_map::iterator itor;
		for (itor = units->begin(); itor != units->end(); ) {
			if (!map.on_board(itor->get_location())) {
				if (!try_add_unit_to_recall_list(itor->get_location(), *itor)) {
					lg::wml_error << "replace_map: Cannot add a unit that would become off-map to the recall list\n";
				}
				// units->erase(itor++);
				units->erase(itor->get_location());
				itor++;
			} else {
				++itor;
			}
		}
	}
	*game_map = map;
	resources::screen->reload_map();
	screen_needs_rebuild = true;
	// ai::manager::raise_map_changed();
}

// Experimental data persistence
WML_HANDLER_FUNCTION(set_global_variable,/**/,pcfg)
{
	if (get_replay_source().at_end() || (network::nconnections() != 0))
		verify_and_set_global_variable(pcfg);
}
WML_HANDLER_FUNCTION(get_global_variable,/**/,pcfg)
{
	verify_and_get_global_variable(pcfg);
}
WML_HANDLER_FUNCTION(clear_global_variable,/**/,pcfg)
{
	if (get_replay_source().at_end() || (network::nconnections() != 0))
		verify_and_clear_global_variable(pcfg);
}

/** Handles all the different types of actions that can be triggered by an event. */

static void commit_new_handlers() {
	// Commit any spawned events-within-events
	while(new_handlers.size() > 0) {
		// event_handlers.push_back(new_handlers.back());
		event_handlers[event_vsize ++] = new game_events::event_handler(new_handlers.back());
		new_handlers.pop_back();
	}
}
static void commit_wmi_commands() {
	// Commit WML Menu Item command changes
	while(wmi_command_changes.size() > 0) {
		wmi_command_change wcc = wmi_command_changes.front();
		const bool is_empty_command = wcc.second->empty();

		wml_menu_item*& mref = resources::state_of_game->wml_menu_items[wcc.first];
		const bool has_current_handler = !mref->command.empty();

		mref->command = *(wcc.second);
		mref->command["name"] = mref->name;
		mref->command["first_time_only"] = false;

		if(has_current_handler) {
			if(is_empty_command) {
				mref->command.add_child("allow_undo");
			}
			// BOOST_FOREACH (game_events::event_handler& hand, event_handlers) {
			for (size_t i = 0; i < event_vsize; i ++) {
				game_events::event_handler& hand = *event_handlers[i];
				if (hand.is_menu_item() && hand.matches_name(mref->name)) {
					LOG_NG << "changing command for " << mref->name << " to:\n" << *wcc.second;
					hand = game_events::event_handler(mref->command, true);
				}
			}
		} else if(!is_empty_command) {
			LOG_NG << "setting command for " << mref->name << " to:\n" << *wcc.second;
			// event_handlers.push_back(game_events::event_handler(mref->command, true));
			event_handlers[event_vsize ++] = new game_events::event_handler(mref->command, true);
		}

		delete wcc.second;
		wmi_command_changes.erase(wmi_command_changes.begin());
	}
}

static bool process_event(game_events::event_handler& handler, const game_events::queued_event& ev)
{
	if (handler.disabled())
		return false;

	unit_map* units = resources::units;
	hero_map& heros = *resources::heros;
	unit* unit1 = NULL;
	unit_map::iterator u_itor;
	if ((u_itor = units->find(ev.loc1)) != units->end()) {
		unit1 = &*u_itor;
	}
	unit* unit2 = NULL;
	artifical* city = NULL;
	if (unit1 && unit1->is_city() && ev.loc2.x == MAGIC_RESIDE) {
		city = unit_2_artifical(unit1);
		unit2 = city->reside_troops()[ev.loc2.y];
	} else if ((u_itor = units->find(ev.loc2)) != units->end()) {
		unit2 = &*u_itor;
	}

	bool filtered_unit1 = false, filtered_unit2 = false;
	scoped_xy_unit first_unit("unit", ev.loc1.x, ev.loc1.y, *units);
	scoped_xy_unit second_unit("second_unit", ev.loc2.x, ev.loc2.y, *units, city);

	scoped_hero hero1("hero", ev.loc2.y, heros);

	scoped_weapon_info first_weapon("weapon", ev.data.child("first"));
	scoped_weapon_info second_weapon("second_weapon", ev.data.child("second"));
	vconfig filters(handler.get_config());


	BOOST_FOREACH (const vconfig &condition, filters.get_children("filter_condition")) {
		if (!game_events::conditional_passed(condition)) {
			return false;
		}
	}

	BOOST_FOREACH (const vconfig &f, filters.get_children("filter")) {
		if (!unit1 || !game_events::unit_matches_filter(*unit1, f)) {
			return false;
		}
		if (!f.empty()) {
			filtered_unit1 = true;
		}
	}

	vconfig::child_list special_filters = filters.get_children("filter_attack");
	bool special_matches = special_filters.empty();
	BOOST_FOREACH (const vconfig &f, special_filters)
	{
		if (unit1 && game_events::matches_special_filter(ev.data.child("first"), f)) {
			special_matches = true;
		}
		if (!f.empty()) {
			filtered_unit1 = true;
		}
	}
	if (!special_matches) {
		return false;
	}

	BOOST_FOREACH (const vconfig &f, filters.get_children("filter_second")) {
		if (!unit2 || !game_events::unit_matches_filter(*unit2, f)) {
			return false;
		}
		if (!f.empty()) {
			filtered_unit2 = true;
		}
	}

	special_filters = filters.get_children("filter_second_attack");
	special_matches = special_filters.empty();
	BOOST_FOREACH (const vconfig &f, special_filters)
	{
		if (unit2 && game_events::matches_special_filter(ev.data.child("second"), f)) {
			special_matches = true;
		}
		if (!f.empty()) {
			filtered_unit2 = true;
		}
	}

	// [filter_hero]
	if (unit1 && unit1->is_city() && ev.loc2.x == MAGIC_HERO) {
		hero& h = heros[ev.loc2.y];
		BOOST_FOREACH (const vconfig &f, filters.get_children("filter_hero")) {
			if (!h.internal_matches_filter(f)) {
				return false;
			}
		}
	}

	if (!special_matches) {
		return false;
	}

	// The event hasn't been filtered out, so execute the handler.
	scoped_context evc;
	handler.handle_event(ev);

	if (ev.name == "select") {
		resources::state_of_game->last_selected = ev.loc1;
	}

	if (screen_needs_rebuild) {
		screen_needs_rebuild = false;
		game_display *screen = resources::screen;
		screen->recalculate_minimap();
		screen->invalidate_all();
		screen->rebuild_all();
	}

	return current_context->mutated;
}

namespace game_events {

	event_handler::event_handler(const config &cfg, bool imi) :
		first_time_only_(cfg["first_time_only"].to_bool(true)),
		disabled_(false), is_menu_item_(imi), cfg_(cfg)
	{}

	void event_handler::handle_event(const game_events::queued_event& event_info)
	{
		if (first_time_only_)
		{
			disabled_ = true;
		}

		if (is_menu_item_) {
			DBG_NG << cfg_["name"] << " will now invoke the following command(s):\n" << cfg_;
		}

		handle_event_commands(event_info, vconfig(cfg_));
	}

	void handle_event_commands(const game_events::queued_event& event_info, const vconfig &cfg)
	{
		resources::lua_kernel->run_wml_action("command", cfg, event_info);
	}

	void handle_event_command(const std::string &cmd,
		const game_events::queued_event &event_info, const vconfig &cfg)
	{
		log_scope2(log_engine, "handle_event_command");
		LOG_NG << "handling command '" << cmd << "' from "
			<< (cfg.is_volatile()?"volatile ":"") << "cfg 0x"
			<< std::hex << std::setiosflags(std::ios::uppercase)
			<< reinterpret_cast<uintptr_t>(&cfg.get_config()) << std::dec << "\n";

		if (!resources::lua_kernel->run_wml_action(cmd, cfg, event_info))
		{
			ERR_NG << "Couldn't find function for wml tag: "<< cmd <<"\n";
		}

		DBG_NG << "done handling command...\n";
	}

	bool event_handler::matches_name(const std::string &name) const
	{
		const t_string& t_my_names = cfg_["name"];
		const std::string& my_names = t_my_names;
		std::string::const_iterator itor,
			it_begin = my_names.begin(),
			it_end = my_names.end(),
			match_it = name.begin(),
			match_begin = name.begin(),
			match_end = name.end();
		int skip_count = 0;
		for(itor = it_begin; itor != it_end; ++itor) {
			bool do_eat = false,
				do_skip = false;
			switch(*itor) {
			case ',':
				if(itor - it_begin - skip_count == match_it - match_begin && match_it == match_end) {
					return true;
				}
				it_begin = itor + 1;
				match_it = match_begin;
				skip_count = 0;
				continue;
			case '\f':
			case '\n':
			case '\r':
			case '\t':
			case '\v':
				do_skip = (match_it == match_begin || match_it == match_end);
				break;
			case ' ':
				do_skip = (match_it == match_begin || match_it == match_end);
				// fall through to case '_'
			case '_':
				do_eat = (match_it != match_end && (*match_it == ' ' || *match_it == '_'));
				break;
			default:
				do_eat = (match_it != match_end && *match_it == *itor);
				break;
			}
			if(do_eat) {
				++match_it;
			} else if(do_skip) {
				++skip_count;
			} else {
				itor = std::find(itor, it_end, ',');
				if(itor == it_end) {
					return false;
				}
				it_begin = itor + 1;
				match_it = match_begin;
				skip_count = 0;
			}
		}
		if(itor - it_begin - skip_count == match_it - match_begin && match_it == match_end) {
			return true;
		}
		return false;
	}

	bool matches_special_filter(const config &cfg, const vconfig& filter)
	{
		if (!cfg) {
			WRN_NG << "attempt to filter attack for an event with no attack data.\n";
			// better to not execute the event (so the problem is more obvious)
			return false;
		}
		const attack_type attack(cfg);
		bool matches = attack.matches_filter(filter.get_parsed_config());

		// Handle [and], [or], and [not] with in-order precedence
		vconfig::all_children_iterator cond_i = filter.ordered_begin();
		vconfig::all_children_iterator cond_end = filter.ordered_end();
		while(cond_i != cond_end)
		{
			const std::string& cond_name = cond_i.get_key();
			const vconfig& cond_filter = cond_i.get_child();

			// Handle [and]
			if(cond_name == "and")
			{
				matches = matches && matches_special_filter(cfg, cond_filter);
			}
			// Handle [or]
			else if(cond_name == "or")
			{
				matches = matches || matches_special_filter(cfg, cond_filter);
			}
			// Handle [not]
			else if(cond_name == "not")
			{
				matches = matches && !matches_special_filter(cfg, cond_filter);
			}
			++cond_i;
		}
		return matches;
	}

	bool unit_matches_filter(const unit &u, const vconfig& filter)
	{
		return u.matches_filter(filter, u.get_location());
	}

	static std::set<std::string> unit_wml_ids;

	manager::manager(const config& cfg)
		: variable_manager()
	{
		std::vector<std::string> name;
		assert(!manager_running);
		BOOST_FOREACH (const config &ev, cfg.child_range("event")) {
			// event_handlers.push_back(game_events::event_handler(ev));
			event_handlers[event_vsize ++] = new game_events::event_handler(ev);
			name.push_back(ev["name"]);
		}
		BOOST_FOREACH (const std::string &id, utils::split(cfg["unit_wml_ids"])) {
			unit_wml_ids.insert(id);
		}

		resources::lua_kernel = new LuaKernel(cfg);
		manager_running = true;

		BOOST_FOREACH (static_wml_action_map::value_type &action, static_wml_actions) {
			resources::lua_kernel->set_wml_action(action.first, action.second);
		}

		const std::string used = cfg["used_items"];
		if(!used.empty()) {
			const std::vector<std::string>& v = utils::split(used);
			for(std::vector<std::string>::const_iterator i = v.begin(); i != v.end(); ++i) {
				used_items.insert(*i);
			}
		}
		int wmi_count = 0;
		typedef std::pair<std::string, wml_menu_item *> item;
		BOOST_FOREACH (const item &itor, resources::state_of_game->wml_menu_items) {
			if (!itor.second->command.empty()) {
				// event_handlers.push_back(game_events::event_handler(itor.second->command, true));
				event_handlers[event_vsize ++] = new game_events::event_handler(itor.second->command, true);
			}
			++wmi_count;
		}
		if(wmi_count > 0) {
			LOG_NG << wmi_count << " WML menu items found, loaded." << std::endl;
		}
	}

	void write_events(config& cfg)
	{
		assert(manager_running);
		for (size_t i = 0; i < event_vsize; i ++) {
			game_events::event_handler* itor = event_handlers[i];
			if (!itor->disabled() && !itor->is_menu_item()) {
				cfg.add_child("event", event_handlers[i]->get_config());
			}
		}

		std::stringstream used;
		std::set<std::string>::const_iterator u;
		for(u = used_items.begin(); u != used_items.end(); ++u) {
			if(u != used_items.begin())
				used << ",";

			used << *u;
		}

		cfg["used_items"] = used.str();
		std::stringstream ids;
		for(u = unit_wml_ids.begin(); u != unit_wml_ids.end(); ++u) {
			if(u != unit_wml_ids.begin())
				ids << ",";

			ids << *u;
		}

		cfg["unit_wml_ids"] = ids.str();

		if (resources::soundsources)
			resources::soundsources->write_sourcespecs(cfg);

		resources::lua_kernel->save_game(cfg);
	}

	manager::~manager() {
		assert(manager_running);
		manager_running = false;
		events_queue.clear();
		// event_handlers.clear();
		for (size_t i = 0; i < event_vsize; i ++) {
			delete event_handlers[i];
		}
		event_vsize = 0;

		delete resources::lua_kernel;
		resources::lua_kernel = NULL;
		unit_wml_ids.clear();
		used_items.clear();
	}

	void raise(const std::string& event,
			const entity_location& loc1,
			const entity_location& loc2,
			const config& data)
	{
		assert(manager_running);
		if(!events_init())
			return;

		LOG_NG << "fire event: " << event << "\n";

		events_queue.push_back(game_events::queued_event(event,loc1,loc2,data));
	}

	bool fire(const std::string& event,
			const entity_location& loc1,
			const entity_location& loc2,
			const config& data)
	{
		assert(manager_running);
		raise(event,loc1,loc2,data);
		return pump();
	}

	void add_events(const config::const_child_itors &cfgs, const std::string &id)
	{
		if(std::find(unit_wml_ids.begin(),unit_wml_ids.end(),id) == unit_wml_ids.end()) {
			unit_wml_ids.insert(id);
			BOOST_FOREACH (const config &new_ev, cfgs) {
				// std::vector<game_events::event_handler> &temp = (pump_manager::count()) ? new_handlers : event_handlers;
				if (pump_manager::count()) {
					std::vector<game_events::event_handler> &temp = new_handlers;
					temp.push_back(game_events::event_handler(new_ev, true));
				} else {
					event_handlers[event_vsize ++] = new game_events::event_handler(new_ev, true);
				}
			}
		}
	}

	void commit()
	{
		if(pump_manager::count() == 1) {
			commit_wmi_commands();
			commit_new_handlers();
		}
		// Dialogs can only be shown if the display is not locked
		if (!resources::screen->video().update_locked()) {
			show_wml_errors();
			show_wml_messages();
		}
	}

	bool pump()
	{
		assert(manager_running);
		if(!events_init())
			return false;

		pump_manager pump_instance;
		if(pump_manager::count() >= game_config::max_loop) {
			ERR_NG << "game_events::pump() waiting to process new events because "
				<< "recursion level would exceed maximum " << game_config::max_loop << '\n';
			return false;
		}

		bool result = false;
		while(events_queue.empty() == false) {
			game_events::queued_event ev = events_queue.front();
			events_queue.pop_front();	// pop now for exception safety
			const std::string& event_name = ev.name;

			// Clear the unit cache, since the best clearing time is hard to figure out
			// due to status changes by WML. Every event will flush the cache.
			unit::clear_status_caches();

			resources::lua_kernel->run_event(ev);

			bool init_event_vars = true;

			// BOOST_FOREACH (game_events::event_handler& handler, event_handlers) {
			for (size_t i = 0; i < event_vsize; ) {
				game_events::event_handler& handler = *event_handlers[i];
				if (!handler.matches_name(event_name)) {
					i ++;
					continue;
				}
				// Set the variables for the event
				if (init_event_vars) {
					resources::state_of_game->get_variable("x1") = ev.loc1.x + 1;
					resources::state_of_game->get_variable("y1") = ev.loc1.y + 1;
					resources::state_of_game->get_variable("x2") = ev.loc2.x + 1;
					resources::state_of_game->get_variable("y2") = ev.loc2.y + 1;
					init_event_vars = false;
				}

				LOG_NG << "processing event '" << event_name << "'\n";
				if(process_event(handler, ev))
					result = true;
				if (handler.disabled()) {
					delete event_handlers[i];
					if (i < event_vsize - 1) {
						memcpy(&(event_handlers[i]), &(event_handlers[i + 1]), (event_vsize - i - 1) * sizeof(game_events::event_handler*));
					}
					event_vsize --;
				} else {
					i ++;
				}
			}

			// Only commit new handlers when finished iterating over event_handlers.
			commit();
		}

		return result;
	}

	entity_location::entity_location(const map_location &loc, size_t id)
		: map_location(loc), id_(id)
	{}

	entity_location::entity_location(const unit &u)
		: map_location(u.get_location()), id_(u.underlying_id())
	{}

	bool entity_location::matches_unit(const unit& u) const
	{
		return id_ == u.underlying_id();
	}

	bool entity_location::requires_unit() const
	{
		return id_ > 0;
	}

	void show_hero_message(hero* h, artifical* city, const std::string& message, int incident)
	{
		game_display& gui = *resources::screen;
		const std::vector<team>& teams = *resources::teams;

		if (!city || teams[city->side() - 1].is_human()) {
			// Parse input text, if not available all fields are empty
			std::string text_input_content;
			int option_chosen = 0;
			std::stringstream title;
			std::string incident_str;

			incident_str = std::string("incident/") + lexical_cast<std::string>(incident) + ".png";
			
			title << h->name(); 
			if (city) {
				title << "\n(" << city->master().name() << ")";
			}

			// gui.hide_tip();
			gui2::show_message(gui.video(), title.str(), message, gui2::tmessage::auto_close, h->image(true), incident_str);

			// Since gui2::show_message needs to do undrawing the
			// chatlines can get garbled and look dirty on screen. Force a
			// redraw to fix it.
			// @todo This hack can be removed once gui2 is finished.
			resources::screen->invalidate_all();
			resources::screen->draw(true, true);
		}
	}

	void show_relation_message(unit_map& units, hero_map& heros, hero& h1, hero& h2, int carry_to)
	{
		// const std::vector<team>& teams = *resources::teams;
		// if (!teams[h1.side_].is_human()) return;
		if (ai_relation(h1, h2)) {
			return;
		}

		std::string message;
		int incident;
		utils::string_map symbols;
		symbols["first"] = help::tintegrate::generate_format(h1.name(), help::tintegrate::hero_color);
		symbols["second"] = help::tintegrate::generate_format(h2.name(), help::tintegrate::hero_color);
		
		if (carry_to == hero::FEELING_CONSORT) {
			if (!h2.is_consort(h1)) {
				message = vgettext("I'm happy! I have married with $second.", symbols);
				incident = game_events::INCIDENT_MARRY;
			}
		} else if (carry_to == hero::FEELING_OATH) {
			if (!h2.is_oath(h1)) {
				message = vgettext("I'm happy! I have oathed with $second.", symbols);
				if (h1.gender_ == hero_gender_male) {
					incident = game_events::INCIDENT_MALEOATH;
				} else {
					incident = game_events::INCIDENT_FEMALEOATH;
				}
			}
		} else if (carry_to == hero::FEELING_HATE) {
			if (h2.is_hate(h1)) {
				message = vgettext("I'm happy! I got rid of hate with $second.", symbols);
				incident = game_events::INCIDENT_RECOMMENDONESELF;
			}
		}
		if (!message.empty()) {
			game_events::show_hero_message(&h1, NULL, message, incident);
		}
	}

	bool ai_relation(hero& h1, hero& h2)
	{
		const std::vector<team>& teams = *resources::teams;
		unit_map& units = *resources::units;
		if (!teams[h1.side_].is_human()) return true;
		if (rpg::h != &h1 && rpg::h != &h2) {
			unit* u = units.find_unit(h1);
			if (!u->is_artifical()) {
				if (!u->human()) {
					return true;
				}
			} else {
				if (rpg::stratum == hero_stratum_citizen) {
					return true;
				} else if (rpg::stratum == hero_stratum_mayor) {
					artifical* city = unit_2_artifical(u);
					if (city->mayor() != rpg::h) {
						return true;
					}
				}
			}
		}
		return false;
	}

	bool confirm_carry_to(hero& h1, hero& h2, int carry_to)
	{
		if (ai_relation(h1, h2)) {
			return true;
		}

		utils::string_map symbols;
		std::string str;
		symbols["first"] = help::tintegrate::generate_format(h1.name(), help::tintegrate::hero_color);
		symbols["second"] = help::tintegrate::generate_format(h2.name(), help::tintegrate::hero_color);
		if (carry_to == hero::FEELING_CONSORT) {
			str = vgettext("$first want to marry with $second, do you agree?", symbols);
		} else if (carry_to == hero::FEELING_OATH) {
			str = vgettext("$first want to oath with $second, do you agree?", symbols);
		}

		config message;
		message["hero"] = h1.number_;
		message["scroll"] = "no";
		message["message"] = str;
		message["yesno"] = true;

		vconfig vcfg(message);
		game_events::queued_event event_info("message", map_location(), map_location(), config());
		int option_chosen = game_events::handle_message(event_info, vcfg);

		return option_chosen? false: true;
}

} // end namespace game_events (2)
