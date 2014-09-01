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

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/campaign_selection.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/widgets/multi_page.hpp"
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
 * campaign_details & & multi_page & m &
 *         A multi page widget that shows more details for the selected
 *         campaign. $
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

void tcampaign_selection::campaign_selected(twindow& window)
{
		const int selected_row =
				find_widget<tlistbox>(&window, "campaign_list", false)
					.get_selected_row();

		tmulti_page& multi_page = find_widget<tmulti_page>(
				&window, "campaign_details", false);

		multi_page.select_page(selected_row);
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
#ifdef GUI2_EXPERIMENTAL_LISTBOX
	connect_signal_notify_modified(list, boost::bind(
			  &tcampaign_selection::campaign_selected
			, this
			, boost::ref(window)));
#else
	list.set_callback_value_change(dialog_callback
			<tcampaign_selection
			, &tcampaign_selection::campaign_selected>);
#endif
	window.keyboard_capture(&list);

	std::vector<config> campaigns = campaigns_;
	campaigns.push_back(config());
	config& sub = campaigns.back();
	sub["id"] = "random_map";
	sub["name"] = dsgettext("wesnoth-multiplayer", "random_map");
	sub["description"] = dsgettext("wesnoth-multiplayer", "random_map description");
	sub["icon"] = "data/campaigns/random_map/images/icon.png";
	sub["image"] = "data/campaigns/random_map/images/image.png";

	/***** Setup campaign details. *****/
	tmulti_page& multi_page = find_widget<tmulti_page>(&window, "campaign_details", false);

	size_t n = 0;
	BOOST_FOREACH (const config &c, campaigns) {
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

		tgrid* grid = list.get_row_grid(list.get_item_count() - 1);
		assert(grid);

		tcontrol* widget = dynamic_cast<tcontrol*>(grid->find("mode", false));
		if (widget) {
			if (mode == "tower") {
				widget->set_label("misc/mode-tower.png");
			} else if (c["rank"].to_int() == 100) {
				widget->set_label("misc/mode-rpg.png");
			}
		}

		ttoggle_panel* toggle = dynamic_cast<ttoggle_panel*>(grid->find("_toggle", true));
		toggle->set_data(n ++);

		/*** Add detail item ***/
		string_map detail_item;
		std::map<std::string, string_map> detail_page;

		detail_item["label"] = c["description"];
		detail_item["use_markup"] = "true";
		detail_page.insert(std::make_pair("description", detail_item));

		detail_item["label"] = c["image"];
		detail_page.insert(std::make_pair("image", detail_item));

		multi_page.add_page(detail_page);
	}
	
	campaign_selected(window);
}

void tcampaign_selection::post_show(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "campaign_list", false);

	tgrid* grid_ptr = list.get_row_grid(list.get_selected_row());
	choice_ = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true))->get_data();
}

} // namespace gui2

