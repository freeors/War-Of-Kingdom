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

#include "gui/dialogs/interior.hpp"

#include "actions.hpp"
#include "foreach.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "game_display.hpp"
#include "team.hpp"
#include "hero.hpp"
#include "artifical.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/toggle_button.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/widgets/settings.hpp"
#include "gui/widgets/stacked_widget.hpp"
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

REGISTER_DIALOG(interior)

tinterior::tinterior(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, int side)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, current_team_(teams_[side - 1])
	, fresh_heros_()
	, type_index_(0)
	, checked_heros_()
	, appoint_(NULL)
	, hero_table_(NULL)
	, artificals_(0)
	, income_(0)
{
	std::vector<hero*>& commercials = current_team_.commercials();
	departments_.push_back(department(department::commercial, _("Commercial"), "interior/commercial.png", "themes/gold.png"));
	hero& h1 = !commercials.empty()? *commercials[0]: hero_invalid;
	hero& h2 = commercials.size() >= 2? *commercials[1]: hero_invalid;
	hero& h3 = commercials.size() >= 3? *commercials[2]: hero_invalid;
	departments_.back().exploiture_ = calculate_exploiture(h1, h2, h3);
}

void tinterior::type_selected(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "type_list", false);
	type_index_ = list.get_selected_row();
	
	checked_heros_.clear();

	const department& t = departments_[type_index_];
	std::vector<hero*>& officials = current_team_.commercials();
	if (!officials.empty()) {
		checked_heros_.insert(std::find(fresh_heros_.begin(), fresh_heros_.end(), officials[0]) - fresh_heros_.begin());
	}
	if (officials.size() >= 2) {
		checked_heros_.insert(std::find(fresh_heros_.begin(), fresh_heros_.end(), officials[1]) - fresh_heros_.begin());
	}
	if (officials.size() >= 3) {
		checked_heros_.insert(std::find(fresh_heros_.begin(), fresh_heros_.end(), officials[2]) - fresh_heros_.begin());
	}
	// appoint_->set_active(!officials.empty());

	refresh_tooltip(window);
}

void tinterior::hero_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	int toggled_index = toggle->get_data();
	std::set<int>::iterator result = checked_heros_.find(toggled_index);

	if (result == checked_heros_.end()) {
		// toggled button isn't in checked_heros_
		if (toggle->get_value()) {
			if (checked_heros_.size() < 3) {
				checked_heros_.insert(toggled_index);
			} else {
				// At most select three heros. decheck it!
				toggle->set_value(false);
				return;
			}
		} else {
			VALIDATE(false, "hero_toggled program error #1");
		}
	} else if (toggle->get_value()) { 
		VALIDATE(false, "hero_toggled program error #2");
	} else {
		checked_heros_.erase(result);
	}

	twindow* window = toggle->get_window();

	bool active = false;

	std::vector<hero*> officials;
	if (departments_[type_index_].type_ == department::commercial) {
		officials = current_team_.commercials();
	}

	if (checked_heros_.size() != officials.size()) {
		active = true;
	} 
	if (!active) {
		for (std::set<int>::const_iterator itor = checked_heros_.begin(); itor != checked_heros_.end(); ++ itor) {
			if (std::find(officials.begin(), officials.end(), fresh_heros_[*itor]) == officials.end()) {
				active = true;
				break;
			}
		}
	}
	appoint_->set_active(active);

	refresh_tooltip(*window);
}

