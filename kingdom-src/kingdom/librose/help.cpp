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

#include "exceptions.hpp"
#include "gettext.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "marked-up_text.hpp"
#include "log.hpp"
#include "sound.hpp"
#include "wml_separators.hpp"
#include "serialization/parser.hpp"
#include "font.hpp"
#include "wml_exception.hpp"
#include "integrate.hpp"

#include <boost/foreach.hpp>
#include <queue>

namespace help {

/// Thrown when the help system fails to parse something.
parse_error::parse_error(const std::string& msg) : game::error(msg) {}

/// Convert the contents to wml attributes, surrounded within
/// [element_name]...[/element_name]. Return the resulting WML.
static std::string convert_to_wml(const std::string &element_name, const std::string &contents);

/// Return the color the string represents. Return font::NORMAL_COLOR if
/// the string is empty or can't be matched against any other color.
SDL_Color string_to_color(const std::string &s);

std::string remove_first_space(const std::string& text);

/// Return the first word in s, not removing any spaces in the start of
/// it.
std::string get_first_word(const std::string &s);

std::map<int, std::string> parse_text(const std::string &text)
{
	std::map<int, std::string> res;
	bool last_char_escape = false;
	const char escape_char = '\\';
	std::stringstream ss;
	size_t pos;
	int item_start = 0;
	enum { ELEMENT_NAME, OTHER } state = OTHER;
	for (pos = 0; pos < text.size(); ++ pos) {
		const char c = text[pos];
		if (c == escape_char && !last_char_escape) {
			last_char_escape = true;
		} else {
			if (state == OTHER) {
				if (c == '<') {
					if (last_char_escape) {
						ss << c;
					} else {
						if (!ss.str().empty()) {
							res.insert(std::make_pair(item_start, ss.str()));
							item_start = pos;
							ss.str("");
						}
						state = ELEMENT_NAME;
					}
				} else {
					ss << c;
				}
			} else if (state == ELEMENT_NAME) {
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
					res.insert(std::make_pair(item_start, element));
					pos = end_pos + end_element_name.size() - 1;
					item_start = pos + 1;
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
		res.insert(std::make_pair(item_start, ss.str()));
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

	} catch (utils::invalid_utf8_exception&) {
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
const std::string section_topic_prefix = "..";

const int normal_font_size = font::SIZE_SMALL;

/// Recursive function used by parse_config.
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
std::string hidden_symbol(bool hidden) 
{
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

std::string generate_about_text()
{
	std::vector<std::string> about_lines;
	// std::vector<std::string> about_lines = about::get_text();

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
		strstr << tintegrate::generate_img(img.str());
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