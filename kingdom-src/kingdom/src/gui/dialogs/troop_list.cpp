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

#include "gui/dialogs/troop_list.hpp"

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

REGISTER_DIALOG(troop_list)

ttroop_list::ttroop_list(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, game_state& gamestate, int side_num)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, gamestate_(gamestate)
	, side_(side_num)
	, candidate_troops_()
	, sorting_widgets_()
	, sorting_widget_(NULL)
	, hero_table_(NULL)
	, window_(NULL)
{
}

void ttroop_list::pre_show(CVideo& /*video*/, twindow& window)
{
	window_ = &window;
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	if (side_ >= 1) {
		label->set_label(_("side troop list"));
	} else {
		label->set_label(_("troop list"));
	}

	hero_table_ = find_widget<tlistbox>(&window, "hero_table", false, true);

	catalog_page(window, STATUS_PAGE, false);

	hero_table_->set_callback_value_change(dialog_callback<ttroop_list, &ttroop_list::city_changed>);

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "ownership", false)
		, boost::bind(
			&ttroop_list::catalog_page
			, this
			, boost::ref(window)
			, (int)OWNERSHIP_PAGE
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "status", false)
		, boost::bind(
			&ttroop_list::catalog_page
			, this
			, boost::ref(window)
			, (int)STATUS_PAGE
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "merit", false)
		, boost::bind(
			&ttroop_list::catalog_page
			, this
			, boost::ref(window)
			, (int)MERIT_PAGE
			, true));

	if (candidate_troops_.empty()) {
		find_widget<tbutton>(&window, "ownership", false).set_active(false);
		find_widget<tbutton>(&window, "status", false).set_active(false);
		find_widget<tbutton>(&window, "merit", false).set_active(false);
		return;
	}
}

void ttroop_list::post_show(twindow& window)
{
}

void ttroop_list::city_changed(twindow& window)
{
	if (candidate_troops_.empty()) {
		return;
	}

	tgrid* grid_ptr = hero_table_->get_row_grid(hero_table_->get_selected_row());
	unsigned int selected_row = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true))->get_data();

	twindow::tinvalidate_layout_blocker invalidate_layout_blocker(window);

	tcontrol* portrait = find_widget<tcontrol>(&window, "portrait", false, true);

	hero& h = candidate_troops_[selected_row]->master();
	portrait->set_label(h.image(true));

	portrait = find_widget<tcontrol>(&window, "second_portrait", false, true);
	if (candidate_troops_[selected_row]->second().valid()) {
		portrait->set_label(candidate_troops_[selected_row]->second().image());
	} else {
		portrait->set_label("");
	}

	portrait = find_widget<tcontrol>(&window, "third_portrait", false, true);
	if (candidate_troops_[selected_row]->third().valid()) {
		portrait->set_label(candidate_troops_[selected_row]->third().image());
	} else {
		portrait->set_label("");
	}
}

void ttroop_list::fill_table(int catalog)
{
	const team& viewing_team = teams_[gui_.viewing_team()];

	for (unit_map::const_iterator it = units_.begin(); it != units_.end(); ++ it) {
		unit* i = dynamic_cast<unit*>(&*it);
		if ((side_ >= 1) && (i->side() != side_)) {
			continue;
		}
		if (i->is_artifical() || i->is_commoner()) {
			continue;
		}
		bool known = viewing_team.knows_about_team(i->side() - 1, false);
		if (!known) {
			// continue;
		}
		candidate_troops_.push_back(&*i);

		std::stringstream str;
		
		/*** Add list item ***/
		string_map table_item;
		std::map<std::string, string_map> table_item_item;

		str.str("");
		str << i->name();
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("name", table_item));

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
			if (known) {
				str << i->packee_type()->type_name();
			} else{
				str << "??";
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("type", table_item));

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
			if (known) {
				str << i->packee_type()->type_name();
			} else{
				str << "??";
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("type", table_item));

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
			str << i->movement_left() << "/" << i->total_movement();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("movement", table_item));

			str.str("");
			if (i->get_state(ustate_tag::PETRIFIED))
				str << _("Petrified ");
			if (i->get_state(ustate_tag::SLOWED))
				str << _("Slowed ");
			if (i->get_state(ustate_tag::POISONED))
				str << _("Poisoned ");
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("status_status", table_item));

		} else if (catalog == MERIT_PAGE) {
			str.str("");
			if (known) {
				str << i->packee_type()->type_name();
			} else{
				str << "??";
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("type", table_item));

			str.str("");
			str << i->cause_damage_;
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("cause_damage", table_item));

			str.str("");
			str << i->been_damage_;
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("been_damage", table_item));

			str.str("");
			str << i->defeat_units_;
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("defeat_units", table_item));

			str.str("");
			str << i->field_turns_;
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("field_turns", table_item));

		}  
		hero_table_->add_row(table_item_item);

		unsigned hero_index = hero_table_->get_item_count() - 1;
		tgrid* grid_ptr = hero_table_->get_row_grid(hero_index);
		ttoggle_panel* toggle = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true));
		toggle->set_data(hero_index);
	}
}