void tinterior::refresh_tooltip(twindow& window)
{
	std::stringstream str;

	tstacked_widget* stacked = find_widget<tstacked_widget>(&window, "middle_top_part", false, true);
	stacked->set_dirty(true);
	
	std::vector<hero*> v;
	int index = 0;
	for (std::set<int>::const_iterator itor = checked_heros_.begin(); itor != checked_heros_.end(); ++ itor, index ++) {
		if (index == 0) {
			v.push_back(fresh_heros_[*itor]);
		} else if (index == 1) {
			v.push_back(fresh_heros_[*itor]);
		} else if (index == 2) {
			v.push_back(fresh_heros_[*itor]);
		}
	}

	// refresh to gui
	tcontrol* control = find_widget<tcontrol>(&window, "master_png", false, true);
	tlabel* label = find_widget<tlabel>(&window, "master_name", false, true);
	if (!v.empty()) {
		control->set_label(v[0]->image());
		label->set_label(v[0]->name());
	} else {
		control->set_label("");
		label->set_label("");
	}

	control = find_widget<tcontrol>(&window, "second_png", false, true);
	label = find_widget<tlabel>(&window, "second_name", false, true);
	if (v.size() >= 2) {
		control->set_label(v[1]->image());
		label->set_label(v[1]->name());
	} else {
		control->set_label("");
		label->set_label("");
	}

	control = find_widget<tcontrol>(&window, "third_png", false, true);
	label = find_widget<tlabel>(&window, "third_name", false, true);
	if (v.size() >= 3) {
		control->set_label(v[2]->image());
		label->set_label(v[2]->name());
	} else {
		control->set_label("");
		label->set_label("");
	}

	// exploiture
	int exploiture = calculate_exploiture(((v.size() >= 1)? *v[0]: hero_invalid), ((v.size() >= 2)? *v[1]: hero_invalid), ((v.size() >= 3)? *v[2]: hero_invalid));
	str.str("");
	if (exploiture > departments_[type_index_].exploiture_) {
		find_widget<tcontrol>(&window, "image_exploiture", false, true)->set_label("misc/increase.png");
		str << exploiture - departments_[type_index_].exploiture_;

	} else if (exploiture < departments_[type_index_].exploiture_) {
		find_widget<tcontrol>(&window, "image_exploiture", false, true)->set_label("misc/decrease.png");
		str << departments_[type_index_].exploiture_ - exploiture;

	} else {
		find_widget<tcontrol>(&window, "image_exploiture", false, true)->set_label("misc/equal.png");
		str << "0";
	}
	find_widget<tlabel>(&window, "tip_exploiture", false, true)->set_label(str.str());
		
	// total
	str.str("");
	str << "x " << artificals_ << " = " << artificals_ * income_ * exploiture / 100;
	find_widget<tlabel>(&window, "tip_total", false, true)->set_label(str.str());
}

void tinterior::appoint(twindow& window)
{
	hero* h1 = &hero_invalid;
	hero* h2 = &hero_invalid;
	hero* h3 = &hero_invalid;

	std::stringstream str;
	std::vector<hero*> v;
	int index = 0;
	for (std::set<int>::const_iterator itor = checked_heros_.begin(); itor != checked_heros_.end(); ++ itor, index ++) {
		if (index == 0) {
			h1 = fresh_heros_[*itor];
			v.push_back(h1);
		} else if (index == 1) {
			h2 = fresh_heros_[*itor];
			v.push_back(h2);
		} else if (index == 2) {
			h3 = fresh_heros_[*itor];
			v.push_back(h3);
		}
	}
	current_team_.set_commercials(v);

	tgrid* grid_ptr;
	tcontrol* control;
	for (index = 0; index < (int)fresh_heros_.size(); index ++) {
		grid_ptr = hero_table_->get_row_grid(index);
		control = dynamic_cast<tcontrol*>(grid_ptr->find("name", true));
		hero& h = *fresh_heros_[index];
		str.str("");
		if (h.official_ == hero_official_commercial) {
			str << "<0,0,255>";
		}
		str << h.name();
		control->set_label(str.str());
	}

	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);
	grid_ptr = list->get_row_grid(type_index_);

	departments_[type_index_].exploiture_ = calculate_exploiture(*h1, *h2, *h3);
	// exploiture data
	str.str("");
	str << artificals_ * income_ * departments_[type_index_].exploiture_ / 100;
	control = dynamic_cast<tcontrol*>(grid_ptr->find("total", true));
	control->set_label(str.str());

	str.str("");
	str << _("Exploiture") << "  " << departments_[type_index_].exploiture_ << "%";
	control = dynamic_cast<tcontrol*>(grid_ptr->find("exploiture", true));
	control->set_label(str.str());

	// official portrait
	control = dynamic_cast<tcontrol*>(grid_ptr->find("master_portrait", true));
	if (h1->valid()) {
		control->set_label(h1->image());
	} else {
		control->set_label("");
	}
	control = dynamic_cast<tcontrol*>(grid_ptr->find("second_portrait", true));
	if (h2->valid()) {
		control->set_label(h2->image());
	} else {
		control->set_label("");
	}
	control = dynamic_cast<tcontrol*>(grid_ptr->find("third_portrait", true));
	if (h3->valid()) {
		control->set_label(h3->image());	
	} else {
		control->set_label("");
	}

	appoint_->set_active(false);
	refresh_tooltip(window);
}

