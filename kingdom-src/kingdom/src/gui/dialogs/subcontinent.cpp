/* $Id: title_screen.cpp 48740 2011-03-05 10:01:34Z mordante $ */
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

#include "gui/dialogs/subcontinent.hpp"

#include "game_display.hpp"
#include "game_config.hpp"
#include "gettext.hpp"
#include "gamestatus.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/hexmap.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/dialogs/siege_tent.hpp"
#include "gui/dialogs/message.hpp"
#include "actions.hpp"
#include "help.hpp"
#include "map.hpp"
#include "formula_string_utils.hpp"

#include <boost/bind.hpp>

#include <algorithm>

extern const config& state_for_siege_uh(game_state& state, const config& game_config, bool subcontinent);
extern void state_for_siege_bh(game_state& state, const config& campaign, config& scenario);
extern const config& state_for_scenario_uh(game_state& state, const config& game_config, const std::string& id, bool subcontinent);

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_title_screen
 *
 * == Title screen ==
 *
 * This shows the title screen.
 *
 * @begin{table}{dialog_widgets}
 * tutorial & & button & m &
 *         The button to start the tutorial. $
 *
 * campaign & & button & m &
 *         The button to start a campaign. $
 *
 * multiplayer & & button & m &
 *         The button to start multiplayer mode. $
 *
 * load & & button & m &
 *         The button to load a saved game. $
 *
 * editor & & button & m &
 *         The button to start the editor. $
 *
 * addons & & button & m &
 *         The button to start managing the addons. $
 *
 * language & & button & m &
 *         The button to select the game language. $
 *
 * credits & & button & m &
 *         The button to show Wesnoth's contributors. $
 *
 * quit & & button & m &
 *         The button to quit Wesnoth. $
 *
 * tips & & multi_page & m &
 *         A multi_page to hold all tips, when this widget is used the area of
 *         the tips doesn't need to be resized when the next or previous button
 *         is pressed. $
 *
 * -tip & & label & o &
 *         Shows the text of the current tip. $
 *
 * -source & & label & o &
 *         The source (the one who's quoted or the book referenced) of the
 *         current tip. $
 *
 * next_tip & & button & m &
 *         The button show the next tip of the day. $
 *
 * previous_tip & & button & m &
 *         The button show the previous tip of the day. $
 *
 * logo & & progress_bar & o &
 *         A progress bar to "animate" the Wesnoth logo. $
 *
 * revision_number & & control & o &
 *         A widget to show the version number when the version number is
 *         known. $
 *
 * @end{table}
 */

REGISTER_DIALOG(subcontinent)

extern int ready_outer_anim(game_display& disp, int type, const config& cfg, bool start, bool cycles);
#define ownership_is_uid(ownership)	((ownership) >= HEROS_MAX_HEROS)
#define uid_from_ownership(ownership)	((ownership) - HEROS_MAX_HEROS)

tsubcontinent::tcity::tcity(const config& cfg)
	: number(cfg["heros_army"].to_int())
	, cityno(cfg["cityno"].to_int())
	, loc(map_location(cfg["x"].to_int(), cfg["y"].to_int()))
	, ownership(-1)
	, endurance(100)
	, times(0)
{
}

tsubcontinent::tparam::tparam(const config& main_cfg)
	: id(main_cfg["id"].str())
	, name(main_cfg["name"].str())
	, npc(HEROS_INVALID_NUMBER)
	, scenario()
	, cities()
	, http_subcontinent()
	, valid(false)
{
	scenario = load_campagin_scenario(id, main_cfg["first_scenario"], "scenario");

	BOOST_FOREACH (const config& side_cfg, scenario.child_range("side")) {
		controller_tag::CONTROLLER control = controller_tag::find(side_cfg["controller"].str());
		if (control == controller_tag::HUMAN) {
			npc = side_cfg["leader"].to_int();
		}
		BOOST_FOREACH (const config& artifical_cfg, side_cfg.child_range("artifical")) {
			cities.push_back(tcity(artifical_cfg));
			tcity& city = cities.back();
			if (control == controller_tag::HUMAN) {
				city.ownership = npc;
			}
		}
	}
}

tsubcontinent::tcity& tsubcontinent::tparam::find_city(int cityno)
{
	std::vector<tcity>::iterator it = cities.begin();
	for (; it != cities.end(); ++ it) {
		if (it->cityno == cityno) {
			break;
		}
	}
	return *it;
}

tsubcontinent::tsubcontinent(game_display& disp, hero_map& heros, game_state& state, const config& game_config)
	: disp_(disp)
	, heros_(heros)
	, state_(state)
	, game_config_(game_config)
	, associate_members_()
	, cursel_(NULL)
	, curcity_(NULL)
	, my_city_(NULL)
	, npc_side_(NULL)
	, local_only_(false)
	, attack_(NULL)
	, repair_(NULL)
	, discard_(NULL)
{
}

tsubcontinent::~tsubcontinent()
{
	std::map<int, unit_animation>& screen_anims = disp_.screen_anims();
	screen_anims.clear();
}

void tsubcontinent::post_build(CVideo& video, twindow& window)
{
}

void tsubcontinent::city_selected(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "city_table", false);
	int selected_row = list.get_selected_row();
	tgrid* grid_ptr = list.get_row_grid(selected_row);
	unsigned int cookie = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true))->get_data();

	switch_to_city(window, cursel_->find_city(cookie));
}

