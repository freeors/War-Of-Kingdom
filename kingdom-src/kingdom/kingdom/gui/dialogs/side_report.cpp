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

#include "gui/dialogs/side_report.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"
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

REGISTER_DIALOG(side_report)

tside_report::tside_report(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, int side_num)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, side_(side_num)
	, current_team_(teams[side_num - 1])
	, candidate_cities_()
	, sorting_widgets_()
	, sorting_widget_(NULL)
	, hero_table_(NULL)
	, window_(NULL)
{
}

bool sort_city_status(const artifical* l, const artifical* r)
{
	size_t lfresh = l->fresh_heros().size(), rfresh = r->fresh_heros().size();
	if (lfresh != rfresh) {
		return lfresh > rfresh;
	}

	double lhp = 1.0 * l->hitpoints() / l->max_hitpoints(), rhp = 1.0 * r->hitpoints() / r->max_hitpoints();
	if (lhp != rhp) {
		return lhp < rhp;
	}

	return l->cityno() < r->cityno();
}

void tside_report::pre_show(CVideo& /*video*/, twindow& window)
{
	window.set_canvas_variable("border", variant("default-border"));

	window_ = &window;
	hero_table_ = find_widget<tlistbox>(&window, "hero_table", false, true);
	candidate_cities_ = current_team_.holden_cities();
	std::sort(candidate_cities_.begin(), candidate_cities_.end(), sort_city_status);

	tlabel* label = find_widget<tlabel>(&window, "status", false, true);
	std::stringstream status_str;
	// ing technology
	const ttechnology* ing_technology = current_team_.ing_technology();
	if (ing_technology) {
		const ttechnology& t = *ing_technology;
		status_str << tintegrate::generate_format(_("Researching technology"), "green") << ": ";
		status_str << t.name();
		status_str << "(";
		std::map<const ttechnology*, int>& halvies = current_team_.half_technologies();
		std::map<const ttechnology*, int>::const_iterator find = halvies.find(&t);
		if (find != halvies.end()) {
			status_str << find->second;
		} else {
			status_str << 0;
		}
		status_str << "/";
		int max_experience = current_team_.technology_max_experience(t);
		if (max_experience != t.max_experience()) {
			status_str << tintegrate::generate_format(max_experience, "blue");
		} else {
			status_str << max_experience;
		}
		status_str << ")";
	} else {
		status_str << tintegrate::generate_format(_("Have researched out all technology"), "green");
	}
	status_str << "\n";

	// treasure
	status_str << tintegrate::generate_format(_("Unallocated treasure"), "green") << ": ";
	status_str << current_team_.holded_treasures().size();
	status_str << "    ";

	// noble
	status_str << tintegrate::generate_format(_("Unallocated noble"), "green") << ": ";
	status_str << current_team_.unappoint_nobles().size();

	label->set_label(status_str.str());

	catalog_page(window, STATUS_PAGE, false);
	hero_table_->set_callback_value_change(dialog_callback3<tside_report, tlistbox, &tside_report::city_changed>);

	ttoggle_button* toggle = find_widget<ttoggle_button>(&window, "show", false, true);
	toggle->set_value(game_config::show_side_report);
}

void tside_report::post_show(twindow& window)
{
	ttoggle_button* toggle = find_widget<ttoggle_button>(&window, "show", false, true);
	game_config::show_side_report = toggle->get_value();
}

void tside_report::city_changed(twindow& window, tlistbox& list, const int type)
{
	if (candidate_cities_.empty()) {
		return;
	}

	twidget* grid_ptr = list.get_row_panel(list.get_selected_row());
	unsigned int selected_row = dynamic_cast<ttoggle_panel*>(grid_ptr)->get_data();
}

