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

#include "gui/dialogs/recruit.hpp"

#include "foreach.hpp"
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
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/widgets/settings.hpp"
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/stacked_widget.hpp"
#include "gui/widgets/window.hpp"
#include "resources.hpp"
#include "play_controller.hpp"

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

REGISTER_DIALOG(recruit)

trecruit::trecruit(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, artifical& city, int cost_exponent, bool rpg_mode)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, current_team_(teams_[city.side() - 1])
	, city_(city)
	, cost_exponent_(cost_exponent)
	, fresh_heros_()
	, unit_types_()
	, type_index_(0)
	, checked_heros_()
	, hero_table_(NULL)
	, rpg_mode_(rpg_mode)
{
}

void trecruit::type_selected2(twindow& window)
{
	twindow::tinvalidate_layout_blocker invalidate_layout_blocker(window);
	type_selected(window);
}

void trecruit::type_selected(twindow& window)
{
	int gold = current_team_.gold();

	tlistbox& list = find_widget<tlistbox>(&window, "type_list", false);

	type_index_ = list.get_selected_row();
	
	tbutton* ok = find_widget<tbutton>(&window, "ok", false, true);
	const unit_type* t = unit_types_[type_index_];
	if (!checked_heros_.empty() && gold >= t->cost() * cost_exponent_ / 100) {
		if (!t->leader() || master()->official_ == hero_official_leader) {
			ok->set_active(true);
		} else {
			ok->set_active(false);
		}
	} else {
		ok->set_active(false);
	}

	refresh_tooltip(window);
}

void trecruit::hero_toggled(twidget* widget)
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
	tbutton* ok = find_widget<tbutton>(window, "ok", false, true);
	const unit_type* t = unit_types_[type_index_];
	if (!checked_heros_.empty() && current_team_.gold() >= t->cost() * cost_exponent_ / 100) {
		if (!t->leader() || master()->official_ == hero_official_leader) {
			ok->set_active(true);
		} else {
			ok->set_active(false);
		}
	} else {
		ok->set_active(false);
	}

	twindow::tinvalidate_layout_blocker invalidate_layout_blocker(*window);
	refresh_tooltip(*window);
}

void trecruit::refresh_tooltip(twindow& window)
{
	tstacked_widget* stacked = find_widget<tstacked_widget>(&window, "middle_top_part", false, true);
	stacked->set_dirty(true);

	tscroll_label* tip = find_widget<tscroll_label>(&window, "tip", false, true);
	if (checked_heros_.empty()) {
		// It is necessary set all relative tips to empty.
		tcontrol* control = find_widget<tcontrol>(&window, "master_png", false, true);
		control->set_label("");
		tlabel* label = find_widget<tlabel>(&window, "master_name", false, true);
		label->set_label("");

		// leadership
		label = find_widget<tlabel>(&window, "tip_leadership", false, true);
		label->set_label("");

		// force
		label = find_widget<tlabel>(&window, "tip_force", false, true);
		label->set_label("");

		// intellect
		label = find_widget<tlabel>(&window, "tip_intellect", false, true);
		label->set_label("");

		// politics
		label = find_widget<tlabel>(&window, "tip_politics", false, true);
		label->set_label("");

		// charm
		label = find_widget<tlabel>(&window, "tip_charm", false, true);
		label->set_label("");

		tip->set_label("");
		return;
	}

	const unit_type* t = unit_types_[type_index_];
	std::vector<const hero*> v;
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
	type_heros_pair pair(t, v);
	unit temp(units_, heros_, teams_, pair, city_.cityno(), false);

	// refresh to gui
	tcontrol* control = find_widget<tcontrol>(&window, "master_png", false, true);
	control->set_label(temp.master().image());
	tlabel* label = find_widget<tlabel>(&window, "master_name", false, true);
	label->set_label(temp.master().name());

	control = find_widget<tcontrol>(&window, "second_png", false, true);
	label = find_widget<tlabel>(&window, "second_name", false, true);
	if (temp.second().valid()) {
		control->set_label(temp.second().image());
		label->set_label(temp.second().name());
	} else {
		control->set_label("");
		label->set_label("");
	}

	control = find_widget<tcontrol>(&window, "third_png", false, true);
	label = find_widget<tlabel>(&window, "third_name", false, true);
	if (temp.third().valid()) {
		control->set_label(temp.third().image());
		label->set_label(temp.third().name());
	} else {
		control->set_label("");
		label->set_label("");
	}

	// leadership
	label = find_widget<tlabel>(&window, "tip_leadership", false, true);
	label->set_label(lexical_cast<std::string>(temp.leadership_));

	// force
	label = find_widget<tlabel>(&window, "tip_force", false, true);
	label->set_label(lexical_cast<std::string>(temp.force_));

	// intellect
	label = find_widget<tlabel>(&window, "tip_intellect", false, true);
	label->set_label(lexical_cast<std::string>(temp.intellect_));

	// politics
	label = find_widget<tlabel>(&window, "tip_politics", false, true);
	label->set_label(lexical_cast<std::string>(temp.politics_));

	// charm
	label = find_widget<tlabel>(&window, "tip_charm", false, true);
	label->set_label(lexical_cast<std::string>(temp.charm_));

	tip->set_label(temp.form_recruit_tip());
}

