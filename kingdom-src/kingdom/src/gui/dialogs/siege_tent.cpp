/* $Id: hero_list.cpp 49598 2011-05-22 17:55:54Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Jörg Hinrichs <joerg.hinrichs@alice-dsl.de>
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

#include "gui/dialogs/siege_tent.hpp"

#include "map.hpp"
#include "game_display.hpp"
#include "gamestatus.hpp"
#include "formula_string_utils.hpp"
#include "serialization/parser.hpp"
#include "gettext.hpp"
#include "team.hpp"
#include "artifical.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/scroll_label.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/widgets/minimap.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "help.hpp"
#include "multiplayer.hpp"
#include "actions.hpp"
#include "marked-up_text.hpp"

#include <boost/bind.hpp>
#include <iomanip>

extern const char* interiors_png[];

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_game_load
 *
 * == Load a game ==
 *
 * This shows the dialog to select and load a savegame file.
 *
 * @begin{table}{dialog_widgets}
 *
 * txtFilter & & text & m &
 *         The filter for the listbox items. $
 *
 * savegame_list & & listbox & m &
 *         List of savegames. $
 *
 * -filename & & control & m &
 *         Name of the savegame. $
 *
 * -date & & control & o &
 *         Date the savegame was created. $
 *
 * preview_pane & & widget & m &
 *         Container widget or grid that contains the items for a preview. The
 *         visible status of this container depends on whether or not something
 *         is selected. $
 *
 * -minimap & & minimap & m &
 *         Minimap of the selected savegame. $
 *
 * -imgLeader & & image & m &
 *         The image of the leader in the selected savegame. $
 *
 * -lblScenario & & label & m &
 *         The name of the scenario of the selected savegame. $
 *
 * -lblSummary & & label & m &
 *         Summary of the selected savegame. $
 *
 * @end{table}
 */

extern void common_genus(tbutton& genus);

REGISTER_DIALOG(siege_tent)

tsiege_tent::tsiege_tent(game_display& disp, hero_map& heros, game_state& state, const config& game_config, tsubcontinent_param* param)
	: disp_(disp)
	, heros_(heros)
	, state_(state)
	, game_config_(game_config)
	, param_(param)
	, local_only_(false)
	, rflip_(true)
	, turns_(game_config::default_siege_turn)
	, attacker_map_str_()
	, defender_map_data_()
	, associate_members_()
	, loaded_defender_()
	, scenario_()
	, defender_(std::make_pair(-1, null_str))
	, reinforce_attacker_(std::make_pair(-1, null_str))
	, reinforce_defender_()
	, attacker_stratagem_(NULL)
	, defender_stratagem_(NULL)
{
}

void tsiege_tent::refresh_interior_ui(tlabel& label, const std::string& interior_str)
{
	int interior[tgroup::interior_count];
	tgroup::interior_from_str_internal(interior_str, interior);

	std::stringstream strstr;
	for (int n = 0; n < tgroup::interior_count; n ++) {
		if (n == tgroup::interior_count / 2) {
			strstr << "\n";
		} else if (n) {
			strstr << "    ";
		}

		strstr << help::tintegrate::generate_img(interiors_png[n]);
		strstr << std::setw(4) << std::setfill(' ') << interior[n];
	}
	label.set_label(strstr.str());
}

