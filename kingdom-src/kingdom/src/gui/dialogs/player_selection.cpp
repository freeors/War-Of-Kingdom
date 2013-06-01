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

#include "gui/dialogs/player_selection.hpp"

#include "foreach.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "game_display.hpp"
#include "card.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#include "gui/dialogs/field.hpp"
#include "gui/widgets/toggle_button.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "game_config.hpp"
#include "loadscreen.hpp"
#include <preferences.hpp>

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

REGISTER_DIALOG(player_selection)

tplayer_selection::tplayer_selection(game_display& gui, hero_map& heros, card_map& cards, const config& cfg, const config& campaign_config, hero& player_hero)
	: ttent(heros, cards, cfg, campaign_config, player_hero, "rpg")
	, gui_(gui)
{
}

tplayer_selection::~tplayer_selection()
{
}

void tplayer_selection::pre_show(CVideo& video, twindow& window)
{
	ttent::pre_show(video, window);

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "player_city", false)
		, boost::bind(
		&tplayer_selection::player_city
		, this
		, boost::ref(window)));

	tcontrol* control = find_widget<tcontrol>(&window, "player_city", false, true);
	if (rows_mem_[player_].hero_ == player_hero_->number_) {
	} else {
		control->set_visible(twidget::INVISIBLE);
	}

	control = find_widget<tcontrol>(&window, "ok", false, true);
	control->set_active(rows_mem_[player_].city_ != -1);
}

void tplayer_selection::player_selected(twindow& window)
{
	ttent::player_selected(window);
	tbutton* button = find_widget<tbutton>(&window, "player_city", false, true);
	button->set_active(rows_mem_[player_].hero_ == player_hero_->number_);
}

void tplayer_selection::player_city(twindow& window)
{
	std::stringstream str;
	std::vector<std::string> items;
	std::vector<int> citynos;
	int activity_index = -1;

	tlistbox* list = find_widget<tlistbox>(&window, "player_list", false, true);

	int index = 0;
	for (std::map<int, int>::const_iterator it = city_map_.begin(); it != city_map_.end(); ++ it, index ++) {
		hero& h = heros_[it->second];
		citynos.push_back(it->first);
		str.str("");
		str << h.name() << "(";
		hero& leader = heros_[city_leader_map_.find(it->first)->second];
		str << leader.name() << ")";
		items.push_back(str.str());
		if (rows_mem_[0].city_ == it->second) {
			activity_index = index;
		}
	}
	if (activity_index == -1) {
		activity_index = 0;
	}

	gui2::tcombo_box dlg(items, activity_index);
	dlg.show(gui_.video());

	activity_index = dlg.selected_index();

	rows_mem_[0].leader_ = city_leader_map_.find(citynos[activity_index])->second;
	rows_mem_[0].city_ = city_map_.find(citynos[activity_index])->second;

	tgrid* grid_ptr = list->get_row_grid(0);
	tcontrol* control = dynamic_cast<tcontrol*>(grid_ptr->find("side", true));
	control->set_label(heros_[rows_mem_[0].leader_].name());

	control = dynamic_cast<tcontrol*>(grid_ptr->find("city", true));
	control->set_label(heros_[rows_mem_[0].city_].name());

	tbutton* ok = find_widget<tbutton>(&window, "ok", false, true);
	ok->set_active(rows_mem_[player_].city_ != -1);
}

} // namespace gui2
