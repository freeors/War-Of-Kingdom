/* $Id: player_selection.cpp 49598 2011-05-22 17:55:54Z mordante $ */
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

#include "gui/dialogs/recruit.hpp"

#include "foreach.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "game_display.hpp"
#include "team.hpp"
#include "hero.hpp"
#include "artifical.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/toggle_button.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/widgets/settings.hpp"
#include "gui/widgets/stacked_widget.hpp"
#include "gui/widgets/window.hpp"
#include "resources.hpp"
#include "play_controller.hpp"

#include <boost/bind.hpp>

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

REGISTER_DIALOG(recruit)

trecruit::trecruit(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, artifical& city, int cost_exponent, bool rpg_mode)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, current_team_(teams_[city.side() - 1])
	, city_(city)
	, cost_exponent_(cost_exponent)
	, fresh_heros_()
	, unit_types_()
	, type_index_(0)
	, checked_heros_()
	, hero_table_(NULL)
	, rpg_mode_(rpg_mode)
{
}

void trecruit::type_selected(twindow& window)
{
	int gold = current_team_.gold();

	tlistbox& list = find_widget<tlistbox>(&window, "type_list", false);

	type_index_ = list.get_selected_row();
	
	tbutton* ok = find_widget<tbutton>(&window, "ok", false, true);
	const unit_type* t = unit_types_[type_index_];
	if (!checked_heros_.empty() && gold >= t->cost() * cost_exponent_ / 100) {
		if (!t->leader() || master()->official_ == hero_official_leader) {
			ok->set_active(true);
		} else {
			ok->set_active(false);
		}
	} else {
		ok->set_active(false);
	}

	refresh_tooltip(window);
}

void trecruit::hero_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	int toggled_index = toggle->get_data();
	std::set<int>::iterator result = checked_heros_.find(toggled_index);

	if (result == checked_heros_.end()) {
		// toggled button isn't in checked_heros_
		if (toggle->get_value()) {
			if (checked_heros_.size() < 3) {
				checked_heros_.insert(toggled_index);
			} else {
				// At most select three heros. decheck it!
				toggle->set_value(false);
				return;
			}
		} else {
			VALIDATE(false, "hero_toggled program error #1");
		}
	} else if (toggle->get_value()) { 
		VALIDATE(false, "hero_toggled program error #2");
	} else {
		checked_heros_.erase(result);
	}

	twindow* window = toggle->get_window();
	tbutton* ok = find_widget<tbutton>(window, "ok", false, true);
	const unit_type* t = unit_types_[type_index_];
	if (!checked_heros_.empty() && current_team_.gold() >= t->cost() * cost_exponent_ / 100) {
		if (!t->leader() || master()->official_ == hero_official_leader) {
			ok->set_active(true);
		} else {
			ok->set_active(false);
		}
	} else {
		ok->set_active(false);
	}
	refresh_tooltip(*window);
}

