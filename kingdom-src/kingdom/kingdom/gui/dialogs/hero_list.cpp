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

#include "gui/dialogs/hero_list.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "team.hpp"
#include "artifical.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/stacked_widget.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/text_box.hpp"
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

REGISTER_DIALOG(hero_list)

int full_number_min;

thero_list::thero_list(std::vector<team>* teams, unit_map* units, hero_map& heros, std::vector<hero*>& partial_heros, int side)
	: teams_(teams)
	, units_(units)
	, heros_(heros)
	, partial_heros_(partial_heros)
	, side_(side)
	, row_2_hero_()
	, sorting_widgets_()
	, sorting_widget_(NULL)
	, layer_panel_(NULL)
	, window_(NULL)
	, bar_(NULL)
	, current_page_(twidget::npos)
{
}

void thero_list::pre_show(CVideo& /*video*/, twindow& window)
{
	window_ = &window;

	layer_panel_ = find_widget<tstacked_widget>(&window, "panel", false, true);

	layers_.push_back(find_widget<tlistbox>(&window, "ownership_list", false, true));
	layers_.push_back(find_widget<tlistbox>(&window, "ability_list", false, true));
	layers_.push_back(find_widget<tlistbox>(&window, "feature_list", false, true));
	layers_.push_back(find_widget<tlistbox>(&window, "adaptability_list", false, true));
	layers_.push_back(find_widget<tlistbox>(&window, "skill_list", false, true));
	layers_.push_back(find_widget<tlistbox>(&window, "tactic_list", false, true));
	layers_.push_back(find_widget<tlistbox>(&window, "personal_list", false, true));
	layers_.push_back(find_widget<tlistbox>(&window, "relation_list", false, true));

	for (std::vector<tlistbox*>::const_iterator it = layers_.begin(); it != layers_.end(); ++ it) {
		tlistbox& list = **it;
		list.set_callback_value_change(dialog_callback3<thero_list, tlistbox, &thero_list::hero_changed>);
	}

	bar_ = find_widget<treport>(&window, "bar", false, true);
	bar_->tabbar_init(true, "dusk_tab");
	bar_->set_boddy(find_widget<twidget>(&window, "bar_panel", false, true));
	std::vector<std::string> labels;
	labels.push_back(_("Ownership"));
	labels.push_back(_("Ability"));
	labels.push_back(dsgettext("wesnoth-hero", "feature"));
	labels.push_back(_("Adaptability"));
	labels.push_back(_("Skill"));
	labels.push_back(dsgettext("wesnoth-hero", "tactic"));
	labels.push_back(_("Personal"));
	labels.push_back(_("Relation"));
	int index = 0;
	for (std::vector<std::string>::const_iterator it = labels.begin(); it != labels.end(); ++ it) {
		tcontrol* widget = bar_->create_child(null_str, null_str, reinterpret_cast<void*>(index ++));
		widget->set_label(*it);
		bar_->insert_child(*widget);
	}
	bar_->select(ABILITY_PAGE);
	bar_->replacement_children();

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "find", false)
		, boost::bind(
			&thero_list::find
			, this
			, boost::ref(window)));

	catalog_page(window, ABILITY_PAGE, false);
}

void thero_list::post_show(twindow& window)
{
}

void thero_list::toggle_report(twidget* widget)
{
	tdialog::toggle_report(widget);

	twindow* window = widget->get_window();
	int cursel = (int)reinterpret_cast<long>(widget->cookie());
	catalog_page(*window, cursel, true);
}

void thero_list::hero_changed(twindow& window, tlistbox& list, const int type)
{
	twidget* grid_ptr = list.get_row_panel(list.get_selected_row());
	int selected_row = (int)reinterpret_cast<long>(grid_ptr->cookie());

	twindow::tinvalidate_layout_blocker invalidate_layout_blocker(window);

	tcontrol* portrait = find_widget<tcontrol>(&window, "portrait", false, true);
	tscroll_label* biography = find_widget<tscroll_label>(&window, "biography", false, true);

	portrait->set_label(heros_[row_2_hero_.find(selected_row)->second].image(true));
	biography->set_label(heros_[row_2_hero_.find(selected_row)->second].biography2(heros_));
}

