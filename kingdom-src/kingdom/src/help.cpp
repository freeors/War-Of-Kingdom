/* $Id: help.cpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
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

/**
 * @file
 * Routines for showing the help-dialog.
 */

#include "global.hpp"

#include "help.hpp"

#include "about.hpp"
#include "exceptions.hpp"
#include "gettext.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "marked-up_text.hpp"
#include "log.hpp"
#include "sound.hpp"
#include "unit.hpp"
#include "wml_separators.hpp"
#include "serialization/parser.hpp"
#include "font.hpp"
#include "wml_exception.hpp"

#include <boost/foreach.hpp>
#include <queue>

namespace help {

/// Thrown when the help system fails to parse something.
parse_error::parse_error(const std::string& msg) : game::error(msg) {}

/// Parse a text string. Return a vector with the different parts of the
/// text. Each markup item is a separate part while the text between
/// markups are separate parts.
std::vector<std::string> parse_text(const std::string &text);

/// Convert the contents to wml attributes, surrounded within
/// [element_name]...[/element_name]. Return the resulting WML.
static std::string convert_to_wml(const std::string &element_name, const std::string &contents);

/// Return the color the string represents. Return font::NORMAL_COLOR if
/// the string is empty or can't be matched against any other color.
SDL_Color string_to_color(const std::string &s);

/// Make a best effort to word wrap s. All parts are less than width.
// std::vector<std::string> split_in_width(const std::string &s, const int font_size, const unsigned width);

std::string remove_first_space(const std::string& text);

/// Return the first word in s, not removing any spaces in the start of
/// it.
std::string get_first_word(const std::string &s);

std::vector<std::string> parse_text(const std::string &text)
{
	std::vector<std::string> res;
	bool last_char_escape = false;
	const char escape_char = '\\';
	std::stringstream ss;
	size_t pos;
	enum { ELEMENT_NAME, OTHER } state = OTHER;
	for (pos = 0; pos < text.size(); ++pos) {
		const char c = text[pos];
		if (c == escape_char && !last_char_escape) {
			last_char_escape = true;
		}
		else {
			if (state == OTHER) {
				if (c == '<') {
					if (last_char_escape) {
						ss << c;
					}
					else {
						res.push_back(ss.str());
						ss.str("");
						state = ELEMENT_NAME;
					}
				}
				else {
					ss << c;
				}
			}
			else if (state == ELEMENT_NAME) {
				if (c == '/') {
					std::string msg = "Erroneous / in element name.";
					throw parse_error(msg);
				}
				else if (c == '>') {
					// End of this name.
					std::stringstream s;
					const std::string element_name = ss.str();
					ss.str("");
					s << "</" << element_name << ">";
					const std::string end_element_name = s.str();
					size_t end_pos = text.find(end_element_name, pos);
					if (end_pos == std::string::npos) {
						std::stringstream msg;
						msg << "Unterminated element: " << element_name;
						throw parse_error(msg.str());
					}
					s.str("");
					const std::string contents = text.substr(pos + 1, end_pos - pos - 1);
					const std::string element = convert_to_wml(element_name, contents);
					res.push_back(element);
					pos = end_pos + end_element_name.size() - 1;
					state = OTHER;
				}
				else {
					ss << c;
				}
			}
			last_char_escape = false;
		}
	}
	if (state == ELEMENT_NAME) {
		std::stringstream msg;
		msg << "Element '" << ss.str() << "' continues through end of string.";
		throw parse_error(msg.str());
	}
	if (ss.str() != "") {
		// Add the last string.
		res.push_back(ss.str());
	}
	return res;
}

std::string convert_to_wml(const std::string &element_name, const std::string &contents)
{
	std::stringstream ss;
	bool in_quotes = false;
	bool last_char_escape = false;
	const char escape_char = '\\';
	std::vector<std::string> attributes;
	// Find the different attributes.
	// No checks are made for the equal sign or something like that.
	// Attributes are just separated by spaces or newlines.
	// Attributes that contain spaces must be in single quotes.
	for (size_t pos = 0; pos < contents.size(); ++pos) {
		const char c = contents[pos];
		if (c == escape_char && !last_char_escape) {
			last_char_escape = true;
		}
		else {
			if (c == '\'' && !last_char_escape) {
				ss << '"';
				in_quotes = !in_quotes;
			}
			else if ((c == ' ' || c == '\n') && !last_char_escape && !in_quotes) {
				// Space or newline, end of attribute.
				attributes.push_back(ss.str());
				ss.str("");
			}
			else {
				ss << c;
			}
			last_char_escape = false;
		}
	}
	if (in_quotes) {
		std::stringstream msg;
		msg << "Unterminated single quote after: '" << ss.str() << "'";
		throw parse_error(msg.str());
	}
	if (ss.str() != "") {
		attributes.push_back(ss.str());
	}
	ss.str("");
	// Create the WML.
	ss << "[" << element_name << "]\n";
	for (std::vector<std::string>::const_iterator it = attributes.begin();
		 it != attributes.end(); ++it) {
		ss << *it << "\n";
	}
	ss << "[/" << element_name << "]\n";
	return ss.str();
}

SDL_Color string_to_color(const std::string &cmp_str)
{
	if (cmp_str == "green") {
		return font::GOOD_COLOR;
	}
	if (cmp_str == "red") {
		return font::BAD_COLOR;
	}
	if (cmp_str == "black") {
		return font::BLACK_COLOR;
	}
	if (cmp_str == "yellow") {
		return font::YELLOW_COLOR;
	}
	if (cmp_str == "white") {
		return font::BIGMAP_COLOR;
	}
	if (cmp_str == "blue") {
		return font::BLUE_COLOR;
	}
	return font::NORMAL_COLOR;
}

std::vector<std::string> split_in_width(const std::string &s, const int font_size,
		const unsigned width, int style)
{
	std::vector<std::string> res;
	try {
	// [see remark#24]
	size_t pos = s.find("\n");
	if (pos == std::string::npos) {
		unsigned unsplit_width = font::line_size(s, font_size, style).w;
		if (unsplit_width <= width) {
			res.push_back(s);
			return res;
		}
	} else {
		const std::string line_str = s.substr(0, pos);
		unsigned unsplit_width = font::line_size(line_str, font_size, style).w;
		if (unsplit_width <= width) {
			res.push_back(line_str);
			res.push_back(s.substr(pos));
			return res;
		}
	}
	
	const std::string& first_line = font::word_wrap_text(s, font_size, width, -1, 1, true);
	// [see remark#21]
	if (!first_line.empty()) {
		res.push_back(first_line);
		if(s.size() > first_line.size()) {
			res.push_back(s.substr(first_line.size()));
		}
	} else {
		res.push_back(s);
	}
	}
	catch (utils::invalid_utf8_exception&)
	{
		throw parse_error (_("corrupted original file"));
	}

	return res;
}

std::string remove_first_space(const std::string& text)
{
	if (text.length() > 0 && text[0] == ' ') {
		return text.substr(1);
	}
	return text;
}

std::string get_first_word(const std::string &s)
{
	size_t first_word_start = s.find_first_not_of(' ');
	if (first_word_start == std::string::npos) {
		return s;
	}
	size_t first_word_end = s.find_first_of(" \n", first_word_start);
	if( first_word_end == first_word_start ) {
		// This word is '\n'.
		first_word_end = first_word_start+1;
	}

	//if no gap(' ' or '\n') found, test if it is CJK character
	std::string re = s.substr(0, first_word_end);

	utils::utf8_iterator ch(re);
	if (ch == utils::utf8_iterator::end(re))
		return re;

	wchar_t firstchar = *ch;
	if (font::is_cjk_char(firstchar)) {
		// [see remark#23]
		std::string re1 = utils::wchar_to_string(firstchar);
		wchar_t previous = firstchar;
		for (++ ch; ch != utils::utf8_iterator::end(re); ++ch) {
			if (font::can_break(previous, *ch)) {
				break;
			}
			re1.append(ch.substr().first, ch.substr().second);
			previous = *ch;
		}
		return re1;
	}
	return re;
}

//
// book part
//

// All sections and topics not referenced from the default toplevel.
section hidden_sections;

const config* game_cfg = NULL;
gamemap* map = NULL;
bool editor = false;

void init_book(const config* _game_cfg, gamemap* _map, bool _editor)
{
	game_cfg = _game_cfg;
	map = _map;
	editor = _editor;
}

void clear_book()
{
	game_cfg = NULL;
	map = NULL;
	hidden_sections.clear();
}

const config dummy_cfg;
const int max_section_level = 15;
// The topic to open by default when opening the help dialog.
const std::string default_show_topic = "introduction_topic";
const std::string unknown_unit_topic = ".unknown_unit";
const std::string unit_prefix = "unit_";
const std::string race_prefix = "race_";
const std::string faction_prefix = "faction_";
const std::string section_topic_prefix = "..";

const int normal_font_size = font::SIZE_SMALL;

/// Recursive function used by parse_config.
void parse_config_internal(const config *help_cfg, const config *section_cfg, section &sec, int level = 0);

bool is_section_topic(const std::string& id)
{
	return id.find(section_topic_prefix) == 0;
}

std::string form_section_topic(const std::string& id)
{
	return section_topic_prefix + id;
}

std::string extract_section_topic(const std::string& id)
{
	return id.substr(section_topic_prefix.size());
}

/// Prepend all chars with meaning inside attributes with a backslash.
std::string escape(const std::string &s)
{
	return utils::escape(s, "'\\");
}

// id starting with '.' are hidden
static std::string hidden_symbol(bool hidden = true) {
	return (hidden ? "." : "");
}

static bool is_visible_id(const std::string &id) {
	return (id.empty() || id[0] != '.');
}

/// Return true if the id is valid for user defined topics and
/// sections. Some IDs are special, such as toplevel and may not be
/// be defined in the config.
static bool is_valid_id(const std::string &id) {
	if (id == "toplevel") {
		return false;
	}
	if (id.find(unit_prefix) == 0 || id.find(hidden_symbol() + unit_prefix) == 0) {
		return false;
	}
	if (id.find("ability_") == 0) {
		return false;
	}
	if (id.find("weaponspecial_") == 0) {
		return false;
	}
	if (id == "hidden") {
		return false;
	}
	return true;
}

/// Class to be used as a function object when generating the about
/// text. Translate the about dialog formatting to format suitable
/// for the help dialog.
class about_text_formatter {
public:
	std::string operator()(const std::string &s) {
		if (s.empty()) return s;
		// Format + as headers, and the rest as normal text.
		if (s[0] == '+')
			return " \n<header>text='" + escape(s.substr(1)) + "'</header>";
		if (s[0] == '-')
			return s.substr(1);
		return s;
	}
};

class treserve_section
{
public:
	treserve_section(const std::string& id, const std::string& generator)
		: id(id)
		, generator_str(generator)
	{}

