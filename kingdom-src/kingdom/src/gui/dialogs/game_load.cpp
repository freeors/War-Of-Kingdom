/* $Id: game_load.cpp 49598 2011-05-22 17:55:54Z mordante $ */
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

#include "gui/dialogs/game_load.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "game_display.hpp"
#include "game_config.hpp"
#include "game_preferences.hpp"
#include "gui/auxiliary/log.hpp"
#include "gui/dialogs/field.hpp"
#include "gui/dialogs/game_delete.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/widgets/minimap.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/scrollbar_panel.hpp"
#include "gui/widgets/window.hpp"
#include "language.hpp"
#include "preferences_display.hpp"
#include "multiplayer.hpp"
#include "help.hpp"

#include <cctype>
#include <boost/foreach.hpp>
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

REGISTER_DIALOG(game_load)

tgame_load::tgame_load(game_display& disp, const config& cache_config)
	: disp_(disp)
	, filename_()
	, games_()
	, www_saves_()
	, cache_config_(cache_config)
	, current_page_(NONE_PAGE)
	, savegame_list_(NULL)
{
}

void tgame_load::sheet_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	int toggled_page = toggle->get_data();

	if (!toggle->get_value()) {
		// At most select one page. recheck it!
		toggle->set_value(true);
	} else {
		for (std::map<int, ttoggle_button*>::iterator it = sheet_.begin(); it != sheet_.end(); ++ it) {
			if (it->second == toggle) {
				continue;
			}
			it->second->set_value(false);
		}
		swap_page(*toggle->get_window(), toggled_page, true);
	}
}

void tgame_load::pre_show(CVideo& /*video*/, twindow& window)
{
	find_widget<tminimap>(&window, "minimap", false).set_config(&cache_config_);

	sheet_.insert(std::make_pair(LOCAL_PAGE, find_widget<ttoggle_button>(&window, "local", false, true)));
	sheet_.insert(std::make_pair(NETWORK_PAGE, find_widget<ttoggle_button>(&window, "network", false, true)));
	for (std::map<int, ttoggle_button*>::iterator it = sheet_.begin(); it != sheet_.end(); ++ it) {
		it->second->set_callback_state_change(boost::bind(&tgame_load::sheet_toggled, this, _1));
		it->second->set_data(it->first);
	}

	connect_signal_mouse_left_click(
			find_widget<tbutton>(&window, "delete", false)
			, boost::bind(
				  &tgame_load::delete_button_callback
				, this
				, boost::ref(window)));

	connect_signal_mouse_left_click(
			find_widget<tbutton>(&window, "xmit", false)
			, boost::bind(
				  &tgame_load::xmit_button_callback
				, this
				, boost::ref(window)));

	savegame_list_ = find_widget<tlistbox>(&window, "savegame_list", false, true);
	sheet_.begin()->second->set_value(true);

	swap_page(window, LOCAL_PAGE, false);
#ifdef GUI2_EXPERIMENTAL_LISTBOX
	connect_signal_notify_modified(*list, boost::bind(
				  &tgame_load::list_item_clicked
				, *this
				, boost::ref(window)));
#else
	savegame_list_->set_callback_value_change(
			dialog_callback<tgame_load, &tgame_load::list_item_clicked>);
#endif


	if (disp_.in_game()) {
		find_widget<tbutton>(&window, "xmit", false).set_visible(twidget::INVISIBLE);
		find_widget<ttoggle_button>(&window, "local", false).set_visible(twidget::INVISIBLE);
		find_widget<ttoggle_button>(&window, "network", false).set_visible(twidget::INVISIBLE);
	}
}

void tgame_load::list_item_clicked(twindow& window)
{
	display_savegame(window);
}

void tgame_load::post_show(twindow& window)
{
}

