/* $Id: game_preferences_display.cpp 47753 2010-11-29 12:34:47Z shadowmaster $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#include "global.hpp"

#define GETTEXT_DOMAIN "kingdom-lib"

#include "display.hpp"
#include "filesystem.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "gui/dialogs/simple_item_selector.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/dialogs/browse.hpp"
#include "gui/widgets/window.hpp"
#include "lobby_preferences.hpp"
#include "preferences_display.hpp"
#include "wml_separators.hpp"
#include "formula_string_utils.hpp"


namespace preferences {

std::string show_wesnothd_server_search(display& disp)
{
	// Showing file_chooser so user can search the wesnothd
	std::string old_path = preferences::get_mp_server_program_name();
	size_t offset = old_path.rfind("/");
	if (offset != std::string::npos)
	{
		old_path = old_path.substr(0, offset);
	}
	else
	{
		old_path = "";
	}
#ifndef _WIN32

#ifndef WESNOTH_PREFIX
#define WESNOTH_PREFIX "/usr"
#endif
	const std::string filename = "wesnothd";
	std::string path = WESNOTH_PREFIX + std::string("/bin");
	if (!is_directory(path))
		path = get_cwd();

#else
	const std::string filename = "kingdomd.exe";
	std::string path = get_cwd();
#endif
	if (!old_path.empty()
			&& is_directory(old_path))
	{
		path = old_path;
	}

	utils::string_map symbols;

	symbols["filename"] = filename;

	const std::string title = utils::interpolate_variables_into_string(
			  _("Find $filename server binary to host networked games")
			, &symbols);

	{
		gui2::tbrowse::tparam param(gui2::tbrowse::TYPE_FILE, true, filename, title);
		gui2::tbrowse dlg(disp, param);
		dlg.show(disp.video());
		int res = dlg.get_retval();
		if (res != gui2::twindow::OK) {
			return null_str;
		}
		return param.result;
	}
/*
	int res = dialogs::show_file_chooser_dialog(disp, path, title, false, filename);
	if (res == 0)
		return path;
	else
		return "";
*/
}


}