	std::string id;
	std::string generator_str;
};

std::map<std::string, treserve_section> reserve_sections;
void fill_reserve_sections()
{
	if (!reserve_sections.empty()) {
		return;
	}
	reserve_sections.insert(std::make_pair("units", treserve_section("units", "sections_generator = races")));
	reserve_sections.insert(std::make_pair("abilities_section", treserve_section("abilities_section", "generator = abilities")));
	reserve_sections.insert(std::make_pair("weapon_specials", treserve_section("weapon_specials", "generator = weapon_specials")));
	reserve_sections.insert(std::make_pair("factions_section", treserve_section("factions_section", "generator = factions")));
	reserve_sections.insert(std::make_pair("about", treserve_section("about", "generator = about")));
}

bool is_reserve_section(const std::string& id)
{
	return reserve_sections.count(id) > 0;
}

topic_text::~topic_text()
{
	if (generator_ && --generator_->count == 0)
		delete generator_;
}

topic_text::topic_text(topic_text const &t): 
	parsed_text_(t.parsed_text_)
	, generator_(t.generator_)
	, from_textdomain_(t.from_textdomain_)
{
	if (generator_)
		++generator_->count;
}

topic_text &topic_text::operator=(topic_generator *g)
{
	if (generator_ && --generator_->count == 0)
		delete generator_;
	generator_ = g;
	from_textdomain_ = false;
	return *this;
}

const t_string& topic_text::parsed_text() const
{
	if (generator_) {
		parsed_text_ = (*generator_)();
		if (--generator_->count == 0)
			delete generator_;
		generator_ = NULL;
	}
	return parsed_text_;
}

void topic_text::set_parsed_text(const t_string& str)
{
	VALIDATE(!generator_, "generator_ must be nul!");

	parsed_text_ = str;
}

bool topic_text::operator==(const topic_text& that) const
{
	if (generator_ || that.generator_) {
		// generate automaticly, no one standard.
		return true;
	}
	return parsed_text_ == that.parsed_text_;
}

bool topic::operator==(const topic &t) const
{
	return t.id == id && t.title == title && t.text == text;
}

void topic::generate(std::stringstream& strstr, const std::string& prefix, const std::string& default_textdomain, section& toplevel) const
{
	strstr << prefix << "[topic]\n";
	strstr << prefix << "\tid = " << id << "\n";
	std::vector<t_string_base::trans_str> trans = title.valuex();
	if (!trans.empty()) {
		if (!trans[0].td.empty() && trans[0].td != default_textdomain) {
			strstr << "#textdomain " << trans[0].td << "\n";
		}
		strstr << prefix << "\ttitle = _ \"" << trans[0].str << "\"\n";
		if (!trans[0].td.empty() && trans[0].td != default_textdomain) {
			strstr << "#textdomain " << default_textdomain << "\n";
		}
	}
	if (!text.parsed_text().empty()) {
		trans = text.parsed_text().valuex();
		if (!trans.empty()) {
			strstr << prefix << "\ttext = _ \"" << trans[0].str << "\"\n";
		}
	}
	if (is_section_topic(id)) {
		const std::string extracted_id = id.substr(section_topic_prefix.size());
		const section* sec = NULL;
		if (!is_reserve_section(extracted_id)) {
			sec = find_section(toplevel, extracted_id);
		}
		strstr << prefix << "\tgenerator = \"contents:";
		if (sec && sec->sections.empty()) {
			strstr << extracted_id;
		} else {
			strstr << "generated";
		}
		strstr << "\"\n";

	} else if (is_reserve_section(id)) {
		std::map<std::string, treserve_section>::const_iterator it = reserve_sections.find(id);
		strstr << prefix << "\t" << it->second.generator_str << "\n";
	}

	strstr << prefix << "[/topic]\n";
}

void generate_pot(std::set<std::string>& msgids, const t_string& tstr, const std::string& default_textdomain)
{
	if (tstr.str().empty()) {
		return;
	}
	std::vector<t_string_base::trans_str> trans = tstr.valuex();
	if (!trans.empty()) {
		if (trans[0].td.empty() || trans[0].td == default_textdomain) {
			msgids.insert(trans[0].str);
		}
	}
	return;
}

void topic::generate_pot(std::set<std::string>& msgids, const std::string& default_textdomain) const
{
	if (!is_section_topic(id)) {
		help::generate_pot(msgids, title, default_textdomain);
	}
	if (!text.generator()) {
		help::generate_pot(msgids, text.parsed_text(), default_textdomain);
	}
}

section::~section()
{
	std::for_each(sections.begin(), sections.end(), delete_section());
}

section::section(const section &sec) :
	title(sec.title),
	id(sec.id),
	topics(sec.topics),
	sections(),
	level(sec.level)
{
	std::transform(sec.sections.begin(), sec.sections.end(),
				   std::back_inserter(sections), create_section());
}

section& section::operator=(const section &sec)
{
	title = sec.title;
	id = sec.id;
	level = sec.level;
	std::copy(sec.topics.begin(), sec.topics.end(), std::back_inserter(topics));
	std::transform(sec.sections.begin(), sec.sections.end(),
				   std::back_inserter(sections), create_section());
	return *this;
}

bool section::operator==(const section &sec) const
{
	if (sec.id != id || sec.title != title) {
		return false;
	}
	if (sec.topics != topics) {
		return false;
	}
	if (sec.sections.size() != sections.size()) {
		return false;
	}
	for (size_t n = 0; n < sections.size(); n ++) {
		if (*sec.sections[n] != *sections[n]) {
			return false;
		}
	}
	return true;
}

void section::add_section(const section& s, int pos)
{
	// after pos
	pos ++;
	if (pos < 0 || pos >= (int)sections.size()) {
		sections.push_back(new section(s));
	} else {
		section_list::iterator it = sections.begin();
		std::advance(it, pos);
		sections.insert(it, new section(s));
	}
}

void section::erase_section(const section& s)
{
	section_list::iterator it = std::find(sections.begin(), sections.end(), &s);
	delete *it;
	sections.erase(it);
}

void section::add_topic(const topic& t, int pos)
{
	// after pos
	pos ++;
	if (pos < 0 || pos >= (int)topics.size()) {
		topics.push_back(t);
	} else {
		topic_list::iterator it = topics.begin();
		std::advance(it, pos);
		topics.insert(it, t);
	}
}

void section::erase_topic(const topic& t)
{
	topic_list::iterator it = std::find_if(topics.begin(), topics.end(), has_id(t.id));
	topics.erase(it);
}

void section::move(const topic& t, bool up)
{
	topic_list::iterator it = std::find_if(topics.begin(), topics.end(), has_id(t.id));
	int n = std::distance(topics.begin(), it);
	topic_list::iterator it2;
	if (up) {
		topic t = *it;
		topics.erase(it);
		it = topics.begin();
		std::advance(it, n - 1);
		topics.insert(it, t);
	} else {
		topic_list::iterator it2 = topics.begin();
		std::advance(it2, n + 1);
		topic t = *it2;
		topics.erase(it2);
		topics.insert(it, t);
	}
}

void section::move(const section& s, bool up)
{
	section_list::iterator it = std::find_if(sections.begin(), sections.end(), has_id(s.id));
	int n = std::distance(sections.begin(), it);
	section_list::iterator it2;
	if (up) {
		section* s = *it;
		sections.erase(it);
		it = sections.begin();
		std::advance(it, n - 1);
		sections.insert(it, s);
	} else {
		section_list::iterator it2 = sections.begin();
		std::advance(it2, n + 1);
		section* s = *it2;
		sections.erase(it2);
		sections.insert(it, s);
	}
}

void section::clear()
{
	id.clear();
	topics.clear();
	std::for_each(sections.begin(), sections.end(), delete_section());
	sections.clear();
}

void section::generate(std::stringstream& strstr, const std::string& prefix, const std::string& default_textdomain, section& toplevel) const
{
	strstr << prefix << "[" << (!level? "toplevel": "section") << "]\n";
	if (level) {
		strstr << prefix << "\tid = " << id << "\n";
		std::vector<t_string_base::trans_str> trans = title.valuex();
		if (!trans.empty()) {
			if (!trans[0].td.empty() && trans[0].td != default_textdomain) {
				strstr << "#textdomain " << trans[0].td << "\n";
			}
			strstr << prefix << "\ttitle = _ \"" << trans[0].str << "\"\n";
			if (!trans[0].td.empty() && trans[0].td != default_textdomain) {
				strstr << "#textdomain " << default_textdomain << "\n";
			}
		}
	}
	if (!sections.empty()) {
		strstr << prefix << "\tsections = ";
		for (section_list::const_iterator it = sections.begin(); it != sections.end(); ++ it) {
			const section& sec = **it;
			if (it != sections.begin()) {
				strstr << ", ";
			}
			strstr << sec.id;
		}
		strstr << "\n";
	}
	if (!topics.empty()) {
		strstr << prefix << "\ttopics = ";
		for (topic_list::const_iterator it = topics.begin(); it != topics.end(); ++ it) {
			const topic& t = *it;
			if (it != topics.begin()) {
				strstr << ", ";
			}
			strstr << t.id;
		}
		strstr << "\n";
	}
	std::map<std::string, treserve_section>::const_iterator it = reserve_sections.find(id);
	if (it != reserve_sections.end()) {
		strstr << prefix << "\t" << it->second.generator_str << "\n";
	}
	strstr << prefix << "[/" << (!level? "toplevel": "section") << "]\n";

	for (section_list::const_iterator it = sections.begin(); it != sections.end(); ++ it) {
		strstr << "\n";
		const section& sec = **it;
		sec.generate(strstr, prefix, default_textdomain, toplevel);
	}

	for (topic_list::const_iterator it = topics.begin(); it != topics.end(); ++ it) {
		strstr << "\n";
		const topic& t = *it;
		t.generate(strstr, prefix, default_textdomain, toplevel);
	}
}

void section::generate_pot(std::set<std::string>& msgids, const std::string& default_textdomain) const
{
	if (!sections.empty()) {
		for (section_list::const_iterator it = sections.begin(); it != sections.end(); ++ it) {
			const section& sec = **it;
			help::generate_pot(msgids, sec.title, default_textdomain);
		}
	}

	for (section_list::const_iterator it = sections.begin(); it != sections.end(); ++ it) {
		const section& sec = **it;
		sec.generate_pot(msgids, default_textdomain);
	}

	for (topic_list::const_iterator it = topics.begin(); it != topics.end(); ++ it) {
		const topic& t = *it;
		t.generate_pot(msgids, default_textdomain);
	}
}

// Helpers for making generation of topics easier.

std::string make_link(const std::string& text, const std::string& dst)
	{
		// some sorting done on list of links may rely on the fact that text is first
		return "<ref>text='" + escape(text) + "' dst='" + escape(dst) + "'</ref>";
	}

std::string jump_to(const unsigned pos)
	{
		std::stringstream ss;
		ss << "<jump>to=" << pos << "</jump>";
		return ss.str();
	}

std::string jump(const unsigned amount)
	{
		std::stringstream ss;
		ss << "<jump>amount=" << amount << "</jump>";
		return ss.str();
	}

std::string bold(const std::string &s)
	{
		std::stringstream ss;
		ss << "<bold>text='" << escape(s) << "'</bold>";
		return ss.str();
	}