std::vector<std::vector<std::string> > tsiege_tent::crop_map(std::vector<std::vector<std::string> >& full_map_str, const map_location& from_loc, const map_location& to_loc)
{
	std::vector<std::vector<std::string> > ret;
	int total_siege_map_w = game_config::siege_map_w + 2;
	int total_siege_map_h = game_config::siege_map_h + 2;
	int x_start;
	int y_start;
	bool left = to_loc.x <= game_config::siege_map_w;
	rflip_ = false;

	if (from_loc.valid()) {
		if (left) {
			if (from_loc.x >= to_loc.x) {
				x_start = from_loc.x - to_loc.x;
			} else {
				x_start = 0;
			}
		} else {
			if (!rflip_) {
				if (from_loc.x - (to_loc.x - game_config::siege_map_w) < (int)full_map_str.size()) {
					x_start = from_loc.x - (to_loc.x - game_config::siege_map_w);
				} else {
					x_start = full_map_str.size() - 2; // because odd/even, maybe + 1
				}
			} else {
				x_start = from_loc.x - (2 * game_config::siege_map_w + 1 - to_loc.x);
			}
		}
		y_start = from_loc.y - to_loc.y;
		if (y_start >= full_map_str.front().size() - total_siege_map_h) {
			y_start = full_map_str.front().size() - total_siege_map_h;
		} else if (y_start < 0) {
			y_start = 0;
		}
	} else {
		x_start = rand() % (full_map_str.size() - total_siege_map_w);
		y_start = rand() % (full_map_str.front().size() - total_siege_map_h);
	}

	if (to_loc.x & 1) {
		if (left) {
			x_start += (x_start & 1) ^ 1;
		} else {
			x_start += x_start & 1;
		}
	} else {
		if (left) {
			x_start += x_start & 1;
		} else {
			x_start += (x_start & 1) ^ 1;
		}
	}
	ret.resize(total_siege_map_w);
	for (int i = 0; i < total_siege_map_w; i ++) {
		if (left || !rflip_) {
			std::vector<std::string>::iterator begin = full_map_str[x_start + i].begin();
			std::advance(begin, y_start);
			std::vector<std::string>::iterator end = begin;
			std::advance(end, total_siege_map_h);
			std::copy(begin, end, std::back_inserter(ret[i]));
		} else {
			std::vector<std::string>::iterator begin = full_map_str[x_start - i].begin();
			std::advance(begin, y_start);
			std::vector<std::string>::iterator end = begin;
			std::advance(end, total_siege_map_h);
			std::copy(begin, end, std::back_inserter(ret[i]));
		}
	}
	return ret;
}

