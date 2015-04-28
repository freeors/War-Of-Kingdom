/* $Id: title_screen.cpp 47226 2010-10-26 21:37:36Z mordante $ */
/*
   Copyright (C) 2008 - 2010 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "kingdom-lib"

#include "gui/dialogs/camp_armory.hpp"

#include "display.hpp"
#include "game_config.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "log.hpp"
#include "gui/auxiliary/timer.hpp"
#include "gui/auxiliary/tips.hpp"
#include "gui/dialogs/language_selection.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "preferences_display.hpp"
#include "artifical.hpp"

#include <boost/bind.hpp>

#include <algorithm>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_title_screen
 *
 * == Title screen ==
 *
 * This shows the title screen.
 *
 * @begin{table}[dialog_widgets]
 * addons & & button & m &
 *         The button to get the addons. $
 *
 * language & & button & m &
 *         The button to change the language. $
 *
 * tips & & multi_page & m &
 *         A multi_page to hold all tips, when this widget is used the area of
 *         the tips doesn't need to be resized when the next or previous button
 *         is pressed. $
 *
 * -tip & & label & o &
 *         The tip of the day. $
 *
 * -source & & label & o &
 *         The source for the tip of the day. $
 *
 * next_tip & & button & m &
 *         The button show the next tip of the day. $
 *
 * previous_tip & & button & m &
 *         The button show the previous tip of the day. $
 *
 * logo & & progress_bar & o &
 *         A progress bar to "animate" the image. $
 *
 * revision_number & & control & o &
 *         A widget to show the version number. $
 *
 * @end{table}
 */

REGISTER_DIALOG(camp_armory)

tcamp_armory::tcamp_armory(std::vector<unit*>& candidate_troops, std::vector<hero*>& candidate_heros, int scenario, artifical* city)
	: candidate_troops_(candidate_troops)
	, candidate_heros_(candidate_heros)
	, scenario_(tscenario(scenario))
	, city_(city)
	, prev_troop_(-1)
	, prev_captain_(twidget::npos)
	, reserve_heros_(0)
	, first_human_troop_(-1)
	, troop_report_(NULL)
	, hero_report_(NULL)
{
	int index = 0;
	for (std::vector<unit*>::const_iterator itor = candidate_troops.begin(); itor != candidate_troops.end(); ++ itor, index ++) {
		const unit& u = **itor;
		hit_points_.push_back(u.hitpoints());
		tactic_degree_.push_back(u.tactic_degree());
		hot_turns_.push_back(u.hot_turns());
		if (first_human_troop_ == -1 && u.human()) {
			first_human_troop_ = index;
		}
	}
	VALIDATE(first_human_troop_ != -1, "tcamp_armory::tcamp_armoty, there is no human troop.");
}

tcamp_armory::~tcamp_armory()
{
}

void tcamp_armory::post_build(CVideo& video, twindow& window)
{
	/** @todo Should become a title screen hotkey. */
}

void tcamp_armory::pre_show(CVideo&, twindow& window)
{
	// set_restore(false);
	window.set_click_dismiss(false);

	// calculate reserve heros.
	if (rpg::stratum == hero_stratum_citizen) {
		reserve_heros_ = candidate_heros_.size();
	} else if(rpg::stratum == hero_stratum_mayor && city_ && city_->mayor() != rpg::h) {
		reserve_heros_ = candidate_heros_.size();
	}

	troop_report_ = find_widget<treport>(&window, "candidate_troop", false, false);
	troop_report_->multiline_init(true, "troop_portrait", 0, 0, 2);

	hero_report_ = find_widget<treport>(&window, "candidate_hero", false, false);
	hero_report_->multiline_init(false, "portrait48", 0, 0, 2);

	// set candidate troops
	ttoggle_button* toggle;
	char id[24];

	prev_troop_ = first_human_troop_;
	refresh_buttons_from_armory_cfg(window, true);

	for (size_t i = 0; i < 3; i ++) {
		sprintf(id, "hero%u", i);
		toggle = find_widget<ttoggle_button>(&window, id, false, false);
		if (toggle) {
			connect_signal_mouse_left_click(
					 *toggle
					, boost::bind(
					  &tcamp_armory::refresh_according_to_captain
					, this
					, boost::ref(window)
					, i));
		}
	}

	tbutton* ok = find_widget<tbutton>(&window, "ok", false, false);
	if (scenario_ == SCENARIO_CAMP) {
		ok->set_label(_("Battle"));
	}
}