	typedef std::vector<std::vector<std::pair<std::string, unsigned int > > > table_spec;
	// Create a table using the table specs. Return markup with jumps
	// that create a table. The table spec contains a vector with
	// vectors with pairs. The pairs are the markup string that should
	// be in a cell, and the width of that cell.
std::string generate_table(const table_spec &tab, const unsigned int spacing=font::relative_size(20))
  {
		table_spec::const_iterator row_it;
		std::vector<std::pair<std::string, unsigned> >::const_iterator col_it;
		unsigned int num_cols = 0;
		for (row_it = tab.begin(); row_it != tab.end(); ++row_it) {
			if (row_it->size() > num_cols) {
				num_cols = row_it->size();
			}
		}
		std::vector<unsigned int> col_widths(num_cols, 0);
		// Calculate the width of all columns, including spacing.
		for (row_it = tab.begin(); row_it != tab.end(); ++row_it) {
			unsigned int col = 0;
			for (col_it = row_it->begin(); col_it != row_it->end(); ++col_it) {
				if (col_widths[col] < col_it->second + spacing) {
					col_widths[col] = col_it->second + spacing;
				}
				++col;
			}
		}
		std::vector<unsigned int> col_starts(num_cols);
		// Calculate the starting positions of all columns
		for (unsigned int i = 0; i < num_cols; ++i) {
			unsigned int this_col_start = 0;
			for (unsigned int j = 0; j < i; ++j) {
				this_col_start += col_widths[j];
			}
			col_starts[i] = this_col_start;
		}
		std::stringstream ss;
		for (row_it = tab.begin(); row_it != tab.end(); ++row_it) {
			unsigned int col = 0;
			for (col_it = row_it->begin(); col_it != row_it->end(); ++col_it) {
				ss << jump_to(col_starts[col]) << col_it->first;
				++col;
			}
			ss << "\n";
		}
		return ss.str();
	}