void trecruit::pre_show(CVideo& /*video*/, twindow& window)
{
	// int side_num = city_.side();
	std::stringstream str;

	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);

	int gold = current_team_.gold();

	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	str << _("Recruit") << "(" << gold << sngettext("unit^Gold", "Gold", gold) << ")";
	label->set_label(str.str());

	switch_type_internal(window);

	list->set_callback_value_change(dialog_callback<trecruit, &trecruit::type_selected2>);

	hero_table_ = find_widget<tlistbox>(&window, "hero_table", false, true);
	fresh_heros_ = city_.fresh_heros();
	std::sort(fresh_heros_.begin(), fresh_heros_.end(), compare_recruit);

	// fill data to hero_table
	catalog_page(window, ABILITY_PAGE, false);

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "ability", false)
		, boost::bind(
			&trecruit::catalog_page
			, this
			, boost::ref(window)
			, (int)ABILITY_PAGE
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "adaptability", false)
		, boost::bind(
			&trecruit::catalog_page
			, this
			, boost::ref(window)
			, (int)ADAPTABILITY_PAGE
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "personal", false)
		, boost::bind(
			&trecruit::catalog_page
			, this
			, boost::ref(window)
			, (int)PERSONAL_PAGE
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "relation", false)
		, boost::bind(
			&trecruit::catalog_page
			, this
			, boost::ref(window)
			, (int)RELATION_PAGE
			, true));

	// prev/next
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "prev", false)
		, boost::bind(
			&trecruit::switch_type
			, this
			, boost::ref(window)
			, false));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "next", false)
		, boost::bind(
			&trecruit::switch_type
			, this
			, boost::ref(window)
			, true));

	tbutton* ok = find_widget<tbutton>(&window, "ok", false, true);
	const unit_type* t = unit_types_[type_index_];
	if (!checked_heros_.empty() && gold >= t->cost() * cost_exponent_ / 100) {
		if (!t->leader() || master()->official_ == hero_official_leader) {
			ok->set_active(true);
		} else {
			ok->set_active(false);
		}
	} else {
		ok->set_active(false);
	}
	tbutton* cancel = find_widget<tbutton>(&window, "cancel", false, true);
	cancel->set_visible(rpg_mode_? twidget::INVISIBLE: twidget::VISIBLE);

	if (rpg_mode_) {
		refresh_tooltip(window);
	}
}

void trecruit::post_show(twindow& window)
{
}

