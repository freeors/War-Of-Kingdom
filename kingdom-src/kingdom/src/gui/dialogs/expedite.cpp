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

#include "gui/dialogs/expedite.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "game_display.hpp"
#include "team.hpp"
#include "artifical.hpp"
#include "map.hpp"
#include "actions.hpp"
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
#include "gui/dialogs/unit_merit.hpp"
#include "help.hpp"
#include "menu_events.hpp"

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

REGISTER_DIALOG(expedite)

texpedite::texpedite(events::menu_handler& menu_handler, game_display& gui, const gamemap& map, std::vector<team>& teams, unit_map& units, hero_map& heros, artifical& city, button_action* disband)
	: menu_handler_(menu_handler)
	, gui_(gui)
	, map_(map)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, city_(city)
	, disband_(disband)
	, troop_index_(0)
	, hero_table_(NULL)
	, window_(NULL)
{
}

void texpedite::type_selected(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "type_list", false);

	twindow::tinvalidate_layout_blocker invalidate_layout_blocker(window);
	list.invalidate_layout();

	troop_index_ = list.get_selected_row();
	refresh_tooltip(window);

	// There is maybe no troop after disband. so troop_index_ maybe equal -1.
	if (troop_index_ >= 0) {
		int size = city_.reside_troops().size();
		const unit& u = *city_.reside_troops()[troop_index_];
		for (int index = 0; index < size; index ++) {
			tgrid* grid_ptr = list.get_row_grid(index);
			// tbutton& disband = find_widget<tbutton>(&window, "disband", false);
			tbutton& disband = *dynamic_cast<tbutton*>(grid_ptr->find("disband", false));
			if (index == troop_index_) {
				disband.set_visible(twidget::VISIBLE);
				disband.set_active(u.human());
			} else {
				disband.set_visible(twidget::INVISIBLE);
			}
		}

		tbutton* ok = find_widget<tbutton>(&window, "ok", false, true);
		ok->set_active(u.human() && can_move(u));
	}
}

void texpedite::refresh_tooltip(twindow& window)
{
	std::vector<unit*>& reside_troops = city_.reside_troops();

	tscroll_label* tip = find_widget<tscroll_label>(&window, "tip", false, true);
	if (reside_troops.empty()) {
		tbutton* button = find_widget<tbutton>(&window, "hero_list", false, true);
		button->set_active(false);

		button = find_widget<tbutton>(&window, "merit", false, true);
		button->set_active(false);

		// It is necessary set all relative tips to empty.
		tip->set_label("");
		return;
	}
	unit& temp = *reside_troops[troop_index_];
	tip->set_label(temp.form_tip(true));
}