void tsiege_tent::pre_show(CVideo& /*video*/, twindow& window)
{
	std::stringstream strstr;

	genus_ = find_widget<tbutton>(&window, "genus", false, true);
	tent::turn_based = false;
	strstr.str("");
	strstr << unit_types.genus(tent::turn_based? tgenus::TURN_BASED: tgenus::HALF_REALTIME).name();
	genus_->set_label(strstr.str());

	connect_signal_mouse_left_click(
		*genus_
		, boost::bind(
			&tsiege_tent::genus
			, this
			, boost::ref(window)));

	scenario_ = load_campagin_scenario(state_.classification().campaign, state_.classification().scenario, state_.classification().campaign_type);
	scenario_["turns"] = turns_;

	if (!param_) {
		if (!group.map().empty()) {
			if (verify_siege_map_data(game_config_, group.map())) {
				form_map_str_rectangle(group.map(), attacker_map_str_);
			}
		}
		if (attacker_map_str_.empty()) {
			const config& map_scenario = game_config_.find_child("multiplayer", "id", game_config::campaign_id_siege);
			form_map_str_rectangle(map_scenario["map_data"], attacker_map_str_);
		}

		if (associate_members_.empty()) {
			strstr.str("");
			strstr << group.leader().name();
			for (std::vector<tgroup::tassociate>::const_iterator it = group.associates().begin(); it != group.associates().end(); ++ it) {
				strstr << ",";
				strstr << it->username;
			}
			associate_members_ = http::membershiplist_map(disp_, heros_, false, strstr.str(), false);
			if (associate_members_.empty()) {
				local_only_ = true;
				associate_members_.insert(std::make_pair(group.leader().uid(), group.to_membership()));
			} else {
				// first is local username, adjust "now" paramter according to it.
				http::membership& m = associate_members_.find(group.leader().uid())->second;
				group.from_local_membership(disp_, heros_, m, false);
				
				m.layout = group.layout();
				m.map = group.map();
			}
			loaded_defender_.insert(group.leader().uid());
		}

	} else {
		map_location attacker_fort_loc, defender_fort_loc;
		BOOST_FOREACH (const config& side_cfg, scenario_.child_range("side")) {
			const config& artifical_cfg = side_cfg.child("artifical");
			controller_tag::CONTROLLER control = controller_tag::find(side_cfg["controller"].str());
			if (control == controller_tag::HUMAN) {
				attacker_fort_loc = map_location(artifical_cfg["x"], artifical_cfg["y"]);
			} else {
				defender_fort_loc = map_location(artifical_cfg["x"], artifical_cfg["y"]);
			}
		}

		std::vector<std::vector<std::string> > full_map_str;
		form_map_str_rectangle(param_->scenario["map_data"].str(), full_map_str);

		attacker_map_str_ = crop_map(full_map_str, param_->aloc, attacker_fort_loc);
		param_->defender_map_str = crop_map(full_map_str, param_->dloc, defender_fort_loc);

		associate_members_ = param_->members;
		loaded_defender_.insert(param_->defender);
	}

	// turns
	tlabel* label = find_widget<tlabel>(&window, "turns", false, true);
	strstr.str("");
	strstr << turns_;
	label->set_label(strstr.str());

	label = find_widget<tlabel>(&window, "attacker_interior", false, true);
	refresh_interior_ui(*label, associate_members_.find(group.leader().uid())->second.interior);

	tbutton* button = find_widget<tbutton>(&window, "save_map", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
			&tsiege_tent::save_map
			, this
			, boost::ref(window)));
	strstr.str("");
	strstr << help::tintegrate::generate_format(dgettext("wesnoth-lib", "Save Map"), "blue");
	button->set_label(strstr.str());
	if (local_only_ || param_) {
		button->set_active(false);
	}

	// attacker
	label = find_widget<tlabel>(&window, "attacker_username", false, true);
	label->set_label(group.leader().name());

	attacker_stratagem_ = &group.stratagem_from_layout_str();
	button = find_widget<tbutton>(&window, "attacker_stratagem", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
			&tsiege_tent::browse_stratagem
			, this
			, boost::ref(window)));
	button->set_label(attacker_stratagem_->name());

	tscroll_label* label2 = find_widget<tscroll_label>(&window, "attacker_stratagem_description", false, true);
	label2->set_label(attacker_stratagem_->description());

	tcontrol* control = find_widget<tcontrol>(&window, "attacker_portrait", false, true);
	control->set_label(group.leader().image());

	button = find_widget<tbutton>(&window, "reinforce_attacker", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
			&tsiege_tent::reinforce_attacker
			, this
			, boost::ref(window)));

	std::vector<tgroup::tassociate> associates = reinforce_attacker_list();
	if (!associates.empty()) {
		set_reinforce_attacker(window, associates.front().uid, associates.front().username);
	} else {
		button->set_active(false);
	}

	// defender
	button = find_widget<tbutton>(&window, "defender_username", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
			&tsiege_tent::defender_username
			, this
			, boost::ref(window)));
	button->set_active(!param_);

	if (!param_) {
		associates = defender_list();
		if (!local_only_ && associates.size() == 1) {
			strstr.str("");
			utils::string_map symbols;
			symbols["associate"] = help::tintegrate::generate_format(_("Associate"), "yellow");
			symbols["side"] = help::tintegrate::generate_format(_("Player side"), "yellow");
			if (group.associates().empty()) {
				strstr << vgettext("wesnoth-lib", "Require seting '$associate' in '$side'. Player that no agreement can be defender, ally agreement can be reinforce.", symbols);
			} else {
				strstr << vgettext("wesnoth-lib", "No attackable player! Require seting '$associate' in '$side', and set at least one no agreement.", symbols);
			}
			show_message(disp_.video(), "", strstr.str());
		}
		set_defender(window, associates.front().uid, associates.front().username);

	} else {
		const http::membership& m = associate_members_.find(param_->defender)->second;
		set_defender(window, m.uid, m.name);
	}

	refresh_reinforce_defender_str(window);
}

void tsiege_tent::refresh_reinforce_defender_str(twindow& window)
{
	tscroll_label* scroll_label = find_widget<tscroll_label>(&window, "reinforce_defender", false, true);
	const http::membership& m = associate_members_.find(defender_.first)->second;

	reinforce_defender_.clear();
	if (!local_only_) {
		std::vector<tgroup::tassociate> associates = tgroup::associate_from_str2(m.associate);
		for (std::vector<tgroup::tassociate>::const_iterator it = associates.begin(); it != associates.end(); ++ it) {
			if (it->uid == reinforce_attacker_.first) {
				continue;
			}
			if (it->agreement == tgroup::tassociate::ally || it->agreement == tgroup::tassociate::requestterminate) {
				reinforce_defender_.push_back(*it);
			}
		}
	}

	std::stringstream strstr;
	for (std::vector<tgroup::tassociate>::const_iterator it = reinforce_defender_.begin(); it != reinforce_defender_.end(); ++ it) {
		if (it != reinforce_defender_.begin()) {
			strstr << ", ";
		}
		strstr << it->username;
	}
	scroll_label->set_label(strstr.str());
}

