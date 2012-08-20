/* $Id: unit_attack.cpp 48883 2011-03-13 10:18:54Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/dialogs/unit_attack.hpp"

#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "unit.hpp"
#include "game_display.hpp"
#include "marked-up_text.hpp"
#include "gettext.hpp"

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_unit_attack
 *
 * == Unit attack ==
 *
 * This shows the dialog for attacking units.
 *
 * @begin{table}{dialog_widgets}
 * attacker_portrait & & image   & o & Shows the portrait of the attacking unit. $
 * attacker_icon     & & image   & o & Shows the icon of the attacking unit. $
 * attacker_name     & & control & o & Shows the name of the attacking unit. $
 *
 * defender_portrait & & image   & o & Shows the portrait of the defending unit. $
 * defender_icon     & & image   & o & Shows the icon of the defending unit. $
 * defender_name     & & control & o & Shows the name of the defending unit. $
 *
 *
 * weapon_list       & & listbox & m & The list with weapons to choos from. $
 * -attacker_weapon  & & control & o & The weapon for the attacker to use. $
 * -defender_weapon  & & control & o & The weapon for the defender to use. $
 *
 * @end{table}
 */

REGISTER_DIALOG(unit_attack)

tunit_attack::tunit_attack(game_display& gui, unit_map& units, unit& attacker, unit& defender, std::vector<battle_context>& bc_vector, unsigned int best)
	: gui_(gui)
	, units_(units)
	, attacker_(attacker)
	, defender_(defender)
	, bc_vector_(bc_vector)
	, best_(best)
{
}

template<class T>
static void set_label(
		  twindow& window
		, const std::string& id
		, const std::string& label)
{
	T* widget = find_widget<T>(&window, id, false, false);
	widget->set_label(label);
}

static void set_attacker_info(twindow& window, unit& u)
{
	std::stringstream str;
	
	tcontrol* control = find_widget<tcontrol>(&window, "attacker_portrait", false, true);
	control->set_label(u.master().image());

	tlabel* label = find_widget<tlabel>(&window, "attacker_arm", false, true);
	str.str("");
	if (!u.packed()) {
		str << u.type_name();
	} else {
		str << u.packee_type()->type_name();
		str << "[" << u.type_name() << "]";
	}
	label->set_label(str.str());

	label = find_widget<tlabel>(&window, "attacker_race", false, true);
	str.str("");
	str << u.race()->name(u.gender());
	label->set_label(str.str());

	label = find_widget<tlabel>(&window, "attacker_level", false, true);
	str.str("");
	str << dgettext("wesnoth", "level") << " " << u.level();
	label->set_label(str.str());

	// hp
	label = find_widget<tlabel>(&window, "attacker_hp", false, true);
	str.str("");
	str << dgettext("wesnoth", "HP: ") << u.hitpoints() << "/" << u.max_hitpoints();
	label->set_label(str.str());

	// xp
	label = find_widget<tlabel>(&window, "attacker_xp", false, true);
	str.str("");
	str << dgettext("wesnoth", "XP: ") << u.experience() << "/" << u.max_experience() << "\n";
	label->set_label(str.str());
}

static void set_defender_info(twindow& window, unit& u)
{
	std::stringstream str;
	
	tcontrol* control = find_widget<tcontrol>(&window, "defender_portrait", false, true);
	control->set_label(u.master().image());

	tlabel* label = find_widget<tlabel>(&window, "defender_arm", false, true);
	str.str("");
	if (!u.packed()) {
		str << u.type_name();
	} else {
		str << u.packee_type()->type_name();
		str << "[" << u.type_name() << "]";
	}
	label->set_label(str.str());

	label = find_widget<tlabel>(&window, "defender_race", false, true);
	str.str("");
	str << u.race()->name(u.gender());
	label->set_label(str.str());

	label = find_widget<tlabel>(&window, "defender_level", false, true);
	str.str("");
	str << dgettext("wesnoth", "level") << " " << u.level();
	label->set_label(str.str());

	// hp
	label = find_widget<tlabel>(&window, "defender_hp", false, true);
	str.str("");
	str << dgettext("wesnoth", "HP: ") << u.hitpoints() << "/" << u.max_hitpoints();
	label->set_label(str.str());

	// xp
	label = find_widget<tlabel>(&window, "defender_xp", false, true);
	str.str("");
	str << dgettext("wesnoth", "XP: ") << u.experience() << "/" << u.max_experience() << "\n";
	label->set_label(str.str());
}

void tunit_attack::set_weapon_info(twindow& window, const std::vector<battle_context>& bc_vector, const int best_weapon)
{
	std::stringstream str;

	tlistbox& weapon_list =
			find_widget<tlistbox>(&window, "weapon_list", false);
	window.keyboard_capture(&weapon_list);

	for (size_t i = 0; i < bc_vector.size(); ++i) {
		const battle_context::unit_stats& att = bc_vector[i].get_attacker_stats();
		const battle_context::unit_stats& def = bc_vector[i].get_defender_stats();
		config tmp_config;
		attack_type no_weapon(tmp_config);
		const attack_type& attw = attack_type(*att.weapon);
		const attack_type& defw = attack_type(def.weapon ? *def.weapon : no_weapon);

		attw.set_specials_context(&attacker_, &defender_, attacker_, true);
		defw.set_specials_context(&attacker_, &defender_, attacker_, false);

		std::map<std::string, string_map> data;
		string_map item;

		item["label"] = attw.icon();
		data.insert(std::make_pair("attacker_weapon_icon", item));

		item["label"] = attw.name();
		data.insert(std::make_pair("attacker_weapon", item));

		str.str("");
		str << att.damage << "-" << att.num_blows << " "  << " (" << att.chance_to_hit << "%)";
		item["label"] = str.str();
		data.insert(std::make_pair("attacker_weapon_damage", item));

		item["label"] = attw.weapon_specials();
		data.insert(std::make_pair("attacker_weapon_specials", item));

		str.str("");
		std::string range = attw.range().empty()? defw.range() : attw.range();
		if (!range.empty()) {
			range = dgettext("wesnoth", range.c_str());
		}
		str << "- " << range << " -";
		item["label"] = str.str();
		data.insert(std::make_pair("weapon_range", item));

		item["label"] = defw.name();
		data.insert(std::make_pair("defender_weapon", item));

		item["label"] = defw.icon();
		data.insert(std::make_pair("defender_weapon_icon", item));

		str.str("");
		if (!defw.get_cfg().empty()) {
			str << def.damage << "-" << def.num_blows << " "  << " (" << def.chance_to_hit << "%)";
		}
		item["label"] = str.str();
		data.insert(std::make_pair("defender_weapon_damage", item));

		item["label"] = defw.weapon_specials();
		data.insert(std::make_pair("defender_weapon_specials", item));

		weapon_list.add_row(data);

	}

	assert(best_weapon < static_cast<int>(weapon_list.get_item_count()));
	weapon_list.select_row(best_weapon);
}

void tunit_attack::pre_show(CVideo& /*video*/, twindow& window)
{
	set_attacker_info(window, attacker_);
	set_defender_info(window, defender_);

	selected_weapon_ = -1;
	set_weapon_info(window, bc_vector_, best_);
}

void tunit_attack::post_show(twindow& window)
{
	if(get_retval() == twindow::OK) {
		selected_weapon_ = find_widget<tlistbox>(&window, "weapon_list", false)
				.get_selected_row();
	}
}

} // namespace gui2

