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
#ifndef ROSE_HELP_HPP_INCLUDED
#define ROSE_HELP_HPP_INCLUDED

class config;

#include "exceptions.hpp"
#include "sdl_utils.hpp"
#include "SDL_ttf.h"
#include "map.hpp"
#include "animation.hpp"
#include "gui/auxiliary/canvas.hpp"

#include <list>

class display;

namespace help {

/// Thrown when the help system fails to parse something.
struct parse_error : public game::error
{
	parse_error(const std::string& msg);
};

struct section;

/// Parse a text string. Return a vector with the different parts of the
/// text. Each markup item is a separate part while the text between
/// markups are separate parts.
std::map<int, std::string> parse_text(const std::string &text);

SDL_Color string_to_color(const std::string &s);
std::vector<std::string> split_in_width(const std::string &s, const int font_size, const unsigned width, int style = TTF_STYLE_NORMAL);
std::string remove_first_space(const std::string& text);
std::string get_first_word(const std::string &s);

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
extern const std::string unit_prefix;
extern const std::string race_prefix;
extern const int normal_font_size;

extern const config* game_cfg;
extern gamemap* map;
extern bool editor;

std::string escape(const std::string &s);
std::string hidden_symbol(bool hidden = true);
void fill_reserve_sections();
bool is_reserve_section(const std::string& id);
bool is_section_topic(const std::string& id);
std::string form_section_topic(const std::string& id);
std::string extract_section_topic(const std::string& id);

void init_book(const config* game_cfg, gamemap* map, bool editor);
void clear_book();
void generate_contents(const std::string& tag, section& toplevel);
void parse_config_internal(const config *help_cfg, const config *section_cfg, section &sec, int level = 0);

section* find_section(section &sec, const std::string &id);
topic* find_topic(section &sec, const std::string &id);
std::pair<section*, int> find_parent(section& sec, const std::string& id);

bool find_topic2(const std::string& dst);
std::vector<topic> generate_topics(const bool sort_generated, const std::string &generator);
void generate_sections(const config *help_cfg, const std::string &generator, section &sec, int level);

} // End namespace help.

std::string single_digit_image(int digit);
std::string multi_digit_image_str(int integer);
SDL_Point multi_digit_image_size(int integer);


#endif