void tinterior::pre_show(CVideo& /*video*/, twindow& window)
{
	int side_num = current_team_.side();
	std::stringstream str;

	const unit_type* ut = unit_types.find_market();
	income_ = ut->gold_income();

	const std::vector<artifical*>& holded_cities = current_team_.holded_cities();
	for (std::vector<artifical*>::const_iterator c_itor = holded_cities.begin(); c_itor != holded_cities.end(); ++ c_itor) {
		const std::vector<hero*>& freshes = (*c_itor)->fresh_heros();
		std::copy(freshes.begin(), freshes.end(), std::back_inserter(fresh_heros_));
		const std::vector<hero*>& finishes = (*c_itor)->finish_heros();
		std::copy(finishes.begin(), finishes.end(), std::back_inserter(fresh_heros_));

		// calculate artificals
		const std::vector<map_location>& economy_area = (*c_itor)->economy_area();
		for (std::vector<map_location>::const_iterator it = economy_area.begin(); it != economy_area.end(); ++ it) {
			unit_map::const_iterator it2 = units_.find(*it);
			if (it2.valid() && it2->type() == ut) {
				artificals_ ++;
			}
		}
	}
	std::sort(fresh_heros_.begin(), fresh_heros_.end(), compare_politics);

	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);

	for (std::vector<department>::const_iterator d_itor = departments_.begin(); d_itor != departments_.end(); ++ d_itor) {
		/*** Add list item ***/
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		std::vector<hero*> officials;
		if (d_itor->type_ == department::commercial) {
			officials = current_team_.commercials();
		}

		// if (gold >= type->cost() * cost_exponent / 100) {
		if (true) {
			list_item["label"] = d_itor->image_;
		} else {
			list_item["label"] = d_itor->image_ + "~GS()";
		}
		list_item_item.insert(std::make_pair("icon", list_item));

		list_item["label"] = d_itor->name_;
		list_item_item.insert(std::make_pair("name", list_item));

		list_item["label"] = d_itor->portrait_;
		list_item_item.insert(std::make_pair("portrait", list_item));

		str.str("");
		str << artificals_ * income_ * d_itor->exploiture_ / 100;
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("total", list_item));

		str.str("");
		str << _("Exploiture") << "  " << d_itor->exploiture_ << "%";
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("exploiture", list_item));

		if (!officials.empty()) {
			list_item["label"] = officials[0]->image();
		} else {
			list_item["label"] = "";
		}
		list_item_item.insert(std::make_pair("master_portrait", list_item));

		if (officials.size() >= 2) {
			list_item["label"] = officials[1]->image();
		} else {
			list_item["label"] = "";
		}
		list_item_item.insert(std::make_pair("second_portrait", list_item));

		if (officials.size() >= 3) {
			list_item["label"] = officials[2]->image();
		} else {
			list_item["label"] = "";
		}
		list_item_item.insert(std::make_pair("third_portrait", list_item));

		list->add_row(list_item_item);
	}

	appoint_ = find_widget<tbutton>(&window, "appoint", false, true);
	appoint_->set_active(false);

	type_selected(window);

	find_widget<tcontrol>(&window, "image_exploiture", false, true)->set_label("misc/equal.png");
	find_widget<tcontrol>(&window, "image_total", false, true)->set_label("buttons/market.png");

	list->set_callback_value_change(dialog_callback<tinterior, &tinterior::type_selected>);

	hero_table_ = find_widget<tlistbox>(&window, "hero_table", false, true);
	
	// fill data to hero_table
	catalog_page(window, ABILITY_PAGE, false);

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "ability", false)
		, boost::bind(
			&tinterior::catalog_page
			, this
			, boost::ref(window)
			, (int)ABILITY_PAGE
			, true));
	if (fresh_heros_.empty()) {
		find_widget<tbutton>(&window, "ability", false).set_active(false);
	}

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "adaptability", false)
		, boost::bind(
			&tinterior::catalog_page
			, this
			, boost::ref(window)
			, (int)ADAPTABILITY_PAGE
			, true));
	if (fresh_heros_.empty()) {
		find_widget<tbutton>(&window, "adaptability", false).set_active(false);
	}

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "relation", false)
		, boost::bind(
			&tinterior::catalog_page
			, this
			, boost::ref(window)
			, (int)RELATION_PAGE
			, true));
	if (fresh_heros_.empty()) {
		find_widget<tbutton>(&window, "relation", false).set_active(false);
	}

	connect_signal_mouse_left_click(
		*appoint_
		, boost::bind(
			&tinterior::appoint
			, this
			, boost::ref(window)));
}

