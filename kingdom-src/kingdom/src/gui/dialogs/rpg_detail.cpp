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

#include "gui/dialogs/rpg_detail.hpp"

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
#include "gui/dialogs/troop_detail.hpp"

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

REGISTER_DIALOG(rpg_detail)

trpg_detail::trpg_detail(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, troop_index_(0)
	, hero_table_(NULL)
{
}

void trpg_detail::type_selected(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "type_list", false);

	troop_index_ = list.get_selected_row();
}

void trpg_detail::rpg_2_ui(twindow& window)
{
	int value;
	std::stringstream str;
	const team& current_team = teams_[rpg::h->side_];
	bool in_troop = !find_unit(units_, *rpg::h)->is_artifical();

	// refresh to gui
	tlabel* label = find_widget<tlabel>(&window, "tip_name", false, true);
	str << dgettext("wesnoth", "hero") << ": " << rpg::h->name();
	label->set_label(str.str());

	str.str("");
	label = find_widget<tlabel>(&window, "tip_loyalty", false, true);
	int loyalty = rpg::h->loyalty(*teams_[rpg::h->side_].leader());
	str << dgettext("wesnoth", "loyalty") << ": " << loyalty;
	label->set_label(str.str());

	// stratum
	str.str("");
	label = find_widget<tlabel>(&window, "tip_stratum", false, true);
	str << _("Stratum") << ": " << hero::stratum_str(rpg::stratum);
	label->set_label(str.str());

	// official
	str.str("");
	label = find_widget<tlabel>(&window, "tip_official", false, true);
	str << _("Official") << ": " << hero::official_str(rpg::h->official_);
	label->set_label(str.str());

	// ownership
	str.str("");
	str << _("Ownership") << ": ";
	str << units_.city_from_cityno(rpg::h->city_)->name();
	label = find_widget<tlabel>(&window, "tip_ownership", false, true);
	label->set_label(str.str());

	// meritorious
	str.str("");
	value = rpg::h->meritorious_;
	str << _("Meritorious") << ": " << value;
	label = find_widget<tlabel>(&window, "tip_meritorious", false, true);
	label->set_label(str.str());
	str.str("");
	if (rpg::stratum == hero_stratum_citizen) {
		utils::string_map symbols;
		std::string message;
		int statement_type = -1;
		if (!rpg::h->meritorious_ && !in_troop) {
			str << _("Wait, leader will order you to recruit");
		} else if (rpg::h->meritorious_ < 1000) {
			symbols["meritorious"] = "1000";
			statement_type = 0;
		} else if (rpg::h->meritorious_ < 2500) {
			symbols["meritorious"] = "2500";
			statement_type = 0;
		} else if (rpg::h->meritorious_ < 5000) {
			symbols["meritorious"] = "5000";
			statement_type = 0;
		} else if (rpg::h->meritorious_ < 8000) {
			symbols["meritorious"] = "8000";
			statement_type = 1;
		} else {
			str << _("Get mroe meritorious, will be appointed mayor");
		}
		if (statement_type == 0) {
			str << vgettext("wesnoth-lib", "To $meritorious, will control more troops", symbols);
		} else if (statement_type == 1) {
			str << vgettext("wesnoth-lib", "To $meritorious, will be appointed mayor", symbols);
		}
	} else if (rpg::stratum == hero_stratum_mayor) {
		if (current_team.holded_cities().size() < 2) {
			str << _("Capture more city, can be independent");
		} else {
			str << _("Be independent will be to leader");
		}
	}
	if (str.str().empty()) {
		str << _("---");
	}
	label = find_widget<tlabel>(&window, "tip_meritorious2", false, true);
	label->set_label(str.str());

	// leadership
	label = find_widget<tlabel>(&window, "tip_leadership", false, true);
	label->set_label(lexical_cast<std::string>(fxptoi9(rpg::h->leadership_)));

	// force
	label = find_widget<tlabel>(&window, "tip_force", false, true);
	label->set_label(lexical_cast<std::string>(fxptoi9(rpg::h->force_)));

	// intellect
	label = find_widget<tlabel>(&window, "tip_intellect", false, true);
	label->set_label(lexical_cast<std::string>(fxptoi9(rpg::h->intellect_)));

	// politics
	label = find_widget<tlabel>(&window, "tip_politics", false, true);
	label->set_label(lexical_cast<std::string>(fxptoi9(rpg::h->politics_)));

	// charm
	label = find_widget<tlabel>(&window, "tip_charm", false, true);
	label->set_label(lexical_cast<std::string>(fxptoi9(rpg::h->charm_)));
/*
	// skill
	str.str("");
	str << dgettext("wesnoth-lib", "Skill") << ": ";
	str << hero::skill_str(hero_skill_demolish) << "(" << hero::adaptability_str2(ftofxp12(temp.skill_[hero_skill_demolish])) << ")";
	label = find_widget<tlabel>(&window, "tip_skill", false, true);
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
*/
}

void trpg_detail::pre_show(CVideo& /*video*/, twindow& window)
{
	rpg_2_ui(window);

	std::stringstream str;
	
	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);

	for (std::set<unit*>::const_iterator itor = rpg::humans.begin(); itor != rpg::humans.end(); ++ itor) {
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
		str << it->type_name() << "(Lv" << it->level() << ")\n";
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

		list->add_row(list_item_item);

		tgrid* grid = list->get_row_grid(list->get_item_count() - 1);
		twidget* widget = grid->find("human", false);
		widget->set_visible(it->human()? twidget::VISIBLE: twidget::INVISIBLE);
	}

	list->set_callback_value_change(dialog_callback<trpg_detail, &trpg_detail::type_selected>);

	tbutton& troops_button = find_widget<tbutton>(&window, "troops", false);
	connect_signal_mouse_left_click(
		troops_button
		, boost::bind(
		&trpg_detail::human_troops
		, this
		, boost::ref(window)));

}

void trpg_detail::human_troops(twindow& window)
{
	std::vector<const unit*> partial_troops;

	for (std::set<unit*>::const_iterator itor = rpg::humans.begin(); itor != rpg::humans.end(); ++ itor) {
		const unit* troop = *itor;
		partial_troops.push_back(troop);
	}

	gui2::ttroop_detail dlg(gui_, teams_, units_, heros_, partial_troops, dgettext("wesnoth-lib", "RPG Troop"));
	try {
		dlg.show(gui_.video());
	} catch(twml_exception& e) {
		e.show(gui_);
		return;
	}
}

void trpg_detail::post_show(twindow& window)
{
}

} // namespace gui2
