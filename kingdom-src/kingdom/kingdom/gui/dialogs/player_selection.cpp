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

#include "gui/dialogs/player_selection.hpp"

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
#include "gui/widgets/listbox.hpp"
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

tplayer_selection::tplayer_selection(display& gui, hero_map& heros, card_map& cards, const config& cfg, const config& campaign_config, hero& player_hero)
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

void tplayer_selection::player_selected(twindow& window, tlistbox& list, const int type)
{
	ttent::player_selected(window, list, type);
	tbutton* button = find_widget<tbutton>(&window, "player_city", false, true);
	std::map<int, int>& city_map = (player_ == 0)? city_map_: empty_city_map_;
	button->set_active(rows_mem_[player_].hero_ == player_hero_->number_ && !city_map.empty());
}

void tplayer_selection::player_city(twindow& window)
{
	std::stringstream str;
	std::vector<tval_str> items;
	std::vector<int> citynos;

	tlistbox* list = find_widget<tlistbox>(&window, "player_list", false, true);
	std::map<int, int>& city_map = (player_ == 0)? city_map_: empty_city_map_;

	int index = 0;
	for (std::map<int, int>::const_iterator it = city_map.begin(); it != city_map.end(); ++ it, index ++) {
		hero& h = heros_[it->second];
		citynos.push_back(it->first);
		str.str("");
		str << h.name();
		if (player_ == 0) {
			str << "(";
			hero& leader = heros_[city_leader_map_.find(it->first)->second];
			str << leader.name() << ")";
		}
		items.push_back(tval_str(it->second, str.str()));
	}

	gui2::tcombo_box dlg(items, rows_mem_[player_].city_);
	dlg.show(gui_.video());

	int activity_index = dlg.selected_index();

	if (rows_mem_[player_].leader_ != player_hero_->number_) {
		rows_mem_[player_].leader_ = city_leader_map_.find(citynos[activity_index])->second;
	}
	rows_mem_[player_].city_ = city_map.find(citynos[activity_index])->second;

	twidget* grid_ptr = list->get_row_panel(player_);
	tcontrol* control = dynamic_cast<tcontrol*>(grid_ptr->find("side", true));
	control->set_label(heros_[rows_mem_[player_].leader_].name());

	control = dynamic_cast<tcontrol*>(grid_ptr->find("city", true));
	control->set_label(heros_[rows_mem_[player_].city_].name());

	tbutton* ok = find_widget<tbutton>(&window, "ok", false, true);
	ok->set_active(rows_mem_[player_].city_ != -1);
}

} // namespace gui2