void tsubcontinent::refresh_title_flag(twindow& window) const
{
	std::stringstream strstr;
	utils::string_map symbols;
	
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	strstr.str("");
	strstr << dgettext("wesnoth-lib", "Crusage for subcontinent");
	label->set_label(strstr.str());

	label = find_widget<tlabel>(&window, "flag", false, true);
	strstr.str("");
	strstr << "(" << help::tintegrate::generate_img("misc/coin.png~SCALE(24, 24)") << group.coin();
	strstr << "  " << help::tintegrate::generate_img("misc/score.png~SCALE(24, 24)") << group.score();
	// tax
	strstr << "    " << help::tintegrate::generate_img("misc/tax.png~SCALE(24, 24)");
	strstr << help::tintegrate::generate_format(group.tax(), "green");

	strstr << ")";
	label->set_label(strstr.str());
}

void tsubcontinent::refresh_city_table(twindow& window, const tparam& param, int cursel) const
{
	std::stringstream strstr;
	tlistbox& list = find_widget<tlistbox>(&window, "city_table", false);
	list.clear();
	for (std::vector<tcity>::const_iterator it = param.cities.begin(); it != param.cities.end(); ++ it) {
		const tcity& city = *it;
		const http::membership* m = NULL;

		if (city.ownership >= HEROS_MAX_HEROS) {
			int uid = uid_from_ownership(city.ownership);
			std::map<int, http::membership>::const_iterator find = associate_members_.find(uid);
			if (find != associate_members_.end()) {
				m = &find->second;
			}
		}

		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		strstr.str("");
		strstr << heros_[city.number].name();
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("name", list_item));

		strstr.str("");
		if (city.ownership >= HEROS_MAX_HEROS) {
			int uid = uid_from_ownership(city.ownership);
			if (m) {
				std::string color;
				if (uid == group.leader().uid()) {
					color = "green";
				} else {
					color = "red";
				}
				strstr << help::tintegrate::generate_format(m->name, color);
			} else {
				strstr << help::tintegrate::generate_format(uid, "red");
			}
		} else if (city.ownership >= 0) {
			strstr << help::tintegrate::generate_format(heros_[cursel_->npc].name(), "yellow");
		} else {
			strstr << "--";
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("ownership", list_item));

		strstr.str("");
		if (m) {
			std::vector<std::string> vstr = utils::split(m->member);
			strstr << vstr.size();
		} else {
			strstr << "--";
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("hero", list_item));

		strstr.str("");
		if (m) {
			strstr << city.endurance << "(";
			strstr << city.times;
			strstr << ")";
		} else {
			strstr << "--";
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("endurance", list_item));

		list.add_row(list_item_item);

		tgrid* grid_ptr = list.get_row_grid(list.get_item_count() - 1);
		ttoggle_panel* toggle = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true));
		toggle->set_data(city.cityno);
	}
	
	if (!param.cities.empty()) {
		if (cursel >= (int)param.cities.size()) {
			cursel = (int)param.cities.size() - 1;
		}
		list.select_row(cursel);
	}
}

const int max_city_endurance = 100;

