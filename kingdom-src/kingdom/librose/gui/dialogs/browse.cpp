/* $Id: campaign_difficulty.cpp 49602 2011-05-22 17:56:13Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/browse.hpp"

#include "display.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "filesystem.hpp"
#include "game_config.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/message.hpp"

// std::tolower
#include <cctype>

#include <boost/bind.hpp>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_campaign_difficulty
 *
 * == Campaign difficulty ==
 *
 * The campaign mode difficulty menu.
 *
 * @begin{table}{dialog_widgets}
 *
 * title & & label & m &
 *         Dialog title label. $
 *
 * message & & scroll_label & o &
 *         Text label displaying a description or instructions. $
 *
 * listbox & & listbox & m &
 *         Listbox displaying user choices, defined by WML for each campaign. $
 *
 * -icon & & control & m &
 *         Widget which shows a listbox item icon, first item markup column. $
 *
 * -label & & control & m &
 *         Widget which shows a listbox item label, second item markup column. $
 *
 * -description & & control & m &
 *         Widget which shows a listbox item description, third item markup column. $
 *
 * @end{table}
 */

REGISTER_DIALOG(browse)

const char tbrowse::path_delim = '/';
const std::string tbrowse::path_delim_str = "/";

tbrowse::tfile::tfile(const std::string& name)
	: name(name)
	, method(tbrowse::METHOD_ALPHA)
{
	utils::transform_tolower(name, lower);
}
bool tbrowse::tfile::operator<(const tbrowse::tfile& that) const 
{
	return lower < that.lower; 
}

tbrowse::tbrowse(display& disp, tparam& param)
	: disp_(disp)
	, param_(param)
	, navigate_(NULL)
	, goto_higher_(NULL)
	, filename_(NULL)
	, ok_(NULL)
	, current_dir_(get_path(param.initial))
{
	if (!is_directory(current_dir_)) {
		current_dir_ = get_path(game_config::preferences_dir);
	}
}

void tbrowse::pre_show(CVideo& /*video*/, twindow& window)
{
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	if (!param_.title.empty()) {
		label->set_label(param_.title);
	}

	filename_ = find_widget<ttext_box>(&window, "filename", false, true);
	ok_ = find_widget<tbutton>(&window, "ok", false, true);
	if (!param_.open.empty()) {
		ok_->set_label(param_.open);
	}

	goto_higher_ = find_widget<tbutton>(&window, "goto-higher", false, true);
	connect_signal_mouse_left_click(
			  *goto_higher_
			, boost::bind(
				&tbrowse::goto_higher
				, this
				, boost::ref(window)));

	navigate_ = find_widget<treport>(&window, "navigate", false, true);
	reload_navigate(window, true);

	tlistbox& list = find_widget<tlistbox>(&window, "default", false);
	// window.keyboard_capture(&list);

	std::map<std::string, string_map> data;

	update_file_lists(window);
	list.set_callback_value_change(dialog_callback<tbrowse, &tbrowse::item_selected>);

	init_entry(window);

	if (param_.readonly) {
		filename_->set_active(false);
	}
	filename_->set_text_changed_callback(boost::bind(&tbrowse::text_changed_callback, this, _1));
}

void tbrowse::post_show(twindow& window)
{
	const std::string& label = filename_->label();
	if (current_dir_ != path_delim_str) {
		param_.result = current_dir_ + path_delim + label;
	} else {
		param_.result = current_dir_ + label;
	}
}

void tbrowse::init_entry(twindow& window)
{
	entries_.push_back(tentry(game_config::preferences_dir, _("My Document"), "misc/document.png", "document"));
#ifdef _WIN32
	entries_.push_back(tentry("c:", _("C:"), "misc/disk.png", "device1"));
#else
	entries_.push_back(tentry("/", _("Device"), "misc/disk.png", "device1"));
#endif
	entries_.push_back(tentry(game_config::path, _("Sandbox"), "misc/dir-res.png", "device2"));
	if (param_.extra.valid()) {
		entries_.push_back(param_.extra);
		entries_.back().id = "extra";
	} else {
		tbutton* b = find_widget<tbutton>(&window, "extra", false, true);
		b->set_visible(twidget::INVISIBLE);
	}

	int index = 0;
	for (std::vector<tentry>::const_iterator it = entries_.begin(); it != entries_.end(); ++ it, index ++) {
		const tentry& entry = *it;

		tbutton* b = find_widget<tbutton>(&window, entry.id, false, true);
		for (int i = 0; i < 4; i ++) {
			b->canvas()[i].set_variable("image", variant(entry.icon));
		}
		b->set_label(entry.label);

		connect_signal_mouse_left_click(
			  *b
			, boost::bind(
				&tbrowse::goto_entry
				, this
				, boost::ref(window)
				, index));
	}
}