void tgame_load::display_savegame(twindow& window)
{
	const int selected_row = savegame_list_->get_selected_row();

	twidget& preview_pane =
			find_widget<twidget>(&window, "preview_pane", false);

	if (selected_row == -1) {
		preview_pane.set_visible(twidget::HIDDEN);
	} else if (current_page_ == LOCAL_PAGE) {
		preview_pane.set_visible(twidget::VISIBLE);

		savegame::save_info& game = games_[selected_row];
		filename_ = game.name;

		config cfg_summary;
		std::string dummy;

		try {
			savegame::manager::load_summary(game.name, cfg_summary, &dummy);
		} catch(game::load_game_failed&) {
			cfg_summary["corrupt"] = "yes";
		}

		find_widget<timage>(&window, "imgLeader", false).
				set_label(cfg_summary["leader_image"]);

		find_widget<tminimap>(&window, "minimap", false).
				set_map_data(cfg_summary["map_data"]);

		find_widget<tlabel>(&window, "lblScenario", false).set_label(game.name);

		std::stringstream str;
		str << format_time_local(cfg_summary["create"].to_long()) << "\n";
		str << _("Total time") << ": " << format_time_elapse(cfg_summary["duration"].to_int());
		str << "(" << help::tintegrate::generate_format(cfg_summary["hash"].to_int(), "yellow") << ")";
		evaluate_summary_string(str, cfg_summary);

		find_widget<tlabel>(&window, "lblSummary", false).set_label(str.str());

		window.invalidate_layout();

	} else if (current_page_ == NETWORK_PAGE) {
		preview_pane.set_visible(twidget::VISIBLE);

		savegame::www_save_info& game = www_saves_[selected_row];
		filename_ = game.name;

		find_widget<tminimap>(&window, "minimap", false).set_map_data("");

		find_widget<tlabel>(&window, "lblScenario", false).set_label(game.name);

		find_widget<tlabel>(&window, "lblSummary", false).set_label("");
	}
}

void tgame_load::evaluate_summary_string(std::stringstream& str
		, const config& cfg_summary){

	const std::string& campaign_type = cfg_summary["campaign_type"];
	if (cfg_summary["corrupt"].to_bool()) {
		str << "\n" << _("#(Invalid)");
	} else if (!campaign_type.empty()) {
		str << "\n";

		if(campaign_type == "scenario") {
			const std::string campaign_id = cfg_summary["campaign"];
			const config *campaign = NULL;
			if (!campaign_id.empty()) {
				if (const config &c = cache_config_.find_child(
						"campaign", "id", campaign_id))

					campaign = &c;
			}
			utils::string_map symbols;
			if (campaign != NULL) {
				symbols["campaign_name"] = (*campaign)["name"];
			} else {
				// Fallback to nontranslatable campaign id.
				symbols["campaign_name"] = "(" + campaign_id + ")";
			}
			str << vgettext("Campaign: $campaign_name", symbols);

			// Display internal id for debug purposes if we didn't above
			if (game_config::debug && (campaign != NULL)) {
				str << '\n' << "(" << campaign_id << ")";
			}
		} else if(campaign_type == "multiplayer") {
			str << _("Multiplayer");
		} else if(campaign_type == "tutorial") {
			str << _("Tutorial");
		} else {
			str << campaign_type;
		}

		str << "\n";

		if (cfg_summary["replay"].to_bool() && !cfg_summary["snapshot"].to_bool(true)) {
			str << _("replay");
		} else if (!cfg_summary["turn"].empty()) {
			str << _("Turn") << " " << cfg_summary["turn"];
		} else {
			str << _("Scenario Start");
		}

		if(!cfg_summary["version"].empty()) {
			str << "\n" << _("Version: ") << cfg_summary["version"];
		}
	}
}

