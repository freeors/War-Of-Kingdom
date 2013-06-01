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
#include "gui/widgets/scroll_label.hpp"
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
	tscroll_label* tip = find_widget<tscroll_label>(&window, "tip", false, true);
	if (partial_troops_.empty()) {
		// It is necessary set all relative tips to empty.
		tip->set_label("");
		return;
	}
	const unit& temp = *(partial_troops_[troop_index_]);
	tip->set_label(temp.form_gui2_tip());
}

void tselect_unit::pre_show(CVideo& /*video*/, twindow& window)
{
	tlabel* title = find_widget<tlabel>(&window, "title", false, true);
	tlabel* select_tip = find_widget<tlabel>(&window, "select_tip", false, true);
	if (type_ == ADVANCE) {
		title->set_label(_("Advance Unit"));
		select_tip->set_label(_("What should our victorious unit become?"));
	} else if (type_ == CHANGE) {
		title->set_label(_("Change unit"));
		select_tip->set_label(_("Which should you change?"));
	} else if (type_ == HUMAN) {
		title->set_label(_("Human Control"));
		select_tip->set_label(_("Which should you control?"));
	}

	std::stringstream str;
	
	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);

	for (std::vector<const unit*>::const_iterator itor = partial_troops_.begin(); itor != partial_troops_.end(); ++ itor) {
		const unit* it = *itor;

		const unit_type* ut = it->type();
		if (it->packed()) {
			ut = it->packee_type();
		}

		int side_num = it->side();
		team& current_team = teams_[side_num - 1];

		/*** Add list item ***/
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		list_item["label"] = it->absolute_image() + "~RC(" + it->team_color() + ">" + team::get_side_color_index(side_num) + ")";
		list_item_item.insert(std::make_pair("icon", list_item));

		if (ut->especial() != NO_ESPECIAL) {
			list_item["label"] = unit_types.especial(ut->especial()).image_;
			list_item_item.insert(std::make_pair("human", list_item));
		}

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
