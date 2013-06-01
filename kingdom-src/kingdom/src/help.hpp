/* $Id: help.hpp 47650 2010-11-21 13:58:53Z mordante $ */
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
#ifndef HELP_HPP_INCLUDED
#define HELP_HPP_INCLUDED

class config;
class display;
class gamemap;

#include "hotkeys.hpp"
#include "construct_dialog.hpp"

#include <list>

namespace help {

struct help_manager {
	help_manager(const config *game_config, gamemap *map);
	~help_manager();
};

struct section;
/// Open a help dialog using a toplevel other than the default. This
/// allows for complete customization of the contents, although not in a
/// very easy way.
void show_help(display &disp, const section &toplevel, const std::string& show_topic="",
			   int xloc=-1, int yloc=-1);

/// Open the help browser. The help browser will have the topic with id
/// show_topic open if it is not the empty string. The default topic
/// will be shown if show_topic is the empty string.
void show_help(display &disp, const std::string& show_topic="", int xloc=-1, int yloc=-1);

/// wrapper to add unit prefix and hidding symbol
void show_unit_help(display &disp, const std::string& unit_id, bool hidden = false,
				int xloc=-1, int yloc=-1);

class help_button : public gui::dialog_button, public hotkey::command_executor {
public:
	help_button(display& disp, const std::string &help_topic);
	~help_button();
	int action(gui::dialog_process_info &info);
	std::string topic() const { return topic_; }
	void join();
	void leave();
private:
	void show_help();
	bool can_execute_command(hotkey::HOTKEY_COMMAND cmd, int/*index*/ =-1) const;

	display &disp_;
	const std::string topic_;
	hotkey::basic_handler *help_hand_;
};


std::vector<std::string> parse_text(const std::string &text);

SDL_Color string_to_color(const std::string &s);
std::vector<std::string> split_in_width(const std::string &s, const int font_size, const unsigned width, int style = TTF_STYLE_NORMAL);
std::string remove_first_space(const std::string& text);
std::string get_first_word(const std::string &s);

// Integrating Text and Graphics 
class tintegrate
{
public:
	static std::string hero_color;
	static std::string object_color;
	static std::string tactic_color;

	enum ALIGNMENT {LEFT, MIDDLE, RIGHT, HERE, BACK};
	static std::string generate_img(const std::string& src, ALIGNMENT align = HERE, bool floating = false);
	static std::string generate_format(const std::string& text, const std::string& color = "", int font_size = 0, bool bold = false, bool italic = false);
	static std::string generate_format(int val, const std::string& color = "", int font_size = 0, bool bold = false, bool italic = false);

	tintegrate(const std::string& src, int maximum_width, int maximum_height, int default_font_size, const SDL_Color& default_font_color);

	/// Return the ID that is crossreferenced at the (screen)
	/// coordinates x, y. If no cross-reference is there, return the
	/// empty string.
	std::string ref_at(const int x, const int y);

	SDL_Rect get_size() const;
	surface get_surface() const;

private:
	/// Convert a string to an alignment. Throw parse_error if
	/// unsuccessful.
	ALIGNMENT str_to_align(const std::string &s);

	static std::string align_to_string(ALIGNMENT align);

	/// An item that is displayed in the text area. Contains the surface
	/// that should be blitted along with some other information.
	struct item {

		item(surface surface, int x, int y, const std::string& text="",
			 const std::string& reference_to="", bool floating=false,
			 bool box=false, ALIGNMENT alignment=HERE);

		item(surface surface, int x, int y,
			 bool floating, bool box=false, ALIGNMENT=HERE);

		/// Relative coordinates of this item.
		SDL_Rect rect;

		surface surf;

		// If this item contains text, this will contain that text.
		std::string text;

		// If this item contains a cross-reference, this is the id
		// of the referenced topic.
		std::string ref_to;

		// If this item is floating, that is, if things should be filled
		// around it.
		bool floating;
		bool box;
		ALIGNMENT align;
	};

	/// Function object to find an item at the specified coordinates.
	class item_at {
	public:
		item_at(const int x, const int y) : x_(x), y_(y) {}
		bool operator()(const item&) const;
	private:
		const int x_, y_;
	};

	// Create appropriate items from configs. Items will be added to the
	// internal vector. These methods check that the necessary
	// attributes are specified.
	void handle_ref_cfg(const config &cfg);
	void handle_img_cfg(const config &cfg);
	void handle_bold_cfg(const config &cfg);
	void handle_italic_cfg(const config &cfg);
	void handle_header_cfg(const config &cfg);
	void handle_jump_cfg(const config &cfg);
	void handle_format_cfg(const config &cfg);

	void draw_contents();

	/// Add an item with text. If ref_dst is something else than the
	/// empty string, the text item will be underlined to show that it
	/// is a cross-reference. The item will also remember what the
	/// reference points to. If font_size is below zero, the default
	/// will be used.
	void add_text_item(const std::string& text, const SDL_Color& color, const std::string& ref_dst="",
					   bool broken_link = false,
					   int font_size=-1, bool bold=false, bool italic=false);

	/// Add an image item with the specified attributes.
	void add_img_item(const std::string& path, const std::string& alignment, const bool floating,
					  const bool box);

	/// Move the current input point to the next line.
	void down_one_line();

	/// Adjust the heights of the items in the last row to make it look
	/// good .
	void adjust_last_row();

	/// Return the width that remain on the line the current input point is at.
	int get_remaining_width();

	/// Return the least x coordinate at which something of the
	/// specified height can be drawn at the specified y coordinate
	/// without interfering with floating images.
	int get_min_x(const int y, const int height=0);

	/// Analogous with get_min_x but return the maximum X.
	int get_max_x(const int y, const int height=0);

	/// Find the lowest y coordinate where a floating img of the
	/// specified width and at the specified x coordinate can be
	/// placed. Start looking at desired_y and continue downwards. Only
	/// check against other floating things, since text and inline
	/// images only can be above this place if called correctly.
	int get_y_for_floating_img(const int width, const int x, const int desired_y);

	/// Add an item to the internal list, update the locations and row
	/// height.
	void add_item(const item& itm);

	std::list<item> items_;
	std::list<item *> last_row_;
	const int title_spacing_;
	// The current input location when creating items.
	std::pair<int, int> curr_loc_;
	const unsigned min_row_height_;
	unsigned curr_row_height_;
	/// The height of all items in total.
	int contents_height_;

	int maximum_width_;
	int maximum_height_;
	int default_font_size_;
	SDL_Color default_font_color_;
};

} // End namespace help.

#endif