void trecruit::refresh_tooltip(twindow& window)
{
	int idx_in_resistances;
	std::stringstream text;

	tstacked_widget* stacked = find_widget<tstacked_widget>(&window, "middle_top_part", false, true);
	stacked->set_dirty(true);
	stacked = find_widget<tstacked_widget>(&window, "right_part", false, true);
	stacked->set_dirty(true);
	
	if (checked_heros_.empty()) {
		// It is necessary set all relative tips to empty.
		tcontrol* control = find_widget<tcontrol>(&window, "master_png", false, true);
		control->set_label("");
		tlabel* label = find_widget<tlabel>(&window, "master_name", false, true);
		label->set_label("");

		// leadership
		label = find_widget<tlabel>(&window, "tip_leadership", false, true);
		label->set_label("");

		// force
		label = find_widget<tlabel>(&window, "tip_force", false, true);
		label->set_label("");

		// intellect
		label = find_widget<tlabel>(&window, "tip_intellect", false, true);
		label->set_label("");

		// politics
		label = find_widget<tlabel>(&window, "tip_politics", false, true);
		label->set_label("");

		// charm
		label = find_widget<tlabel>(&window, "tip_charm", false, true);
		label->set_label("");

		// hp
		label = find_widget<tlabel>(&window, "tip_hp", false, true);
		label->set_label("");

		// xp
		label = find_widget<tlabel>(&window, "tip_xp", false, true);
		label->set_label("");

		// movement
		label = find_widget<tlabel>(&window, "tip_movement", false, true);
		label->set_label("");

		// arm
		label = find_widget<tlabel>(&window, "tip_arm", false, true);
		label->set_label("");

		// adaptability
		label = find_widget<tlabel>(&window, "tip_adaptability", false, true);
		label->set_label("");

		// abilities
		label = find_widget<tlabel>(&window, "tip_abilities", false, true);
		label->set_label("");

		// feature
		label = find_widget<tlabel>(&window, "tip_feature", false, true);
		label->set_label("");

		// attack
		label = find_widget<tlabel>(&window, "tip_attack", false, true);
		label->set_label("");
	
		// resistance
		for (idx_in_resistances = 0; idx_in_resistances < 7; idx_in_resistances ++) {
			text.str("");
			text << "tip_resistance" << idx_in_resistances;
			label = find_widget<tlabel>(&window, text.str(), false, true);
			label->set_label("");
		}
		return;
	}

	const unit_type* t = unit_types_[type_index_];
	std::vector<const hero*> v;
	int index = 0;
	for (std::set<int>::const_iterator itor = checked_heros_.begin(); itor != checked_heros_.end(); ++ itor, index ++) {
		if (index == 0) {
			v.push_back(fresh_heros_[*itor]);
		} else if (index == 1) {
			v.push_back(fresh_heros_[*itor]);
		} else if (index == 2) {
			v.push_back(fresh_heros_[*itor]);
		}
	}
	type_heros_pair pair(t, v);
	unit temp(units_, heros_, pair, city_.cityno(), false);

	std::stringstream str;
	// refresh to gui
	tcontrol* control = find_widget<tcontrol>(&window, "master_png", false, true);
	control->set_label(temp.master().image());
	tlabel* label = find_widget<tlabel>(&window, "master_name", false, true);
	label->set_label(temp.master().name());

	control = find_widget<tcontrol>(&window, "second_png", false, true);
	label = find_widget<tlabel>(&window, "second_name", false, true);
	if (temp.second().valid()) {
		control->set_label(temp.second().image());
		label->set_label(temp.second().name());
	} else {
		control->set_label("");
		label->set_label("");
	}

	control = find_widget<tcontrol>(&window, "third_png", false, true);
	label = find_widget<tlabel>(&window, "third_name", false, true);
	if (temp.third().valid()) {
		control->set_label(temp.third().image());
		label->set_label(temp.third().name());
	} else {
		control->set_label("");
		label->set_label("");
	}

	// leadership
	label = find_widget<tlabel>(&window, "tip_leadership", false, true);
	label->set_label(lexical_cast<std::string>(temp.leadership_));

	// force
	label = find_widget<tlabel>(&window, "tip_force", false, true);
	label->set_label(lexical_cast<std::string>(temp.force_));

	// intellect
	label = find_widget<tlabel>(&window, "tip_intellect", false, true);
	label->set_label(lexical_cast<std::string>(temp.intellect_));

	// politics
	label = find_widget<tlabel>(&window, "tip_politics", false, true);
	label->set_label(lexical_cast<std::string>(temp.politics_));

	// charm
	label = find_widget<tlabel>(&window, "tip_charm", false, true);
	label->set_label(lexical_cast<std::string>(temp.charm_));

	// hp
	label = find_widget<tlabel>(&window, "tip_hp", false, true);
	label->set_label(lexical_cast<std::string>(temp.max_hitpoints()));

	// xp
	label = find_widget<tlabel>(&window, "tip_xp", false, true);
	label->set_label(lexical_cast<std::string>(temp.max_experience()));

	// movement
	label = find_widget<tlabel>(&window, "tip_movement", false, true);
	label->set_label(lexical_cast<std::string>(temp.total_movement()));

	// arm
	label = find_widget<tlabel>(&window, "tip_arm", false, true);
	str << temp.type_name() << "(Lv" << temp.level() << ")";
	label->set_label(str.str());

	// adaptability
	str.str("");
	label = find_widget<tlabel>(&window, "tip_adaptability", false, true);
	str << hero::arms_str(temp.arms()) << "(" << hero::adaptability_str2(ftofxp12(temp.adaptability_[temp.arms()])) << ")";
	label->set_label(str.str());

	// abilities
	str.str("");
	std::vector<std::string> abilities_tt;
	abilities_tt = temp.ability_tooltips(true);
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
					str << "\n";
				}
			}
			str << (*a);
		}
	}
	label = find_widget<tlabel>(&window, "tip_abilities", false, true);
	label->set_label(str.str());

	// feature
	str.str("");
	index = 0;
	for (int i = 0; i < HEROS_MAX_FEATURE; i ++) {
		if (unit_feature_val2(temp, i) == hero_feature_single_result) {
			if (index > 0) {
				if (index != 2) {
					str << ", ";
				} else {
					str << "\n";
				}
			}
			index ++;
			str << temp.master().feature_str(i);
		}
	}
	label = find_widget<tlabel>(&window, "tip_feature", false, true);
	label->set_label(str.str());

	// attack
	str.str("");
	std::vector<attack_type>* attacks_ptr = const_cast<std::vector<attack_type>*>(&temp.attacks());
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
	
	// resistance
	std::set<std::string> resistances_table;
	utils::string_map resistances = temp.get_base_resistances();
	bool att_def_diff = false;
	const map_location& loc = temp.get_location();
	idx_in_resistances = 0;
	for (utils::string_map::iterator resist = resistances.begin(); resist != resistances.end(); ++resist, idx_in_resistances ++) {
		// str << gettext(resist->first.c_str()) << ": ";
		str.str("");

		if (loc.valid()) {
			// Some units have different resistances when
			// attacking or defending.
			int res_att = 100 - temp.resistance_against(resist->first, true, loc);
			int res_def = 100 - temp.resistance_against(resist->first, false, loc);
			if (res_att == res_def) {
				str << res_def;
			} else {
				str << res_att << "% / " << res_def; // (Att / Def)
				att_def_diff = true;
			}
		} else {
			str << 100 - lexical_cast_default<int>(resist->second.c_str());
		}
		str << "%";
		text.str("");
		text << "tip_resistance" << idx_in_resistances;
		label = find_widget<tlabel>(&window, text.str(), false, true);
		label->set_label(str.str());
	}
}

