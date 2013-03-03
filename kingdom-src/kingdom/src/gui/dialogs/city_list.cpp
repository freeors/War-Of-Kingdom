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

#include "gui/dialogs/city_list.hpp"

#include "foreach.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "hero.hpp"
#include "team.hpp"
#include "artifical.hpp"
#include "card.hpp"
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

REGISTER_DIALOG(city_list)

tcity_list::tcity_list(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, game_state& gamestate, int side_num)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, gamestate_(gamestate)
	, side_(side_num)
	, candidate_cities_()
	, sorting_widgets_()
	, sorting_widget_(NULL)
	, hero_table_(NULL)
	, window_(NULL)
{
}

void tcity_list::pre_show(CVideo& /*video*/, twindow& window)
{
	window_ = &window;
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	if (side_ >= 1) {
		label->set_label(_("side city list"));
	} else {
		label->set_label(_("city list"));
	}

	hero_table_ = find_widget<tlistbox>(&window, "hero_table", false, true);

	catalog_page(window, STATUS_PAGE, false);

	hero_table_->set_callback_value_change(dialog_callback<tcity_list, &tcity_list::city_changed>);

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "status", false)
		, boost::bind(
			&tcity_list::catalog_page
			, this
			, boost::ref(window)
			, (int)STATUS_PAGE
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "units", false)
		, boost::bind(
			&tcity_list::catalog_page
			, this
			, boost::ref(window)
			, (int)UNITS_PAGE
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "interior", false)
		, boost::bind(
			&tcity_list::catalog_page
			, this
			, boost::ref(window)
			, (int)INTERIOR_PAGE
			, true));
}

void tcity_list::post_show(twindow& window)
{
}

void tcity_list::city_changed(twindow& window)
{
	if (candidate_cities_.empty()) {
		return;
	}

	tgrid* grid_ptr = hero_table_->get_row_grid(hero_table_->get_selected_row());
	unsigned int selected_row = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true))->get_data();

	twindow::tinvalidate_layout_blocker invalidate_layout_blocker(window);

	tcontrol* portrait = find_widget<tcontrol>(&window, "portrait", false, true);

	hero& h = candidate_cities_[selected_row]->master();
	portrait->set_label(h.image());
}

void tcity_list::fill_table(int catalog)
{
	city_map& citys = units_.get_city_map();
	for (city_map::const_iterator i = citys.begin(); i != citys.end(); ++ i) {
		if ((side_ >= 1) && (i->side() != side_)) {
			continue;
		}
		team& current_team = teams_[i->side() - 1];
		candidate_cities_.push_back(&*i);

		std::stringstream str;
		
		/*** Add list item ***/
		string_map table_item;
		std::map<std::string, string_map> table_item_item;

		str.str("");
		str << i->name();
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("name", table_item));

		if (catalog == STATUS_PAGE) {
			str.str("");
			str << teams_[i->side() - 1].name();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("side", table_item));

			str.str("");
			str << i->level();
			if (i->especial() != NO_ESPECIAL) {
				str << "(" << unit_types.especial(i->especial()).name_ << ")";
			}
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

		} else if (catalog == UNITS_PAGE) {
			str.str("");
			str << i->fresh_heros().size();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("fresh_hero", table_item));

			str.str("");
			str << i->finish_heros().size();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("finish_hero", table_item));

			str.str("");
			str << i->wander_heros().size();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("wander_hero", table_item));

			str.str("");
			str << i->reside_troops().size();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("reside_troop", table_item));

			str.str("");
			str << i->field_troops().size();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("field_troop", table_item));

		} else if (catalog == INTERIOR_PAGE) {
			str.str("");
			if (i->decree().first) {
				str << i->decree().first->name() << "(" << i->decree().second << ")";
			} else {
				str << dgettext("wesnoth-lib", "None");
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("decree", table_item));

			str.str("");
			str << i->field_commoners().size();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("field", table_item));

			str.str("");
			str << i->reside_commoners().size();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("reside", table_item));

			str.str("");
			str << i->total_gold_income(current_team.market_increase_);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("gold_income", table_item));

			str.str("");
			str << i->total_technology_income(current_team.technology_increase_);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("technology_income", table_item));

		}
		hero_table_->add_row(table_item_item);

		unsigned hero_index = hero_table_->get_item_count() - 1;
		tgrid* grid_ptr = hero_table_->get_row_grid(hero_index);
		ttoggle_panel* toggle = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true));
		toggle->set_data(hero_index);
	}
}