	// Return the width for the image with filename.
unsigned image_width(const std::string &filename)
	{
		image::locator loc(filename);
		surface surf(image::get_image(loc));
		if (surf != NULL) {
			return surf->w;
		}
		return 0;
	}

void push_tab_pair(std::vector<std::pair<std::string, unsigned int> > &v, const std::string &s)
	{
		v.push_back(std::make_pair(s, font::line_width(s, normal_font_size)));
	}

std::string make_unit_link(const std::string& type_id)
{
	std::string link;

	const unit_type *type = unit_types.find(type_id);
	if (!type) {
		std::cerr << "Unknown unit type : " << type_id << "\n";
		// don't return an hyperlink (no page)
		// instead show the id (as hint)
		link = type_id;
	} else if (!type->hide_help()) {
		std::string name = type->type_name();
		std::string ref_id;
		ref_id = unit_prefix + type->id();
		link =  make_link(name, ref_id);
	} // if hide_help then link is an empty string

	return link;
}

std::vector<std::string> make_unit_links_list(const std::vector<std::string>& type_id_list, bool ordered)
{
	std::vector<std::string> links_list;
	BOOST_FOREACH (const std::string &type_id, type_id_list) {
		std::string unit_link = make_unit_link(type_id);
		if (!unit_link.empty())
			links_list.push_back(unit_link);
	}

	if (ordered)
		std::sort(links_list.begin(), links_list.end());

	return links_list;
}

std::vector<topic> generate_weapon_special_topics(const bool sort_generated)
{
	std::vector<topic> topics;

	std::map<t_string, std::string> special_description;
	std::map<t_string, std::set<std::string, string_less> > special_units;

	BOOST_FOREACH (const unit_type_data::unit_type_map::value_type &i, unit_types.types())
	{
		const unit_type &type = i.second;
		// Only show the weapon special if we find it on a unit that
		// detailed description should be shown about.
		std::vector<attack_type> attacks = type.attacks();
		for (std::vector<attack_type>::const_iterator it = attacks.begin();
					it != attacks.end(); ++it) {
#if defined(_KINGDOM_EXE) || !defined(_WIN32)
			std::vector<t_string> specials = (*it).special_tooltips(true);
#else
			std::vector<t_string> specials;
#endif
			for (std::vector<t_string>::iterator sp_it = specials.begin();
					sp_it != specials.end() && sp_it+1 != specials.end(); sp_it+=2)
			{
				if (special_description.find(*sp_it) == special_description.end()) {
					std::string description = *(sp_it+1);
					const size_t colon_pos = description.find(':');
					if (colon_pos != std::string::npos) {
						// Remove the first colon and the following newline.
						description.erase(0, colon_pos + 2);
					}
					special_description[*sp_it] = description;
				}

				if (!type.hide_help()) {
					//add a link in the list of units having this special
					std::string type_name = type.type_name();
					std::string ref_id = unit_prefix + type.id();
					//we put the translated name at the beginning of the hyperlink,
					//so the automatic alphabetic sorting of std::set can use it
					std::string link =  "<ref>text='" + escape(type_name) + "' dst='" + escape(ref_id) + "'</ref>";
					special_units[*sp_it].insert(link);
				}
			}
		}
	}

	for (std::map<t_string, std::string>::iterator s = special_description.begin();
			s != special_description.end(); ++s) {
		// use untranslated name to have universal topic id
		std::string id = "weaponspecial_" + s->first.base_str();
		std::stringstream text;
		text << s->second;
		text << "\n\n" << _("<header>text='Units having this special attack'</header>") << "\n";
		std::set<std::string, string_less> &units = special_units[s->first];
		for (std::set<std::string, string_less>::iterator u = units.begin(); u != units.end(); ++u) {
			text << (*u) << "\n";
		}

		topics.push_back( topic(s->first, id, text.str()) );
	}

	if (sort_generated)
		std::sort(topics.begin(), topics.end(), title_less());
	return topics;
}

std::vector<topic> generate_ability_topics(const bool sort_generated)
{
	std::vector<topic> topics;
	std::map<t_string, std::string> ability_description;
	std::map<t_string, std::set<std::string, string_less> > ability_units;
	// Look through all the unit types, check if a unit of this type
	// should have a full description, if so, add this units abilities
	// for display. We do not want to show abilities that the user has
	// not encountered yet.
	BOOST_FOREACH (const unit_type_data::unit_type_map::value_type &i, unit_types.types())
	{
		const unit_type &type = i.second;

		std::vector<t_string> const* abil_vecs[2];
		abil_vecs[0] = &type.abilities();

		std::vector<std::string> const* desc_vecs[2];
		desc_vecs[0] = &type.ability_tooltips();

		for(int i=0; i<1; ++i) {
			std::vector<t_string> const& abil_vec = *abil_vecs[i];
			std::vector<std::string> const& desc_vec = *desc_vecs[i];
			for(size_t j=0; j < abil_vec.size(); ++j) {
				t_string const& abil_name = abil_vec[j];
				if (ability_description.find(abil_name) == ability_description.end()) {
					//new ability, generate a descripion
					if(j >= desc_vec.size()) {
						ability_description[abil_name] = "";
					} else {
						std::string const& abil_desc = desc_vec[j];
						const size_t colon_pos = abil_desc.find(':');
						if(colon_pos != std::string::npos && colon_pos + 1 < abil_desc.length()) {
							// Remove the first colon and the following newline.
							ability_description[abil_name] = abil_desc.substr(colon_pos + 2);
						} else {
							ability_description[abil_name] = abil_desc;
						}
					}
				}

				if (!type.hide_help()) {
					//add a link in the list of units having this ability
					std::string type_name = type.type_name();
					std::string ref_id = unit_prefix +  type.id();
					//we put the translated name at the beginning of the hyperlink,
					//so the automatic alphabetic sorting of std::set can use it
					std::string link =  "<ref>text='" + escape(type_name) + "' dst='" + escape(ref_id) + "'</ref>";
					ability_units[abil_name].insert(link);
				}
			}
		}
	}

	for (std::map<t_string, std::string>::iterator a = ability_description.begin(); a != ability_description.end(); ++a) {
		// we generate topic's id using the untranslated version of the ability's name
		std::string id = "ability_" + a->first.base_str();
		std::stringstream text;
		text << a->second;  //description
		text << "\n\n" << _("<header>text='Units having this ability'</header>") << "\n";
		std::set<std::string, string_less> &units = ability_units[a->first];
		for (std::set<std::string, string_less>::iterator u = units.begin(); u != units.end(); ++u) {
			text << (*u) << "\n";
		}

		topics.push_back( topic(a->first, id, text.str()) );
	}

	if (sort_generated)
		std::sort(topics.begin(), topics.end(), title_less());
	return topics;
}

std::vector<topic> generate_faction_topics(const bool sort_generated)
{
	std::vector<topic> topics;
	const config& era = game_cfg->child("era");
	if (era) {
		std::vector<std::string> faction_links;
		BOOST_FOREACH (const config &f, era.child_range("multiplayer_side")) {
			const std::string& id = f["id"];
			if (id == "Random")
				continue;

			std::stringstream text;

			const std::string& description = f["description"];
			if (!description.empty()) {
				text << description << "\n";
				text << "\n";
			}

			text << "<header>text='" << _("Leaders:") << "'</header>" << "\n";
			const std::vector<std::string> leaders =
					make_unit_links_list( utils::split(f["leader"]), true );
			BOOST_FOREACH (const std::string &link, leaders) {
				text << link << "\n";
			}

			text << "\n";

			text << "<header>text='" << _("Recruits:") << "'</header>" << "\n";
			const std::vector<std::string> recruits =
					make_unit_links_list( utils::split(f["recruit"]), true );
			BOOST_FOREACH (const std::string &link, recruits) {
				text << link << "\n";
			}

			const std::string name = f["name"];
			const std::string ref_id = faction_prefix + id;
			topics.push_back( topic(name, ref_id, text.str()) );
			faction_links.push_back(make_link(name, ref_id));
		}

		std::stringstream text;
		text << "<header>text='" << _("Era:") << " " << era["name"] << "'</header>" << "\n";
		text << "\n";
		const std::string& description = era["description"];
		if (!description.empty()) {
			text << description << "\n";
			text << "\n";
		}

		text << "<header>text='" << _("Factions:") << "'</header>" << "\n";

		std::sort(faction_links.begin(), faction_links.end());
		BOOST_FOREACH (const std::string &link, faction_links) {
			text << link << "\n";
		}

		topics.push_back( topic(_("Factions"), "..factions_section", text.str()) );
	} else {
		topics.push_back( topic( _("Factions"), "..factions_section",
			_("Factions are only used in multiplayer")) );
	}

	if (sort_generated)
		std::sort(topics.begin(), topics.end(), title_less());
	return topics;
}

class unit_topic_generator: public topic_generator
{
	const unit_type& type_;
	typedef std::pair< std::string, unsigned > item;
	void push_header(std::vector< item > &row, char const *name) const {
		row.push_back(item(bold(name), font::line_width(name, normal_font_size, TTF_STYLE_BOLD)));
	}
public:
	unit_topic_generator(const unit_type &t): type_(t) {}
	virtual std::string operator()() const {
		// this will force the lazy loading to build this unit
		unit_types.find(type_.id(), unit_type::WITHOUT_ANIMATIONS);

		std::stringstream ss;
		std::string clear_stringstream;
		const std::string detailed_description = type_.unit_description();
		const unit_type& female_type = type_.get_gender_unit_type(unit_race::FEMALE);
		const unit_type& male_type = type_.get_gender_unit_type(unit_race::MALE);

		// Show the unit's image and its level.
		ss << "<img>src='" << male_type.image() << "~RC(" << male_type.flag_rgb() << ">1)" << "'</img> ";

		if (&female_type != &male_type) {
			ss << "<img>src='" << female_type.image() << "~RC(" << female_type.flag_rgb() << ">1)" << "'</img> ";
		}

		ss << "<format>font_size=" << font::relative_size(11) << " text=' " << escape(_("level"))
		   << " " << type_.level() << "'</format>";

		const std::string male_portrait = "";
		const std::string female_portrait = "";

		if (male_portrait.empty() == false && male_portrait != male_type.image()) {
			ss << "<img>src='" << male_portrait << "' align='right'</img> ";
		}

		if (female_portrait.empty() == false && female_portrait != male_portrait && female_portrait != female_type.image()) {
			ss << "<img>src='" << female_portrait << "' align='right'</img> ";
		}

		ss << "\n";

		// Print cross-references to units that this unit advances from/to.
		// Cross reference to the topics containing information about those units.
		const bool first_reverse_value = true;
		bool reverse = first_reverse_value;
		do {
			std::vector<std::string> adv_units =
				reverse ? type_.advances_from() : type_.advances_to();
			bool first = true;

			BOOST_FOREACH (const std::string &adv, adv_units)
			{
				const unit_type *type = unit_types.find(adv);
				if (!type || type->hide_help()) continue;

				if (first) {
					if (reverse)
						ss << _("Advances from: ");
					else
						ss << _("Advances to: ");
					first = false;
				} else
					ss << ", ";

				std::string lang_unit = type->type_name();
				std::string ref_id;
				ref_id = unit_prefix + type->id();
				ss << "<ref>dst='" << escape(ref_id) << "' text='" << escape(lang_unit) << "'</ref>";
			}
			ss << "\n"; //added even if empty, to avoid shifting

			reverse = !reverse; //switch direction
		} while(reverse != first_reverse_value); // don't restart

		// Print the race of the unit, cross-reference it to the
		// respective topic.
		const std::string race_id = type_.race();
		std::string race_name;
		if (const unit_race *r = unit_types.find_race(race_id)) {
			race_name = r->plural_name();
		} else {
			race_name = _ ("race^Miscellaneous");
		}
		ss << _("Race: ");
		ss << "<ref>dst='" << escape("..race_"+race_id) << "' text='" << escape(race_name) << "'</ref>";
		ss << "\n";

		// Print the abilities the units has, cross-reference them
		// to their respective topics.
		if (!type_.abilities().empty()) {
			ss << _("Abilities: ");
			for(std::vector<t_string>::const_iterator ability_it = type_.abilities().begin(),
				 ability_end = type_.abilities().end();
				 ability_it != ability_end; ++ability_it) {
				const std::string ref_id = "ability_" + ability_it->base_str();
				std::string lang_ability = gettext(ability_it->c_str());
				ss << "<ref>dst='" << escape(ref_id) << "' text='" << escape(lang_ability)
				   << "'</ref>";
				if (ability_it + 1 != ability_end)
					ss << ", ";
			}
			ss << "\n";
		}

		ss << "\n";

		// Print some basic information such as HP and movement points.
		ss << _("HP: ") << type_.hitpoints() << jump(30)
		   << _("Moves: ") << type_.movement() << jump(30)
		   << _("Cost: ") << type_.cost() << jump(30)
		   << _("Alignment: ")
		   << "<ref>dst='time_of_day' text='"
		   << type_.alignment_description(type_.alignment(), type_.genders().front())
		   << "'</ref>"
		   << jump(30);
		if (type_.can_advance())
			ss << _("Required XP: ") << type_.experience_needed();

		// Print the detailed description about the unit.
		ss << "\n\n" << detailed_description;

		// Print the different attacks a unit has, if it has any.
		std::vector<attack_type> attacks = type_.attacks();
		if (!attacks.empty()) {
			// Print headers for the table.
			ss << "\n\n<header>text='" << escape(_("unit help^Attacks"))
			   << "'</header>\n\n";
			table_spec table;

			std::vector<item> first_row;
			// Dummy element, icons are below.
			first_row.push_back(item("", 0));
			push_header(first_row, _("unit help^Name"));
			push_header(first_row, _("Type"));
			push_header(first_row, _("Strikes"));
			push_header(first_row, _("Range"));
			push_header(first_row, _("Special"));
			table.push_back(first_row);
			// Print information about every attack.
			for(std::vector<attack_type>::const_iterator attack_it = attacks.begin(),
				 attack_end = attacks.end();
				 attack_it != attack_end; ++attack_it) {
				std::string lang_weapon = attack_it->name();
				std::string lang_type = gettext(attack_it->type().c_str());
				std::vector<item> row;
				std::stringstream attack_ss;
				attack_ss << "<img>src='" << (*attack_it).icon() << "'</img>";
				row.push_back(std::make_pair(attack_ss.str(),
							     image_width(attack_it->icon())));
				push_tab_pair(row, lang_weapon);
				push_tab_pair(row, lang_type);
				attack_ss.str(clear_stringstream);
				attack_ss << attack_it->damage() << '-' << attack_it->num_attacks() << " " << attack_it->accuracy_parry_description();
				push_tab_pair(row, attack_ss.str());
				attack_ss.str(clear_stringstream);
				push_tab_pair(row, _((*attack_it).range().c_str()));
				// Show this attack's special, if it has any. Cross
				// reference it to the section describing the
				// special.
#if defined(_KINGDOM_EXE) || !defined(_WIN32)
				std::vector<t_string> specials = attack_it->special_tooltips(true);
#else
				std::vector<t_string> specials;
#endif
				if(!specials.empty())
				{
					std::string lang_special = "";
					std::vector<t_string>::iterator sp_it;
					for (sp_it = specials.begin(); sp_it != specials.end(); ++sp_it) {
						const std::string ref_id = std::string("weaponspecial_")
							+ sp_it->base_str();
						lang_special = (*sp_it);
						attack_ss << "<ref>dst='" << escape(ref_id)
								  << "' text='" << escape(lang_special) << "'</ref>";
						if((sp_it + 1) != specials.end() && (sp_it + 2) != specials.end())
						{
							attack_ss << ", "; //comma placed before next special
						}
						++sp_it; //skip description
					}
					row.push_back(std::make_pair(attack_ss.str(),
						font::line_width(lang_special, normal_font_size)));

				}
				table.push_back(row);
			}
			ss << generate_table(table);
		}

		// Print the resistance table of the unit.
		ss << "\n\n<header>text='" << escape(_("Resistances"))
		   << "'</header>\n\n";
		table_spec resistance_table;
		std::vector<item> first_res_row;
		push_header(first_res_row, _("Attack Type"));
		push_header(first_res_row, _("Resistance"));
		resistance_table.push_back(first_res_row);
		const unit_movement_type &movement_type = type_.movement_type();
		utils::string_map dam_tab = movement_type.damage_table();
		for(utils::string_map::const_iterator dam_it = dam_tab.begin(), dam_end = dam_tab.end();
			 dam_it != dam_end; ++dam_it) {
			std::vector<item> row;
			int resistance = 100 - atoi((*dam_it).second.c_str());
			char resi[16];
			snprintf(resi,sizeof(resi),"% 4d%%",resistance);	// range: -100% .. +70%
			std::string color;
			if (resistance < 0)
				color = "red";
			else if (resistance <= 20)
				color = "yellow";
			else if (resistance <= 40)
				color = "white";
			else
				color = "green";

			std::string lang_weapon = gettext(dam_it->first.c_str());
			push_tab_pair(row, lang_weapon);
			std::stringstream str;
			str << "<format>color=" << color << " text='"<< resi << "'</format>";
			const std::string markup = str.str();
			str.str(clear_stringstream);
			str << resi;
			row.push_back(std::make_pair(markup,
						     font::line_width(str.str(), normal_font_size)));
			resistance_table.push_back(row);
		}
		ss << generate_table(resistance_table);

		if (map != NULL) {
			// Print the terrain modifier table of the unit.
			ss << "\n\n<header>text='" << escape(_("Terrain Modifiers"))
			   << "'</header>\n\n";
			std::vector<item> first_row;
			table_spec table;
			push_header(first_row, _("Terrain"));
			push_header(first_row, _("Defense"));
			push_header(first_row, _("Movement Cost"));

			table.push_back(first_row);

			const t_translation::t_list& terrain_list = map->get_terrain_list();
			for (t_translation::t_list::const_iterator terrain_it = terrain_list.begin(); terrain_it != terrain_list.end(); ++ terrain_it) {
				const t_translation::t_terrain terrain = *terrain_it;
				if (terrain == t_translation::FOGGED || terrain == t_translation::VOID_TERRAIN || terrain == t_translation::OFF_MAP_USER)
					continue;
				const terrain_type& info = map->get_terrain_info(terrain);

				if (info.union_type().size() == 1 && info.union_type()[0] == info.number() && info.is_nonnull()) {
					std::vector<item> row;
					const std::string& name = info.name();
					const std::string id = info.id();
					const int moves = movement_type.movement_cost(*map,terrain);
					std::stringstream str;
					str << "<ref>text='" << escape(name) << "' dst='"
						<< escape(std::string("terrain_") + id) << "'</ref>";
					row.push_back(std::make_pair(str.str(),
						font::line_width(name, normal_font_size)));

					//defense  -  range: +10 % .. +70 %
					str.str(clear_stringstream);
					const int defense =
						100 - movement_type.defense_modifier(*map,terrain);
					std::string color;
					if (defense < 0)
						color = "red";
					else if (defense < 20)
						color = "yellow";
					else if (defense < 40)
						color = "white";
					else
						color = "green";

					str << "<format>color=" << color << " text='"<< defense << "%'</format>";
					const std::string markup = str.str();
					str.str(clear_stringstream);
					str << defense << "%";
					row.push_back(std::make_pair(markup,
						     font::line_width(str.str(), normal_font_size)));

					//movement  -  range: 1 .. 5, unit_movement_type::UNREACHABLE=impassable
					str.str(clear_stringstream);
					const bool cannot_move = moves > type_.movement();
					if (cannot_move)		// cannot move in this terrain
						color = "red";
					else if (moves > 1)
						color = "yellow";
					else
						color = "white";

					str << "<format>color=" << color << " text='";
					// A 5 MP margin; if the movement costs go above
					// the unit's max moves + 5, we replace it with dashes.
					if(cannot_move && (moves > type_.movement() + 5)) {
						str << "'-'";
					} else {
						str << moves;
					}
					str << "'</format>";
					push_tab_pair(row, str.str());

					table.push_back(row);
				}
			}
			ss << generate_table(table);
		}
		return ss.str();
	}
};

void generate_races_sections(const config *help_cfg, section &sec, int level)
{
	std::set<std::string, string_less> races;
	std::set<std::string, string_less> visible_races;

	BOOST_FOREACH (const unit_type_data::unit_type_map::value_type &i, unit_types.types())
	{
		const unit_type &type = i.second;
		races.insert(type.race());
		if (!type.hide_help())
			visible_races.insert(type.race());
	}

	std::stringstream text;

	for(std::set<std::string, string_less>::iterator it = races.begin(); it != races.end(); ++it) {
		section race_section;
		config section_cfg;

		bool hidden = (visible_races.count(*it) == 0);

		section_cfg["id"] = hidden_symbol(hidden) + race_prefix + *it;

		std::string title;
		if (const unit_race *r = unit_types.find_race(*it)) {
			title = r->plural_name();
		} else {
			title = _ ("race^Miscellaneous");
		}
		section_cfg["title"] = title;

		section_cfg["generator"] = "units:" + *it;

		parse_config_internal(help_cfg, &section_cfg, race_section, level+1);
		sec.add_section(race_section);
	}
}


std::vector<topic> generate_unit_topics(const bool sort_generated, const std::string& race)
{
	std::vector<topic> topics;
	std::set<std::string, string_less> race_units;

	BOOST_FOREACH (const unit_type_data::unit_type_map::value_type &i, unit_types.types())
	{
		const unit_type &type = i.second;

		if (type.race() != race)
			continue;

		const std::string type_name = type.type_name();
		const std::string ref_id = hidden_symbol(type.hide_help()) + unit_prefix +  type.id();
		topic unit_topic(type_name, ref_id, "");
		unit_topic.text = new unit_topic_generator(type);
		topics.push_back(unit_topic);

		if (!type.hide_help()) {
			// we also record an hyperlink of this unit
			// in the list used for the race topic
			std::string link =  "<ref>text='" + escape(type_name) + "' dst='" + escape(ref_id) + "'</ref>";
			race_units.insert(link);
		}
	}

	//generate the hidden race description topic
	std::string race_id = "..race_"+race;
	std::string race_name;
	std::string race_description;
	if (const unit_race *r = unit_types.find_race(race)) {
		race_name = r->plural_name();
		race_description = r->description();
		// if (description.empty()) description =  _("No description Available");
	} else {
		race_name = _ ("race^Miscellaneous");
		// description =  _("Here put the description of the Miscellaneous race");
	}

	std::stringstream text;
	text << race_description;
	text << "\n\n" << _("<header>text='Units of this race'</header>") << "\n";
	for (std::set<std::string, string_less>::iterator u = race_units.begin(); u != race_units.end(); ++u) {
		text << (*u) << "\n";
	}
	topics.push_back(topic(race_name, race_id, text.str()) );

	if (sort_generated)
		std::sort(topics.begin(), topics.end(), title_less());
	return topics;
}

std::vector<topic> generate_topics(const bool sort_generated, const std::string &generator)
{
	std::vector<topic> res;
	if (editor || generator.empty()) {
		return res;
	}

	if (generator == "abilities") {
		res = generate_ability_topics(sort_generated);
	} else if (generator == "weapon_specials") {
		res = generate_weapon_special_topics(sort_generated);
	} else if (generator == "factions") {
		res = generate_faction_topics(sort_generated);
	} else {
		std::vector<std::string> parts = utils::split(generator, ':', utils::STRIP_SPACES);
		if (parts[0] == "units" && parts.size()>1) {
			res = generate_unit_topics(sort_generated, parts[1]);
		}
	}

	return res;
}

void generate_sections(const config *help_cfg, const std::string &generator, section &sec, int level)
{
	if (editor) {
		return;
	}

	if (generator == "races") {
		generate_races_sections(help_cfg, sec, level);
	}
}

std::string generate_about_text()
{
#if defined(_KINGDOM_EXE) || !defined(_WIN32)
	std::vector<std::string> about_lines = about::get_text();
#else
	std::vector<std::string> about_lines;
#endif
	std::vector<std::string> res_lines;
	std::transform(about_lines.begin(), about_lines.end(), std::back_inserter(res_lines),
				   about_text_formatter());
	res_lines.erase(std::remove(res_lines.begin(), res_lines.end(), ""), res_lines.end());
	std::string text = utils::join(res_lines, "\n");
	return text;
}

std::string generate_separate_text_links()
{
	std::stringstream strstr;

	strstr << "\n\n";
	return strstr.str();
}

std::string generate_contents_links(const std::string& section_name, config const *help_cfg)
{
	config const &section_cfg = help_cfg->find_child("section", "id", section_name);
	if (!section_cfg) {
		return std::string();
	}

	std::ostringstream res;

		std::vector<std::string> topics = utils::quoted_split(section_cfg["topics"]);

		// we use an intermediate structure to allow a conditional sorting
		typedef std::pair<std::string,std::string> link;
		std::vector<link> topics_links;

		std::vector<std::string>::iterator t;
		// Find all topics in this section.
		for (t = topics.begin(); t != topics.end(); ++t) {
			if (config const &topic_cfg = help_cfg->find_child("topic", "id", *t)) {
				std::string id = topic_cfg["id"];
				if (is_visible_id(id))
					topics_links.push_back(link(topic_cfg["title"], id));
			}
		}

		if (section_cfg["sort_topics"] == "yes") {
			std::sort(topics_links.begin(),topics_links.end());
		}

		if (!topics_links.empty()) {
			res << generate_separate_text_links();
		}
		std::vector<link>::iterator l;
		for (l = topics_links.begin(); l != topics_links.end(); ++l) {
			std::string link =  "<ref>text='" + escape(l->first) + "' dst='" + escape(l->second) + "'</ref>";
			res << link <<"\n";
		}

		return res.str();
}

std::string generate_contents_links(const section &sec, const std::vector<topic>& topics)
{
		std::stringstream res;

		if (!sec.sections.empty() || !topics.empty()) {
			res << generate_separate_text_links();
		}

		section_list::const_iterator s;
		for (s = sec.sections.begin(); s != sec.sections.end(); ++s) {
			if (is_visible_id((*s)->id)) {
				std::string link =  "<ref>text='" + escape((*s)->title) + "' dst='.." + escape((*s)->id) + "'</ref>";
				res << link <<"\n";
			}
		}

		std::vector<topic>::const_iterator t;
		for (t = topics.begin(); t != topics.end(); ++t) {
			if (is_visible_id(t->id)) {
				std::string link =  "<ref>text='" + escape(t->title) + "' dst='" + escape(t->id) + "'</ref>";
				res << link <<"\n";
			}
		}

		return res.str();
}

std::string generate_topic_text(const std::string &generator, const config *help_cfg, const section &sec, const std::vector<topic>& generated_topics)
{
	std::string empty_string = "";

	if (editor || generator == "") {
		return empty_string;
	} else if (generator == "about") {
		return generate_about_text();
	} else {
		std::vector<std::string> parts = utils::split(generator, ':');
		if (parts.size()>1 && parts[0] == "contents") {
			if (parts[1] == "generated") {
				return generate_contents_links(sec, generated_topics);
			} else {
				return generate_contents_links(parts[1], help_cfg);
			}
		}
	}

	return empty_string;
}

topic* find_topic(section &sec, const std::string &id)
{
	topic_list::iterator tit =
		std::find_if(sec.topics.begin(), sec.topics.end(), has_id(id));
	if (tit != sec.topics.end()) {
		return &(*tit);
	}
	section_list::iterator sit;
	for (sit = sec.sections.begin(); sit != sec.sections.end(); ++sit) {
		topic *t = find_topic(*(*sit), id);
		if (t != NULL) {
			return t;
		}
	}
	return NULL;
}

section* find_section(section &sec, const std::string &id)
{
	section_list::iterator sit =
		std::find_if(sec.sections.begin(), sec.sections.end(), has_id(id));
	if (sit != sec.sections.end()) {
		return *sit;
	}
	for (sit = sec.sections.begin(); sit != sec.sections.end(); ++sit) {
		section *s = find_section(*(*sit), id);
		if (s != NULL) {
			return s;
		}
	}
	return NULL;
}

std::pair<section*, int> find_parent(section& sec, const std::string& id)
{
	section_list::iterator sit =
		std::find_if(sec.sections.begin(), sec.sections.end(), has_id(id));
	if (sit != sec.sections.end()) {
		return std::make_pair(&sec, std::distance(sec.sections.begin(), sit));
	}
	topic_list::iterator tit =
		std::find_if(sec.topics.begin(), sec.topics.end(), has_id(id));
	if (tit != sec.topics.end()) {
		return std::make_pair(&sec, std::distance(sec.topics.begin(), tit));
	}
	std::pair<section*, int> ret(NULL, -1);
	for (sit = sec.sections.begin(); sit != sec.sections.end(); ++sit) {
		ret = find_parent(*(*sit), id);
		if (ret.first != NULL) {
			return ret;
		}
	}
	return ret;
}

/// Return true if the section with id section_id is referenced from
/// another section in the config, or the toplevel.
bool section_is_referenced(const std::string &section_id, const config &cfg)
{
	if (const config &toplevel = cfg.child("toplevel"))
	{
		const std::vector<std::string> toplevel_refs
			= utils::quoted_split(toplevel["sections"]);
		if (std::find(toplevel_refs.begin(), toplevel_refs.end(), section_id)
			!= toplevel_refs.end()) {
			return true;
		}
	}

	BOOST_FOREACH (const config &section, cfg.child_range("section"))
	{
		const std::vector<std::string> sections_refd
			= utils::quoted_split(section["sections"]);
		if (std::find(sections_refd.begin(), sections_refd.end(), section_id)
			!= sections_refd.end()) {
			return true;
		}
	}
	return false;
}

/// Return true if the topic with id topic_id is referenced from
/// another section in the config, or the toplevel.
bool topic_is_referenced(const std::string &topic_id, const config &cfg)
{
	if (const config &toplevel = cfg.child("toplevel"))
	{
		const std::vector<std::string> toplevel_refs
			= utils::quoted_split(toplevel["topics"]);
		if (std::find(toplevel_refs.begin(), toplevel_refs.end(), topic_id)
			!= toplevel_refs.end()) {
			return true;
		}
	}

	BOOST_FOREACH (const config &section, cfg.child_range("section"))
	{
		const std::vector<std::string> topics_refd
			= utils::quoted_split(section["topics"]);
		if (std::find(topics_refd.begin(), topics_refd.end(), topic_id)
			!= topics_refd.end()) {
			return true;
		}
	}
	return false;
}

void parse_config_internal(const config *help_cfg, const config *section_cfg,
						   section &sec, int level)
{
	if (level > max_section_level) {
		std::cerr << "Maximum section depth has been reached. Maybe circular dependency?"
				  << std::endl;
	}
	else if (section_cfg != NULL) {
		const std::vector<std::string> sections = utils::quoted_split((*section_cfg)["sections"]);
		sec.level = level;
		std::string id = level == 0 ? "toplevel" : (*section_cfg)["id"].str();
		if (level != 0) {
			if (!is_valid_id(id)) {
				std::stringstream ss;
				ss << "Invalid ID, used for internal purpose: '" << id << "'";
				throw help::parse_error(ss.str());
			}
		}
		t_string title = level == 0 ? "" : (*section_cfg)["title"].t_str();
		sec.id = id;
		sec.title = title;
		std::vector<std::string>::const_iterator it;
		// Find all child sections.
		for (it = sections.begin(); it != sections.end(); ++it) {
			if (const config &child_cfg = help_cfg->find_child("section", "id", *it))
			{
				section child_section;
				parse_config_internal(help_cfg, &child_cfg, child_section, level + 1);
				sec.add_section(child_section);
			}
			else {
				std::stringstream ss;
				ss << "Help-section '" << *it << "' referenced from '"
				   << id << "' but could not be found.";
				throw help::parse_error(ss.str());
			}
		}

		generate_sections(help_cfg, (*section_cfg)["sections_generator"], sec, level);
		//TODO: harmonize topics/sections sorting
		if ((*section_cfg)["sort_sections"] == "yes") {
			std::sort(sec.sections.begin(),sec.sections.end(), section_less());
		}

		bool sort_topics = false;
		bool sort_generated = true;

		if ((*section_cfg)["sort_topics"] == "yes") {
		  sort_topics = true;
		  sort_generated = false;
		} else if ((*section_cfg)["sort_topics"] == "no") {
		  sort_topics = false;
    	  sort_generated = false;
		} else if ((*section_cfg)["sort_topics"] == "generated") {
		  sort_topics = false;
		  sort_generated = true;
		} else if ((*section_cfg)["sort_topics"] != "") {
		  std::stringstream ss;
		  ss << "Invalid sort option: '" << (*section_cfg)["sort_topics"] << "'";
		  throw help::parse_error(ss.str());
		}

		std::vector<topic> generated_topics =
		generate_topics(sort_generated,(*section_cfg)["generator"]);

		const std::vector<std::string> topics_id = utils::quoted_split((*section_cfg)["topics"]);
		std::vector<topic> topics;

		// Find all topics in this section.
		for (it = topics_id.begin(); it != topics_id.end(); ++it) {
			if (const config &topic_cfg = help_cfg->find_child("topic", "id", *it))
			{
				t_string text = topic_cfg["text"].t_str();
				text += generate_topic_text(topic_cfg["generator"], help_cfg, sec, generated_topics);
				topic child_topic(topic_cfg["title"], topic_cfg["id"], text);
				if (!is_valid_id(child_topic.id)) {
					std::stringstream ss;
					ss << "Invalid ID, used for internal purpose: '" << id << "'";
					throw help::parse_error(ss.str());
				}
				topics.push_back(child_topic);
			}
			else {
				std::stringstream ss;
				ss << "Help-topic '" << *it << "' referenced from '" << id
				   << "' but could not be found." << std::endl;
				throw help::parse_error(ss.str());
			}
		}

		if (sort_topics) {
			std::sort(topics.begin(),topics.end(), title_less());
			std::sort(generated_topics.begin(),
			  generated_topics.end(), title_less());
			std::merge(generated_topics.begin(),
			  generated_topics.end(),topics.begin(),topics.end()
			  ,std::back_inserter(sec.topics),title_less());
		}
		else {
			std::copy(topics.begin(), topics.end(),
			  std::back_inserter(sec.topics));
			std::copy(generated_topics.begin(),
			  generated_topics.end(),
			  std::back_inserter(sec.topics));
		}
	}
}

section parse_config(const config *cfg)
{
	section sec;
	if (cfg != NULL) {
		config const &toplevel_cfg = cfg->child("toplevel");
		parse_config_internal(cfg, toplevel_cfg ? &toplevel_cfg : NULL, sec);
	}
	return sec;
}

void generate_contents(const std::string& tag, section& toplevel)
{
	toplevel.clear();
	hidden_sections.clear();

	const config *help_config = &game_cfg->find_child("book", "id", tag);
	if (!*help_config) {
		help_config = &dummy_cfg;
	}
	try {
		toplevel = parse_config(help_config);
		// Create a config object that contains everything that is
		// not referenced from the toplevel element. Read this
		// config and save these sections and topics so that they
		// can be referenced later on when showing help about
		// specified things, but that should not be shown when
		// opening the help browser in the default manner.
		config hidden_toplevel;
		std::stringstream ss;
		BOOST_FOREACH (const config &section, help_config->child_range("section"))
		{
			const std::string id = section["id"];
			if (find_section(toplevel, id) == NULL) {
				// This section does not exist referenced from the
				// toplevel. Hence, add it to the hidden ones if it
				// is not referenced from another section.
				if (!section_is_referenced(id, *help_config)) {
					if (ss.str() != "") {
						ss << ",";
					}
					ss << id;
				}
			}
		}
		hidden_toplevel["sections"] = ss.str();
		ss.str("");
		BOOST_FOREACH (const config &topic, help_config->child_range("topic"))
		{
			const std::string id = topic["id"];
			if (find_topic(toplevel, id) == NULL) {
				if (!topic_is_referenced(id, *help_config)) {
					if (ss.str() != "") {
						ss << ",";
					}
					ss << id;
				}
			}
		}
		hidden_toplevel["topics"] = ss.str();
		config hidden_cfg = *help_config;
		// Change the toplevel to our new, custom built one.
		hidden_cfg.clear_children("toplevel");
		hidden_cfg.add_child("toplevel", hidden_toplevel);
		hidden_sections = parse_config(&hidden_cfg);
	}
	catch (help::parse_error e) {
		std::stringstream msg;
		msg << "Parse error when parsing help text: '" << e.message << "'";
		VALIDATE(false, msg.str());
	}
}

} // End namespace help.

std::string single_digit_image(int digit)
{
	if (digit < 0 || digit > 9) {
		return null_str;
	}
	std::stringstream strstr;
	strstr << "misc/digit.png~CROP(" << 8 * digit << ", 0, 8, 12)";
	return strstr.str();
}

std::string multi_digit_image_str(int integer)
{
	if (integer < 0) {
		return null_str;
	}
	
	std::vector<int> digits;
	
	digits.push_back(integer % 10);
	int descend = integer / 10;
	while (descend) {
		digits.push_back(descend % 10);
		descend /= 10;
	}

	std::stringstream img, strstr;
	for (std::vector<int>::const_reverse_iterator it = digits.rbegin(); it != digits.rend(); ++ it) {
		int digit = *it;
		img.str("");
		img << "misc/digit.png~CROP(" << 8 * digit << ", 0, 8, 12)";
		strstr << help::tintegrate::generate_img(img.str());
	}
	return strstr.str();
}

SDL_Point multi_digit_image_size(int integer)
{
	int digits = 1;
	int descend = integer / 10;
	while (descend) {
		digits ++;
		descend /= 10;
	}

	SDL_Point ret;
	ret.x = 8 * digits;
	ret.y = 12;
	return ret;
}