void generate_fix_unit(config& unit_cfg, const type_heros_pair& pair, const map_location& loc, std::set<std::string>& traits)
{
	unit_cfg["type"] = pair.first->id();

	std::stringstream strstr;
	strstr.str("");
	for (std::vector<const hero*>::const_iterator it = pair.second.begin(); it != pair.second.end(); ++ it) {
		int number = (*it)->number_;
		if (it != pair.second.begin()) {
			strstr << ", ";
		}
		strstr << number;
	}
	unit_cfg["heros_army"] = strstr.str();

	if (pair.first->especial() != NO_ESPECIAL) {
		unit_cfg["especial"] = unit_types.especial(pair.first->especial()).id_;
	}
	
	unit_cfg["cityno"] = 0;
	unit_cfg["task"] = unit::TASK_STATION;

	unit_cfg["x"] = loc.x + 1;
	unit_cfg["y"] = loc.y + 1;

	strstr.str("");
	for (std::set<std::string>::const_iterator it = traits.begin(); it != traits.end(); ++ it) {
		if (it != traits.begin()) {
			strstr << ",";
		}
		strstr << *it;
	}
	unit_cfg["traits"] = strstr.str();
}

void generate_event_unit(config& unit_cfg, const type_heros_pair& pair, int side, const map_location& loc, std::set<std::string>& traits)
{
	unit_cfg["type"] = pair.first->id();

	std::stringstream strstr;
	strstr.str("");
	for (std::vector<const hero*>::const_iterator it = pair.second.begin(); it != pair.second.end(); ++ it) {
		int number = (*it)->number_;
		if (it != pair.second.begin()) {
			strstr << ", ";
		}
		strstr << number;
	}
	unit_cfg["heros_army"] = strstr.str();

	unit_cfg["side"] = side + 1;
	unit_cfg["cityno"] = 0;

	if (loc.valid()) {
		unit_cfg["x"] = loc.x + 1;
		unit_cfg["y"] = loc.y + 1;
	}

	strstr.str("");
	for (std::set<std::string>::const_iterator it = traits.begin(); it != traits.end(); ++ it) {
		if (it != traits.begin()) {
			strstr << ",";
		}
		strstr << *it;
	}
	unit_cfg["traits"] = strstr.str();
}

void tsiege_tent::post_show(twindow& window)
{
	if (get_retval() != gui2::twindow::OK) {
		return;
	}
	if (defender_.first < 0) {
		return;
	}

	std::stringstream strstr;
	config* human_side = NULL;
	config* ai_side = NULL;

	std::string attacker_technologies, defender_technologies;
	http::membership& m = associate_members_.find(defender_.first)->second;
	tgroup g;
	g.from_membership(heros_, m);
	other_group.insert(std::make_pair(g.leader().number_, g));
	tgroup& defender_group = other_group.find(g.leader().number_)->second;
	int sides = scenario_.child_count("side");
	for (int n = 0; n < sides; n ++) {
		config& side = scenario_.child("side", n);
		config& artifical_cfg = side.child("artifical");
		const ttechnology* stratagem;
		// think there is one city only.
		if (side["controller"] == "human") {
			group.to_side_cfg(side, rand());
			human_side = &side;
			stratagem = attacker_stratagem_;

			attacker_technologies = side["technologies"].str();

			// set stratagem of human side.
			strstr.str("");
			strstr << attacker_technologies;
			if (strstr.str().empty()) {
				strstr << stratagem->id();
			} else {
				strstr << "," << stratagem->id();
			}
			side["technologies"] = strstr.str();
			side["build"] = null_str;

			if (param_ && param_->attacker_city != HEROS_ROAM_CITY) {
				artifical_cfg["heros_army"] = param_->attacker_city;
			}

		} else if (side["controller"] == "ai") {
			defender_group.to_side_cfg(side, rand());
			defender_technologies = side["technologies"].str();
			side["build"] = null_str;

			ai_side = &side;

			if (param_ && param_->defender_city != HEROS_ROAM_CITY) {
				artifical_cfg["heros_army"] = param_->defender_city;
			}
		}
	}

	if (reinforce_attacker_.first >= 0) {
		g.from_membership(heros_, associate_members_.find(reinforce_attacker_.first)->second);
		other_group.insert(std::make_pair(g.leader().number_, g));

		config& side = scenario_.add_child("side");
		side["side"] = sides + 1;
		side["leader"] = g.leader().number_;
		side["controller"] = "ai";
		side["gold"] = 0;
		side["income"] = 0;
		side["technologies"] = attacker_technologies;

		side["team_name"] = (*human_side)["side"].to_int();
		(*human_side)["team_name"] = sides + 1;

		genereate_roam_troops(side, g, game_config::inforce_attacker_loc, "start");

		sides = scenario_.child_count("side");
	}

	if (!reinforce_defender_.empty()) {
		tgroup::tassociate& a = reinforce_defender_[rand() % reinforce_defender_.size()];
		if (associate_members_.find(a.uid) != associate_members_.end()) {
			g.from_membership(heros_, associate_members_.find(a.uid)->second);
		} else {
			http::membership m2 = http::membership_from_uid(disp_, heros_, false, a.uid);
			if (m2.uid >= 0) {
				g.from_membership(heros_, m2);
			} else {
				g.reset();
			}
		}
		if (g.valid()) {
			other_group.insert(std::make_pair(g.leader().number_, g));

			config& side = scenario_.add_child("side");
			side["side"] = sides + 1;
			side["leader"] = g.leader().number_;
			side["controller"] = "ai";
			side["gold"] = 0;
			side["income"] = 0;
			side["technologies"] = defender_technologies;

			side["team_name"] = (*ai_side)["side"].to_int();
			(*ai_side)["team_name"] = sides + 1;

			genereate_roam_troops(side, g, game_config::inforce_defender_loc, "turn 2");
		}
	}
}