void tside_report::fill_table(tlistbox& list, int catalog)
{
	for (std::vector<artifical*>::const_iterator it = candidate_cities_.begin(); it != candidate_cities_.end(); ++ it) {
		artifical& city = **it;
		std::stringstream str;
		
		/*** Add list item ***/
		string_map table_item;
		std::map<std::string, string_map> table_item_item;

		str.str("");
		str << city.name();
		str << "(Lv";
		str << tintegrate::generate_format(city.level(), "green");
		str << ")";
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("name", table_item));

		if (catalog == STATUS_PAGE) {
			str.str("");
			if (city.hitpoints() < city.max_hitpoints() / 2) {
				str << tintegrate::generate_format(city.hitpoints(), "red");
			} else if (city.hitpoints() < city.max_hitpoints()) {
				str << tintegrate::generate_format(city.hitpoints(), "yellow");
			} else {
				str << city.hitpoints();
			}
			str << "/" << city.max_hitpoints();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("hp", table_item));

			str.str("");
			str << city.experience() << "/";
			if (city.can_advance()) {
				str << city.max_experience();
			} else {
				str << "-";
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("xp", table_item));

			str.str("");
			str << tintegrate::generate_format(city.fresh_heros().size(), "green");
			str << "/";
			str << tintegrate::generate_format(city.finish_heros().size(), "red");
			str << "/";
			str << tintegrate::generate_format(city.wander_heros().size(), "yellow");
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("hero", table_item));

			str.str("");
			str << tintegrate::generate_format(city.reside_troops().size(), "yellow");
			str << "/";
			str << city.field_troops().size();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("troop", table_item));

			str.str("");
			str << tintegrate::generate_format(city.total_gold_income(current_team_.market_increase_), "yellow");
			str << "/";
			str << tintegrate::generate_format(city.total_technology_income(current_team_.technology_increase_), "green");
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("income", table_item));

			size_t built = 0;
			size_t building = 0;
			const std::vector<map_location>& ea = city.economy_area();
			for (std::vector<map_location>::const_iterator it2 = ea.begin(); it2 != ea.end(); ++ it2) {
				unit* u = units_.find_unit(*it2);
				if (!u || !hero::is_ea_artifical(u->type()->master())) {
					continue;
				}
				if (u->get_state(ustate_tag::BUILDING)) {
					building ++;
				} else {
					built ++;
				}
			}
			str.str("");
			str << tintegrate::generate_format(building + built, building? "yellow": "green");
			str << "/";
			if (building + built != ea.size()) {
				str << tintegrate::generate_format(ea.size(), "red");
			} else {
				str << ea.size();
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("ea", table_item));
		}
		list.add_row(table_item_item);

		unsigned hero_index = list.get_item_count() - 1;
		twidget* grid_ptr = list.get_row_panel(hero_index);
		ttoggle_panel* toggle = dynamic_cast<ttoggle_panel*>(grid_ptr);
		toggle->set_data(hero_index);
	}
}

void tside_report::catalog_page(twindow& window, int catalog, bool swap)
{
	fill_table(*hero_table_, catalog);
	city_changed(window, *hero_table_);

	// in order to support sortable, form relative data.
	std::vector<tbutton*> widgets;
	if (catalog == STATUS_PAGE) {
		widgets.push_back(&find_widget<tbutton>(&window, "button_name", false));
		widgets.back()->set_active(false);
		widgets.push_back(&find_widget<tbutton>(&window, "button_hp", false));
		widgets.back()->set_active(false);
		widgets.push_back(&find_widget<tbutton>(&window, "button_xp", false));
		widgets.back()->set_active(false);
		widgets.push_back(&find_widget<tbutton>(&window, "button_hero", false));
		widgets.back()->set_active(false);
		widgets.push_back(&find_widget<tbutton>(&window, "button_troop", false));
		widgets.back()->set_active(false);
		widgets.push_back(&find_widget<tbutton>(&window, "button_income", false));
		widgets.back()->set_label(tintegrate::generate_img("misc/gold.png") + " " + tintegrate::generate_img("misc/technology.png"));
		widgets.back()->set_active(false);
		widgets.push_back(&find_widget<tbutton>(&window, "button_ea", false));
		widgets.back()->set_active(false);
	}
	for (std::vector<tbutton*>::iterator i = widgets.begin(); i != widgets.end(); ++ i) {
		tbutton& widget = **i;
		connect_signal_mouse_left_click(
			widget
			, boost::bind(
				&tside_report::sort_column
				, this
				, boost::ref(widget)));
	}
	sorting_widgets_[catalog] = widgets;

	// speeden compare_row, remember this catalog.
	current_page_ = catalog;
}

bool tside_report::compare_row(twidget& row1, twidget& row2)
{
	unsigned int i1 = dynamic_cast<ttoggle_panel*>(&row1)->get_data();
	unsigned int i2 = dynamic_cast<ttoggle_panel*>(&row2)->get_data();

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
			// hp
			if (u1.max_hitpoints() > u2.max_hitpoints() || (u1.max_hitpoints() == u2.max_hitpoints() && u1.hitpoints() > u2.hitpoints())) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[3]) {
			// xp
			if (u1.experience() > u2.experience() || (u1.experience() == u2.experience() && u1.max_experience() > u2.max_experience())) {
				result = false;
			}
		}

	} 

	return ascend_? result: !result;
}

static bool callback_compare_row(void* caller, twidget& row1, twidget& row2)
{
	return reinterpret_cast<tside_report*>(caller)->compare_row(row1, row2);
}

void tside_report::sort_column(tbutton& widget)
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
	hero_table_->invalidate_layout(false);
}

} // namespace gui2