void tcamp_armory::post_show(twindow& window)
{
	size_t size = candidate_troops_.size();
	for (size_t i = 0; i < size; i ++) {
		unit& u = *candidate_troops_[i];

		int hit_points = hit_points_[i];
		if (hit_points > u.max_hitpoints()) {
			u.heal_all();
		} else {
			u.set_hitpoints(hit_points);
		}

		int tactic_degree = tactic_degree_[i];
		if (tactic_degree > u.max_tactic_point() * game_config::tactic_degree_per_point) {
			u.set_tactic_degree(u.max_tactic_point() * game_config::tactic_degree_per_point);
		} else {
			u.set_tactic_degree(tactic_degree);
		}

		int hot_turns = hot_turns_[i];
		if (hot_turns > u.max_hot_turns()) {
			u.set_hot_turns(u.max_hot_turns());
		} else {
			u.set_hot_turns(hot_turns);
		}
	}
}

void tcamp_armory::refresh_according_to_troop(twindow& window, const int curr)
{
	std::stringstream str;

	if (candidate_troops_.empty()) {
		return;
	}

	// remember this index
	prev_troop_ = curr;

	unit& u = *candidate_troops_[curr];
	ttoggle_button* toggle = NULL;

	toggle = find_widget<ttoggle_button>(&window, "hero0", false, false);
	toggle->set_canvas_variable("image", variant(u.master().image()));
	toggle->set_value(false);

	toggle = find_widget<ttoggle_button>(&window, "hero1", false, false);
	if (u.second().valid()) {
		toggle->set_canvas_variable("image", variant(u.second().image()));
	} else {
		toggle->set_canvas_variable("image", variant(""));
	}
	toggle->set_value(false);

	toggle = find_widget<ttoggle_button>(&window, "hero2", false, false);
	if (u.third().valid()) {
		toggle->set_canvas_variable("image", variant(u.third().image()));
	} else {
		toggle->set_canvas_variable("image", variant(""));
	}
	toggle->set_value(false);

	int last_captain;
	if (u.second().valid()) {
		toggle->set_active(true);
		if (u.third().valid()) {
			last_captain = 2;
		} else {
			last_captain = 1;
		}
	} else {
		toggle->set_active(false);
		last_captain = 0;
	}

	if (prev_captain_ == 0) {
		prev_captain_ = twidget::npos;
	}
	refresh_according_to_captain(window, (last_captain + 1) % 3);

	// image of arms
	toggle = find_widget<ttoggle_button>(&window, "arms_image", false, false);
	update_troop_ui2(toggle, &u);
	toggle->set_active(false);

	tlabel* label;
	// hp
	label = find_widget<tlabel>(&window, "tip_hp", false, true);
	str.str("");
	str << "HP: " << u.max_hitpoints();
	label->set_label(str.str());

	// xp
	label = find_widget<tlabel>(&window, "tip_xp", false, true);
	str.str("");
	str << "XP: " << u.max_experience();
	label->set_label(str.str());

	// mp
	label = find_widget<tlabel>(&window, "tip_mp", false, true);
	str.str("");
	str << "MP: " << u.total_movement();
	label->set_label(str.str());

	// traits
	tscroll_label* label2 = find_widget<tscroll_label>(&window, "tip_traits", false, true);
	label2->set_label(u.form_tip(true));

	str.str("");
	str << "candidate_hero" << candidate_heros_.size();
	toggle = find_widget<ttoggle_button>(&window, str.str(), false, false);
	if (toggle) {
		toggle->set_active(u.second().valid()? true: false);
	}
}

void tcamp_armory::refresh_according_to_captain(twindow& window, const int curr)
{
	char id[24];

	if (prev_captain_ == curr) {
		sprintf(id, "hero%u", curr);
		ttoggle_button* curr_captain = find_widget<ttoggle_button>(&window, id, false, false);
		curr_captain->set_value(true);
		return;
	}

	if (prev_captain_ != twidget::npos) {
		sprintf(id, "hero%u", prev_captain_);
		ttoggle_button* prev_captain = find_widget<ttoggle_button>(&window, id, false, false);
		prev_captain->set_value(false);
		prev_captain->set_dirty();

		// window.invalidate_layout();
	}

	// current captain
	sprintf(id, "hero%u", curr);
	ttoggle_button* curr_captain = find_widget<ttoggle_button>(&window, id, false, false);
	curr_captain->set_value(true);

	curr_captain->set_dirty();

	// remember this index
	prev_captain_ = curr;

	tcontrol* widget = hero_report_->get_widget(hero_report_->childs() - 1);
	unit& u = *candidate_troops_[prev_troop_];
	if ((curr == 1 && !u.second().valid()) || (curr == 2 && !u.third().valid())) {
		widget->set_active(false);
	} else if (curr == 0 && !u.second().valid() && !u.third().valid()) {
		widget->set_active(false);
	} else {
		widget->set_active(true);
	}
}

