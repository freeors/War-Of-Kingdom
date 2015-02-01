/* $Id: campaign_difficulty.cpp 49602 2011-05-22 17:56:13Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>
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

#include "gui/dialogs/build_ea.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/listbox.hpp"
#include <boost/bind.hpp>
#include "game_display.hpp"
#include "team.hpp"
#include "unit_map.hpp"
#include "gamestatus.hpp"
#include "artifical.hpp"
#include "marked-up_text.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "integrate.hpp"
#include "actions.hpp"

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_campaign_difficulty
 *
 * == Campaign difficulty ==
 *
 * The campaign mode difficulty menu.
 *
 * @begin{table}{dialog_widgets}
 *
 * title & & label & m &
 *         Dialog title label. $
 *
 * message & & scroll_label & o &
 *         Text label displaying a description or instructions. $
 *
 * listbox & & listbox & m &
 *         Listbox displaying user choices, defined by WML for each campaign. $
 *
 * -icon & & control & m &
 *         Widget which shows a listbox item icon, first item markup column. $
 *
 * -label & & control & m &
 *         Widget which shows a listbox item label, second item markup column. $
 *
 * -description & & control & m &
 *         Widget which shows a listbox item description, third item markup column. $
 *
 * @end{table}
 */

REGISTER_DIALOG(build_ea)

bool compare_ut_master(const unit_type* l, const unit_type* r)
{
	return l->master() <= r->master();
}

tbuild_ea::tbuild_ea(display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, const gamemap& map, team& current_team, int cost_exponent, const map_location& loc)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, map_(map)
	, current_team_(current_team)
	, cost_exponent_(cost_exponent)
	, loc_(loc)
	, ownership_(NULL)
	, can_build_ea_()
	, valid_()
	, selected_ut_(NULL)
{
	const std::set<const unit_type*>& can_build = current_team_.builds();

	// ea
	std::map<const map_location, int>::const_iterator it = unit_map::economy_areas_.find(loc);
	if (it != unit_map::economy_areas_.end()) {
		ownership_ = units_.city_from_cityno(it->second);
	}
	if (ownership_) {
		for (std::set<const unit_type*>::const_iterator it = can_build.begin(); it != can_build.end(); ++ it) {
			const unit_type* ut = *it;
			if (hero::is_ea_artifical(ut->master())) {
				can_build_ea_.push_back(ut);
			}
		}
	}

	// wall
	artifical* wall_city = loc_can_build_wall(units_, map_, loc_, current_team_);
	if (wall_city) {
		for (std::set<const unit_type*>::const_iterator it = can_build.begin(); it != can_build.end(); ++ it) {
			const unit_type* ut = *it;
			if (ut->master() == hero::number_wall || ut->master() == hero::number_keep) {
				can_build_ea_.push_back(ut);
			}
		}
		if (!ownership_) {
			ownership_ = wall_city;
		}
	}

	std::sort(can_build_ea_.begin(), can_build_ea_.end(), compare_ut_master);

	VALIDATE(ownership_, "tbuild_ea::tbuild_ea, cannot find owership city.");
}

void refresh_notice(twindow& window, const unit_type* ut)
{
	std::stringstream strstr;

	tlabel& label = find_widget<tlabel>(&window, "notice", false);
	int master = ut? ut->master(): HEROS_INVALID_NUMBER;
	if (master != HEROS_INVALID_NUMBER) {
		strstr << tintegrate::generate_img("misc/notice.png") << " ";
	}
	if (tent::mode != mode_tag::LAYOUT) {
		if (master == hero::number_technology) {
			strstr << _("Technology income of side equals all cities income divided by number of city.");
		} else if (master == hero::number_tactic) {
			strstr << _("More tactic's effect that belong to same city can not be added.");
		} else if (master == hero::number_keep || master == hero::number_wall) {
			strstr << _("Must exist tow keep at least before building wall.");
		}
	} else {
		if (master == hero::number_market) {
			strstr << _("Economy increase holden card at scenario start.");
		} else if (master == hero::number_tactic) {
			strstr << _("Military increase hero's leadership at scenario start.");
		} else if (master == hero::number_school) {
			strstr << _("Culture increase hero's charm at scenario start.");
		} else if (master == hero::number_technology) {
			strstr << _("Science increase holden technology at scenario start.");
		}
	}
	label.set_label(strstr.str());
}

void tbuild_ea::tactic_selected(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "default", false);
	int selected = list.get_selected_row();

	if (selected >= 0) {
		selected_ut_ = can_build_ea_[selected];
	}

	refresh_notice(window, selected_ut_);

	tbutton* ok = find_widget<tbutton>(&window, "ok", false, true);
	ok->set_active(selected >= 0 && valid_[selected]);
}

void tbuild_ea::pre_show(CVideo& /*video*/, twindow& window)
{
	std::vector<hero*> captain;
	std::stringstream strstr;
	std::string color;

	int gold = current_team_.gold();

	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	strstr.str("");
	strstr << _("Build") << "(" << gold << sngettext("unit^Gold", "Gold", gold) << ")";
	label->set_label(strstr.str());

	tlistbox& list = find_widget<tlistbox>(&window, "default", false);
	for (std::vector<const unit_type*>::const_iterator it = can_build_ea_.begin(); it != can_build_ea_.end(); ++ it) {
		const unit_type* ut = *it;
		int cost = tent::mode != mode_tag::LAYOUT? (ut->cost() * cost_exponent_ / 100): 0;

		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		bool valid = true;

		if (gold >= cost) {
			if (ut->master() == hero::number_tactic) {
				if (tent::mode != mode_tag::LAYOUT && ownership_->tactic_on_ea()) {
					valid = false;
				}
			} else if (ut->master() == hero::number_wall) {
				if (calculate_keeps(units_, *ownership_) < 2) {
					valid = false;
				}
			}
		} else {
			valid = false;
		}
		
		
		if (valid) {
			list_item["label"] = ut->image() + "~RC(" + ut->flag_rgb() + ">" + team::get_side_color_index(current_team_.side()) + ")";
		} else {
			list_item["label"] = ut->image() + "~GS()";
		}

		list_item_item.insert(std::make_pair("icon", list_item));

		list_item["label"] = ut->type_name();
		list_item_item.insert(std::make_pair("name", list_item));

		strstr.str("");
		if (gold >= cost) {
			strstr << cost;
		} else {
			strstr << tintegrate::generate_format(cost, "red");
		}
		strstr << " " << sngettext("unit^Gold", "Gold", cost);
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("cost", list_item));

		strstr.str("");
		if (tent::mode != mode_tag::LAYOUT) {
			strstr << ut->unit_description();
		} else {
			utils::string_map symbols;
			std::string item;
			int master = ut->master();
			if (master == hero::number_market) {
				item = _("Economy");
			} else if (master == hero::number_tactic) {
				item = _("Military");
			} else if (master == hero::number_school) {
				item = _("Culture");
			} else if (master == hero::number_technology) {
				item = _("Science");
			}
			symbols["item"] = tintegrate::generate_format(item, "yellow");
			strstr << vgettext("wesnoth-lib", "Increase $item every day.", symbols);
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("desc", list_item));

		list.add_row(list_item_item);

		valid_.push_back(valid);
	}

	list.set_callback_value_change(dialog_callback<tbuild_ea, &tbuild_ea::tactic_selected>);

	tactic_selected(window);
}

void tbuild_ea::post_show(twindow& window)
{
}

const unit_type* tbuild_ea::get_selected_ut() const
{
	return selected_ut_;
}

}