void thero_list::find(twindow& window)
{
	ttext_box& text_box = find_widget<ttext_box>(&window, "name_input", false);
	std::string key = text_box.get_value();
	if (key.empty()) {
		return;
	}
	size_t key_size = strlen(key.c_str());

	tlistbox& list = *layers_[current_page_];
	int found, idx, hero_size = list.get_item_count();
	int first_find_line = list.get_selected_row() + 1;
	
	for (found = idx = first_find_line; idx < hero_size; found ++, idx ++) {
		twidget* row_panel = list.get_row_panel(idx);
		int sorted_number = (int)(reinterpret_cast<long>(row_panel->cookie()));
		hero& h = heros_[row_2_hero_.find(sorted_number)->second];
		if (!strncasecmp(key.c_str(), h.name().c_str(), key_size)) {
			break;
		}
	}
	if (found == hero_size) {
		for (found = 0, idx = 0; idx < first_find_line; found ++, idx ++) {
			twidget* row_panel = list.get_row_panel(idx);
			int sorted_number = (int)(reinterpret_cast<long>(row_panel->cookie()));
			hero& h = heros_[row_2_hero_.find(sorted_number)->second];
			if (!strncasecmp(key.c_str(), h.name().c_str(), key_size)) {
				break;
			}
		}
		if (found == first_find_line) {
			found = hero_size;
		}
	}

	if (found != hero_size) {
		list.select_row(found);
		list.scroll_to_row(found);
		hero_changed(window, list);
	}
}

void thero_list::fill_table(tlistbox& list, int catalog)
{
	row_2_hero_.clear();
	if (partial_heros_.empty()) {
		for (hero_map::iterator h = heros_.begin(); h != heros_.end(); ++ h) {
			if (side_ != -1 && h->side_ != side_ - 1) {
				continue;
			}
			if (h->number_ < hero::number_normal_min) {
				// continue;
			}
			if (h->status_ == hero_status_unstage && !preferences::developer()) {
				continue;
			}
			fill_table_row(list, *h, catalog);
		}
	} else {
		for (std::vector<hero*>::iterator h = partial_heros_.begin(); h != partial_heros_.end(); ++ h) {
			fill_table_row(list, **h, catalog);
		}
	}
}