void tinterior::post_show(twindow& window)
{
}

void tinterior::catalog_page(twindow& window, int catalog, bool swap)
{
	if (catalog < MIN_PAGE || catalog > MAX_PAGE) {
		return;
	}
	int index = catalog - MIN_PAGE;
	
	if (window.alternate_index() == index) {
		// desired page is the displaying page, do nothing.
		return;
	}

	std::vector<int> features;
	std::stringstream str;

	int selected_row = 0;
	if (swap) {
		selected_row = hero_table_->get_selected_row();
	}

	window.alternate_uh(hero_table_, index);

	int hero_index = 0;
	for (std::vector<hero*>::iterator itor = fresh_heros_.begin(); itor != fresh_heros_.end(); ++ itor, hero_index ++) {
		/*** Add list item ***/
		string_map table_item;
		std::map<std::string, string_map> table_item_item;

		hero* h = *itor;

		if (catalog == ABILITY_PAGE) {
			features.clear();
			for (int tmp = 0; tmp < HEROS_MAX_FEATURE; tmp ++) {
				if (hero_feature_val2(*h, tmp)) {
					features.push_back(tmp);
				}
			}

			str.str("");
			table_item["use_markup"] = "true";
			if (h->official_ == hero_official_commercial) {
				str << "<0,0,255>";
			} else if (h->official_ != HEROS_NO_OFFICIAL) {
				str << "<255,0,0>";
			}
			str << h->name();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("name", table_item));
			
			table_item["label"] = lexical_cast<std::string>(h->loyalty(*teams_[h->side_].leader()));;
			table_item_item.insert(std::make_pair("loyalty", table_item));

			str.str("");
			str << fxptoi9(h->politics_);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("politics", table_item));

			str.str("");
			str << hero::adaptability_str2(h->skill_[hero_skill_commercial]);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("commercial", table_item));

			if (!features.empty()) {
				table_item["label"] = h->feature_str(features.front());
			} else {
				table_item["label"] = "";
			}
			table_item_item.insert(std::make_pair("feature", table_item));

			str.str("");
			str << fxptoi9(h->leadership_);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("leadership", table_item));

			str.str("");
			str << fxptoi9(h->force_);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("force", table_item));

			str.str("");
			str << fxptoi9(h->intellect_);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("intellect", table_item));

			str.str("");
			str << fxptoi9(h->charm_);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("charm", table_item));

		} else if (catalog == ADAPTABILITY_PAGE) {
			str.str("");
			table_item["use_markup"] = "true";
			if (h->official_ == hero_official_commercial) {
				str << "<0,0,255>";
			} else if (h->official_ != HEROS_NO_OFFICIAL) {
				str << "<255,0,0>";
			}
			str << h->name();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("name", table_item));

			table_item["label"] = hero::adaptability_str2(h->arms_[0]);
			table_item_item.insert(std::make_pair("arm0", table_item));

			table_item["label"] = hero::adaptability_str2(h->arms_[1]);
			table_item_item.insert(std::make_pair("arm1", table_item));

			table_item["label"] = hero::adaptability_str2(h->arms_[2]);
			table_item_item.insert(std::make_pair("arm2", table_item));

			table_item["label"] = hero::adaptability_str2(h->arms_[3]);
			table_item_item.insert(std::make_pair("arm3", table_item));

			table_item["label"] = hero::adaptability_str2(h->arms_[4]);
			table_item_item.insert(std::make_pair("arm4", table_item));

		} else if (catalog == RELATION_PAGE) {
			str.str("");
			table_item["use_markup"] = "true";
			if (h->official_ == hero_official_commercial) {
				str << "<0,0,255>";
			} else if (h->official_ != HEROS_NO_OFFICIAL) {
				str << "<255,0,0>";
			}
			str << h->name();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("name", table_item));

			if (h->parent_[0].hero_ != HEROS_INVALID_NUMBER) {
				table_item["label"] = heros_[h->parent_[0].hero_].name();
			} else {
				table_item["label"] = "";
			}
			table_item_item.insert(std::make_pair("father", table_item));

			if (h->parent_[1].hero_ != HEROS_INVALID_NUMBER) {
				table_item["label"] = heros_[h->parent_[1].hero_].name();
			} else {
				table_item["label"] = "";
			}
			table_item_item.insert(std::make_pair("mother", table_item));

			str.str("");
			for (uint32_t i = 0; i < HEROS_MAX_OATH; i ++) {
				if (h->oath_[i].hero_ != HEROS_INVALID_NUMBER) {
					if (i == 0) {
						str << heros_[h->oath_[i].hero_].name();
					} else {
						str << " " << heros_[h->oath_[i].hero_].name();
					}
				}
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("oath", table_item));

			str.str("");
			for (uint32_t i = 0; i < HEROS_MAX_CONSORT; i ++) {
				if (h->consort_[i].hero_ != HEROS_INVALID_NUMBER) {
					if (i == 0) {
						str << heros_[h->consort_[i].hero_].name();
					} else {
						str << " " << heros_[h->consort_[i].hero_].name();
					}
				}
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("consort", table_item));

			str.str("");
			for (uint32_t i = 0; i < HEROS_MAX_INTIMATE; i ++) {
				if (h->intimate_[i].hero_ != HEROS_INVALID_NUMBER) {
					if (i == 0) {
						str << heros_[h->intimate_[i].hero_].name();
					} else {
						str << " " << heros_[h->intimate_[i].hero_].name();
					}
				}
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("intimate", table_item));

			str.str("");
			for (uint32_t i = 0; i < HEROS_MAX_HATE; i ++) {
				if (h->hate_[i].hero_ != HEROS_INVALID_NUMBER) {
					if (i == 0) {
						str << heros_[h->hate_[i].hero_].name();
					} else {
						str << " " << heros_[h->hate_[i].hero_].name();
					}
				}
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("hate", table_item));

		}
		hero_table_->add_row(table_item_item);

		tgrid* grid_ptr = hero_table_->get_row_grid(hero_index);
		ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(grid_ptr->find("prefix", true));
		toggle->set_callback_state_change(boost::bind(&tinterior::hero_toggled, this, _1));
		toggle->set_data(hero_index);
		if (checked_heros_.find(hero_index) != checked_heros_.end()) {
			toggle->set_value(true);
		}
		if (h->official_ != HEROS_NO_OFFICIAL && h->official_ != hero_official_commercial) {
			toggle->set_active(false);
		} else if (h->loyalty(*current_team_.leader()) < game_config::wander_loyalty_threshold) {
			toggle->set_active(false);
		}
	}
	if (swap) {
		window.alternate_bh(hero_table_, index);
		hero_table_->select_row(selected_row);
	}
}

} // namespace gui2