void tgame_load::delete_button_callback(twindow& window)
{
	if (current_page_ != LOCAL_PAGE) {
		return;
	}

	const size_t index = size_t(savegame_list_->get_selected_row());
	if (index < games_.size()) {

		// See if we should ask the user for deletion confirmation
		if (preferences::ask_delete_saves()) {
			if(!gui2::tgame_delete::execute(window.video())) {
				return;
			}
		}

		// Delete the file
		savegame::manager::delete_game(games_[index].name);

		// Remove it from the list of saves
		games_.erase(games_.begin() + index);
		savegame_list_->remove_row(index);

		display_savegame(window);
	}
}

void tgame_load::xmit_button_callback(twindow& window)
{
	const size_t index = size_t(savegame_list_->get_selected_row());
	if (current_page_ == LOCAL_PAGE) {
		if (index < games_.size()) {
			// See if we should ask the user for deletion confirmation
			std::string message = dsgettext("wesnoth", "You can upload one only, it will delete existed, continue?");
			const int res = gui2::show_message(disp_.video(), dsgettext("wesnoth", "Confirm"), message, gui2::tmessage::yes_no_buttons);
			if (res == gui2::twindow::CANCEL) {
				return;
			}
			http::upload_save(disp_, get_saves_dir() + "/" + games_[index].name);
		}
	} else if (current_page_ == NETWORK_PAGE) {
		if (index < www_saves_.size()) {

			std::string fname = http::download_save(disp_, www_saves_[index].sid);
			if (!fname.empty()) {
				sheet_.find(LOCAL_PAGE)->second->set_value(true);
				sheet_toggled(sheet_.find(LOCAL_PAGE)->second);
			}
		}
	}
}

void tgame_load::fill_local(twindow& window)
{
	find_widget<tcontrol>(&window, "delete", false).set_active(true);
	find_widget<tcontrol>(&window, "ok", false).set_active(true);
	std::string str = help::tintegrate::generate_format(_("Upload"), "blue");
	find_widget<tcontrol>(&window, "xmit", false).set_label(str);

	{
		cursor::setter cur(cursor::WAIT);
		games_ = savegame::manager::get_saves_list();
	}
	savegame_list_->clear();
	BOOST_FOREACH (const savegame::save_info game, games_) {
		std::map<std::string, string_map> data;
		string_map item;

		item["label"] = game.name;
		data.insert(std::make_pair("filename", item));

		item["label"] = game.format_time_summary();
		data.insert(std::make_pair("date", item));

		savegame_list_->add_row(data);
	}
	display_savegame(window);
}

void tgame_load::fill_network(twindow& window)
{
	find_widget<tcontrol>(&window, "delete", false).set_active(false);
	find_widget<tcontrol>(&window, "ok", false).set_active(false);
	std::string str = help::tintegrate::generate_format(_("Download"), "blue");
	find_widget<tcontrol>(&window, "xmit", false).set_label(str);

	{
		cursor::setter cur(cursor::WAIT);
		www_saves_ = http::list_save(disp_);
	}
	savegame_list_->clear();
	BOOST_FOREACH (const savegame::www_save_info save, www_saves_) {
		std::map<std::string, string_map> data;
		string_map item;

		item["label"] = save.name;;
		data.insert(std::make_pair("filename", item));

		item["label"] = save.username;
		data.insert(std::make_pair("username", item));

		item["label"] = save.format_time_upload();
		data.insert(std::make_pair("date", item));

		savegame_list_->add_row(data);
	}
	display_savegame(window);
}

void tgame_load::swap_page(twindow& window, int page, bool swap)
{
	if (page < MIN_PAGE || page > MAX_PAGE) {
		return;
	}
	int index = page - MIN_PAGE;

	if (window.alternate_index() == index) {
		// desired page is the displaying page, do nothing.
		return;
	}
	window.alternate_uh(savegame_list_, index);

	current_page_ = page;

	if (page == LOCAL_PAGE) {
		fill_local(window);

	} else if (page == NETWORK_PAGE) {
		fill_network(window);
	}
	window.alternate_bh(swap? savegame_list_: NULL, index);
}

} // namespace gui2
