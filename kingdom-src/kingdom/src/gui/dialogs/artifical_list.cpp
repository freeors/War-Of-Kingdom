/* $Id: hero_list.cpp 49598 2011-05-22 17:55:54Z mordante $ */
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

#include "gui/dialogs/artifical_list.hpp"

#include "foreach.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "hero.hpp"
#include "team.hpp"
#include "artifical.hpp"
#include "game_display.hpp"
#include "gamestatus.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/scroll_label.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "network.hpp"
#include "ai/manager.hpp"

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

REGISTER_DIALOG(artifical_list)

tartifical_list::tartifical_list(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, game_state& gamestate, int side_num)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, gamestate_(gamestate)
	, side_(side_num)
	, candidate_artificals_()
	, hero_table_(NULL)
{
}

void tartifical_list::pre_show(CVideo& /*video*/, twindow& window)
{
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	if (side_ >= 1) {
		label->set_label(_("side artifical list"));
	} else {
		label->set_label(_("artifical list"));
	}

	hero_table_ = find_widget<tlistbox>(&window, "hero_table", false, true);

	catalog_page(window, OWNERSHIP_PAGE, false);

	hero_table_->set_callback_value_change(dialog_callback<tartifical_list, &tartifical_list::city_changed>);

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "ownership", false)
		, boost::bind(
			&tartifical_list::catalog_page
			, this
			, boost::ref(window)
			, (int)OWNERSHIP_PAGE
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "status", false)
		, boost::bind(
			&tartifical_list::catalog_page
			, this
			, boost::ref(window)
			, (int)STATUS_PAGE
			, true));

	if (candidate_artificals_.empty()) {
		find_widget<tbutton>(&window, "ownership", false).set_active(false);
		find_widget<tbutton>(&window, "status", false).set_active(false);
		return;
	}
}

void tartifical_list::post_show(twindow& window)
{
}

void tartifical_list::city_changed(twindow& window)
{
	int selected_row = hero_table_->get_selected_row();

	if (candidate_artificals_.empty()) {
		return;
	}

	twindow::tinvalidate_layout_blocker invalidate_layout_blocker(window);

	tcontrol* portrait = find_widget<tcontrol>(&window, "portrait", false, true);

	portrait->set_label(candidate_artificals_[selected_row]->absolute_image());
}

void tartifical_list::fill_table(int catalog)
{
	const team& viewing_team = teams_[gui_.viewing_team()];

	for (unit_map::const_iterator i = units_.begin(); i != units_.end(); ++i) {
		if ((side_ >= 1) && (i->side() != side_)) {
			continue;
		}
		if (!i->is_artifical() || i->is_city()) {
			continue;
		}
		bool known = viewing_team.knows_about_team(i->side() - 1, network::nconnections() > 0);
		if (!known) {
			// continue;
		}
		candidate_artificals_.push_back(const_unit_2_artifical(&*i));

		std::stringstream str;
		
		/*** Add list item ***/
		string_map table_item;
		std::map<std::string, string_map> table_item_item;

		str.str("");
		str << i->name();
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("name", table_item));

		table_item["label"] = i->type_name();
		table_item_item.insert(std::make_pair("type", table_item));

		if (catalog == OWNERSHIP_PAGE) {
			str.str("");
			if (known) {
				str << teams_[i->side() - 1].name();
			} else {
				str << "??";
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("side", table_item));

			str.str("");
			if (known && units_.city_from_cityno(i->cityno())) {
				str << units_.city_from_cityno(i->cityno())->name();
			} else{
				str << "";
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("city", table_item));

		} else if (catalog == STATUS_PAGE) {
			str.str("");
			str << i->level();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("level", table_item));

			str.str("");
			str << i->hitpoints()  << "/" << i->max_hitpoints();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("hp", table_item));

			str.str("");
			str << i->experience() << "/";
			if (i->can_advance()) {
				str << i->max_experience();
			} else {
				str << "-";
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("xp", table_item));

			str.str("");
			str << utils::join(i->trait_names(), ", ");
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("traits", table_item));

			str.str("");
			if (i->get_state(unit::STATE_PETRIFIED))
				str << _("Petrified ");
			if (i->get_state(unit::STATE_SLOWED))
				str << _("Slowed ");
			if (i->get_state(unit::STATE_POISONED))
				str << _("Poisoned ");
			if (i->get_state("invisible"))
				str << _("Invisible");
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("status_status", table_item));

		} 
		hero_table_->add_row(table_item_item);
	}
}

void tartifical_list::catalog_page(twindow& window, int catalog, bool swap)
{
	if (catalog < MIN_PAGE || catalog > MAX_PAGE) {
		return;
	}
	int index = catalog - MIN_PAGE;

	if (window.alternate_index() == index) {
		// desired page is the displaying page, do nothing.
		return;
	}
	
	int selected_row = 0;
	if (swap) {
		selected_row = hero_table_->get_selected_row();
	}

	window.alternate_uh(hero_table_, index);

	fill_table(catalog);

	if (swap) {
		window.alternate_bh(hero_table_, index);
		hero_table_->select_row(selected_row);
	} else {
		city_changed(window);
	}
}

} // namespace gui2