bool tcamp_armory::refresh_according_to_hero(twindow& window, const int curr)
{
	unit& u = *candidate_troops_[prev_troop_];
	hero* replaced_hero = NULL;
	tcontrol* widget;
	bool remove = false;
	bool require_replacement = false;
	
	// extra replaced hero pointer
	if (prev_captain_ == 0) {
		replaced_hero = &u.master();
	} else if (prev_captain_ == 1) {
		if (u.second().valid()) {
			replaced_hero = &u.second();
		}
	} else if (prev_captain_ == 2) {
		if (u.third().valid()) {
			replaced_hero = &u.third();
		}
	}

	if (!replaced_hero && curr == candidate_heros_.size()) {
		widget = hero_report_->get_widget(curr);
		widget->set_active(false);
		return true;
	} else if (u.type()->require() == unit_type::REQUIRE_LEADER) {
		if (replaced_hero && replaced_hero->official_ == hero_official_leader) { 
			return true;
		}
	} else if (u.type()->require() == unit_type::REQUIRE_FEMALE) {
		if (replaced_hero && curr != candidate_heros_.size() && candidate_heros_[curr]->gender_ != hero_gender_female) {
			bool other_has_female = false;
			if (candidate_heros_[curr]->official_ != hero_official_leader) {
				if (u.master().number_ != replaced_hero->number_ && u.master().gender_ == hero_gender_female) {
					other_has_female = true;
				}
				if (!other_has_female && u.second().valid() && u.second().number_ != replaced_hero->number_ && u.second().gender_ == hero_gender_female) {
					other_has_female = true;
				} 
				if (!other_has_female && u.third().valid() && u.third().number_ != replaced_hero->number_ && u.third().gender_ == hero_gender_female) {
					other_has_female = true;
				}
			} else {
				other_has_female = candidate_heros_[curr]->gender_ == hero_gender_female;
			}

			if (!other_has_female) {
				widget = hero_report_->get_widget(curr);
				widget->set_active(false);
				return true;
			}
		}

	}

	// replace!!
	std::vector<hero*> used_heros;
	used_heros.push_back(&u.master());
	if (u.second().valid()) {
		used_heros.push_back(&u.second());
	}
	if (u.third().valid()) {
		used_heros.push_back(&u.third());
	}
	if (prev_captain_ < (int)used_heros.size()) {
		used_heros.erase(used_heros.begin() + prev_captain_);
	}
	if (curr != candidate_heros_.size()) {
		used_heros.push_back(candidate_heros_[curr]);
	}
	// sort
	std::sort(used_heros.begin(), used_heros.end(), sort_recruit(u.type()));

	u.replace_captains(used_heros);

	widget = hero_report_->get_widget(curr);

	// update candidate heros
	if (replaced_hero) {
		if (curr != candidate_heros_.size()) {
			candidate_heros_[curr] = replaced_hero;
			widget->set_label(replaced_hero->image());
		} else {
			candidate_heros_.push_back(replaced_hero);
			tcontrol* inserting = hero_report_->create_child(null_str, null_str, reinterpret_cast<void*>(replaced_hero->number_));
			inserting->set_label(replaced_hero->image());
			hero_report_->insert_child(*inserting, curr);

			tcontrol* widget2 = hero_report_->get_widget(curr + 1);
			widget2->set_active(false);
			
			require_replacement = true;
		}
		// sort
		// std::sort(candidate_heros_.begin(), candidate_heros_.end(), compare_leadership);
	} else if (curr != candidate_heros_.size()) {
		candidate_heros_.erase(candidate_heros_.begin() + curr);
		hero_report_->erase_child(curr);
		remove = true;
		require_replacement = true;
	}

	
	ttoggle_button* troop_widget = dynamic_cast<ttoggle_button*>(troop_report_->get_widget(prev_troop_));
	update_troop_ui(troop_widget, u);

	refresh_buttons_from_armory_cfg(window);

	if (rpg::stratum == hero_stratum_citizen) {
		bool found = false;
		for (size_t i = reserve_heros_; i < candidate_heros_.size(); i ++) {
			if (candidate_heros_[i] == rpg::h) {
				found = true;
				break;
			}
		}
		tbutton* ok = find_widget<tbutton>(&window, "ok", false, false);
		ok->set_active(!found);
	}

	if (require_replacement) {
		hero_report_->replacement_children();
	}

	return !remove;
}