gui2::tbutton* tbrowse::create_navigate_button(twindow& window, const std::string& label, int index)
{
	gui2::tbutton* widget = create_button(null_str, "text", NULL);
	widget->set_label(label);

	connect_signal_mouse_left_click(
		*widget
		, boost::bind(
			&tbrowse::click_navigate
			, this
			, boost::ref(window)
			, _3
			, _4
			, index));
	return widget;
}

void tbrowse::reload_navigate(twindow& window, bool first)
{
	const gui2::tgrid::tchild* children = navigate_->content_grid()->children();
	int childs = navigate_->content_grid()->children_vsize();

	size_t prefix_chars;
	std::string adjusted_current_dir;
	std::vector<std::string> vstr;

	if (current_dir_ != path_delim_str) {
#ifdef _WIN32
		// prefix_chars = 1;
		// adjusted_current_dir = path_delim_str + current_dir_;

		// this version don't support change volume in table. so easy market above. use below temporary.
		// Should modify filesystem function about Win32 in furture, it requrie use UNICODE.
		prefix_chars = 0;
		adjusted_current_dir = current_dir_;
#else
		prefix_chars = 0;
		adjusted_current_dir = current_dir_;
#endif
		vstr = utils::split(adjusted_current_dir, path_delim, utils::STRIP_SPACES);
	} else {
		prefix_chars = 0;
		adjusted_current_dir = current_dir_;
		vstr.push_back(path_delim_str);
	}

	int vsize = (int)vstr.size();
	std::vector<std::string> ids;
	int start_sect = 0;

	int can_disp_count = 4;
	if (first) {
		const int initial_count = 3;
		can_disp_count = initial_count;
	}
	if (vsize > can_disp_count) {
		start_sect = vsize - can_disp_count;
		ids.push_back("...<<");
		for (int n = vsize - can_disp_count + 1; n < vsize; n ++) {
			ids.push_back(vstr[n]);
		}
	} else {
		ids = vstr;
	}

	while (childs > (int)ids.size()) {
		// require reverse erase.
		navigate_->erase_child(-- childs);
	}
	
	int start_pos = -1;
	cookie_paths_.clear();
	while (start_sect --) {
		start_pos = adjusted_current_dir.find(path_delim, start_pos + 1);
	}

	childs = navigate_->content_grid()->children_vsize();
	int n = 0;
	gui2::tbutton* widget;
	for (std::vector<std::string>::const_iterator it = ids.begin(); it != ids.end(); ++ it, n ++) {
		if (n < childs) {
			widget = dynamic_cast<tbutton*>(children[n].widget_);
			widget->set_label(*it);
		} else {
			widget = create_navigate_button(window, *it, n);
			navigate_->insert_child(*widget);
		}
		start_pos = adjusted_current_dir.find(path_delim, start_pos + 1);
		if (start_pos) {
			cookie_paths_.push_back(adjusted_current_dir.substr(prefix_chars, start_pos - prefix_chars));
		} else {
			// like unix system. first character is path_delim.
			cookie_paths_.push_back(path_delim_str);
			widget->set_label(cookie_paths_.back());
		}
	}
	navigate_->replacement_children();

	goto_higher_->set_active(ids.size() >= 2);
}

void tbrowse::click_navigate(twindow& window, bool& handled, bool& halt, int index)
{
	current_dir_ = cookie_paths_[index];
	update_file_lists(window);

	reload_navigate(window, false);

	// clike maybe erase this button.
	handled = true;
	halt = true;
}

void tbrowse::goto_entry(twindow& window, int index)
{
	current_dir_ = entries_[index].path;
	update_file_lists(window);

	reload_navigate(window, false);

}

const std::string& get_browse_icon(bool dir)
{
	static const std::string dir_icon = "misc/dir.png";
	static const std::string file_icon = "misc/file.png";
	return dir? dir_icon: file_icon;
}

void tbrowse::add_row(twindow& window, tlistbox& list, const std::string& name, bool dir)
{
	std::stringstream ss;

	string_map list_item;
	std::map<std::string, string_map> list_item_item;

	list_item["label"] = "misc/open.png~SCALE(32, 32)";
	list_item_item.insert(std::make_pair("open", list_item));

	ss << tintegrate::generate_img(get_browse_icon(dir) + "~SCALE(24, 24)") << name;
	list_item["label"] = ss.str();
	list_item_item.insert(std::make_pair("name", list_item));

	list_item["label"] = "---";
	list_item_item.insert(std::make_pair("date", list_item));

	list_item["label"] = dir? null_str: "---";
	list_item_item.insert(std::make_pair("size", list_item));

	list.add_row(list_item_item);

	int index = list.get_item_count() - 1;
	tgrid* grid_ptr = list.get_row_grid(index);
	ttoggle_panel* toggle = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true));

	tbutton* button = find_widget<tbutton>(grid_ptr, "open", false, true);
	if (dir) {
		connect_signal_mouse_left_click(
			*button
			, boost::bind(
				&tbrowse::open
				, this
				, boost::ref(window)
				, _3
				, _4
				, (int)true
				, (int)index));
	} else {
		button->set_visible(twidget::HIDDEN);
		button->set_active(false);
	}
}