void tsiege_tent::genereate_roam_troops(config& side_cfg, tgroup& g, const map_location& loc, const std::string& event_name)
{
	int side_index = side_cfg["side"].to_int() - 1;
	std::vector<hero*> fresh_heros;
	std::vector<const hero*> expedite_heros;

	size_t max_army_heros;
	if (((int)g.members().size() + 1) / 3 >= game_config::max_reinforcements) {
		max_army_heros = 3;
	} else {
		max_army_heros = 2;
	}

	for (std::vector<tgroup::tmember>::const_iterator it = g.members().begin(); it != g.members().end(); ++ it) {
		fresh_heros.push_back(it->h);
	}
	std::sort(fresh_heros.begin(), fresh_heros.end(), compare_leadership);

	std::set<std::string> traits;

	expedite_heros.clear();
	expedite_heros.push_back(&g.leader());
	const unit_type* ut = unit_types.keytypes().begin()->second;
	if (g.leader().utype_ != HEROS_NO_UTYPE) {
		ut = unit_types.keytype(g.leader().utype_);
	}
	select_expedite_heros(heros_, max_army_heros, fresh_heros, expedite_heros, *ut);
	generate_fix_unit(side_cfg.add_child("unit"), std::make_pair(ut, expedite_heros), loc, traits);
	for (std::vector<const hero*>::const_iterator it = expedite_heros.begin(); it != expedite_heros.end(); ++ it) {
		hero& h = heros_[(**it).number_];
		h.status_ = hero_status_idle;
	}

	config* event_cfg_ptr = NULL;
	if (config& sub = scenario_.find_child("event", "name", event_name)) {
		event_cfg_ptr = &sub;
	} else {
		event_cfg_ptr = &scenario_.add_child("event");
		(*event_cfg_ptr)["name"] = event_name;
	}
	config& event_cfg = *event_cfg_ptr;

	int allocatable = game_config::max_reinforcements - 1;
	while (!fresh_heros.empty() && allocatable > 0) {
		const hero& master = *fresh_heros[0];
		expedite_heros.clear();
		expedite_heros.push_back(&master);
		ut = unit_types.keytypes().begin()->second;
		if (master.utype_ != HEROS_NO_UTYPE) {
			ut = unit_types.keytype(master.utype_);
		}
		fresh_heros.erase(fresh_heros.begin());
		select_expedite_heros(heros_, max_army_heros, fresh_heros, expedite_heros, *ut);
		generate_event_unit(event_cfg.add_child("unit"), std::make_pair(ut, expedite_heros), side_index, loc, traits);
		for (std::vector<const hero*>::const_iterator it = expedite_heros.begin(); it != expedite_heros.end(); ++ it) {
			hero& h = heros_[(**it).number_];
			h.status_ = hero_status_idle;
		}
		allocatable --;
	}
}