void trecruit::pre_show(CVideo& /*video*/, twindow& window)
{
	// int side_num = city_.side();
	std::stringstream str;

	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);

	int gold = current_team_.gold();

	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	str << _("Recruit") << "(" << gold << sngettext("unit^Gold", "Gold", gold) << ")";
	label->set_label(str.str());

	switch_type_internal(window);

	list->set_callback_value_change(dialog_callback<trecruit, &trecruit::type_selected>);

	hero_table_ = find_widget<tlistbox>(&window, "hero_table", false, true);
	fresh_heros_ = city_.fresh_heros();
	std::sort(fresh_heros_.begin(), fresh_heros_.end(), compare_recruit);

	// fill data to hero_table
	catalog_page(window, ABILITY_PAGE, false);

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "ability", false)
		, boost::bind(
			&trecruit::catalog_page
			, this
			, boost::ref(window)
			, (int)ABILITY_PAGE
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "adaptability", false)
		, boost::bind(
			&trecruit::catalog_page
			, this
			, boost::ref(window)
			, (int)ADAPTABILITY_PAGE
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "personal", false)
		, boost::bind(
			&trecruit::catalog_page
			, this
			, boost::ref(window)
			, (int)PERSONAL_PAGE
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "relation", false)
		, boost::bind(
			&trecruit::catalog_page
			, this
			, boost::ref(window)
			, (int)RELATION_PAGE
			, true));

	// prev/next
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "prev", false)
		, boost::bind(
			&trecruit::switch_type
			, this
			, boost::ref(window)
			, false));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "next", false)
		, boost::bind(
			&trecruit::switch_type
			, this
			, boost::ref(window)
			, true));

	tbutton* ok = find_widget<tbutton>(&window, "ok", false, true);
	const unit_type* t = unit_types_[type_index_];
	if (!checked_heros_.empty() && gold >= t->cost() * cost_exponent_ / 100) {
		if (!t->leader() || master()->official_ == hero_official_leader) {
			ok->set_active(true);
		} else {
			ok->set_active(false);
		}
	} else {
		ok->set_active(false);
	}
	tbutton* cancel = find_widget<tbutton>(&window, "cancel", false, true);
	cancel->set_visible(rpg_mode_? twidget::INVISIBLE: twidget::VISIBLE);

	if (rpg_mode_) {
		refresh_tooltip(window);
	}
}