void tsubcontinent::refresh_action_button(twindow& window, tcity* city)
{
	const int max_fallen = 4;
	bool can_attack = true;
	bool can_repair = true;
	bool can_discard = true;

	if (local_only_ || !city) {
		can_attack = city && city->ownership < 0;
		can_repair = can_discard = false;

	} else if (city->ownership >= HEROS_MAX_HEROS) {
		int uid = uid_from_ownership(city->ownership);
		if (uid == group.leader().uid()) {
			can_attack = false;
			can_repair = city->endurance < max_city_endurance;
			can_discard = true;
		} else {
			can_attack = city->times < max_fallen;
			can_repair = false;
			can_discard = false;
		}

	} else if (city->ownership >= 0) {
		can_attack = can_repair = can_discard = false;

	} else {
		can_attack = true;
		can_repair = false;
		can_discard = false;
	}
	attack_->set_active(can_attack);
	repair_->set_active(can_repair);
	discard_->set_active(can_discard);
}

void tsubcontinent::pre_show(CVideo& video, twindow& window)
{
	refresh_title_flag(window);

	std::set<int> ownerships;
	ownerships.insert(group.leader().uid()); // must reget player's membership
	std::map<std::string, http::tsubcontinent_record> http_subcontinents = http::list_subcontinent(disp_, heros_);
	for (std::map<std::string, http::tsubcontinent_record>::const_iterator it = http_subcontinents.begin(); it != http_subcontinents.end(); it ++) {
		for (std::map<int, http::tsubcontinent_city>::const_iterator it2 = it->second.city.begin(); it2 != it->second.city.end(); ++ it2) {
			ownerships.insert(it2->second.uid);
		}
	}

	std::stringstream strstr;
	strstr.str("");
	for (std::set<int>::const_iterator it = ownerships.begin(); it != ownerships.end(); ++ it) {
		if (it != ownerships.begin()) {
			strstr << ",";
		}
		strstr << *it;
	}

	if (!http_subcontinents.empty()) {
		associate_members_ = http::membershiplist_map(disp_, heros_, false, strstr.str(), true);
	}

	if (associate_members_.empty()) {
		local_only_ = true;
		associate_members_.insert(std::make_pair(group.leader().uid(), group.to_membership()));
	} else {
		// find local username, adjust "now" paramter according to it.
		http::membership& m = associate_members_.find(group.leader().uid())->second;
		group.from_local_membership(disp_, heros_, m, false);
		
		m.layout = group.layout();
		m.map = group.map();
	}

	thexmap& hexmap = find_widget<thexmap>(&window, "hexmap", false);
	hexmap.set_config(&game_config_);
	hexmap.set_callback_size_change(boost::bind(&tsubcontinent::size_change, this, _1));

	attack_ = &find_widget<tbutton>(&window, "attack", false);
	connect_signal_mouse_left_click(
		*attack_
		, boost::bind(
			&tsubcontinent::attack
			, this
			, boost::ref(window)));

	repair_ = &find_widget<tbutton>(&window, "repair", false);
	connect_signal_mouse_left_click(
		*repair_
		, boost::bind(
			&tsubcontinent::repair
			, this
			, boost::ref(window)));

	discard_ = &find_widget<tbutton>(&window, "discard", false);
	connect_signal_mouse_left_click(
		*discard_
		, boost::bind(
			&tsubcontinent::discard
			, this
			, boost::ref(window)));

	BOOST_FOREACH (const config& c, game_config_.child_range("campaign")) {
		if (!c["subcontinent"].to_bool()) {
			continue;
		}
		if (mode_tag::find(c["mode"].str()) != mode_tag::RPG) {
			continue;
		}
		params_.push_back(tparam(c));
		tparam& param = params_.back();

		std::map<std::string, http::tsubcontinent_record>::iterator find = http_subcontinents.find(param.id);
		if (find != http_subcontinents.end()) {
			param.http_subcontinent = find->second;

			for (std::vector<tcity>::iterator it = param.cities.begin(); it != param.cities.end(); ++ it) {
				std::map<int, http::tsubcontinent_city>::iterator find2 = param.http_subcontinent.city.find(it->cityno);
				if (find2 != param.http_subcontinent.city.end()) {
					it->ownership = HEROS_MAX_HEROS + find2->second.uid;
					it->endurance = find2->second.endurance;
					it->times = find2->second.times;
				}
			}
		}

	}

	if (!params_.empty()) {
		switch_to_subcontinent(window, params_.front());
	}

	tlistbox* list = &find_widget<tlistbox>(&window, "subcontinent_table", false);
	for (std::vector<tparam>::const_iterator it = params_.begin(); it != params_.end(); ++ it) {
		const tparam& param = *it;

		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		strstr.str("");
		if (!param.http_subcontinent.valid()) {
			strstr << help::tintegrate::generate_format(param.name, "red");
		} else {
			strstr << param.name;
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("name", list_item));

		list_item["label"] = heros_[param.npc].name();
		list_item_item.insert(std::make_pair("npc", list_item));

		strstr.str("");
		strstr << param.cities.size();
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("cities", list_item));

		strstr.str("");
		if (param.http_subcontinent.valid()) {
			strstr << param.http_subcontinent.city.size();
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("players", list_item));

		list->add_row(list_item_item);
	}

	if (cursel_) {
		refresh_city_table(window, *cursel_, 0);
		if (!cursel_->cities.empty()) {
			switch_to_city(window, cursel_->cities.front());
		}

		list = &find_widget<tlistbox>(&window, "city_table", false);
		list->set_callback_value_change(dialog_callback<tsubcontinent, &tsubcontinent::city_selected>);
	}
}

