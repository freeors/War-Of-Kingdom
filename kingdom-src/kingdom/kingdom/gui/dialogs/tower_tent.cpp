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

#include "gui/dialogs/tower_tent.hpp"

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

const int tower_employ_heros = 6;

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

REGISTER_DIALOG(tower_tent)

ttower_tent::ttower_tent(display& gui, hero_map& heros, card_map& cards, const config& cfg, const config& campaign_config, hero& player_hero)
	: ttent(heros, cards, cfg, campaign_config, player_hero, "tower")
	, trandom_map(cfg, mode_tag::TOWER)
	, gui_(gui)
{
}

ttower_tent::~ttower_tent()
{
}

void ttower_tent::pre_show(CVideo& video, twindow& window)
{
	ttent::pre_show(video, window);
	trandom_map::pre_show(window);

	tent::employ_count = tower_employ_heros;

	ai_count_ = find_widget<tbutton>(&window, "ai_count", false, true);
	turns_ = find_widget<tbutton>(&window, "turns", false, true);

	connect_signal_mouse_left_click(
		*ai_count_
		, boost::bind(
			&ttower_tent::ai_count
			, this
			, boost::ref(window)));
	connect_signal_mouse_left_click(
		*turns_
		, boost::bind(
			&ttower_tent::turns
			, this
			, boost::ref(window)));

	std::stringstream strstr;
	strstr.str("");
	tent::ai_count = 30;
	strstr << tent::ai_count;
	ai_count_->set_label(strstr.str());

	tent::turns = 50;
	strstr.str("");
	strstr << tent::turns;
	turns_->set_label(strstr.str());
}

void ttower_tent::ai_count(twindow& window)
{
	// The possible eras to play
	std::vector<tval_str> items;

	items.push_back(tval_str(30, "30"));
	items.push_back(tval_str(40, "40"));
	items.push_back(tval_str(50, "50"));
	items.push_back(tval_str(60, "60"));
	items.push_back(tval_str(70, "70"));

	gui2::tcombo_box dlg(items, tent::ai_count);
	dlg.show(gui_.video());

	int selected = dlg.selected_index();
	tent::ai_count = items[selected].val;

	std::stringstream strstr;
	strstr << tent::ai_count;
	ai_count_->set_label(strstr.str());
}

void ttower_tent::turns(twindow& window)
{
	// The possible eras to play
	std::vector<tval_str> items;

	items.push_back(tval_str(20, "20"));
	items.push_back(tval_str(30, "30"));
	items.push_back(tval_str(50, "50"));
	items.push_back(tval_str(70, "70"));
	items.push_back(tval_str(100, "100"));

	gui2::tcombo_box dlg(items, tent::turns);
	dlg.show(gui_.video());

	int selected = dlg.selected_index();
	tent::turns = items[selected].val;

	std::stringstream strstr;
	strstr << tent::turns;
	turns_->set_label(strstr.str());
}

void ttower_tent::update_map_settings(twindow& window)
{
}

} // namespace gui2
