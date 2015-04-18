/* $Id: mp_create_game.cpp 49597 2011-05-22 17:55:49Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/mp_create_game.hpp"

#include "game_display.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "gui/auxiliary/timer.hpp"
#include "gui/dialogs/field.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/integer_selector.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/slider.hpp"
#include "../../settings.hpp"
#include "filesystem.hpp"
#include "map.hpp"
#include "map_exception.hpp"
#include "map_create.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/mp_create_game_set_password.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "formula_string_utils.hpp"

#include <boost/foreach.hpp>
#include <boost/bind.hpp>

namespace gui2 {

extern void common_genus(tbutton& genus);

REGISTER_DIALOG(mp_create_game)

tmp_create_game::tmp_create_game(display& gui, const config& cfg, bool local_only) :
	trandom_map(cfg, mode_tag::RPG, local_only)
	, gui_(gui)
	, num_turns_(0)
	, era_index_(0)
	, era_(NULL)
	, local_only_(local_only)
	, launch_game_(NULL)
	, maximal_defeated_activity_(NULL)
	, fog_(register_bool("fog",
			true,
			preferences::fog,
			preferences::set_fog))
	, observers_(register_bool("observers",
			true,
			preferences::allow_observers,
			preferences::set_allow_observers))
	, shroud_(register_bool("shroud",
			true,
			preferences::shroud,
			preferences::set_shroud))

	, turns_(register_integer("turn_count",
		true,
		preferences::turns ,
		preferences::set_turns))
	, experience_(register_integer("experience_modifier",
		true,
		preferences::xp_modifier ,
		preferences::set_xp_modifier))
{
}

void tmp_create_game::pre_show(CVideo& /*video*/, twindow& window)
{
	std::stringstream strstr;

	name_entry_ = find_widget<ttext_box>(&window, "game_name", false, true);

	genus_ = find_widget<tbutton>(&window, "genus", false, true);
	tent::turn_based = false;
	strstr.str("");
	strstr << unit_types.genus(tent::turn_based? tgenus::TURN_BASED: tgenus::HALF_REALTIME).name();
	genus_->set_label(strstr.str());

	connect_signal_mouse_left_click(
		*genus_
		, boost::bind(
			&tmp_create_game::genus
			, this
			, boost::ref(window)));

	era_ = find_widget<tbutton>(&window, "era", false, true);
	launch_game_ = find_widget<tbutton>(&window, "ok", false, true);

	find_widget<tslider>(&window, "experience_modifier", false, true)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "observers", false, true)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "time_limit", false, true)->set_visible(twidget::INVISIBLE);
			
	maximal_defeated_activity_ = find_widget<tbutton>(&window, "maximal_defeated_activity", false, true);
	
	connect_signal_mouse_left_click(
		*era_
		, boost::bind(
			&tmp_create_game::era
			, this
			, boost::ref(window)));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "password", false)
		, boost::bind(
			&tmp_create_game::password
			, this
			, boost::ref(window)));

	connect_signal_mouse_left_click(
		*maximal_defeated_activity_
		, boost::bind(
			&tmp_create_game::maximal_defeated_activity
			, this
			, boost::ref(window)));

	if (size_t(preferences::era()) < cfg_.child_count("era")) {
		era_index_ = preferences::era();
	}
	const config& era_cfg = cfg_.child("era", era_index_);
	era_->set_label(era_cfg["name"]);

	utils::string_map i18n_symbols;
	i18n_symbols["login"] = preferences::login();
	name_entry_->set_value(vgettext("$login|'s game", i18n_symbols));

	strstr.str("");
	strstr << game_config::maximal_defeated_activity;
	maximal_defeated_activity_->set_label(strstr.str());

	observers_->set_widget_value(window, preferences::allow_observers());

	trandom_map::pre_show(window);

	join();
}

void tmp_create_game::post_update_map(twindow& window, int select)
{
}

void tmp_create_game::update_map_settings(twindow& window)
{
	const bool use_scenario_settings = parameters_.saved_game;

	fog_->widget_set_enabled(window, !use_scenario_settings, false);
	shroud_->widget_set_enabled(window, !use_scenario_settings, false);
	
	turns_->widget_set_enabled(window, !use_scenario_settings, false);
	experience_->widget_set_enabled(window, !use_scenario_settings, false);

	if (use_scenario_settings) {
		fog_->set_widget_value(window, parameters_.scenario_data["fog"].to_bool());
		shroud_->set_widget_value(window, parameters_.scenario_data["shroud"].to_bool());
	
		turns_->set_widget_value(window, ::settings::get_turns(parameters_.scenario_data["turns"]));
		experience_->set_widget_value(window, ::settings::get_xp_modifier(parameters_.scenario_data["experience_modifier"]));

	} else {

		// Fixme we should store the value and reuse it later...
		fog_->set_widget_value(window, preferences::fog());
		shroud_->set_widget_value(window, preferences::shroud());
		
		turns_->set_widget_value(window, preferences::turns());
		experience_->set_widget_value(window, preferences::xp_modifier());
	}
}