void tsubcontinent::switch_to_subcontinent(twindow& window, tparam& param)
{
	thexmap& hexmap = find_widget<thexmap>(&window, "hexmap", false);
	hexmap.set_map_data(thexmap::TILE_MAP, param.scenario["map_data"].str());

	BOOST_FOREACH (const config& side_cfg, param.scenario.child_range("side")) {
		controller_tag::CONTROLLER control = controller_tag::find(side_cfg["controller"].str());
		if (control == controller_tag::HUMAN) {
			npc_side_ = &side_cfg;
		}
	}

	cursel_ = &param;
	my_city_ = NULL;
	for (std::vector<tcity>::iterator it = cursel_->cities.begin(); it != cursel_->cities.end(); ++ it) {
		tcity& city = *it;
		if (ownership_is_uid(city.ownership) && uid_from_ownership(city.ownership) == group.leader().uid()) {
			my_city_ = &city;
		}
	}
}

void tsubcontinent::switch_to_city(twindow& window, tcity& city)
{
	refresh_action_button(window, &city);

	curcity_ = &city;
}

void tsubcontinent::size_change(thexmap* widget)
{
	tpoint standard(800, 600);
	tpoint size = widget->get_size();

	outer_anim::zoom = std::make_pair(1.0 * size.x / standard.x, 1.0 * size.y / standard.y);
	outer_anim::rect = widget->get_rect();
}

std::vector<std::vector<std::string> > tsubcontinent::crop_map(std::vector<std::vector<std::string> >& full_map_str, const map_location& from_loc, const map_location& to_loc)
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

void to_side_cfg(config& side, const config& from_side, int city_number)
{
	side["leader"] = from_side["leader"].to_int();
	config& sub_s = side.child("artifical");
	sub_s["heros_army"] = city_number;

	std::stringstream service, wander;

	BOOST_FOREACH (const config& c, from_side.child_range("artifical")) {
		std::string str = c["service_heros"].str();
		if (!str.empty()) {
			if (!service.str().empty()) {
				service << ", ";
			}
			service << str;
		}
		str = c["wander_heros"].str();
		if (!str.empty()) {
			if (!wander.str().empty()) {
				wander << ", ";
			}
			wander << str;
		}
	}

	sub_s["service_heros"] = service.str();
	sub_s["wander_heros"] = wander.str();
}

void tsubcontinent::attack(twindow& window)
{
	if (ownership_is_uid(curcity_->ownership)) {
		const config& campaign = state_for_siege_uh(state_, game_config_, true);
		{
			tsubcontinent_param param(uid_from_ownership(curcity_->ownership), associate_members_, cursel_->scenario, my_city_? my_city_->loc: map_location(), curcity_->loc, my_city_? my_city_->number: HEROS_ROAM_CITY, curcity_->number);
			gui2::tsiege_tent dlg(disp_, heros_, state_, game_config_, &param);
			try {
				dlg.show(disp_.video());
			} catch(twml_exception& e) {
				e.show(disp_);
			}
			if (dlg.get_retval() != gui2::twindow::OK) {
				return;
			}
			tent::subcontinent = std::make_pair(cursel_->scenario["id"].str(), curcity_->cityno);
			state_for_siege_bh(state_, campaign, dlg.get_scenario());
		}
		window.set_retval(SIEGE);

	} else if (curcity_->ownership < 0) {

		const config& campaign = state_for_scenario_uh(state_, game_config_, null_str, true);
		config scenario = load_campagin_scenario(state_.classification().campaign, state_.classification().scenario, state_.classification().campaign_type);

		int sides = scenario.child_count("side");
		config* human_side = NULL;
		config* ai_side = NULL;
		map_location attacker_fort_loc, defender_fort_loc;
		for (int n = 0; n < sides; n ++) {
			config& side = scenario.child("side", n);
			config& artifical_cfg = side.child("artifical");
			// const ttechnology* stratagem;
			// think there is one city only.
			if (side["controller"] == "human") {
				group.to_side_cfg(side, rand());
				if (my_city_) {
					artifical_cfg["heros_army"] = my_city_->number;
				}
				human_side = &side;
				attacker_fort_loc = map_location(artifical_cfg["x"], artifical_cfg["y"]);
/*
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
*/

			} else if (side["controller"] == "ai") {
				to_side_cfg(side, *npc_side_, curcity_->number);
				defender_fort_loc = map_location(artifical_cfg["x"], artifical_cfg["y"]);

				ai_side = &side;
			}
		}

		std::vector<std::vector<std::string> > full_map_str;
		form_map_str_rectangle(cursel_->scenario["map_data"].str(), full_map_str);

		std::vector<std::vector<std::string> > attacker_map_str = crop_map(full_map_str, my_city_? my_city_->loc: map_location(), attacker_fort_loc);
		std::vector<std::vector<std::string> > defender_map_str = crop_map(full_map_str, curcity_->loc, defender_fort_loc);

		scenario["map_data"] = combine_map(defender_map_str, attacker_map_str, rflip_);

		tent::turn_based = true;
		tent::subcontinent = std::make_pair(cursel_->scenario["id"].str(), curcity_->cityno);

		state_for_siege_bh(state_, campaign, scenario);
		window.set_retval(SCENARIO);
	}
}

