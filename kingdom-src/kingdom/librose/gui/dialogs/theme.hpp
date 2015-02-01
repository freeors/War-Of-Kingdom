/* $Id: lobby_player_info.hpp 48440 2011-02-07 20:57:31Z mordante $ */
/*
   Copyright (C) 2009 - 2011 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_THEME__HPP_INCLUDED
#define GUI_DIALOGS_THEME__HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "config.hpp"
#include "lobby.hpp"

class display;
class controller_base;

namespace theme {

struct tborder 
{
	tborder();
	tborder(const config& cfg);

	double size;
	SDL_Color view_rectange_color;

	std::string background_image;
	std::string tile_image;

	std::string corner_image_top_left;
	std::string corner_image_bottom_left;

	std::string corner_image_top_right_odd;
	std::string corner_image_top_right_even;

	std::string corner_image_bottom_right_odd;
	std::string corner_image_bottom_right_even;

	std::string border_image_left;
	std::string border_image_right;

	std::string border_image_top_odd;
	std::string border_image_top_even;

	std::string border_image_bottom_odd;
	std::string border_image_bottom_even;

};

// All on-screen objects have 'anchoring' in the x and y dimensions.
// 'fixed' means that they have fixed co-ordinates and don't move.
// 'top anchored' means they are anchored to the top (or left) side
// of the screen - the top (or left) edge stays a constant distance
// from the top of the screen.
// 'bottom anchored' is the inverse of top anchored.
// 'proportional' means the location and dimensions change
// proportionally to the screen size.
enum ANCHORING { FIXED, TOP_ANCHORED, PROPORTIONAL, BOTTOM_ANCHORED };
ANCHORING read_anchor(const std::string& str);
SDL_Rect calculate_relative_loc(const config& cfg, int screen_w, int screen_h);
const config* set_resolution(const config& cfg, const SDL_Rect& screen, const std::string& patch, config& resolved_config);
const config* set_resolution2(const config& cfg, int screen_w, int screen_h);

void set_known_themes(const config* cfg);
extern std::map<std::string, config> known_themes;
}

namespace gui2 {

class tscroll_label;
class treport;

struct tcontext_menu {
	enum {F_HIDE = 0x1, F_PARAM = 0x2};

	tcontext_menu(const std::string& id)
		: main_id(id)
		, menus()
		, report(NULL)
	{}

	static std::pair<std::string, std::string> extract_item(const std::string& id);
	static size_t decode_flag_str(const std::string& str);

	void load(display& disp, const config& cfg);
	void show(const display& gui, const controller_base& controller, const std::string& id) const;
	void hide() const;

	std::string main_id;
	std::map<std::string, std::vector<std::string> > menus;
	gui2::treport* report;
};

class ttheme : public tdialog, public tlobby::tlog_handler
{
public:
	ttheme(display& disp, controller_base& controller, const config& cfg);

	virtual ~ttheme();

	void post_layout();

	virtual twidget* get_object(const std::string& id) const;
	virtual void set_object_active(const std::string& id, bool active) const;
	virtual void set_object_visible(const std::string& id, const twidget::tvisible visible) const;
	virtual void set_object_surface(const std::string& id, const surface& surf) const;

	twidget* get_report(int num) const;
	void set_report_label(int num, const std::string& label) const;
	void set_report_surface(int num, const surface& surf) const;

	std::vector<tcontext_menu>& context_menus() { return context_menus_; }
	tcontext_menu* context_menu(const std::string& id);

protected:
	void click_generic_handler(tcontrol& widget, const std::string& sparam);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	void handle(tlobby::tstate s, const std::string& msg);

	virtual void app_pre_show() {}
	
protected:
	const config& cfg_;
	twindow* window_;
	mutable std::map<std::string, twidget*> cached_widgets_;
	std::map<int, const std::string> reports_;
	std::vector<tcontext_menu> context_menus_;

private:
	display& disp_;
	controller_base& controller_;
	tscroll_label* log_;
};

} //end namespace gui2

#endif