void trecruit::post_show(twindow& window)
{
}

void trecruit::catalog_page(twindow& window, int catalog, bool swap)
{
	if (catalog < MIN_PAGE || catalog > MAX_PAGE) {
		return;
	}
	int index = catalog - MIN_PAGE;
	
	if (window.alternate_index() == index) {
		// desired page is the displaying page, do nothing.
		return;
	}

	std::vector<int> features;
	std::stringstream str;

	int selected_row = 0;
	if (swap) {
		selected_row = hero_table_->get_selected_row();
	}

	window.alternate_uh(hero_table_, index);

	const hero* rpg_hero = rpg::h;
	int hero_index = 0;
	int activity_ajdusted;
	const treasure_map& treasures = unit_types.treasures();

	for (std::vector<hero*>::iterator itor = fresh_heros_.begin(); itor != fresh_heros_.end(); ++ itor, hero_index ++) {
		/*** Add list item ***/
		string_map table_item;
		std::map<std::string, string_map> table_item_item;

		hero* h = *itor;

		if (catalog == ABILITY_PAGE) {
			table_item["use_markup"] = "true";
			str.str("");
			if (h->official_ == hero_official_commercial) {
				str << "<0,0,255>";
			} else if (h->activity_ < HEROS_FULL_ACTIVITY) {
				str << "<255,0,0>";
			}
			str << h->name();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("name", table_item));
			
			table_item["label"] = lexical_cast<std::string>(h->loyalty(*teams_[h->side_].leader()));;
			table_item_item.insert(std::make_pair("loyalty", table_item));

			table_item["label"] = hero::feature_str(h->feature_);
			table_item_item.insert(std::make_pair("feature", table_item));

			if (h->tactic_ != HEROS_NO_TACTIC) {
				table_item["label"] = unit_types.tactic(h->tactic_).name();
			} else {
				table_item["label"] = "";
			}
			table_item_item.insert(std::make_pair("tactic", table_item));

			str.str("");
			if (h->activity_ < HEROS_FULL_ACTIVITY) {
				activity_ajdusted = 1 * h->leadership_ * h->activity_ / HEROS_FULL_ACTIVITY;
				str << fxptoi9(activity_ajdusted) << "/";
			}
			str << fxptoi9(h->leadership_);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("leadership", table_item));

			str.str("");
			if (h->activity_ < HEROS_FULL_ACTIVITY) {
				activity_ajdusted = 1 * h->force_ * h->activity_ / HEROS_FULL_ACTIVITY;
				str << fxptoi9(activity_ajdusted) << "/";
			}
			str << fxptoi9(h->force_);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("force", table_item));

			str.str("");
			if (h->activity_ < HEROS_FULL_ACTIVITY) {
				activity_ajdusted = 1 * h->intellect_ * h->activity_ / HEROS_FULL_ACTIVITY;
				str << fxptoi9(activity_ajdusted) << "/";
			}
			str << fxptoi9(h->intellect_);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("intellect", table_item));

			str.str("");
			if (h->activity_ < HEROS_FULL_ACTIVITY) {
				activity_ajdusted = 1 * h->politics_ * h->activity_ / HEROS_FULL_ACTIVITY;
				str << fxptoi9(activity_ajdusted) << "/";
			}
			str << fxptoi9(h->politics_);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("politics", table_item));

			str.str("");
			if (h->activity_ < HEROS_FULL_ACTIVITY) {
				activity_ajdusted = 1 * h->charm_ * h->activity_ / HEROS_FULL_ACTIVITY;
				str << fxptoi9(activity_ajdusted) << "/";
			}
			str << fxptoi9(h->charm_);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("charm", table_item));

			str.str("");
			str << 100 * h->activity_ / HEROS_FULL_ACTIVITY << "%";
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("activity", table_item));

		} else if (catalog == ADAPTABILITY_PAGE) {
			table_item["use_markup"] = "true";
			str.str("");
			if (h->official_ == hero_official_commercial) {
				str << "<0,0,255>";
			} else if (h->activity_ < HEROS_FULL_ACTIVITY) {
				str << "<255,0,0>";
			}
			str << h->name();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("name", table_item));

			table_item["label"] = hero::adaptability_str2(h->arms_[0]);
			table_item_item.insert(std::make_pair("arm0", table_item));

			table_item["label"] = hero::adaptability_str2(h->arms_[1]);
			table_item_item.insert(std::make_pair("arm1", table_item));

			table_item["label"] = hero::adaptability_str2(h->arms_[2]);
			table_item_item.insert(std::make_pair("arm2", table_item));

			table_item["label"] = hero::adaptability_str2(h->arms_[3]);
			table_item_item.insert(std::make_pair("arm3", table_item));

			table_item["label"] = hero::adaptability_str2(h->arms_[4]);
			table_item_item.insert(std::make_pair("arm4", table_item));

		} else if (catalog == PERSONAL_PAGE) {
			table_item["use_markup"] = "true";
			str.str("");
			if (h->official_ == hero_official_commercial) {
				str << "<0,0,255>";
			} else if (h->activity_ < HEROS_FULL_ACTIVITY) {
				str << "<255,0,0>";
			}
			str << h->name();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("name", table_item));

			table_item["label"] = hero::gender_str(h->gender_);
			table_item_item.insert(std::make_pair("gender", table_item));

			str.str("");
			str << hero::treasure_str(h->treasure_);
			if (h->treasure_ != HEROS_NO_TREASURE) {
				treasure_map::const_iterator it = treasures.find(h->treasure_);
				str << "(";
				if (it != treasures.end()) {
					for (std::vector<int>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++ it2) {
						if (it2 == it->second.begin()) {
							str << hero::feature_str(*it2);
						} else {
							str << " " << hero::feature_str(*it2);
						}	
					}
				}
				str << ")";
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("treasure", table_item));

		} else if (catalog == RELATION_PAGE) {
			table_item["use_markup"] = "true";
			str.str("");
			if (h->official_ == hero_official_commercial) {
				str << "<0,0,255>";
			} else if (h->activity_ < HEROS_FULL_ACTIVITY) {
				str << "<255,0,0>";
			}
			str << h->name();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("name", table_item));

			if (h->parent_[0].hero_ != HEROS_INVALID_NUMBER) {
				table_item["label"] = heros_[h->parent_[0].hero_].name();
			} else {
				table_item["label"] = "";
			}
			table_item_item.insert(std::make_pair("father", table_item));

			if (h->parent_[1].hero_ != HEROS_INVALID_NUMBER) {
				table_item["label"] = heros_[h->parent_[1].hero_].name();
			} else {
				table_item["label"] = "";
			}
			table_item_item.insert(std::make_pair("mother", table_item));

			str.str("");
			for (uint32_t i = 0; i < HEROS_MAX_OATH; i ++) {
				if (h->oath_[i].hero_ != HEROS_INVALID_NUMBER) {
					if (i == 0) {
						str << heros_[h->oath_[i].hero_].name();
					} else {
						str << " " << heros_[h->oath_[i].hero_].name();
					}
				}
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("oath", table_item));

			str.str("");
			for (uint32_t i = 0; i < HEROS_MAX_CONSORT; i ++) {
				if (h->consort_[i].hero_ != HEROS_INVALID_NUMBER) {
					if (i == 0) {
						str << heros_[h->consort_[i].hero_].name();
					} else {
						str << " " << heros_[h->consort_[i].hero_].name();
					}
				}
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("consort", table_item));

			str.str("");
			for (uint32_t i = 0; i < HEROS_MAX_INTIMATE; i ++) {
				if (h->intimate_[i].hero_ != HEROS_INVALID_NUMBER) {
					if (i == 0) {
						str << heros_[h->intimate_[i].hero_].name();
					} else {
						str << " " << heros_[h->intimate_[i].hero_].name();
					}
				}
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("intimate", table_item));

			str.str("");
			for (uint32_t i = 0; i < HEROS_MAX_HATE; i ++) {
				if (h->hate_[i].hero_ != HEROS_INVALID_NUMBER) {
					if (i == 0) {
						str << heros_[h->hate_[i].hero_].name();
					} else {
						str << " " << heros_[h->hate_[i].hero_].name();
					}
				}
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("hate", table_item));

		}
		hero_table_->add_row(table_item_item);

		tgrid* grid_ptr = hero_table_->get_row_grid(hero_index);
		ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(grid_ptr->find("prefix", true));
		toggle->set_callback_state_change(boost::bind(&trecruit::hero_toggled, this, _1));
		toggle->set_data(hero_index);
		if (rpg_mode_ && h == rpg_hero) {
			toggle->set_active(false);
			toggle->set_value(true);
			if (!checked_heros_.count(hero_index)) {
				checked_heros_.insert(hero_index);
			}
		} else {
			if (checked_heros_.find(hero_index) != checked_heros_.end()) {
				toggle->set_value(true);
			}
			if (h->activity_ < game_config::minimal_activity) {
				toggle->set_active(false);
			}
		}
	}
	if (swap) {
		window.alternate_bh(hero_table_, index);
		hero_table_->select_row(selected_row);
	}
}