void tsubcontinent::execute_bh(twindow& window, int tag)
{
	refresh_action_button(window, NULL);

	std::pair<http::tsubcontinent_record, http::membership> p = http::subcontinent(disp_, heros_, tag, cursel_->scenario["id"].str(), curcity_->cityno);
	if (p.second.uid >= 0) {
		group.from_local_membership(disp_, heros_, p.second, true);
		refresh_title_flag(window);

		for (std::vector<tcity>::iterator it = cursel_->cities.begin(); it != cursel_->cities.end(); ++ it) {
			std::map<int, http::tsubcontinent_city>::iterator find2 = p.first.city.find(it->cityno);
			if (find2 != p.first.city.end()) {
				it->ownership = HEROS_MAX_HEROS + find2->second.uid;
				it->endurance = find2->second.endurance;
				it->times = find2->second.times;

			} else if (ownership_is_uid(it->ownership)) {
				it->ownership = -1;
				if (my_city_ == &*it) {
					my_city_ = NULL;
				}
			}
		}
		// as so far i don't knonw whay it will result mis-align.
		// for reach target, simple use relayout.
		window.invalidate_layout();

		refresh_city_table(window, *cursel_, curcity_->cityno - 1);
	}
	refresh_action_button(window, curcity_);
}

void tsubcontinent::repair(twindow& window)
{
	utils::string_map symbols;
	std::stringstream err;
	int coin = 0;
	int score = 100;

	symbols["score"] = help::tintegrate::generate_format(score, "red");
	symbols["do"] = help::tintegrate::generate_format(dsgettext("wesnoth-lib", "Repair"), "yellow");
	std::string message = vgettext("wesnoth-lib", "Spend $score score may increase 10 endurance.\nDo you want to spend $score score to $do?", symbols);
	int res = gui2::show_message(disp_.video(), "", message, gui2::tmessage::yes_no_buttons);
	if (res == gui2::twindow::CANCEL) {
		return;
	}

	if (sum_score(group.coin(), group.score()) < score) {
		symbols.clear();
		symbols["coin"] = help::tintegrate::generate_format(coin, "red");
		symbols["score"] = help::tintegrate::generate_format(score, "red");
		err << vgettext("wesnoth-lib", "Repertory is not enough to pay $coin coin and $score score. If lack one only, can exchange between coin and score.", symbols);
		gui2::show_message(disp_.video(), "", err.str());
		return;
	}

	execute_bh(window, http::subcontinent_tag_repair);
}

void tsubcontinent::discard(twindow& window)
{
	utils::string_map symbols;
	std::stringstream err;

	symbols["city"] = help::tintegrate::generate_format(heros_[curcity_->number].name(), "yellow");
	std::string message = vgettext("wesnoth-lib", "Do you want to discard $city?", symbols);
	int res = gui2::show_message(disp_.video(), "", message, gui2::tmessage::yes_no_buttons);
	if (res == gui2::twindow::CANCEL) {
		return;
	}

	execute_bh(window, http::subcontinent_tag_discard);
}

} // namespace gui2