void thero_list::fill_table_row(tlistbox& list, hero& h, int catalog)
{
	std::vector<int> features;
	std::stringstream str;
	int val;

	/*** Add list item ***/
	string_map table_item;
	std::map<std::string, string_map> table_item_item;

	if (catalog == OWNERSHIP_PAGE) {
		table_item["label"] = h.name();
		table_item_item.insert(std::make_pair("name", table_item));

		if (h.side_ != HEROS_INVALID_SIDE) {
			table_item["label"] =  (*teams_)[h.side_].name();
		} else {
			table_item["label"] = "---";
		}
		table_item_item.insert(std::make_pair("side", table_item));

		artifical* city = units_? units_->city_from_cityno(h.city_): NULL;
		if (city) {
			table_item["label"] = city->name();
		} else {
			table_item["label"] = "---";
		}
		table_item_item.insert(std::make_pair("city", table_item));

		str.str("");
		int catalog_diff = -1;
		if (h.side_ != HEROS_INVALID_SIDE) {
			str << h.loyalty(*(*teams_)[h.side_].leader()) << "/";

			catalog_diff = posix_abs((int)(*teams_)[h.side_].leader()->base_catalog_ - h.base_catalog_);
			if (catalog_diff > HERO_MAX_LOYALTY / 2) {
				catalog_diff = HERO_MAX_LOYALTY - catalog_diff;
			}
		} else {
			str << "--/";
		}
		str << fxptoi8(h.float_catalog_) << "." << fxpmod8(h.float_catalog_);

		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("loyalty", table_item));

		str.str("");
		str << (int)h.base_catalog_;
		if (catalog_diff >= 0) {
			str << "(" << catalog_diff << ")";
		} else {
			str << "(--)";
		}
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("hero_catalog", table_item));

		table_item["label"] = lexical_cast<std::string>(h.meritorious_);
		table_item_item.insert(std::make_pair("meritorious", table_item));

	} else if (catalog == ABILITY_PAGE) {
		table_item["label"] = h.name();
		table_item_item.insert(std::make_pair("name", table_item));

		str.str("");
		str << fxptoi9(h.leadership_);
		val = fxpmod9(h.leadership_);
		str << "." << val;
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("leadership", table_item));

		str.str("");
		str << fxptoi9(h.force_);
		val = fxpmod9(h.force_);
		str << "." << val;
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("force", table_item));

		str.str("");
		str << fxptoi9(h.intellect_);
		val = fxpmod9(h.intellect_);
		str << "." << val;
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("intellect", table_item));

		str.str("");
		str << fxptoi9(h.spirit_);
		val = fxpmod9(h.spirit_);
		str << "." << val;
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("spirit", table_item));

		str.str("");
		str << fxptoi9(h.charm_);
		val = fxpmod9(h.charm_);
		str << "." << val;
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("charm", table_item));

		table_item["label"] = lexical_cast<std::string>((int)h.activity_);
		table_item_item.insert(std::make_pair("activity", table_item));

	} else if (catalog == FEATURE_PAGE) {
		table_item["label"] = h.name();
		table_item_item.insert(std::make_pair("name", table_item));

		table_item["label"] = hero::feature_str(h.feature_);
		table_item_item.insert(std::make_pair("feature", table_item));

		table_item["label"] = hero::feature_desc_str(h.feature_);
		table_item_item.insert(std::make_pair("explain", table_item));

	} else if (catalog == ADAPTABILITY_PAGE) {
		table_item["label"] = h.name();
		table_item_item.insert(std::make_pair("name", table_item));

		str.str("");
		str << hero::adaptability_str2(h.arms_[0]);
		val = fxpmod12(h.arms_[0]);
		str << "." << val;
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("arm0", table_item));

		str.str("");
		str << hero::adaptability_str2(h.arms_[1]);
		val = fxpmod12(h.arms_[1]);
		str << "." << val;
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("arm1", table_item));

		str.str("");
		str << hero::adaptability_str2(h.arms_[2]);
		val = fxpmod12(h.arms_[2]);
		str << "." << val;
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("arm2", table_item));

		str.str("");
		str << hero::adaptability_str2(h.arms_[3]);
		val = fxpmod12(h.arms_[3]);
		str << "." << val;
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("arm3", table_item));

		str.str("");
		str << hero::adaptability_str2(h.arms_[4]);
		val = fxpmod12(h.arms_[4]);
		str << "." << val;
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("arm4", table_item));

	} else if (catalog == SKILL_PAGE) {
		table_item["label"] = h.name();
		table_item_item.insert(std::make_pair("name", table_item));

		str.str("");
		str << hero::adaptability_str2(h.skill_[hero_skill_commercial]);
		val = fxpmod12(h.skill_[hero_skill_commercial]);
		str << "." << val;
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("skill0", table_item));

		str.str("");
		str << hero::adaptability_str2(h.skill_[hero_skill_invent]);
		val = fxpmod12(h.skill_[hero_skill_invent]);
		str << "." << val;
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("skill1", table_item));

		str.str("");
		str << hero::adaptability_str2(h.skill_[hero_skill_encourage]);
		val = fxpmod12(h.skill_[hero_skill_encourage]);
		str << "." << val;
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("skill3", table_item));

		str.str("");
		str << hero::adaptability_str2(h.skill_[hero_skill_hero]);
		val = fxpmod12(h.skill_[hero_skill_hero]);
		str << "." << val;
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("skill4", table_item));

		str.str("");
		str << hero::adaptability_str2(h.skill_[hero_skill_demolish]);
		val = fxpmod12(h.skill_[hero_skill_demolish]);
		str << "." << val;
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("skill5", table_item));

		str.str("");
		str << hero::adaptability_str2(h.skill_[hero_skill_formation]);
		val = fxpmod12(h.skill_[hero_skill_formation]);
		str << "." << val;
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("skill6", table_item));

	} else if (catalog == COMMAND_PAGE) {
		table_item["label"] = h.name();
		table_item_item.insert(std::make_pair("name", table_item));

		const ttactic* t = NULL;
		if (h.tactic_ != HEROS_NO_TACTIC) {
			t = &unit_types.tactic(h.tactic_);
		}
		if (t) {
			table_item["label"] = t->name();
		} else {
			table_item["label"] = "";
		}
		table_item_item.insert(std::make_pair("tactic", table_item));
		if (t) {
			table_item["label"] = t->description();
		} else {
			table_item["label"] = "";
		}
		table_item_item.insert(std::make_pair("explain", table_item));

	} else if (catalog == PERSONAL_PAGE) {
		table_item["label"] = h.name();
		table_item_item.insert(std::make_pair("name", table_item));

		table_item["label"] = hero::gender_str(h.gender_);
		table_item_item.insert(std::make_pair("gender", table_item));

		str.str("");
		if (h.treasure_ != HEROS_NO_TREASURE) {
			const ttreasure& t = unit_types.treasure(h.treasure_);
			str << t.name();
			str << "(";
			str << hero::feature_str(t.feature());
			str << ")";
		}
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("treasure", table_item));

		table_item["label"] = hero::status_str(h.status_);
		table_item_item.insert(std::make_pair("action", table_item));

		table_item["label"] = hero::official_str(h.official_);
		table_item_item.insert(std::make_pair("official", table_item));

		str.str("");
		if (h.noble_ != HEROS_NO_NOBLE) {
			const tnoble& t = unit_types.noble(h.noble_);
			str << t.name();
		}
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("noble", table_item));

	} else if (catalog == RELATION_PAGE) {
		table_item["label"] = h.name();
		table_item_item.insert(std::make_pair("name", table_item));

		str.str("");
		if (h.parent_[0].hero_ != HEROS_INVALID_NUMBER) {
			str << heros_[h.parent_[0].hero_].name();
		}
		val = h.parent_[0].feeling_;
		// str << "(" << val << ")";
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("father", table_item));

		str.str("");
		if (h.parent_[1].hero_ != HEROS_INVALID_NUMBER) {
			str << heros_[h.parent_[1].hero_].name();
		}
		val = h.parent_[1].feeling_;
		// str << "(" << val << ")";
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("mother", table_item));

		str.str("");
		for (uint32_t i = 0; i < HEROS_MAX_OATH; i ++) {
			if (h.oath_[i].hero_ != HEROS_INVALID_NUMBER) {
				if (i == 0) {
					str << heros_[h.oath_[i].hero_].name();
				} else {
					str << " " << heros_[h.oath_[i].hero_].name();
				}
			}
			val = h.oath_[i].feeling_;
			// str << "(" << val << ")";
		}
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("oath", table_item));

		str.str("");
		for (uint32_t i = 0; i < HEROS_MAX_CONSORT; i ++) {
			if (h.consort_[i].hero_ != HEROS_INVALID_NUMBER) {
				if (i == 0) {
					str << heros_[h.consort_[i].hero_].name();
				} else {
					str << " " << heros_[h.consort_[i].hero_].name();
				}
			}
			val = h.consort_[i].feeling_;
			// str << "(" << val << ")";
		}
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("consort", table_item));

		str.str("");
		for (uint32_t i = 0; i < HEROS_MAX_INTIMATE; i ++) {
			if (h.intimate_[i].hero_ != HEROS_INVALID_NUMBER) {
				if (i == 0) {
					str << heros_[h.intimate_[i].hero_].name();
				} else {
					str << " " << heros_[h.intimate_[i].hero_].name();
				}
			}
			val = h.intimate_[i].feeling_;
			// str << "(" << val << ")";
		}
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("intimate", table_item));

		str.str("");
		for (uint32_t i = 0; i < HEROS_MAX_HATE; i ++) {
			if (h.hate_[i].hero_ != HEROS_INVALID_NUMBER) {
				if (i == 0) {
					str << heros_[h.hate_[i].hero_].name();
				} else {
					str << " " << heros_[h.hate_[i].hero_].name();
				}
			}
			val = h.hate_[i].feeling_;
			// str << "(" << val << ")";
		}
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("hate", table_item));

	}
	list.add_row(table_item_item);

	unsigned row_index = list.get_item_count() - 1;
	twidget* row_panel = list.get_row_panel(row_index);
	row_panel->set_cookie(reinterpret_cast<void*>(row_index));
	row_2_hero_.insert(std::make_pair(row_index, h.number_));
}

