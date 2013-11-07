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

#define GETTEXT_DOMAIN "wesnoth-lib"

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
#include "gui/widgets/multi_page.hpp"
#include "gui/widgets/progress_bar.hpp"
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
	, prev_captain_(-1)
	, reserve_heros_(0)
	, first_human_troop_(-1)
{
	int index = 0;
	for (std::vector<unit*>::const_iterator itor = candidate_troops.begin(); itor != candidate_troops.end(); ++ itor, index ++) {
		const unit& u = **itor;
		hit_points_.push_back(u.hitpoints());
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
	window.set_enter_disabled(true);
	window.set_escape_disabled(true);

	// calculate reserve heros.
	if (rpg::stratum == hero_stratum_citizen) {
		reserve_heros_ = candidate_heros_.size();
	} else if(rpg::stratum == hero_stratum_mayor && city_ && city_->mayor() != rpg::h) {
		reserve_heros_ = candidate_heros_.size();
	}

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
	tbutton* cancel = find_widget<tbutton>(&window, "cancel", false, false);
	if (scenario_ == SCENARIO_CAMP) {
		ok->set_label(_("Battle"));
		cancel->set_label(_("Quit"));
	} else {
		cancel->set_visible(twidget::INVISIBLE);
	}
}

void tcamp_armory::post_show(twindow& window)
{
	size_t size = candidate_troops_.size();
	for (size_t i = 0; i < size; i ++) {
		int hit_points = hit_points_[i];
		if (hit_points > candidate_troops_[i]->max_hitpoints()) {
			candidate_troops_[i]->heal_all();
		} else {
			candidate_troops_[i]->set_hitpoints(hit_points);
		}
	}
}

void tcamp_armory::refresh_according_to_troop(twindow& window, const int curr)
{
	std::stringstream str;
	size_t i;

	if (candidate_troops_.empty()) {
		return;
	}
	if (prev_troop_ == curr) {
		if (prev_troop_ != -1) {
			str.str("");
			str << "candidate_troop" << prev_troop_;
			ttoggle_button* prev_troop = find_widget<ttoggle_button>(&window, str.str(), false, false);
			prev_troop->set_value(true);
		}
		return;
	}

	if (prev_troop_ != -1) {
		str.str("");
		str << "candidate_troop" << prev_troop_;
		ttoggle_button* prev_troop = find_widget<ttoggle_button>(&window, str.str(), false, false);
		prev_troop->set_value(false);
		prev_troop->set_dirty();

		window.invalidate_layout();
	}

	// current troop
	str.str("");
	str << "candidate_troop" << curr;
	ttoggle_button* curr_troop = find_widget<ttoggle_button>(&window, str.str(), false, false);
	// curr_troop->set_value(true);
	curr_troop->set_dirty();

	// remember this index
	prev_troop_ = curr;

	unit& u = *candidate_troops_[curr];
	ttoggle_button* toggle = NULL;

	toggle = find_widget<ttoggle_button>(&window, "hero0", false, false);
	for (i = 0; i < 6; i ++) {
		toggle->canvas()[i].set_variable("image", variant(u.master().image()));
	}
	toggle->set_value(false);

	toggle = find_widget<ttoggle_button>(&window, "hero1", false, false);
	for (i = 0; i < 6; i ++) {
		if (u.second().valid()) {
			toggle->canvas()[i].set_variable("image", variant(u.second().image()));
		} else {
			toggle->canvas()[i].set_variable("image", variant(""));
		}
	}
	toggle->set_value(false);

	toggle = find_widget<ttoggle_button>(&window, "hero2", false, false);
	for (i = 0; i < 6; i ++) {
		if (u.third().valid()) {
			toggle->canvas()[i].set_variable("image", variant(u.third().image()));
		} else {
			toggle->canvas()[i].set_variable("image", variant(""));
		}
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
		prev_captain_ = -1;
	}
	refresh_according_to_captain(window, (last_captain + 1) % 3);

	// image of arms
	toggle = find_widget<ttoggle_button>(&window, "arms_image", false, false);
	update_troop_ui2(toggle, &u);
	toggle->set_active(false);

	tlabel* label;
	// leadership
	label = find_widget<tlabel>(&window, "tip_leadership", false, true);
	label->set_label(lexical_cast<std::string>(u.leadership_));

	// force
	label = find_widget<tlabel>(&window, "tip_force", false, true);
	label->set_label(lexical_cast<std::string>(u.force_));

	// intellect
	label = find_widget<tlabel>(&window, "tip_intellect", false, true);
	label->set_label(lexical_cast<std::string>(u.intellect_));

	// politics
	label = find_widget<tlabel>(&window, "tip_politics", false, true);
	label->set_label(lexical_cast<std::string>(u.politics_));

	// charm
	label = find_widget<tlabel>(&window, "tip_charm", false, true);
	label->set_label(lexical_cast<std::string>(u.charm_));

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
	str.str("");
	str << _("Traits") << ": " << utils::join(u.trait_names(), ", ");
	label = find_widget<tlabel>(&window, "tip_traits", false, true);
	label->set_label(str.str());

	// abilities
	str.str("");
	std::vector<std::string> abilities_tt;
	abilities_tt = u.ability_tooltips(true);
	str << _("Abilities") << ": ";
	if (!abilities_tt.empty()) {
		std::vector<t_string> abilities;
		for (std::vector<std::string>::const_iterator a = abilities_tt.begin(); a != abilities_tt.end(); a += 2) {
			abilities.push_back(*a);
		}

		for (std::vector<t_string>::const_iterator a = abilities.begin(); a != abilities.end(); a++) {
			if (a != abilities.begin()) {
				if (a - abilities.begin() != 2) {
					str << ", ";
				} else {
					str << "\n    ";
				}
			}
			str << (*a);
		}
	}
	label = find_widget<tlabel>(&window, "tip_abilities", false, true);
	label->set_label(str.str());

	// feature
	str.str("");
	int index = 0;
	str << _("Feature") << ": ";
	for (int i = 0; i < HEROS_MAX_FEATURE; i ++) {
		if (unit_feature_val2(u, i) == hero_feature_single_result) {
			if (index > 0) {
				if (index != 2) {
					str << ", ";
				} else {
					str << "\n    ";
				}
			}
			index ++;
			str << u.master().feature_str(i);
		}
	}
	label = find_widget<tlabel>(&window, "tip_feature", false, true);
	label->set_label(str.str());

	// adaptability
	str.str("");
	str << _("Adaptability") << ": ";
	str << hero::arms_str(u.arms()) << "(" << hero::adaptability_str2(ftofxp12(u.adaptability_[u.arms()])) << ")";
	label = find_widget<tlabel>(&window, "tip_adaptability", false, true);
	label->set_label(str.str());

	// attack
	str.str("");
	std::vector<attack_type>* attacks_ptr = const_cast<std::vector<attack_type>*>(&u.attacks());
	for (std::vector<attack_type>::const_iterator at_it = attacks_ptr->begin(); at_it != attacks_ptr->end(); ++at_it) {
		// see generate_report() in generate_report.cpp
		str << at_it->name() << " (" << dgettext("wesnoth", at_it->type().c_str()) << ")\n";

		std::string accuracy = at_it->accuracy_parry_description();
		if(accuracy.empty() == false) {
			accuracy += " ";
		}

		str << "  " << at_it->damage() << "-" << at_it->num_attacks()
			<< " " << accuracy << "- " << dgettext("wesnoth", at_it->range().c_str());

		std::string special = at_it->weapon_specials(true);
		if (!special.empty()) {
			str << "(" << special << ")";
		}
		str << "\n";
	}
	label = find_widget<tlabel>(&window, "tip_attack", false, true);
	label->set_label(str.str());


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

	if (prev_captain_ != -1) {
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

	sprintf(id, "candidate_hero%u", candidate_heros_.size());
	ttoggle_button* toggle = find_widget<ttoggle_button>(&window, id, false, false);
	if (!toggle) {
		return;
	}
	unit& u = *candidate_troops_[prev_troop_];
	if ((curr == 1 && !u.second().valid()) || (curr == 2 && !u.third().valid())) {
		toggle->set_active(false);
	} else if (curr == 0 && !u.second().valid() && !u.third().valid()) {
		toggle->set_active(false);
	} else {
		toggle->set_active(true);
	}
}

void tcamp_armory::refresh_according_to_hero(twindow& window, const int curr)
{
	char id[24];
	unit& u = *candidate_troops_[prev_troop_];
	hero* replaced_hero = NULL;
	ttoggle_button* toggle;
	
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
		sprintf(id, "candidate_hero%u", curr);
		toggle = find_widget<ttoggle_button>(&window, id, false, false);
		toggle->set_value(false);
		return;
	} else if (u.type()->require() == unit_type::REQUIRE_LEADER) {
		if (replaced_hero && replaced_hero->official_ == hero_official_leader) { 
			sprintf(id, "candidate_hero%u", curr);
			toggle = find_widget<ttoggle_button>(&window, id, false, false);
			toggle->set_value(false);
			return;
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
				sprintf(id, "candidate_hero%u", curr);
				toggle = find_widget<ttoggle_button>(&window, id, false, false);
				toggle->set_value(false);
				return;
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

	// update candidate heros
	if (replaced_hero) {
		if (curr != candidate_heros_.size()) {
			candidate_heros_[curr] = replaced_hero;
		} else {
			candidate_heros_.push_back(replaced_hero);
		}
		// sort
		// std::sort(candidate_heros_.begin(), candidate_heros_.end(), compare_leadership);
	} else if (curr != candidate_heros_.size()) {
		candidate_heros_.erase(candidate_heros_.begin() + curr);
	}

	sprintf(id, "candidate_hero%u", curr);
	toggle = find_widget<ttoggle_button>(&window, id, false, false);
	toggle->set_value(false);

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

	window.invalidate_layout();
}

void tcamp_armory::update_troop_ui(ttoggle_button* toggle, unit* u)
{
	std::string str;
	std::stringstream text;
	size_t i;

	if (u) {
		str = u->master().image();
	} else {
		str = "";
	}
	for (i = 0; i < 6; i ++) {
		toggle->canvas()[i].set_variable("master", variant(str));
	}
	if (u && u->second().valid()) {
		str = u->second().image();
	} else {
		str = "";
	}
	for (i = 0; i < 6; i ++) {
		toggle->canvas()[i].set_variable("second", variant(str));
	}
	if (u && u->third().valid()) {
		str = u->third().image();
	} else {
		str = "";
	}
	for (i = 0; i < 6; i ++) {
		toggle->canvas()[i].set_variable("third", variant(str));
	}
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
	if (u && u->human()) {
		toggle->set_active(true);
	} else {
		toggle->set_active(false);
	}
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

void tcamp_armory::update_hero_ui(ttoggle_button* toggle, hero* h, size_t index)
{
	std::stringstream str;
	
	str.str("");
	if (h) {
		str << h->image();
		if (index < reserve_heros_) {
			str << "~GS()";
		}
	}
	for (size_t i = 0; i < 6; i ++) {
		toggle->canvas()[i].set_variable("image", variant(str.str()));
	}

	str.str("");
	if (h) {
		str << h->name();
	}
	toggle->set_label(str.str());
	// toggle->set_active(h? true: false);
}

void tcamp_armory::refresh_buttons_from_armory_cfg(twindow& window, bool first)
{
	// set candidate troops
	std::stringstream id;

	ttoggle_button* toggle;
	for (size_t i = 0; i < 12; i ++) {
		id.str("");
		id << "candidate_troop" << i;
		toggle = find_widget<ttoggle_button>(&window, id.str(), false, false);
		if (toggle) {
			if (i < candidate_troops_.size()) {
				update_troop_ui(toggle, candidate_troops_[i]);
			} else {
				update_troop_ui(toggle, NULL);
			}
			if (first) {
				connect_signal_mouse_left_click(
					 *toggle
					, boost::bind(
					  &tcamp_armory::refresh_according_to_troop
					, this
					, boost::ref(window)
					, i));
				if (i == first_human_troop_) {
					toggle->set_value(true);
				}
			}
		}
	}

	unit& u = *candidate_troops_[prev_troop_ == -1? 0: prev_troop_];
	for (size_t i = 0; i < 16; i ++) {
		id.str("");
		id << "candidate_hero" << i;
		toggle = find_widget<ttoggle_button>(&window, id.str(), false, false);
		if (toggle) {
			if (i < candidate_heros_.size()) {
				hero& h = *(candidate_heros_[i]);

				update_hero_ui(toggle, &h, i);
				if (i < reserve_heros_) {
					toggle->set_active(false);
				} else {
					toggle->set_active(true);
				}
			} else {
				update_hero_ui(toggle, NULL, i);
				if (i == candidate_heros_.size() && u.second().valid()) {
					toggle->set_active(true);
				} else {
					toggle->set_active(false);
				}
			}
			if (first) {
				connect_signal_mouse_left_click(
					 *toggle
					, boost::bind(
					  &tcamp_armory::refresh_according_to_hero
					, this
					, boost::ref(window)
					, i));
			}
		}
	}

	int curr_troop = prev_troop_;
	if (prev_troop_ == -1) {
		curr_troop = 0;
	} else {
		prev_troop_ = -1;
	}

	refresh_according_to_troop(window, curr_troop);
}

void tcamp_armory::do_battle(twindow& window)
{
}

} // namespace gui2