void texpedite::pre_show(CVideo& /*video*/, twindow& window)
{
	window_ = &window;

	int side_num = city_.side();
	team& current_team = teams_[side_num - 1];
	std::stringstream str;

	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);

	int cost_exponent = current_team.cost_exponent();
	std::vector<unit*>& reside_troops = city_.reside_troops();
	int troop_index = 0;
	for (std::vector<unit*>::const_iterator it = reside_troops.begin(); it != reside_troops.end(); ++ it, troop_index ++) {
		unit& u = **it;
		/*** Add list item ***/
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		list_item["label"] = u.absolute_image() + "~RC(" + u.team_color() + ">" + team::get_side_color_index(side_num) + ")";
		list_item_item.insert(std::make_pair("icon", list_item));

		// type/name
		str.str("");
		str << u.type_name() << "(Lv" << u.level() << ")\n";
		str << u.name();
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("type", list_item));

		// list_item["label"] = it->name();
		// list_item_item.insert(std::make_pair("name", list_item));

		// hp
		str.str("");
		str << u.hitpoints() << "/\n" << u.max_hitpoints();
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("hp", list_item));

		// xp
		str.str("");
		str << u.experience() << "/\n";
		if (u.can_advance()) {
			str << u.max_experience();
		} else {
			str << "-";
		}
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("xp", list_item));

		// movement
		str.str("");
		str << u.movement_left() << "/" << u.total_movement();
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("movement", list_item));

		str.str("");
		str << u.cost() * cost_exponent / 100 << "/" << calculate_disband_income(u, cost_exponent);
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("cost", list_item));

		list->add_row(list_item_item);

		tgrid* grid_ptr = list->get_row_grid(troop_index);
		twidget* widget = grid_ptr->find("human", false);
		widget->set_visible(u.human()? twidget::VISIBLE: twidget::INVISIBLE);

		// tbutton& disband = find_widget<tbutton>(&window, "disband", false);
		tbutton& disband = *dynamic_cast<tbutton*>(grid_ptr->find("disband", false));
		connect_signal_mouse_left_click(
			disband
			, boost::bind(
				&texpedite::disband
				, this
				, _3, _4
				, troop_index));
		if (troop_index) {
			disband.set_visible(twidget::INVISIBLE);
		} else {
			disband.set_visible(twidget::VISIBLE);
			disband.set_active(reside_troops[troop_index]->human());
		}

	}

	list->set_callback_value_change(dialog_callback<texpedite, &texpedite::type_selected>);

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "hero_list", false)
		, boost::bind(
			&texpedite::hero_list
			, this
			, boost::ref(window)));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "merit", false)
		, boost::bind(
			&texpedite::merit
			, this
			, boost::ref(window)));

	refresh_tooltip(window);

	tbutton* ok = find_widget<tbutton>(&window, "ok", false, true);
	const unit& u = *reside_troops[troop_index_];
	ok->set_active(can_move(u) && u.human());
}

void texpedite::hero_list(twindow& window)
{
	std::vector<unit*>& reside_troops = city_.reside_troops();
	unit& temp = *reside_troops[troop_index_];

	std::vector<hero*> v;
	v.push_back(&temp.master());
	if (temp.second().valid()) {
		v.push_back(&temp.second());
	}
	if (temp.third().valid()) {
		v.push_back(&temp.third());
	}
	menu_handler_.hero_list(v);
}

void texpedite::merit(twindow& window)
{
	std::vector<unit*>& reside_troops = city_.reside_troops();
	unit& temp = *reside_troops[troop_index_];

	tunit_merit merit(gui_, temp);
	merit.show(gui_.video());
}

void texpedite::post_show(twindow& window)
{
}

void texpedite::disband(bool& handled, bool& halt, int index)
{
	twindow& window = *window_;
	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);

	if (!disband_->button_pressed(index)) {
		return;
	}

	list->remove_row(index);

	int size = city_.reside_troops().size();
	for (int i = index; i < size; i ++) {
		tgrid* grid_ptr = list->get_row_grid(i);
		// tbutton& disband = find_widget<tbutton>(&window, "disband", false);
		tbutton& disband = *dynamic_cast<tbutton*>(grid_ptr->find("disband", false));

		disconnect_signal_mouse_left_click(
			disband
			, boost::bind(
				&texpedite::disband
				, this
				, _3, _4
				, i + 1));

		connect_signal_mouse_left_click(
			disband
			, boost::bind(
				&texpedite::disband
				, this
				, _3, _4
				, i));

		disband.set_active(city_.reside_troops()[i]->human());
	}

	tbutton* ok = find_widget<tbutton>(&window, "ok", false, true);
	if (!city_.reside_troops().empty()) {
		if (index == size) {
			index --;
		}
		list->select_row(index);
		const unit& u = *city_.reside_troops()[index];
		ok->set_active(can_move(u) && u.human());
	} else {
		ok->set_active(false);
	}
	type_selected(window);

	handled = true;
	halt = true;

	window.invalidate_layout();
}

bool texpedite::can_move(const unit& u)
{
	for (size_t i = 0; i < city_.adjacent_size_; i ++) {
		if (u.movement_cost(map_[city_.adjacent_[i]]) <= u.movement_left()) {
			return true;
		}
	}
	return false;
}

} // namespace gui2
