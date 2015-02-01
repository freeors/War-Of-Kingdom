/* $Id: editor_main.cpp 46551 2010-09-17 15:35:00Z alink $ */
/*
   Copyright (C) 2008 - 2010 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#define GETTEXT_DOMAIN "wesnoth-editor"

#include "editor_controller.hpp"

#include "gettext.hpp"
#include "filesystem.hpp"
#include "hero.hpp"

#include <boost/algorithm/string/replace.hpp>

lg::log_domain log_editor("editor");

namespace editor {

EXIT_STATUS start(const config& game_conf, CVideo& video, hero_map& heros, int mode, const std::string& filename /* = "" */,
	bool take_screenshot /* = false */, const std::string& screenshot_filename /* = "map_screenshot.bmp" */)
{
	EXIT_STATUS e = EXIT_ERROR;
	try {
		editor_controller editor(game_conf, video, heros, mode);
		if (!filename.empty()) {
			if (is_directory(filename)) {
				editor.set_default_dir(filename);
				editor.load_map_dialog(true);
			} else {
				editor.load_map(filename, false);
			}
		}
		if(take_screenshot) {
			editor.do_screenshot(screenshot_filename);
			e = EXIT_NORMAL;
		} else {
			e = editor.main_loop();
		}
	} catch (editor_exception& e) {
		ERR_ED << "Editor exception in editor::start: " << e.what() << "\n";
		throw;
	}
	if (editor_action::get_instance_count() != 0) {
		ERR_ED << "Possibly leaked " << editor_action::get_instance_count() << " action objects\n";
	}
	return e;
}

} //end namespace editor
