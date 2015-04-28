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

#include "gui/dialogs/independence.hpp"

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

REGISTER_DIALOG(independence)

tindependence::tindependence(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, artifical* aggressing)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, current_team_(teams[rpg::h->side_])
	, aggressing_(aggressing)
	, candidate_cities_()
{
}

void tindependence::type_selected(twindow& window, tlistbox& list, const int type)
{
}

void tindependence::pre_show(CVideo& /*video*/, twindow& window)
{
	window.set_canvas_variable("border", variant("default-border"));

	std::stringstream str;

	tlabel* label = find_widget<tlabel>(&window, "tip", false, true);
	utils::string_map symbols;
	symbols["side"] = current_team_.name();
	str.str("");
	str << "<format>";
	if (current_team_.defeat_vote()) {
		str << "color='green' text='" << vgettext2("It is all up with the wind, $side will fallen.", symbols);
	} else {
		str << "color='red' text='" << vgettext2("There are enough troops, $side will be again you.", symbols);
	}
	str << "'</format>";
	label->set_label(str.str());

	tlistbox* list = find_widget<tlistbox>(&window, "city_list", false, true);

	int human_resides, ai_resides, human_fields, ai_fields;
	std::vector<artifical*>& holden_cities = current_team_.holden_cities();
	for (std::vector<artifical*>::iterator it = holden_cities.begin(); it != holden_cities.end(); ++ it) {
		artifical& city = *unit_2_artifical(*it);
		/*** Add list item ***/
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		list_item["label"] = city.absolute_image();
		list_item_item.insert(std::make_pair("icon", list_item));

		list_item["label"] = city.name();
		list_item_item.insert(std::make_pair("name", list_item));

		// hp
		str.str("");
		str << city.hitpoints() << "/\n" << city.max_hitpoints();
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("hp", list_item));

		// xp
		str.str("");
		str << city.experience() << "/\n";
		if (city.can_advance()) {
			str << city.max_experience();
		} else {
			str << "-";
		}
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("xp", list_item));

		// reside hero
		str.str("");
		str << city.finish_heros().size() + city.fresh_heros().size();
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("reside_hero", list_item));

		// reside troop
		str.str("");
		human_resides = ai_resides = 0;
		std::vector<unit*>& resides = city.reside_troops();
		for (std::vector<unit*>::const_iterator it = resides.begin(); it != resides.end(); ++ it) {
			const unit& u = **it;
			if (u.human()) {
				human_resides ++;
			} else {
				ai_resides ++;
			}
		}
		str << human_resides << "/" << ai_resides;
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("reside_troop", list_item));

		// field troop
		str.str("");
		 human_fields = ai_fields = 0;
		std::vector<unit*>& fields = city.field_troops();
		for (std::vector<unit*>::const_iterator it = fields.begin(); it != fields.end(); ++ it) {
			if ((*it)->human()) {
				human_fields ++;
			} else {
				ai_fields ++;
			}
		}
		str << human_fields << "/" << ai_fields;
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("field_troop", list_item));

		// vote
		str.str("");
		if (city.independence_vote(aggressing_)) {
			str << "misc/independence.png";
		}
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("vote", list_item));

		list->add_row(list_item_item);

		twidget* grid_ptr = list->get_row_panel(list->get_item_count() - 1);
		twidget* widget = grid_ptr->find("human", false);
		widget->set_visible(city.cityno() == rpg::h->city_? twidget::VISIBLE: twidget::INVISIBLE);

		candidate_cities_.push_back(&city);
	}

	list->set_callback_value_change(dialog_callback3<tindependence, tlistbox, &tindependence::type_selected>);
}

void tindependence::post_show(twindow& window)
{
}

} // namespace gui2