void trecruit::catalog_page(twindow& window, int catalog, bool swap)
{
	if (catalog < MIN_PAGE || catalog > MAX_PAGE) {
		return;
	}
	int index = catalog - MIN_PAGE;
	
	if (window.alternate_index() == index) {
		// desired page is the displaying page, do nothing.
		return;
	}


	// avoid to effect unit_type listbox.
	twindow::tinvalidate_layout_blocker invalidate_layout_blocker(window);

	std::vector<int> features;
	std::stringstream str;

	int selected_row = 0;
	if (swap) {
		selected_row = hero_table_->get_selected_row();
	}

	window.alternate_uh(hero_table_, index);

	const hero* rpg_hero = rpg::h;
	int hero_index = 0;
	int activity_ajdusted;

	for (std::vector<hero*>::iterator itor = fresh_heros_.begin(); itor != fresh_heros_.end(); ++ itor, hero_index ++) {
		/*** Add list item ***/
		string_map table_item;
		std::map<std::string, string_map> table_item_item;

		hero* h = *itor;

		if (catalog == ABILITY_PAGE) {
			str.str("");
			str << "<format>";
			if (h->official_ == hero_official_commercial) {
				str << "color=blue ";
			} else if (h->activity_ < HEROS_FULL_ACTIVITY) {
				str << "color=red ";
			}
			str << "text='" << h->name() << "'</format>";
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("name", table_item));
			
			table_item["label"] = lexical_cast<std::string>(h->loyalty(*teams_[h->side_].leader()));;
			table_item_item.insert(std::make_pair("loyalty", table_item));

			table_item["label"] = hero::feature_str(h->feature_);
			table_item_item.insert(std::make_pair("feature", table_item));

			if (h->tactic_ != HEROS_NO_TACTIC) {
				table_item["label"] = unit_types.tactic(h->tactic_).name();
			} else {
				table_item["label"] = "";
			}
			table_item_item.insert(std::make_pair("tactic", table_item));

			str.str("");
			if (h->activity_ < HEROS_FULL_ACTIVITY) {
				activity_ajdusted = 1 * h->leadership_ * h->activity_ / HEROS_FULL_ACTIVITY;
				str << fxptoi9(activity_ajdusted) << "/";
			}
			str << fxptoi9(h->leadership_);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("leadership", table_item));

			str.str("");
			if (h->activity_ < HEROS_FULL_ACTIVITY) {
				activity_ajdusted = 1 * h->force_ * h->activity_ / HEROS_FULL_ACTIVITY;
				str << fxptoi9(activity_ajdusted) << "/";
			}
			str << fxptoi9(h->force_);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("force", table_item));

			str.str("");
			if (h->activity_ < HEROS_FULL_ACTIVITY) {
				activity_ajdusted = 1 * h->intellect_ * h->activity_ / HEROS_FULL_ACTIVITY;
				str << fxptoi9(activity_ajdusted) << "/";
			}
			str << fxptoi9(h->intellect_);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("intellect", table_item));

			str.str("");
			if (h->activity_ < HEROS_FULL_ACTIVITY) {
				activity_ajdusted = 1 * h->politics_ * h->activity_ / HEROS_FULL_ACTIVITY;
				str << fxptoi9(activity_ajdusted) << "/";
			}
			str << fxptoi9(h->politics_);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("politics", table_item));

			str.str("");
			if (h->activity_ < HEROS_FULL_ACTIVITY) {
				activity_ajdusted = 1 * h->charm_ * h->activity_ / HEROS_FULL_ACTIVITY;
				str << fxptoi9(activity_ajdusted) << "/";
			}
			str << fxptoi9(h->charm_);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("charm", table_item));

			str.str("");
			str << 100 * h->activity_ / HEROS_FULL_ACTIVITY << "%";
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("activity", table_item));

		} else if (catalog == ADAPTABILITY_PAGE) {
			table_item["use_markup"] = "true";
			str.str("");
			str << "<format>";
			if (h->official_ == hero_official_commercial) {
				str << "color=blue ";
			} else if (h->activity_ < HEROS_FULL_ACTIVITY) {
				str << "color=red ";
			}
			str << "text='" << h->name() << "'</format>";
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

		} else if (catalog == PERSONAL_PAGE) {
			table_item["use_markup"] = "true";
			str.str("");
			str << "<format>";
			if (h->official_ == hero_official_commercial) {
				str << "color=blue ";
			} else if (h->activity_ < HEROS_FULL_ACTIVITY) {
				str << "color=red ";
			}
			str << "text='" << h->name() << "'</format>";
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("name", table_item));

			table_item["label"] = hero::gender_str(h->gender_);
			table_item_item.insert(std::make_pair("gender", table_item));

			str.str("");
			if (h->treasure_ != HEROS_NO_TREASURE) {
				const ttreasure& t = unit_types.treasure(h->treasure_);
				str << t.name();
				str << "(";
				str << hero::feature_str(t.feature());
				str << ")";
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("treasure", table_item));

		} else if (catalog == RELATION_PAGE) {
			table_item["use_markup"] = "true";
			str.str("");
			str << "<format>";
			if (h->official_ == hero_official_commercial) {
				str << "color=blue ";
			} else if (h->activity_ < HEROS_FULL_ACTIVITY) {
				str << "color=red ";
			}
			str << "text='" << h->name() << "'</format>";
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
		toggle->set_callback_state_change(boost::bind(&trecruit::hero_toggled, this, _1));
		toggle->set_data(hero_index);
		if (rpg_mode_ && h == rpg_hero) {
			toggle->set_active(false);
			toggle->set_value(true);
			if (!checked_heros_.count(hero_index)) {
				checked_heros_.insert(hero_index);
			}
		} else {
			if (checked_heros_.find(hero_index) != checked_heros_.end()) {
				toggle->set_value(true);
			}
			if (h->activity_ < game_config::minimal_activity) {
				toggle->set_active(false);
			}
		}
	}
	if (swap) {
		window.alternate_bh(hero_table_, index);
		hero_table_->select_row(selected_row);
	}
}