void ttroop_list::catalog_page(twindow& window, int catalog, bool swap)
{
	if (catalog < MIN_PAGE || catalog > MAX_PAGE) {
		return;
	}
	int index = catalog - MIN_PAGE;

	if (hero_table_->current_page() == index) {
		// desired page is the displaying page, do nothing.
		return;
	}
	
	unsigned int selected_row = 0;
	if (swap) {
		// because sort, order is changed.
		selected_row = hero_table_->get_selected_row();
		tgrid* grid_ptr = hero_table_->get_row_grid(hero_table_->get_selected_row());
		selected_row = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true))->get_data();
	}

	hero_table_->swap_uh(window, index);

	fill_table(catalog);

	if (swap) {
		hero_table_->swap_bh(window);
		hero_table_->select_row(selected_row);
		// swap to other page, there is no sorted column.
		sorting_widget_ = NULL;
	} else {
		city_changed(window);
	}

	// in order to support sortable, form relative data.
	std::vector<tbutton*> widgets;
	if (catalog == OWNERSHIP_PAGE) {
		widgets.push_back(&find_widget<tbutton>(&window, "button_name", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_type", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_side", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_city", false));
	} else if (catalog == STATUS_PAGE) {
		widgets.push_back(&find_widget<tbutton>(&window, "button_name", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_type", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_level", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_hp", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_xp", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_traits", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_movement", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_status", false));
		widgets.back()->set_active(false);
	} else if (catalog == MERIT_PAGE) {
		widgets.push_back(&find_widget<tbutton>(&window, "button_name", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_type", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_cause_damage", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_been_damage", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_defeat_units", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_field_turns", false));
	}
	for (std::vector<tbutton*>::iterator i = widgets.begin(); i != widgets.end(); ++ i) {
		tbutton& widget = **i;
		connect_signal_mouse_left_click(
			widget
			, boost::bind(
				&ttroop_list::sort_column
				, this
				, boost::ref(widget)));
	}
	sorting_widgets_[catalog] = widgets;

	// speeden compare_row, remember this catalog.
	current_page_ = catalog;
}

bool ttroop_list::compare_row(tgrid& row1, tgrid& row2)
{
	unsigned int i1 = dynamic_cast<ttoggle_panel*>(row1.find("_toggle", true))->get_data();
	unsigned int i2 = dynamic_cast<ttoggle_panel*>(row2.find("_toggle", true))->get_data();

	const unit& u1 = *candidate_troops_[i1];
	const unit& u2 = *candidate_troops_[i2];

	bool result = true;
	std::vector<tbutton*>& widgets = sorting_widgets_[current_page_];

	if (current_page_ == OWNERSHIP_PAGE) {
		if (sorting_widget_ == widgets[0]) {
			// name
			result = utils::utf8str_compare(u1.name(), u2.name());
		} else if (sorting_widget_ == widgets[1]) {
			// type
			result = utils::utf8str_compare(u1.packee_type()->id(), u2.packee_type()->id());
		} else if (sorting_widget_ == widgets[2]) {
			// side
			result = utils::utf8str_compare(teams_[u1.side() - 1].name(), teams_[u2.side() - 1].name());
		} else if (sorting_widget_ == widgets[3]) {
			// city
			std::string str1, str2;
			artifical* city = units_.city_from_cityno(u1.cityno());
			if (city) {
				str1 = city->name();
			}
			city = units_.city_from_cityno(u2.cityno());
			if (city) {
				str2 = city->name();
			}
			result = utils::utf8str_compare(str1, str2);
		} 

	} else if (current_page_ == STATUS_PAGE) {
		if (sorting_widget_ == widgets[0]) {
			// name
			result = utils::utf8str_compare(u1.name(), u2.name());
		} else if (sorting_widget_ == widgets[1]) {
			// type
			result = utils::utf8str_compare(u1.packee_type()->id(), u2.packee_type()->id());
		} else if (sorting_widget_ == widgets[2]) {
			// level
			if (u1.level() > u2.level()) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[3]) {
			// hp
			if (u1.max_hitpoints() > u2.max_hitpoints() || (u1.max_hitpoints() == u2.max_hitpoints() && u1.hitpoints() > u2.hitpoints())) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[4]) {
			// xp
			if (u1.experience() > u2.experience() || (u1.experience() == u2.experience() && u1.max_experience() > u2.max_experience())) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[5]) {
			// traits
			result = utils::utf8str_compare(utils::join(u1.trait_names(), ", "), utils::join(u2.trait_names(), ", "));
		} else if (sorting_widget_ == widgets[6]) {
			// movement
			if (u1.movement_left() > u2.movement_left() || (u1.movement_left() == u2.movement_left() && u1.total_movement() > u2.total_movement())) {
				result = false;
			}
		}

	} else if (current_page_ == MERIT_PAGE) {
		if (sorting_widget_ == widgets[0]) {
			// name
			result = utils::utf8str_compare(u1.name(), u2.name());
		} else if (sorting_widget_ == widgets[1]) {
			// type
			result = utils::utf8str_compare(u1.packee_type()->id(), u2.packee_type()->id());
		} else if (sorting_widget_ == widgets[2]) {
			// cause_damage
			if (u1.cause_damage_ > u2.cause_damage_) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[3]) {
			// been_damage
			if (u1.been_damage_ > u2.been_damage_) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[4]) {
			// defeat_units
			if (u1.defeat_units_ > u2.defeat_units_) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[5]) {
			// field_turns
			if (u1.field_turns_ > u2.field_turns_) {
				result = false;
			}
		}

	}

	return ascend_? result: !result;
}

static bool callback_compare_row(void* caller, tgrid& row1, tgrid& row2)
{
	return reinterpret_cast<ttroop_list*>(caller)->compare_row(row1, row2);
}

void ttroop_list::sort_column(tbutton& widget)
{
	twindow& window = *window_;
	if (sorting_widget_ && (sorting_widget_ != &widget)) {
		sorting_widget_->set_sort(tbutton::NONE);
	}
	// sorting_widget_ must valid before widget.set_sort
	sorting_widget_ = &widget;

	widget.set_sort(tbutton::TOGGLE);
	ascend_ = true;
	if (widget.get_sort() == tbutton::DESCEND) {
		ascend_ = false;
	}
	
	hero_table_->sort(this, callback_compare_row);
	hero_table_->invalidate_layout();
}

} // namespace gui2
