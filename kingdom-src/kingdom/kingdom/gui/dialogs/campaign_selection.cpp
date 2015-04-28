/* $Id: campaign_selection.cpp 48740 2011-03-05 10:01:34Z mordante $ */
/*
   Copyright (C) 2009 - 2011 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/dialogs/campaign_selection.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/scrollbar_panel.hpp"
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/window.hpp"
#include "serialization/string_utils.hpp"
#include "gettext.hpp"
#include "unit_types.hpp"
#include <preferences.hpp>

#include <boost/foreach.hpp>
#include <boost/bind.hpp>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_campaign_selection
 *
 * == Campaign selection ==
 *
 * This shows the dialog which allows the user to choose which campaign to
 * play.
 *
 * @begin{table}{dialog_widgets}
 *
 * campaign_list & & listbox & m &
 *         A listbox that contains all available campaigns. $
 *
 * -icon & & image & o &
 *         The icon for the campaign. $
 *
 * -name & & control & o &
 *         The name of the campaign. $
 *
 * -victory & & image & o &
 *         The icon to show when the user finished the campaign. The engine
 *         determines whether or not the user has finished the campaign and
 *         sets the visible flag for the widget accordingly. $
 *
  * -image & & image & o &
 *         The image for the campaign. $
 *
 * -description & & control & o &
 *         The description of the campaign. $
 *
 * @end{table}
 */

REGISTER_DIALOG(campaign_selection)

void tcampaign_selection::campaign_selected(twindow& window, tlistbox& list, const int type)
{
	const int selected_row = list.get_selected_row();

	tscrollbar_panel& multi_page = find_widget<tscrollbar_panel>(&window, "campaign_details", false);

	tscroll_label& label = find_widget<tscroll_label>(&window, "description", false);
	label.set_label(campaigns_[selected_row].description);

	tcontrol& image = find_widget<tcontrol>(&window, "image", false);
	image.set_label(campaigns_[selected_row].image);

	window.invalidate_layout();
}

void tcampaign_selection::pre_show(CVideo& /*video*/, twindow& window)
{
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	if (catalog_ == TUTORIAL_CATALOG) {
		label->set_label(_("Tutorial"));
	}

	/***** Setup campaign list. *****/
	tlistbox& list =
			find_widget<tlistbox>(&window, "campaign_list", false);

	list.set_callback_value_change(dialog_callback3<tcampaign_selection, tlistbox, &tcampaign_selection::campaign_selected>);

	window.keyboard_capture(&list);

	std::vector<config> campaigns_cfg = campaigns_cfg_;
	campaigns_cfg.push_back(config());
	config& sub = campaigns_cfg.back();
	sub["id"] = "random_map";
	sub["name"] = _("random_map");
	sub["description"] = _("random_map description");
	sub["icon"] = "data/app-kingdom/campaigns/random_map/images/icon.png";
	sub["image"] = "data/app-kingdom/campaigns/random_map/images/image.png";

	size_t n = 0;
	BOOST_FOREACH (const config &c, campaigns_cfg) {
		const std::string catalog = c["catalog"].str();
		if (catalog_ == NONE_CATALOG) {
			if (!catalog.empty()) {
				n ++;
				continue;
			}
		} else if (catalog_ == TUTORIAL_CATALOG) {
			if (catalog != "tutorial") {
				n ++;
				continue;
			}
		}
		const std::string mode = c["mode"].str();
		if ((!preferences::developer() && c["subcontinent"].to_bool()) || mode_tag::find(mode) == mode_tag::SIEGE) {
			n ++;
			continue;
		}

		/*** Add list item ***/
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		list_item["label"] = c["icon"];
		list_item_item.insert(std::make_pair("icon", list_item));

		list_item["label"] = c["name"];
		list_item_item.insert(std::make_pair("name", list_item));

		list.add_row(list_item_item);

		twidget* panel = list.get_row_panel(list.get_item_count() - 1);

		tcontrol* widget = dynamic_cast<tcontrol*>(panel->find("mode", false));
		if (widget) {
			if (mode == "tower") {
				widget->set_label("misc/mode-tower.png");
			} else if (c["rank"].to_int() == 100) {
				widget->set_label("misc/mode-rpg.png");
			}
		}

		ttoggle_panel* toggle = dynamic_cast<ttoggle_panel*>(panel);
		toggle->set_data(n ++);

		/*** Add detail item ***/
		campaigns_.push_back(tcampaign(c["description"].str(), c["image"].str()));
	}
	
	campaign_selected(window, list);
}

void tcampaign_selection::post_show(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "campaign_list", false);

	twidget* grid_ptr = list.get_row_panel(list.get_selected_row());
	choice_ = dynamic_cast<ttoggle_panel*>(grid_ptr)->get_data();
}

} // namespace gui2

