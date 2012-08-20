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

#include "gui/dialogs/select_unit.hpp"

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
#include "gui/widgets/window.hpp"

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

REGISTER_DIALOG(select_unit)

tselect_unit::tselect_unit(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, std::vector<const unit*>& partial_troops, int type)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, partial_troops_(partial_troops)
	, type_(type)
	, troop_index_(0)
	, hero_table_(NULL)
{
}

void tselect_unit::type_selected(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "type_list", false);

	troop_index_ = list.get_selected_row();
	refresh_tooltip(window);
}

void tselect_unit::refresh_tooltip(twindow& window)
{
	std::stringstream text;
	int index;

	if (partial_troops_.empty()) {
		// It is necessary set all relative tips to empty.
		tlabel* label = find_widget<tlabel>(&window, "tip_name", false, true);
		label->set_label("");

		// tip_loyalty
		label = find_widget<tlabel>(&window, "tip_loyalty", false, true);
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
/*
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
*/
		// traits
		label = find_widget<tlabel>(&window, "tip_traits", false, true);
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
		label = find_widget<tlabel>(&window, "tip_resistance", false, true);
		label->set_label("");
		return;
	}
	const unit& temp = *(partial_troops_[troop_index_]);

	std::stringstream str;
	int loyalty, hero_count = 1;
	// refresh to gui
	tlabel* label = find_widget<tlabel>(&window, "tip_name", false, true);
	str << dgettext("wesnoth", "hero") << ": " << temp.master().name();
	text << temp.master().loyalty(*teams_[temp.master().side_].leader());
	loyalty = temp.master().loyalty(*teams_[temp.master().side_].leader());
	if (temp.second().valid()) {
		str << "  " << temp.second().name();
		text << ", " << temp.second().loyalty(*teams_[temp.second().side_].leader());
		loyalty += temp.second().loyalty(*teams_[temp.second().side_].leader());
		hero_count ++;
	}
	if (temp.third().valid()) {
		str << " " << temp.third().name();
		text << ", " << temp.third().loyalty(*teams_[temp.third().side_].leader());
		loyalty += temp.third().loyalty(*teams_[temp.third().side_].leader());
		hero_count ++;
	}
	label->set_label(str.str());

	str.str("");
	label = find_widget<tlabel>(&window, "tip_loyalty", false, true);
	str << dgettext("wesnoth", "loyalty") << ": " << loyalty / hero_count << "/(" << text.str() << ")";
	label->set_label(str.str());

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
/*
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
*/

	// traits
	str.str("");
	str << dgettext("wesnoth", "Traits") << ": " << utils::join(temp.trait_names(), ", ");
	label = find_widget<tlabel>(&window, "tip_traits", false, true);
	label->set_label(str.str());

	// abilities
	str.str("");
	std::vector<std::string> abilities_tt;
	abilities_tt = temp.ability_tooltips(true);
	str << dgettext("wesnoth", "Abilities") << ": ";
	if (!abilities_tt.empty()) {
		std::vector<t_string> abilities;
		for (std::vector<std::string>::const_iterator a = abilities_tt.begin(); a != abilities_tt.end(); a += 2) {
			abilities.push_back(*a);
		}

		for (std::vector<t_string>::const_iterator a = abilities.begin(); a != abilities.end(); a++) {
			if (a != abilities.begin()) {
				str << ", ";
			}
			str << (*a);
		}
	}
	label = find_widget<tlabel>(&window, "tip_abilities", false, true);
	label->set_label(str.str());

	// feature
	str.str("");
	index = 0;
	str << dgettext("wesnoth", "feature") << ": ";
	for (int i = 0; i < HEROS_MAX_FEATURE; i ++) {
		if (unit_feature_val2(temp, i) == hero_feature_single_result) {
			if (index > 0) {
				str << ", ";
			}
			index ++;
			str << temp.master().feature_str(i);
		}
	}
	label = find_widget<tlabel>(&window, "tip_feature", false, true);
	label->set_label(str.str());

	// adaptability
	str.str("");
	str << dgettext("wesnoth", "adaptability") << ": ";
	str << hero::arms_str(temp.arms()) << "(" << hero::adaptability_str2(ftofxp12(temp.adaptability_[temp.arms()])) << ")";
	label = find_widget<tlabel>(&window, "tip_adaptability", false, true);
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
	int idx_in_resistances;
	std::set<std::string> resistances_table;
	utils::string_map resistances = temp.get_base_resistances();
	bool att_def_diff = false;
	const map_location& loc = temp.get_location();
	idx_in_resistances = 0;
	str.str("");
	for (utils::string_map::iterator resist = resistances.begin(); resist != resistances.end(); ++resist, idx_in_resistances ++) {
		str << dgettext("wesnoth", resist->first.c_str()) << ": ";

		const map_location& loc = temp.get_location();
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
		if (idx_in_resistances % 2) {
			str << "%\n";
		} else {
			str << "%  ";
		}
	}
	label = find_widget<tlabel>(&window, "tip_resistance", false, true);
	label->set_label(str.str());
}

void tselect_unit::pre_show(CVideo& /*video*/, twindow& window)
{
	tlabel* title = find_widget<tlabel>(&window, "title", false, true);
	tlabel* select_tip = find_widget<tlabel>(&window, "select_tip", false, true);
	if (type_ == ADVANCE) {
		title->set_label(_("Advance Unit"));
		select_tip->set_label(_("What should our victorious unit become?"));
	} else if (type_ == HUMAN) {
		title->set_label(_("Human Control"));
		select_tip->set_label(_("Which should you control?"));
	}

	std::stringstream str;
	
	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);

	for (std::vector<const unit*>::const_iterator itor = partial_troops_.begin(); itor != partial_troops_.end(); ++ itor) {
		const unit* it = *itor;

		int side_num = it->side();
		team& current_team = teams_[side_num - 1];

		/*** Add list item ***/
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		list_item["label"] = it->absolute_image() + "~RC(" + it->team_color() + ">" + team::get_side_color_index(side_num) + ")";
		list_item_item.insert(std::make_pair("icon", list_item));

		// type/name
		str.str("");
		if (it->packed()) {
			str << it->packee_type()->type_name();
		} else {
			str << it->type_name();
		}
		str << "(Lv" << it->level() << ")\n";
		str << it->name();
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("type", list_item));

		// list_item["label"] = it->name();
		// list_item_item.insert(std::make_pair("name", list_item));

		// hp
		str.str("");
		str << it->hitpoints() << "/\n" << it->max_hitpoints();
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("hp", list_item));

		// xp
		str.str("");
		str << it->experience() << "/\n";
		if (it->can_advance()) {
			str << it->max_experience();
		} else {
			str << "-";
		}
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("xp", list_item));

		// movement
		str.str("");
		str << it->movement_left() << "/" << it->total_movement();
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("movement", list_item));

		list->add_row(list_item_item);
	}

	list->set_callback_value_change(dialog_callback<tselect_unit, &tselect_unit::type_selected>);

	refresh_tooltip(window);
}

void tselect_unit::post_show(twindow& window)
{
}

} // namespace gui2
