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

#define GETTEXT_DOMAIN "kingdom-lib"

#include "gui/dialogs/artifical_list.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"
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
#include "gui/widgets/stacked_widget.hpp"
#include "gui/widgets/listbox.hpp"
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
	, current_page_(twidget::npos)
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

	page_panel_ = find_widget<tstacked_widget>(&window, "page", false, true);

	lists_.push_back(find_widget<tlistbox>(&window, "ownership_list", false, true));
	lists_.push_back(find_widget<tlistbox>(&window, "status_list", false, true));

	for (std::vector<tlistbox*>::const_iterator it = lists_.begin(); it != lists_.end(); ++ it) {
		tlistbox& list = **it;
		list.set_callback_value_change(dialog_callback3<tartifical_list, tlistbox, &tartifical_list::city_changed>);
	}
	catalog_page(window, OWNERSHIP_PAGE, false);

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

void tartifical_list::city_changed(twindow& window, tlistbox& list, const int type)
{
	int selected_row = list.get_selected_row();

	if (candidate_artificals_.empty()) {
		return;
	}

	twindow::tinvalidate_layout_blocker invalidate_layout_blocker(window);

	tcontrol* portrait = find_widget<tcontrol>(&window, "portrait", false, true);

	portrait->set_label(candidate_artificals_[selected_row]->absolute_image());
}

void tartifical_list::fill_table(tlistbox& list, int catalog)
{
	const team& viewing_team = teams_[gui_.viewing_team()];

	for (unit_map::const_iterator it = units_.begin(); it != units_.end(); ++ it) {
		unit* i = dynamic_cast<unit*>(&*it);

		if ((side_ >= 1) && (i->side() != side_)) {
			continue;
		}
		if (!i->is_artifical() || i->is_city()) {
			continue;
		}
		bool known = viewing_team.knows_about_team(i->side() - 1, false);
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
			if (i->get_state(ustate_tag::PETRIFIED)) {
				str << _("Petrified ");
			}
			if (i->get_state(ustate_tag::SLOWED)) {
				str << _("Slowed ");
			}
			if (i->get_state(ustate_tag::POISONED)) {
				str << _("Poisoned ");
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("status_status", table_item));

		} 
		list.add_row(table_item_item);
	}
}

void tartifical_list::catalog_page(twindow& window, int catalog, bool swap)
{
	if (!page_panel_) {
		return;
	}
	if (current_page_ == catalog) {
		return;
	}

	unsigned int selected_row = 0;
	if (swap) {
		// because sort, order is changed.
		tlistbox& list = *lists_[current_page_];
		selected_row = list.get_selected_row();
		list.clear();
	}
	page_panel_->set_radio_layer(catalog);

	tlistbox& list = *lists_[catalog];
	fill_table(list, catalog);

	if (swap) {
		list.select_row(selected_row);
		list.invalidate_layout(true);
	} else {
		city_changed(window, list);
	}

	current_page_ = catalog;
}

} // namespace gui2