void thero_list::catalog_page(twindow& window, int catalog, bool swap)
{
	if (!layer_panel_) {
		return;
	}
	if (current_page_ == catalog) {
		return;
	}

	unsigned int selected_row = 0;
	if (swap) {
		// because sort, order is changed.
		tlistbox& list = *layers_[current_page_];
		selected_row = list.get_selected_row();
		list.clear();
	}

	layer_panel_->set_radio_layer(catalog);

	tlistbox& list = *layers_[catalog];
	fill_table(list, catalog);

	if (swap) {
		list.select_row(selected_row);
		list.invalidate_layout(true);
		// swap to other page, there is no sorted column.
		if (sorting_widget_) {
			sorting_widget_->set_sort(tbutton::NONE);
			sorting_widget_ = NULL;
		}
	} else {
		hero_changed(window, list);
	}

	if (!sorting_widgets_.count(catalog)) {
		// in order to support sortable, form relative data.
		std::vector<tbutton*> widgets;
		if (catalog == OWNERSHIP_PAGE) {
			widgets.push_back(&find_widget<tbutton>(&window, "button_name", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_side", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_city", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_loyalty", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_catalog", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_meritorious", false));
		} else if (catalog == ABILITY_PAGE) {
			widgets.push_back(&find_widget<tbutton>(&window, "button_name", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_leadership", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_force", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_intellect", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_spirit", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_charm", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_activity", false));
		} else if (catalog == FEATURE_PAGE) {
			widgets.push_back(&find_widget<tbutton>(&window, "button_name", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_feature", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_explain", false));
			widgets.back()->set_active(false);
		} else if (catalog == ADAPTABILITY_PAGE) {
			widgets.push_back(&find_widget<tbutton>(&window, "button_name", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_arm0", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_arm1", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_arm2", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_arm3", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_arm4", false));
		} else if (catalog == SKILL_PAGE) {
			widgets.push_back(&find_widget<tbutton>(&window, "button_name", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_skill0", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_skill1", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_skill3", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_skill4", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_skill5", false));
		} else if (catalog == COMMAND_PAGE) {
			widgets.push_back(&find_widget<tbutton>(&window, "button_name", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_tactic", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_explain", false));
			widgets.back()->set_active(false);
		} else if (catalog == PERSONAL_PAGE) {
			widgets.push_back(&find_widget<tbutton>(&window, "button_name", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_gender", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_treasure", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_action", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_official", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_noble", false));
		} else if (catalog == RELATION_PAGE) {
			widgets.push_back(&find_widget<tbutton>(&window, "button_name", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_father", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_mother", false));
			widgets.push_back(&find_widget<tbutton>(&window, "button_oath", false));
			widgets.back()->set_active(false);
			widgets.push_back(&find_widget<tbutton>(&window, "button_consort", false));
			widgets.back()->set_active(false);
			widgets.push_back(&find_widget<tbutton>(&window, "button_intimate", false));
			widgets.back()->set_active(false);
			widgets.push_back(&find_widget<tbutton>(&window, "button_hate", false));
			widgets.back()->set_active(false);
		}
		for (std::vector<tbutton*>::iterator i = widgets.begin(); i != widgets.end(); ++ i) {
			tbutton& widget = **i;
			connect_signal_mouse_left_click(
				widget
				, boost::bind(
					&thero_list::sort_column
					, this
					, boost::ref(widget)
					, catalog));
		}
		sorting_widgets_[catalog] = widgets;
	}

	// speeden compare_row, remember this catalog.
	current_page_ = catalog;
}

bool thero_list::compare_row(twidget& row1, twidget& row2)
{
	int i1 = (int)reinterpret_cast<long>(row1.cookie());
	int i2 = (int)reinterpret_cast<long>(row2.cookie());

	hero* h1 = &heros_[row_2_hero_.find(i1)->second];
	hero* h2 = &heros_[row_2_hero_.find(i2)->second];


	bool result = true;
	std::vector<tbutton*>& widgets = sorting_widgets_[current_page_];

	if (current_page_ == OWNERSHIP_PAGE) {
		if (sorting_widget_ == widgets[0]) {
			// name
			result = utils::utf8str_compare(h1->name(), h2->name());
		} else if (sorting_widget_ == widgets[1]) {
			// side
			std::string str1, str2;
			if (h1->side_ != HEROS_INVALID_SIDE) {
				str1 =  (*teams_)[h1->side_].name();
			}
			if (h2->side_ != HEROS_INVALID_SIDE) {
				str2 =  (*teams_)[h2->side_].name();
			}
			result = utils::utf8str_compare(str1, str2);
		} else if (sorting_widget_ == widgets[2]) {
			// city
			std::string str1, str2;
			artifical* city = units_? units_->city_from_cityno(h1->city_): NULL;
			if (city) {
				str1 = city->name();
			}
			city = units_? units_->city_from_cityno(h2->city_): NULL;
			if (city) {
				str2 = city->name();
			}
			result = utils::utf8str_compare(str1, str2);
		} else if (sorting_widget_ == widgets[3]) {
			// loyalty
			int l1 = 0;
			int l2 = 0;
			if (h1->side_ != HEROS_INVALID_SIDE) {
				l1 = h1->loyalty(*(*teams_)[h1->side_].leader());
			}
			if (h2->side_ != HEROS_INVALID_SIDE) {
				l2 = h2->loyalty(*(*teams_)[h2->side_].leader());
			}
			if (l1 > l2) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[4]) {
			// catalog
			if (h1->base_catalog_ > h2->base_catalog_) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[5]) {
			// meritorious
			if (h1->meritorious_ > h2->meritorious_) {
				result = false;
			}
		} 

	} else if (current_page_ == ABILITY_PAGE) {
		if (sorting_widget_ == widgets[0]) {
			// name
			result = utils::utf8str_compare(h1->name(), h2->name());
		} else if (sorting_widget_ == widgets[1]) {
			// leadership
			if (h1->leadership_ > h2->leadership_) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[2]) {
			// force
			if (h1->force_ > h2->force_) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[3]) {
			// intellect
			if (h1->intellect_ > h2->intellect_) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[4]) {
			// spirit
			if (h1->spirit_ > h2->spirit_) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[5]) {
			// charm
			if (h1->charm_ > h2->charm_) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[6]) {
			// activity
			if (h1->activity_ > h2->activity_) {
				result = false;
			}
		}

	} else if (current_page_ == FEATURE_PAGE) {
		if (sorting_widget_ == widgets[0]) {
			// name
			result = utils::utf8str_compare(h1->name(), h2->name());
		} else if (sorting_widget_ == widgets[1]) {
			// feature
			result = utils::utf8str_compare(hero::feature_str(h1->feature_), hero::feature_str(h2->feature_));
		}

	} else if (current_page_ == ADAPTABILITY_PAGE) {
		if (sorting_widget_ == widgets[0]) {
			// name
			result = utils::utf8str_compare(h1->name(), h2->name());
		} else if (sorting_widget_ == widgets[1]) {
			// arm0
			if (h1->arms_[0] > h2->arms_[0]) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[2]) {
			// arm1
			if (h1->arms_[1] > h2->arms_[1]) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[3]) {
			// arm2
			if (h1->arms_[2] > h2->arms_[2]) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[4]) {
			// arm3
			if (h1->arms_[3] > h2->arms_[3]) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[5]) {
			// arm4
			if (h1->arms_[4] > h2->arms_[4]) {
				result = false;
			}
		}

	} else if (current_page_ == SKILL_PAGE) {
		if (sorting_widget_ == widgets[0]) {
			// name
			result = utils::utf8str_compare(h1->name(), h2->name());
		} else if (sorting_widget_ == widgets[1]) {
			// skill0
			if (h1->skill_[hero_skill_commercial] > h2->skill_[hero_skill_commercial]) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[2]) {
			// skill1
			if (h1->skill_[hero_skill_invent] > h2->skill_[hero_skill_invent]) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[3]) {
			// skill3
			if (h1->skill_[hero_skill_encourage] > h2->skill_[hero_skill_encourage]) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[4]) {
			// skill4
			if (h1->skill_[hero_skill_hero] > h2->skill_[hero_skill_hero]) {
				result = false;
			}
		} else if (sorting_widget_ == widgets[5]) {
			// skill5
			if (h1->skill_[hero_skill_demolish] > h2->skill_[hero_skill_demolish]) {
				result = false;
			}
		}

	} else if (current_page_ == COMMAND_PAGE) {
		if (sorting_widget_ == widgets[0]) {
			// name
			result = utils::utf8str_compare(h1->name(), h2->name());
		} else if (sorting_widget_ == widgets[1]) {
			// tactic
			std::string str1, str2;
			if (h1->tactic_ != HEROS_NO_TACTIC) {
				str1 = unit_types.tactic(h1->tactic_).name();
			}
			if (h2->tactic_ != HEROS_NO_TACTIC) {
				str2 = unit_types.tactic(h2->tactic_).name();
			}
			result = utils::utf8str_compare(str1, str2);
		}

	} else if (current_page_ == PERSONAL_PAGE) {
		if (sorting_widget_ == widgets[0]) {
			// name
			result = utils::utf8str_compare(h1->name(), h2->name());
		} else if (sorting_widget_ == widgets[1]) {
			// gender
			result = utils::utf8str_compare(hero::gender_str(h1->gender_), hero::gender_str(h2->gender_));
		} else if (sorting_widget_ == widgets[2]) {
			// treasure
			std::string str1, str2;
			if (h1->treasure_ != HEROS_NO_TREASURE) {
				str1 = unit_types.treasure(h1->treasure_).name();
			}
			if (h2->treasure_ != HEROS_NO_TREASURE) {
				str2 = unit_types.treasure(h2->treasure_).name();
			}
			result = utils::utf8str_compare(str1, str2);
		} else if (sorting_widget_ == widgets[3]) {
			// action
			result = utils::utf8str_compare(hero::status_str(h1->status_), hero::status_str(h2->status_));
		} else if (sorting_widget_ == widgets[4]) {
			// official
			result = utils::utf8str_compare(hero::official_str(h1->official_), hero::official_str(h2->official_));
		} else if (sorting_widget_ == widgets[5]) {
			// noble
			std::string str1, str2;
			if (h1->noble_ != HEROS_NO_NOBLE) {
				str1 = unit_types.noble(h1->noble_).name();
			}
			if (h2->noble_ != HEROS_NO_NOBLE) {
				str2 = unit_types.noble(h2->noble_).name();
			}
			result = utils::utf8str_compare(str1, str2);
		}

	} else if (current_page_ == RELATION_PAGE) {
		if (sorting_widget_ == widgets[0]) {
			// name
			result = utils::utf8str_compare(h1->name(), h2->name());
		} else if (sorting_widget_ == widgets[1]) {
			// father
			std::string str1, str2;
			if (h1->parent_[0].hero_ != HEROS_INVALID_NUMBER) {
				str1 = h1->name();
			}
			if (h2->parent_[0].hero_ != HEROS_INVALID_NUMBER) {
				str2 = h2->name();
			}
			result = utils::utf8str_compare(str1, str2);
		} else if (sorting_widget_ == widgets[2]) {
			// mother
			std::string str1, str2;
			if (h1->parent_[1].hero_ != HEROS_INVALID_NUMBER) {
				str1 = h1->name();
			}
			if (h2->parent_[1].hero_ != HEROS_INVALID_NUMBER) {
				str2 = h2->name();
			}
			result = utils::utf8str_compare(str1, str2);
		}
	}

	return ascend_? result: !result;
}

static bool callback_compare_row(void* caller, twidget& row1, twidget& row2)
{
	return reinterpret_cast<thero_list*>(caller)->compare_row(row1, row2);
}

void thero_list::sort_column(tbutton& widget, int catalog)
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

	tlistbox& list = *layers_[catalog];
	list.sort(this, callback_compare_row);
	list.invalidate_layout(false);
}

} // namespace gui2
