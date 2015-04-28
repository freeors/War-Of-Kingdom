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

#define GETTEXT_DOMAIN "kingdom-lib"

#include "gui/dialogs/play_card.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "game_display.hpp"
#include "team.hpp"
#include "card.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/scroll_label.hpp"
#include "gui/dialogs/city_list.hpp"
#include "gui/widgets/listbox.hpp"
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

REGISTER_DIALOG(play_card)

tplay_card::tplay_card(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, card_map& cards, game_state& gamestate, int side, button_action* discard)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, cards_(cards)
	, gamestate_(gamestate)
	, current_team_(teams[side - 1])
	, discard_(discard)
	, card_index_(0)
	, hero_table_(NULL)
	, window_(NULL)
{
}

void tplay_card::card_selected(twindow& window, tlistbox& list, const int type)
{
	twindow::tinvalidate_layout_blocker block(window);

	card_index_ = list.get_selected_row();
	refresh_tooltip(window);

	int size = current_team_.holded_cards().size();
	for (int index = 0; index < size; index ++) {
		twidget* grid_ptr = list.get_row_panel(index);
		tbutton& discard = *dynamic_cast<tbutton*>(grid_ptr->find("discard", false));
		if (index == card_index_) {
			discard.set_visible(twidget::VISIBLE);
		} else {
			discard.set_visible(twidget::INVISIBLE);
		}
	}
	list.invalidate_layout(true);
}

void tplay_card::refresh_tooltip(twindow& window)
{
	std::stringstream text;
	std::vector<size_t>& holded_cards = current_team_.holded_cards();
	
	tscroll_label* label = find_widget<tscroll_label>(&window, "tip", false, true);
	if (holded_cards.empty()) {
		// It is necessary set all relative tips to empty.
		label->set_label("");
		return;
	}
	card& c = cards_[holded_cards[card_index_]];

	std::stringstream ss;

	// refresh to gui
	ss << tintegrate::generate_format(c.name(), "green");
	ss << "\n\n";
	ss << c.desc();

	label->set_label(ss.str());
}

void tplay_card::pre_show(CVideo& /*video*/, twindow& window)
{
	std::stringstream str;

	window_ = &window;
	tlistbox* list = find_widget<tlistbox>(&window, "card_list", false, true);

	std::vector<size_t>& holded_cards = current_team_.holded_cards();
	int card_index = 0;
	for (std::vector<size_t>::const_iterator itor = holded_cards.begin(); itor != holded_cards.end(); ++ itor, card_index ++) {
		card& c = cards_[*itor];
		/*** Add list item ***/
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		list_item["label"] = c.image();
		list_item_item.insert(std::make_pair("icon", list_item));

		// name
		str.str("");
		str << c.name();
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("name", list_item));

		// points
		str.str("");
		str << c.points();
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("points", list_item));

		// range
		str.str("");
		int range = c.range();
		if (range == card::NONE) {
			str << dgettext("wesnoth-card", "None");
		} else if (range == card::SINGLE) {
			str << dgettext("wesnoth-card", "Single");
		} else if (range == card::MULTI) {
			str << dgettext("wesnoth-card", "Multi");
		} 
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("range", list_item));
/*
		// movement
		str.str("");
		str << it->movement_left() << "/" << it->total_movement();
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("movement", list_item));

		str.str("");
		str << it->cost() * cost_exponent / 100 << "/" << it->cost() * 2 / 3;
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("cost", list_item));
*/
		list->add_row(list_item_item);

		twidget* grid_ptr = list->get_row_panel(card_index);
		tbutton& discard = *dynamic_cast<tbutton*>(grid_ptr->find("discard", false));
		connect_signal_mouse_left_click(
			discard
			, boost::bind(
				&tplay_card::discard
				, this
				, _3, _4
				, card_index));
		if (card_index) {
			discard.set_visible(twidget::INVISIBLE);
		} else {
			discard.set_visible(twidget::VISIBLE);
		}
	}

	list->set_callback_value_change(dialog_callback3<tplay_card, tlistbox, &tplay_card::card_selected>);

	refresh_tooltip(window);
}

void tplay_card::post_show(twindow& window)
{
}

void tplay_card::discard(bool& handled, bool& halt, int index)
{
	twindow& window = *window_;
	tlistbox* list = find_widget<tlistbox>(&window, "card_list", false, true);

	if (!discard_->button_pressed(index)) {
		return;
	}
	list->remove_row(index);

	int size = current_team_.holded_cards().size();
	for (int i = index; i < size; i ++) {
		twidget* grid_ptr = list->get_row_panel(i);
		tbutton& discard = *dynamic_cast<tbutton*>(grid_ptr->find("discard", false));

		disconnect_signal_mouse_left_click(
			discard
			, boost::bind(
				&tplay_card::discard
				, this
				, _3, _4
				, i + 1));

		connect_signal_mouse_left_click(
			discard
			, boost::bind(
				&tplay_card::discard
				, this
				, _3, _4
				, i));
	}

	if (!current_team_.holded_cards().empty()) {
		if (index == size) {
			index --;
		}
		list->select_row(index);
	} else {
		tbutton* ok = find_widget<tbutton>(&window, "ok", false, true);
		ok->set_active(false);
	}
	card_selected(window, *list);

	handled = true;
	halt = true;
}

void tplay_card::contex(twindow& window)
{
	int side_num = current_team_.side();
	gui2::tcity_list dlg(gui_, teams_, units_, heros_, gamestate_, side_num, gui2::tcity_list::INTERIOR_PAGE);

	try {
		dlg.show(gui_.video());
	} catch(twml_exception& e) {
		e.show(gui_);
		return;
	}
}

} // namespace gui2