void tsiege_tent::reinforce_attacker(twindow& window)
{
	std::stringstream strstr;
	std::vector<std::string> items;
	std::vector<tval_str> associate_map;
	int actived_index = 0;

	std::set<int> agreement;
	agreement.insert(tgroup::tassociate::ally);
	agreement.insert(tgroup::tassociate::requestterminate);

	std::vector<tgroup::tassociate> candidate = group.associates_from_agreement(agreement);
	for (std::vector<tgroup::tassociate>::const_iterator it = candidate.begin(); it != candidate.end(); ++ it) {
		strstr.str("");
		strstr << it->username;
		strstr << "(" <<  utils::split(associate_members_.find(it->uid)->second.member).size() << ")";
		associate_map.push_back(tval_str(it->uid, strstr.str()));
	}

	for (std::vector<tval_str>::iterator it = associate_map.begin(); it != associate_map.end(); ++ it) {
		items.push_back(it->str);
		if (reinforce_attacker_.first == it->val) {
			actived_index = std::distance(associate_map.begin(), it);
		}
	}
	
	gui2::tcombo_box dlg(items, actived_index);
	dlg.show(disp_.video());

	int selected = dlg.selected_index();
	if (selected == actived_index) {
		return;
	}

	const tgroup::tassociate& a = group.associate(associate_map[selected].val);
	set_reinforce_attacker(window, a.uid, a.username);

	refresh_reinforce_defender_str(window);
}

std::vector<tgroup::tassociate> tsiege_tent::defender_list() const
{
	std::set<int> agreement;
	agreement.insert(tgroup::tassociate::none);
	agreement.insert(tgroup::tassociate::requestally);

	std::vector<tgroup::tassociate> ret;
	if (!local_only_) {	
		ret = group.associates_from_agreement(agreement);
	}
	ret.push_back(tgroup::tassociate(group.leader().name(), group.leader().uid(), tgroup::tassociate::none, 0));

	return ret;
}

std::vector<tgroup::tassociate> tsiege_tent::reinforce_attacker_list() const
{
	std::set<int> agreement;
	agreement.insert(tgroup::tassociate::ally);
	agreement.insert(tgroup::tassociate::requestterminate);

	std::vector<tgroup::tassociate> ret;
	if (!local_only_) {
		ret = group.associates_from_agreement(agreement);
	}

	return ret;
}

void tsiege_tent::defender_username(twindow& window)
{
	std::stringstream strstr;
	std::vector<std::string> items;
	std::vector<tval_str> associate_map;
	int actived_index = 0;

	std::vector<tgroup::tassociate> candidate = defender_list();
	for (std::vector<tgroup::tassociate>::const_iterator it = candidate.begin(); it != candidate.end(); ++ it) {
		strstr.str("");
		strstr << it->username;
		strstr << "(" <<  utils::split(associate_members_.find(it->uid)->second.member).size() << ")";
		associate_map.push_back(tval_str(it->uid, strstr.str()));
	}

	for (std::vector<tval_str>::iterator it = associate_map.begin(); it != associate_map.end(); ++ it) {
		items.push_back(it->str);
		if (defender_.first == it->val) {
			actived_index = std::distance(associate_map.begin(), it);
		}
	}
	
	gui2::tcombo_box dlg(items, actived_index);
	dlg.show(disp_.video());

	int selected = dlg.selected_index();
	if (selected == actived_index) {
		return;
	}

	int uid = associate_map[selected].val;
	set_defender(window, uid, associate_members_.find(uid)->second.name);

	refresh_reinforce_defender_str(window);
}

void tsiege_tent::browse_stratagem(twindow& window)
{
	attacker_stratagem_ = &::browse_stratagem(disp_, stratagem_tag::find(attacker_stratagem_->id()));

	tbutton* button = find_widget<tbutton>(&window, "attacker_stratagem", false, true);
	button->set_label(attacker_stratagem_->name());

	tscroll_label* label2 = find_widget<tscroll_label>(&window, "attacker_stratagem_description", false, true);
	label2->set_label(attacker_stratagem_->description());
}