void tcity_list::catalog_page(twindow& window, int catalog, bool swap)
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
		// because sort, order is changed.
		selected_row = hero_table_->get_selected_row();
		tgrid* grid_ptr = hero_table_->get_row_grid(hero_table_->get_selected_row());
		selected_row = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true))->get_data();
	}

	window.alternate_uh(hero_table_, index);

	fill_table(catalog);

	if (swap) {
		window.alternate_bh(hero_table_, index);
		hero_table_->select_row(selected_row);
		// swap to other page, there is no sorted column.
		sorting_widget_ = NULL;
	} else {
		city_changed(window);
	}

	// in order to support sortable, form relative data.
	std::vector<tbutton*> widgets;
	if (catalog == STATUS_PAGE) {
		widgets.push_back(&find_widget<tbutton>(&window, "button_name", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_side", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_level", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_hp", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_xp", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_status", false));
		widgets.back()->set_active(false);
	} else if (catalog == UNITS_PAGE) {
		widgets.push_back(&find_widget<tbutton>(&window, "button_name", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_fresh", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_finish", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_wander", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_reside", false));
		widgets.push_back(&find_widget<tbutton>(&window, "button_field", false));
	} else if (catalog == INTERIOR_PAGE) {
		widgets.push_back(&find_widget<tbutton>(&window, "button_name", false));
		widgets.back()->set_active(false);
		widgets.push_back(&find_widget<tbutton>(&window, "button_decree", false));
		widgets.back()->set_active(false);
		widgets.push_back(&find_widget<tbutton>(&window, "button_field", false));
		widgets.back()->set_active(false);
		widgets.push_back(&find_widget<tbutton>(&window, "button_reside", false));
		widgets.back()->set_active(false);
		widgets.push_back(&find_widget<tbutton>(&window, "button_gold_income", false));
		widgets.back()->set_active(false);
		widgets.push_back(&find_widget<tbutton>(&window, "button_technology_income", false));
		widgets.back()->set_active(false);
	}
	for (std::vector<tbutton*>::iterator i = widgets.begin(); i != widgets.end(); ++ i) {
		tbutton& widget = **i;
		connect_signal_mouse_left_click(
			widget
			, boost::bind(
				&tcity_list::sort_column
				, this
				, boost::ref(widget)));
	}
	sorting_widgets_[catalog] = widgets;

	// speeden compare_row, remember this catalog.
	current_page_ = catalog;
}

bool tcity_list::compare_row(tgrid& row1, tgrid& row2)
{
	unsigned int i1 = dynamic_cast<ttoggle_panel*>(row1.find("_toggle", true))->get_data();
	unsigned int i2 = dynamic_cast<ttoggle_panel*>(row2.find("_toggle", true))->get_data();

	const artifical& u1 = *candidate_cities_[i1];
	const artifical& u2 = *candidate_cities_[i2];

	bool result = true;
	std::vector<tbutton*>& widgets = sorting_widgets_[current_page_];

	if (current_page_ == STATUS_PAGE) {
		if (sorting_widget_ == widgets[0]) {
			// name
			result = utils::utf8str_compare(u1.name(), u2.name());
		} else if (sorting_widget_ == widgets[1]) {
			// side
			result = utils::utf8str_compare(teams_[u1.side() - 1].name(), teams_[u2.side() - 1].name());
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
		}

	} else if (current_page_ == UNITS_PAGE) {
		if (sorting_widget_ == widgets[0]) {
			// name
			result = utils::utf8str_compare(u1.name(), u2.name());
		} else if (sorting_widget_ == widgets[1]) {
			// fresh heros
			if (u1.fresh_heros().size() > u2.fresh_heros().size()) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[2]) {
			// finish heros
			if (u1.finish_heros().size() > u2.finish_heros().size()) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[3]) {
			// wander heros
			if (u1.wander_heros().size() > u2.wander_heros().size()) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[4]) {
			// reside troops
			if (u1.reside_troops().size() > u2.reside_troops().size()) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[5]) {
			// field troops
			if (u1.field_troops().size() > u2.field_troops().size()) {
				result = false;
			}
		}

	}

	return ascend_? result: !result;
}

static bool callback_compare_row(void* caller, tgrid& row1, tgrid& row2)
{
	return reinterpret_cast<tcity_list*>(caller)->compare_row(row1, row2);
}

void tcity_list::sort_column(tbutton& widget)
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