void tmp_create_game::genus(twindow& window)
{
	common_genus(*genus_);
}

void tmp_create_game::era(twindow& window)
{
	// The possible eras to play
	std::vector<tval_str> eras;
	int index = 0;
	BOOST_FOREACH (const config &er, cfg_.child_range("era")) {
		eras.push_back(tval_str(index ++, er["name"]));
	}
	if (eras.empty()) {
		gui2::show_transient_message(gui_.video(), "", _("No eras found."));
		throw config::error(_("No eras found"));
	}

	gui2::tcombo_box dlg(eras, era_index_);
	dlg.show(gui_.video());

	era_index_ = dlg.selected_index();
	era_->set_label(eras[era_index_].str);
	preferences::set_era(era_index_);
}

void tmp_create_game::password(twindow& window)
{
	gui2::tmp_create_game_set_password pwd_dlg(parameters_.password);
	pwd_dlg.show(gui_.video());
	// parameters_.password = pwd_dlg.password();
}

void tmp_create_game::maximal_defeated_activity(twindow& window)
{
	// The possible eras to play
	std::vector<tval_str> activities;
	activities.push_back(tval_str(0, "0"));
	activities.push_back(tval_str(50, "50"));
	activities.push_back(tval_str(100, "100"));
	activities.push_back(tval_str(150, "150"));
	
	gui2::tcombo_box dlg(activities, game_config::maximal_defeated_activity);
	dlg.show(gui_.video());

	game_config::maximal_defeated_activity = dlg.selected_val();

	std::stringstream str;
	str << game_config::maximal_defeated_activity;
	maximal_defeated_activity_->set_label(str.str());
}

void tmp_create_game::post_show(twindow& window)
{
	num_turns_ = turns_->get_widget_value(window) < ::settings::turns_max? 
		turns_->get_widget_value(window): -1;

	const int mp_countdown_turn_bonus_val = preferences::countdown_turn_bonus();
	const int mp_countdown_action_bonus_val = preferences::countdown_action_bonus();
	const int mp_countdown_reservoir_time_val = preferences::countdown_reservoir_time();
	int mp_countdown_init_time_val = preferences::countdown_init_time();
	if (mp_countdown_reservoir_time_val > 0 && mp_countdown_init_time_val > mp_countdown_reservoir_time_val)
		mp_countdown_init_time_val = mp_countdown_reservoir_time_val;

	// Updates the values in the "parameters_" member to match
	// the values selected by the user with the widgets:
	parameters_.name = name_entry_->get_value();

	config::const_child_itors era_list = cfg_.child_range("era");
	std::advance(era_list.first, era_index_);
	parameters_.mp_era = (*era_list.first)["id"].str();

	// CHECK
	parameters_.mp_countdown_init_time = mp_countdown_init_time_val;
	parameters_.mp_countdown_turn_bonus = mp_countdown_turn_bonus_val;
	parameters_.mp_countdown_reservoir_time = mp_countdown_reservoir_time_val;
	parameters_.mp_countdown_action_bonus = mp_countdown_action_bonus_val;
	parameters_.mp_countdown = false;
	parameters_.village_gold = 2;
	parameters_.xp_modifier = experience_->get_widget_value(window);
	parameters_.fog_game = fog_->get_widget_value(window);
	parameters_.shroud_game = shroud_->get_widget_value(window);
	parameters_.allow_observers = observers_->get_widget_value(window);
	// parameters_.share_view = vision_combo_.selected() == 0;
	parameters_.share_view = true;
	// parameters_.share_maps = vision_combo_.selected() == 1;
	parameters_.share_maps = false;

	// treat password as checksum
	parameters_.password = game_config::checksum;
}

bool tmp_create_game::handle(int tag, tsock::ttype type, const config& data)
{
	if (type == tsock::t_disconnected && !local_only_) {
		launch_game_->get_window()->set_retval(twindow::CANCEL);
	}
	return false;
}

} // namespace gui2

