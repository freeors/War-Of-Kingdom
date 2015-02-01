/* $Id: editor_controller.hpp 47816 2010-12-05 18:08:38Z mordante $ */
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

#ifndef STUDIO_MKWIN_CONTROLLER_HPP_INCLUDED
#define STUDIO_MKWIN_CONTROLLER_HPP_INCLUDED

#include "controller_base.hpp"
#include "mouse_handler_base.hpp"
#include "mkwin_display.hpp"
#include "unit_map.hpp"
#include "map.hpp"
#include "gui/auxiliary/window_builder.hpp"

enum EXIT_STATUS {
	EXIT_NORMAL,
	EXIT_QUIT_TO_DESKTOP,
	EXIT_RELOAD_DATA,
	EXIT_ERROR
};

struct tsheet_node 
{
	tsheet_node(unit* parent = NULL, int number = 0) 
		: parent(parent)
		, number(number)
	{}

	unit* parent;
	int number;
};

/**
 * The editor_controller class containts the mouse and keyboard event handling
 * routines for the editor. It also serves as the main editor class with the
 * general logic.
 */
class mkwin_controller : public controller_base, public events::mouse_handler_base
{
public:
	static const int default_width = 5;
	static const int default_height = 4;

	/**
	 * The constructor. A initial map context can be specified here, the controller
	 * will assume ownership and delete the pointer during destruction, but changes
	 * to the map can be retrieved between the main loop's end and the controller's
	 * destruction.
	 */
	mkwin_controller(const config &top_config, CVideo& video);

	~mkwin_controller();

	/** Editor main loop */
	EXIT_STATUS main_loop();

	void init_gui(CVideo& video);
	void init_music(const config& top_config);

	mkwin_display& gui() { return *gui_; }
	const mkwin_display& gui() const { return *gui_; }

	const map_location& selected_hex() const { return selected_hex_; }
	unit* get_window() const;
	bool window_has_valid(bool show_error) const;

	void mouse_motion(int x, int y, const bool browse, bool update);
	bool left_click(int x, int y, const bool browse);
	bool right_click(int x, int y, const bool browse);

	void click_widget(const std::string& type, const std::string& definition);
	void sheet_toggled(gui2::twidget* widget, int index, bool save);
	void simulate_toggle_sheet(int index, bool save);
	const std::vector<gui2::tlinked_group>& linked_groups() const { return linked_groups_; }
	void set_status();
	const unit* copied_unit() const { return copied_unit_; }
	void set_copied_unit(unit* u);

	const std::vector<tsheet_node>& parent_nodes() const { return parent_nodes_; }
	std::vector<tsheet_node>& unit_nodes() { return unit_nodes_; }

	bool in_context_menu(const std::string& id) const;
	bool actived_context_menu(const std::string& id) const;

	void do_right_click();

	void pre_change_resolution();
	void post_change_resolution();

private:
	/** command_executor override */
	void execute_command2(int command, const std::string& sparam);

	void reload_map(int w, int h);

	void widget_setting(bool special);
	void linked_group_setting();
	void erase_widget();
	void copy_widget();
	void paste_widget();
	void change_map(int dir, bool extend, int index);
	void move_line(bool horizontal, int index);
	void swap_line(bool horizontal, int index);
	void run();
	void system();
	void load_window();
	void save_window(bool as);
	void quit_confirm(EXIT_STATUS mode);

	bool can_paste(const unit* u) const;

	void switch_sheet_to(int index, bool save);
	void fill_spacer();
	config generate_window_cfg();

protected:
	/* controller_base overrides */
	mouse_handler_base& get_mouse_handler_base();
	mkwin_display& get_display() { return *gui_; }
	const mkwin_display& get_display() const { return *gui_; }

private:
	/** The display object used and owned by the editor. */
	boost::scoped_ptr<mkwin_display> gui_;

	gamemap map_;
	unit_map units_;

	map_location selected_hex_;
	unit* current_unit_;
	unit::tchild top_;
	std::vector<tsheet_node> parent_nodes_;
	std::vector<tsheet_node> unit_nodes_;
	std::vector<std::string> textdomains_;
	std::vector<gui2::tlinked_group> linked_groups_;

	std::pair<std::string, config> original_;
	unit* copied_unit_;
	
	/** Quit main loop flag */
	bool do_quit_;
	EXIT_STATUS quit_mode_;
};

#endif