void tsiege_tent::set_defender(twindow& window, int uid, const std::string& username)
{
	std::stringstream strstr;
	if (uid != group.leader().uid()) {
		strstr << username;
	} else {
		strstr << help::tintegrate::generate_format(username, "red");
	}
	strstr << "(" <<  utils::split(associate_members_.find(uid)->second.member).size() << ")";
	find_widget<tbutton>(&window, "defender_username", false, true)->set_label(strstr.str());

	defender_.first = uid;
	defender_.second = username;

	http::membership& m = associate_members_.find(uid)->second;
	if (loaded_defender_.find(uid) == loaded_defender_.end()) {
		http::membership m2 = http::membership_from_uid(disp_, heros_, false, uid);
		if (m2.uid >= 0) {
			m = m2;
			loaded_defender_.insert(uid);
		}
	}

	tlabel* label = find_widget<tlabel>(&window, "defender_interior", false, true);
	refresh_interior_ui(*label, m.interior);

	tcontrol* control = find_widget<tcontrol>(&window, "defender_portrait", false, true);
	int gender = hero::gender_from_filed_str(m.field);
	control->set_label(hero::image_from_uid(uid, gender, false));

	if (!param_) {
		defender_stratagem_ = &tgroup::stratagem_from_layout_str_internal(m.layout);
	} else {
		defender_stratagem_ = &stratagem_tag::technology(stratagem_tag::miao_shou_hui_chun);
	}
	label = find_widget<tlabel>(&window, "defender_stratagem", false, true);
	label->set_label(defender_stratagem_->name());

	tscroll_label* label2 = find_widget<tscroll_label>(&window, "defender_stratagem_description", false, true);
	label2->set_label(defender_stratagem_->description());

	if (!param_) {
		defender_map_data_.clear();
		if (!m.map.empty()) {
			if (verify_siege_map_data(game_config_, m.map)) {
				defender_map_data_ = m.map;
			}
		}
		if (defender_map_data_.empty()) {
			const config& map_scenario = game_config_.find_child("multiplayer", "id", game_config::campaign_id_siege);
			defender_map_data_ = map_scenario["map_data"].str();
		}
		scenario_["map_data"] = combine_map(defender_map_data_, attacker_map_str_, rflip_);
		set_save_map_status(window);
	} else {
		scenario_["map_data"] = combine_map(param_->defender_map_str, attacker_map_str_, rflip_);
	}

	tminimap& minimap = find_widget<tminimap>(&window, "minimap", false);
	minimap.set_config(&game_config_);
	minimap.set_map_data(scenario_["map_data"]);
}

void tsiege_tent::set_reinforce_attacker(twindow& window, int uid, const std::string& username)
{
	std::stringstream strstr;
	strstr << username;
	strstr << "(" <<  utils::split(associate_members_.find(uid)->second.member).size() << ")";
	find_widget<tbutton>(&window, "reinforce_attacker", false, true)->set_label(strstr.str());

	reinforce_attacker_.first = uid;
	reinforce_attacker_.second = username;
}

void tsiege_tent::save_map(twindow& window)
{
	std::stringstream strstr;
	utils::string_map symbols;

	symbols["username"] = help::tintegrate::generate_format(defender_.second, "green");
	strstr.str("");
	strstr << vgettext("wesnoth-lib", "Do you want to save $username|'s map to your city map?", symbols);
	int res = gui2::show_message(disp_.video(), "", strstr.str(), gui2::tmessage::yes_no_buttons);
	if (res != gui2::twindow::OK) {
		return;
	}

	std::map<int, std::string> block;
	std::string map_data;
	block.insert(std::make_pair((int)http::block_tag_map, defender_map_data_));
	http::membership m = http::upload_data(disp_, heros_, block, false);
	if (m.uid >= 0) {
		group.from_local_membership(disp_, heros_, m, true);

		http::membership& m2 = associate_members_.find(group.leader().uid())->second;
		m2.layout = group.layout();
		m2.map = group.map();

		form_map_str_rectangle(m2.map, attacker_map_str_);
		scenario_["map_data"] = combine_map(defender_map_data_, attacker_map_str_, rflip_);

		tminimap& minimap = find_widget<tminimap>(&window, "minimap", false);
		minimap.set_config(&game_config_);
		minimap.set_map_data(scenario_["map_data"]);

		set_save_map_status(window);
	}
}

void tsiege_tent::set_save_map_status(twindow& window)
{
	std::vector<std::vector<std::string> > defender_map_str;
	form_map_str_rectangle(defender_map_data_, defender_map_str);

	tbutton* button = find_widget<tbutton>(&window, "save_map", false, true);
	button->set_active(attacker_map_str_ != defender_map_str);
}

void tsiege_tent::genus(twindow& window)
{
	common_genus(*genus_);
}

} // namespace gui2