void tcamp_armory::update_troop_ui(ttoggle_button* toggle, unit& u)
{
	std::string str;
	std::stringstream text;

	str = u.master().image();
	toggle->set_canvas_variable("master", variant(str));
	
	if (u.second().valid()) {
		str = u.second().image();
	} else {
		str = "";
	}
	toggle->set_canvas_variable("second", variant(str));

	if (u.third().valid()) {
		str = u.third().image();
	} else {
		str = "";
	}
	toggle->set_canvas_variable("third", variant(str));

	text.str("");
	text << "misc/digit.png~CROP(" << 8 * u.level() << ", 0, 8, 12)";
	toggle->set_canvas_variable("level", variant(text.str()));

	str = u.type()->image();
	toggle->set_canvas_variable("image", variant(str));

	toggle->set_label(u.name());
	toggle->set_active(u.human());
}

void tcamp_armory::update_troop_ui2(ttoggle_button* toggle, unit* u)
{
	std::string str;
	std::stringstream text;
	size_t i;

	text.str("");
	if (u) {
		text << "misc/digit.png~CROP(" << 8 * u->level() << ", 0, 8, 12)";
	}
	for (i = 0; i < 6; i ++) {
		toggle->canvas()[i].set_variable("level", variant(text.str()));
	}
	if (u) {
		str = u->type()->image();
	} else {
		str = "";
	}
	for (i = 0; i < 6; i ++) {
		toggle->canvas()[i].set_variable("image", variant(str));
	}
	if (u) {
		toggle->set_label(u->name());
	} else {
		toggle->set_label("");
	}
	toggle->set_active(u? true: false);
}

void tcamp_armory::refresh_buttons_from_armory_cfg(twindow& window, bool first)
{
	if (first) {
		for (int i = 0; i < (int)candidate_troops_.size(); i ++) {
			unit& u = *(candidate_troops_[i]);
			tcontrol* widget = troop_report_->create_child(null_str, null_str, NULL);
			update_troop_ui(dynamic_cast<ttoggle_button*>(widget), u);
			troop_report_->insert_child(*widget);
		}
		troop_report_->select(0);

		unit& u = *candidate_troops_[prev_troop_ == -1? 0: prev_troop_];
		for (int i = 0; i < (int)candidate_heros_.size(); i ++) {
			hero& h = *(candidate_heros_[i]);
			tcontrol* widget = hero_report_->create_child(null_str, null_str, reinterpret_cast<void*>(h.number_));
			
			widget->set_label(h.image());
			widget->set_active(i >= reserve_heros_);
			hero_report_->insert_child(*widget);
		}
		// discharge
		tcontrol* widget = hero_report_->create_child(null_str, null_str, reinterpret_cast<void*>(HEROS_INVALID_NUMBER));
		widget->set_label("hero-64/default.png");
		widget->set_active(u.second().valid());
		hero_report_->insert_child(*widget);
	}

	int curr_troop = prev_troop_;
	if (prev_troop_ == -1) {
		curr_troop = 0;
	} else {
		prev_troop_ = -1;
	}

	refresh_according_to_troop(window, curr_troop);
}

void tcamp_armory::toggle_report(twidget* widget)
{
	treport* report = treport::get_report(widget);
	tdialog::toggle_report(widget);

	twindow* window = widget->get_window();
	return refresh_according_to_troop(*window, report->get_at(widget));
}

bool tcamp_armory::click_report(twidget* widget)
{
	treport* report = treport::get_report(widget);

	twindow* window = widget->get_window();
	return refresh_according_to_hero(*window, report->get_at(widget));
}

void tcamp_armory::do_battle(twindow& window)
{
}

} // namespace gui2

