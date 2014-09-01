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

#include "exceptions.hpp"
#include "sdl_utils.hpp"
#include "SDL_ttf.h"
#include "map.hpp"

#include <list>

namespace help {

/// Thrown when the help system fails to parse something.
struct parse_error : public game::error
{
	parse_error(const std::string& msg);
};

struct section;

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
	static double screen_ratio;

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

//
// book
//
struct section;

typedef std::vector<section *> section_list;

/// Generate a topic text on the fly.
class topic_generator
{
	unsigned count;
	friend class topic_text;
public:
	topic_generator(): count(1) {}
	virtual std::string operator()() const = 0;
	virtual ~topic_generator() {}
};

class text_topic_generator: public topic_generator {
	std::string text_;
public:
	text_topic_generator(std::string const &t): text_(t) {}
	virtual std::string operator()() const { return text_; }
};

/// The text displayed in a topic. It is generated on the fly with the information
/// contained in generator_.
class topic_text
{
	mutable t_string parsed_text_;
	mutable topic_generator *generator_;
	mutable bool from_textdomain_;
public:
	~topic_text();
	topic_text():
		parsed_text_()
		, generator_(NULL)
		, from_textdomain_(true)
	{
	}

	topic_text(const t_string &t):
		parsed_text_(t)
		, generator_(NULL)
		, from_textdomain_(true)
	{
	}

	explicit topic_text(topic_generator *g):
		parsed_text_()
		, generator_(g)
		, from_textdomain_(false)
	{
	}
	topic_text &operator=(topic_generator *g);
	topic_text(topic_text const &t);

	const topic_generator* generator() const { return generator_; }
	const t_string& parsed_text() const;

	void set_parsed_text(const t_string& str);
	bool operator==(const topic_text& that) const;
	bool operator!=(const topic_text& that) const { return !operator==(that); }
};

/// A topic contains a title, an id and some text.
struct topic
{
	topic() :
		title(),
		id(),
		text()
	{
	}

	topic(const t_string &_title, const std::string &_id) :
		title(_title),
		id(_id),
		text()
	{
	}

	topic(const t_string &_title, const std::string &_id, const t_string &_text)
		: title(_title), id(_id), text(_text) {}
	topic(const t_string &_title, const std::string &_id, topic_generator *g)
		: title(_title), id(_id), text(g) {}
	/// Two topics are equal if their IDs are equal.
	bool operator==(const topic &) const;
	bool operator!=(const topic &t) const { return !operator==(t); }

	void generate(std::stringstream& strstr, const std::string& prefix, const std::string& default_textdomain, section& toplevel) const;
	void generate_pot(std::set<std::string>& msgids, const std::string& default_textdomain) const;

	std::string id;
	t_string title;
	mutable topic_text text;
};

typedef std::list<topic> topic_list;

/// A section contains topics and sections along with title and ID.
struct section {
	section() :
		title(""),
		id(""),
		topics(),
		sections(),
		level()
	{
	}

	section(const section&);
	section& operator=(const section&);
	~section();
	/// Two sections are equal if their IDs are equal.
	bool operator==(const section &) const;
	bool operator!=(const section &that) const { return !operator==(that); }

	/// Allocate memory for and add the section.
	void add_section(const section& s, int pos = -2);
	void erase_section(const section& s);

	void add_topic(const topic& t, int pos = -2);
	void erase_topic(const topic& t);

	void move(const topic& t, bool up);
	void move(const section& s, bool up);

	void clear();
	bool is_null() const { return id.empty(); }
	void generate(std::stringstream& strstr, const std::string& prefix, const std::string& default_textdomain, section& toplevel) const;
	void generate_pot(std::set<std::string>& msgids, const std::string& default_textdomain) const;

	std::string id;
	t_string title;
	topic_list topics;
	section_list sections;
	int level;
};

/// To be used as a function object to locate sections and topics
/// with a specified ID.
class has_id
{
public:
	has_id(const std::string &id) : id_(id) {}
	bool operator()(const topic &t) { return t.id == id_; }
	bool operator()(const section &s) { return s.id == id_; }
	bool operator()(const section *s) { return s != NULL && s->id == id_; }
private:
	const std::string id_;
};

/// To be used as a function object when sorting topic lists on the title.
class title_less
{
public:
	bool operator()(const topic &t1, const topic &t2) {
            return strcoll(t1.title.c_str(), t2.title.c_str()) < 0; }
};

/// To be used as a function object when sorting section lists on the title.
class section_less
{
public:
	bool operator()(const section* s1, const section* s2) {
            return strcoll(s1->title.c_str(), s2->title.c_str()) < 0; }
};

class string_less
{
public:
	bool operator() (const std::string &s1, const std::string &s2) const {
		return strcoll(s1.c_str(), s2.c_str()) < 0;
	}
};

struct delete_section
{
	void operator()(section *s) { delete s; }
};

struct create_section
{
	section *operator()(const section *s) { return new section(*s); }
	section *operator()(const section &s) { return new section(s); }
};

extern const config dummy_cfg;
extern const std::string section_topic_prefix;

void fill_reserve_sections();
bool is_reserve_section(const std::string& id);
bool is_section_topic(const std::string& id);
std::string form_section_topic(const std::string& id);
std::string extract_section_topic(const std::string& id);

void init_book(const config* game_cfg, gamemap* map, bool editor);
void clear_book();
void generate_contents(const std::string& tag, section& toplevel);

section* find_section(section &sec, const std::string &id);
topic* find_topic(section &sec, const std::string &id);
std::pair<section*, int> find_parent(section& sec, const std::string& id);

bool find_topic2(const std::string& dst);

} // End namespace help.

std::string single_digit_image(int digit);
std::string multi_digit_image_str(int integer);
SDL_Point multi_digit_image_size(int integer);


#endif