void trecruit::switch_type(twindow& window, bool next)
{
	if (next) {
		if (game_config::current_level == game_config::max_level) {
			return;
		}
		game_config::current_level ++;
	} else {
		if (game_config::current_level == game_config::min_level) {
			return;
		}
		game_config::current_level --;
	}
	twindow::tinvalidate_layout_blocker invalidate_layout_blocker(window);
	switch_type_internal(window);
}

void trecruit::switch_type_internal(twindow& window)
{
	int side_num = city_.side();
	std::stringstream str;
	int gold = current_team_.gold();

	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);
	list->clear();
	unit_types_.clear();

	const std::vector<const unit_type*>& recruits = city_.recruits(game_config::current_level);
	str.str("");
	str << "trecruit::switch_type_internal, there is no type in current_level(" << game_config::current_level << ")!";
	VALIDATE(!recruits.empty(), str.str());
	for (std::vector<const unit_type*>::const_iterator it = recruits.begin(); it != recruits.end(); ++it) {
		/*** Add list item ***/
		const unit_type* type = *it;
		VALIDATE(type, std::string("could not find unit type ") + type->id());
		
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		if (gold >= type->cost() * cost_exponent_ / 100) {
			list_item["label"] = type->image() + "~RC(" + type->flag_rgb() + ">" + team::get_side_color_index(side_num) + ")";
		} else {
			list_item["label"] = type->image() + "~GS()";
		}

		list_item_item.insert(std::make_pair("icon", list_item));

		list_item["label"] = type->type_name();
		list_item_item.insert(std::make_pair("name", list_item));

		str.str("");
		str << type->cost() * cost_exponent_ / 100 << " " << sngettext("unit^Gold", "Gold", type->cost() * cost_exponent_ / 100);
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("cost", list_item));

		list->add_row(list_item_item);

		tgrid* grid_ptr = list->get_row_grid(unit_types_.size());
		tcontrol* widget = dynamic_cast<tcontrol*>(grid_ptr->find("utype_icon", false));
		if (type->leader()) {
			widget->set_label("utype/commander.png");
		} else if (type->especial() != NO_ESPECIAL) {
			widget->set_label(unit_types.especial(type->especial()).image_);
		}

		unit_types_.push_back(type);
	}
	window.invalidate_layout();
	if (type_index_ >= (int)list->get_item_count()) {
		type_index_ = list->get_item_count() - 1;
	}
	list->select_row(type_index_);

	tbutton* prev = find_widget<tbutton>(&window, "prev", false, true);
	prev->set_active(game_config::current_level != game_config::min_level);
	tbutton* next = find_widget<tbutton>(&window, "next", false, true);
	next->set_active(game_config::current_level != game_config::max_level);

	str.str("");
	str << "misc/digit-big.png~CROP(" << 30 * game_config::current_level << ", 0, 30, 45)";
	find_widget<tcontrol>(&window, "level", false, false)->set_label(str.str());

	type_selected(window);
}

hero* trecruit::master() const
{
	if (!checked_heros_.empty()) {
		return fresh_heros_[*checked_heros_.begin()];
	} else {
		return &hero_invalid;
	}
}

hero* trecruit::second() const
{
	if (checked_heros_.size() >= 2) {
		std::set<int>::const_iterator itor = checked_heros_.begin();
		++ itor;
		return fresh_heros_[*itor];
	} else {
		return &hero_invalid;
	}
}

hero* trecruit::third() const
{
	if (checked_heros_.size() >= 3) {
		std::set<int>::const_iterator itor = checked_heros_.begin();
		++ itor;
		++ itor;
		return fresh_heros_[*itor];
	} else {
		return &hero_invalid;
	}
}

const unit_type* trecruit::unit_type_ptr() const
{
	return unit_types_[type_index_];	
}

} // namespace gui2
