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
#ifndef ROSE_INTEGRATE_HPP_INCLUDED
#define ROSE_INTEGRATE_HPP_INCLUDED

#include "exceptions.hpp"
#include "sdl_utils.hpp"
#include "SDL_ttf.h"
#include "animation.hpp"
#include "gui/auxiliary/canvas.hpp"

#include <list>

class config;
class display;

// Integrating Text and Graphics 
class tintegrate
{
public:
	static std::string hero_color;
	static std::string object_color;
	static std::string tactic_color;
	static double screen_ratio;
	static const char* require_escape;
	static const std::string anim_tag_prefix;

	enum ALIGNMENT {LEFT, MIDDLE, RIGHT, HERE, BACK};
	static std::string generate_img(const std::string& src, ALIGNMENT align = HERE, bool floating = false);
	static std::string generate_format(const std::string& text, const std::string& color = "", int font_size = 0, bool bold = false, bool italic = false);
	static std::string generate_format(int val, const std::string& color = "", int font_size = 0, bool bold = false, bool italic = false);

	static bool is_require_escape_unicode(wchar_t ch);
	static std::string stuff_escape(const std::string& str);
	static std::string drop_escape(const std::string& str);

	tintegrate(const std::string& src, int maximum_width, int maximum_height, int default_font_size, const SDL_Color& default_font_color, bool editable = false);
	~tintegrate();

	int get_src_text_size(int pos, const std::string& text) const;
	int get_src_text_size2(int pos, const std::string& text) const;
	int forward_pos(int start, const std::string& key) const;

	/// Return the ID that is crossreferenced at the (screen)
	/// coordinates x, y. If no cross-reference is there, return the
	/// empty string.
	std::string ref_at(const int x, const int y);
	SDL_Rect holden_rect(int x, int y) const;

	SDL_Rect editable_at(const int x, const int y) const;
	SDL_Rect editable_at2(int pos) const;
	SDL_Rect calculate_cursor(int src_pos) const;
	int calculate_src_pos(int x, int y) const;
	bool at_end(int x, int y) const;
	std::string before_str(int x, int y) const;
	std::string handle_selection(int startx, int starty, int endx, int endy, SDL_Rect* new_pt) const;
	std::string handle_char(bool del, int startx, int starty, const bool backspace, SDL_Rect& new_pt) const;
	std::string insert_str(bool text, int x, int y, const std::string& str, SDL_Rect& new_pt) const;
	SDL_Rect key_arrow(int x, int y, bool up) const;
	
	std::string substr_from_src(int from, int size = 1048576) const;

	SDL_Rect get_size() const;
	surface get_surface();
	void animated_undraw(display& disp, surface& screen, const SDL_Rect& clip_rect);
	void animated_draw(display& disp, surface& screen, const SDL_Rect& clip_rect);
	void draw_bubble_shape(surface& canvas, game_logic::map_formula_callable& variables);

	int get_maximum_width() const { return maximum_width_; }
	int get_maximum_height() const { return maximum_height_; }

	bool exist_anim() const { return exist_anim_; }
	bool exist_bubble() const { return !bubble_anims_.empty(); }

	void set_layout_offset(int x, int y) { layout_offset_.x = x; layout_offset_.y = y; }

	bool align_bottom() const { return align_bottom_; }
	void set_align_bottom(bool value) { align_bottom_ = value; }

	/// An item that is displayed in the text area. Contains the surface
	/// that should be blitted along with some other information.
	struct titem {

		titem(surface surface, int tag_pos, int pos, int x, int y, const std::string& text,
			 const std::string& reference_to, int font_size, int style, int src_size,
			 bool floating=false, bool box = false, ALIGNMENT alignment = HERE);

		titem(surface surface, int pos, int x, int y, int src_size, bool floating, bool box, ALIGNMENT);

		titem(const config& cfg, int pos, const SDL_Rect& rect, int src_size, bool floating, bool box, ALIGNMENT);

		bool text_type() const { return !text.empty() || !rect.w; }
		bool line_spacer() const { return !rect.w; }
		int src_end_is_lf(const std::string& src) const;
		char src_char_from_item_offset(const std::string& src, int offset) const;
		int offset_from_item_offset(const std::string& src, int offset, bool oft_is_idx) const;

		// Relative coordinates of this item.
		SDL_Rect rect;

		SDL_Rect holden_rect;

		surface surf;

		config anim;

		int tag_pos;

		int pos;

		// If this item contains text, this will contain that text.
		std::string text;

		// If this item contains a cross-reference, this is the id
		// of the referenced topic.
		std::string ref_to;

		// If this item is text, the size of font
		int font_size;

		// If this item is text, state is text style
		int style;

		// text size in src that it crosspond to. 
		int src_size;

		// If this item is floating, that is, if things should be filled
		// around it.
		bool floating;
		bool box;
		ALIGNMENT align;
	};

	struct tlocator {
		tlocator(const config& cfg, int start, int end = 0x7fffffff)
			: cfg(cfg)
			, start(start)
			, end(end)
			, rect(empty_rect)
		{}

		config cfg;
		int start;
		int end;
		SDL_Rect rect;
		std::vector<gui2::tcanvas::tshape_ptr> shapes;
	};
	void fill_locator_rect(std::vector<tlocator>& locator, bool use_max_width);

private:
	/// Convert a string to an alignment. Throw parse_error if
	/// unsuccessful.
	ALIGNMENT str_to_align(const std::string &s);

	static std::string align_to_string(ALIGNMENT align);

	/// Function object to find an item at the specified coordinates.
	class item_at {
	public:
		item_at(const int x, const int y) : x_(x), y_(y) {}
		bool operator()(const titem&) const;
	private:
		const int x_, y_;
	};

	struct tloc_result {
		tloc_result(int index = -1)
			: index(index)
		{}

		int index;		// hit index of items_. -1: fail
		std::list<titem>::const_iterator it; // it is filled by caller code.
	};
	tloc_result location_from_pixel(int x, int y, bool fail_to_back) const;

	// Create appropriate items from configs. Items will be added to the
	// internal vector. These methods check that the necessary
	// attributes are specified.
	void handle_ref_cfg(int start, const config &cfg);
	void handle_img_cfg(int start, const config &cfg);
	void handle_bold_cfg(int start, const config &cfg);
	void handle_italic_cfg(int start, const config &cfg);
	void handle_header_cfg(int start, const config &cfg);
	void handle_jump_cfg(int start, const config &cfg);
	void handle_format_cfg(int start, const config &cfg);

	/// Add an item with text. If ref_dst is something else than the
	/// empty string, the text item will be underlined to show that it
	/// is a cross-reference. The item will also remember what the
	/// reference points to. If font_size is below zero, the default
	/// will be used.
	void add_text_item(int tag_start, int start, const std::string& text, const SDL_Color& color, const std::string& ref_dst="",
					   bool broken_link = false,
					   int font_size=-1, bool bold=false, bool italic=false);

	/// Add an image item with the specified attributes.
	void add_img_item(int start, const std::string& path, const std::string& alignment, const bool floating,
					  const bool box, const config& cfg);

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
	void add_item(const titem& itm);

	void validate_str(int start, int size, const std::string& text) const;

private:
	std::string src_;
	bool editable_;
	std::list<titem> items_;
	std::list<titem *> last_row_;
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
	bool align_bottom_;
	bool exist_anim_;
	std::map<int, int> anims_;
	std::vector<tlocator> bubble_anims_;
	SDL_Point layout_offset_;
};

extern tintegrate* share_canvas_integrate;

#endif