void tbrowse::reload_file_table(twindow& window, int cursel)
{
	tlistbox* list = find_widget<tlistbox>(&window, "default", false, true);
	list->clear();

	int size = int(dirs_in_current_dir_.size() + files_in_current_dir_.size());
	for (std::set<tfile>::const_iterator it = dirs_in_current_dir_.begin(); it != dirs_in_current_dir_.end(); ++ it) {
		const tfile& file = *it;
		add_row(window, *list, file.name, true);
	}
	for (std::set<tfile>::const_iterator it = files_in_current_dir_.begin(); it != files_in_current_dir_.end(); ++ it) {
		const tfile& file = *it;
		add_row(window, *list, file.name, false);
	}
	if (size) {
		if (cursel >= size) {
			cursel = size - 1;
		}
		list->select_row(cursel);
	}

	item_selected(window);

	window.invalidate_layout();
}

void tbrowse::update_file_lists(twindow& window)
{
	files_in_current_dir_.clear();
	dirs_in_current_dir_.clear();

	std::vector<std::string> files, dirs;

	std::string adjust_current_dir;
#ifdef _WIN32
	adjust_current_dir = conv_ansi_utf8_2(current_dir_, false);
#else
	adjust_current_dir = current_dir_;
#endif
	get_files_in_dir(adjust_current_dir, &files, &dirs, FILE_NAME_ONLY);

	// files and dirs of get_files_in_dir returned are unicode16 format
	for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++ it) {
		const std::string& str = *it;
#ifdef _WIN32
		files_in_current_dir_.insert(tfile(conv_ansi_utf8_2(str, true)));
#else
		files_in_current_dir_.insert(str);
#endif
	}
	for (std::vector<std::string>::const_iterator it = dirs.begin(); it != dirs.end(); ++ it) {
		const std::string& str = *it;
#ifdef _WIN32
		dirs_in_current_dir_.insert(tfile(conv_ansi_utf8_2(str, true)));
#else
		dirs_in_current_dir_.insert(str);
#endif
	}

	reload_file_table(window, 0);
}

void tbrowse::open(twindow& window, bool& handled, bool& halt, bool dir, int index)
{
	std::set<tfile>::const_iterator it = dirs_in_current_dir_.begin();
	std::advance(it, index);

	if (current_dir_ != path_delim_str) {
		current_dir_ = current_dir_ + path_delim + it->name;
	} else {
		current_dir_ = current_dir_ + it->name;
	}
	update_file_lists(window);

	reload_navigate(window, false);

	// click will erase all buttons in table. include myself.
	handled = true;
	halt = true;
}

void tbrowse::goto_higher(twindow& window)
{
	size_t pos = current_dir_.rfind(path_delim);
	if (pos == std::string::npos) {
		goto_higher_->set_active(false);
		return;
	}

	current_dir_ = current_dir_.substr(0, pos);
	update_file_lists(window);

	reload_navigate(window, false);
}

std::string tbrowse::get_path(const std::string& file_or_dir) const 
{
	std::string res_path = file_or_dir;
	std::replace(res_path.begin(), res_path.end(), '\\', path_delim);

	// get rid of all path_delim at end.
	size_t s = res_path.size();
	while (s > 1 && res_path.at(s - 1) == path_delim) {
		res_path.erase(s - 1);
		s = res_path.size();
	}

	if (!::is_directory(file_or_dir)) {
		size_t index = file_or_dir.find_last_of(path_delim);
		if (index != std::string::npos) {
			res_path = file_or_dir.substr(0, index);
		}
	}
	return res_path;
}

void tbrowse::item_selected(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "default", false);
	int row = list.get_selected_row();

	tristate dir = t_unset;
	if (row >= 0) {
		std::set<tfile>::const_iterator it;
		if (row < (int)dirs_in_current_dir_.size()) {
			dir = t_true;
			it = dirs_in_current_dir_.begin();
		} else {
			dir = t_false;
			it = files_in_current_dir_.begin();
			row -= dirs_in_current_dir_.size();
		}
		std::advance(it, row);
		selected_ = it->name;
	} else {
		selected_.empty();
	}

	set_ok_active(window, dir);
}

void tbrowse::set_ok_active(twindow& window, tristate dir)
{
	bool active = false;
	if (dir == t_true) {
		if (param_.flags & TYPE_DIR) {
			active = true;
		}
	} else if (dir == t_false) {
		if (param_.flags & TYPE_FILE) {
			active = true;
		}
	}

	if (!active) {
		selected_.clear();
	}
	set_filename(window);

	return ok_->set_active(active);
}

void tbrowse::text_changed_callback(ttext_box* widget)
{
	const std::string& label = widget->label();
	ok_->set_active(!label.empty());
}

void tbrowse::set_filename(twindow& window)
{
	filename_->set_label(selected_);
}

}