void trecruit::switch_type(twindow& window, bool next)
{
	if (next) {
		if (game_config::current_level == game_config::max_level) {
			return;
		}
		game_config::current_level ++;
	} else {
		if (game_config::current_level == game_config::min_level) {
			return;
		}
		game_config::current_level --;
	}
	switch_type_internal(window);
}

void trecruit::switch_type_internal(twindow& window)
{
	int side_num = city_.side();
	std::stringstream str;
	int gold = current_team_.gold();

	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);
	list->clear();
	unit_types_.clear();

	const std::vector<const unit_type*>& recruits = city_.recruits(game_config::current_level);
	for (std::vector<const unit_type*>::const_iterator it = recruits.begin(); it != recruits.end(); ++it) {
		/*** Add list item ***/
		const unit_type* type = *it;
		VALIDATE(type, std::string("could not find unit type ") + type->id());
		
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		if (gold >= type->cost() * cost_exponent_ / 100) {
			list_item["label"] = type->image() + "~RC(" + type->flag_rgb() + ">" + team::get_side_color_index(side_num) + ")";
		} else {
			list_item["label"] = type->image() + "~GS()";
		}

		list_item_item.insert(std::make_pair("icon", list_item));

		list_item["label"] = type->type_name();
		list_item_item.insert(std::make_pair("name", list_item));

		str.str("");
		str << type->cost() * cost_exponent_ / 100 << " " << sngettext("unit^Gold", "Gold", type->cost() * cost_exponent_ / 100);
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("cost", list_item));

		list->add_row(list_item_item);

		tgrid* grid_ptr = list->get_row_grid(unit_types_.size());
		twidget* widget = grid_ptr->find("leader", false);
		widget->set_visible(type->leader()? twidget::VISIBLE: twidget::INVISIBLE);

		unit_types_.push_back(type);
	}
	window.invalidate_layout();
	if (type_index_ >= (int)list->get_item_count()) {
		type_index_ = list->get_item_count() - 1;
	}
	list->select_row(type_index_);

	tbutton* prev = find_widget<tbutton>(&window, "prev", false, true);
	prev->set_active(game_config::current_level != game_config::min_level);
	tbutton* next = find_widget<tbutton>(&window, "next", false, true);
	next->set_active(game_config::current_level != game_config::max_level);

	str.str("");
	str << "misc/digit-big.png~CROP(" << 30 * game_config::current_level << ", 0, 30, 45)";
	find_widget<tcontrol>(&window, "level", false, false)->set_label(str.str());

	type_selected(window);
}

hero* trecruit::master() const
{
	if (!checked_heros_.empty()) {
		return fresh_heros_[*checked_heros_.begin()];
	} else {
		return &hero_invalid;
	}
}

hero* trecruit::second() const
{
	if (checked_heros_.size() >= 2) {
		std::set<int>::const_iterator itor = checked_heros_.begin();
		++ itor;
		return fresh_heros_[*itor];
	} else {
		return &hero_invalid;
	}
}

hero* trecruit::third() const
{
	if (checked_heros_.size() >= 3) {
		std::set<int>::const_iterator itor = checked_heros_.begin();
		++ itor;
		++ itor;
		return fresh_heros_[*itor];
	} else {
		return &hero_invalid;
	}
}

const unit_type* trecruit::unit_type_ptr() const
{
	return unit_types_[type_index_];	
}

} // namespace gui2
