/* $Id: editor_display.hpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
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

#ifndef STUDIO_UNIT_HPP_INCLUDED
#define STUDIO_UNIT_HPP_INCLUDED

#include "base_unit.hpp"
#include "gui/auxiliary/widget_definition.hpp"
#include "gui/dialogs/cell_setting.hpp"

class mkwin_controller;
class mkwin_display;

class unit: public base_unit
{
public:
	enum {NONE, WIDGET, WINDOW, COLUMN, ROW};
	struct tchild
	{
		tchild()
			: window(NULL)
		{}

		void clear(bool del)
		{
			if (del) {
				if (window) {
					delete window;
				}
				for (std::vector<unit*>::const_iterator it = rows.begin(); it != rows.end(); ++ it) {
					delete *it;
				}
				for (std::vector<unit*>::const_iterator it = cols.begin(); it != cols.end(); ++ it) {
					delete *it;
				}
				for (std::vector<unit*>::const_iterator it = units.begin(); it != units.end(); ++ it) {
					delete *it;
				}
			}
			window = NULL;
			rows.clear();
			cols.clear();
			units.clear();
		}
		bool is_all_spacer() const
		{
			for (std::vector<unit*>::const_iterator it = units.begin(); it != units.end(); ++ it) {
				const unit* u = *it;
				if (!u->is_spacer()) {
					return false;
				}
			}
			return true;
		}

		void generate(config& cfg) const;
		void from(mkwin_controller& controller, mkwin_display& disp, unit* parent, const config& cfg);

		unit* window;
		std::vector<unit*> rows;
		std::vector<unit*> cols;
		std::vector<unit*> units;
	};

	unit(mkwin_controller& controller, mkwin_display& disp, const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget, unit* parent);
	unit(mkwin_controller& controller, mkwin_display& disp, int type, unit* parent);
	unit(const unit& that);
	~unit();

	bool require_sort() const { return true; }
	void redraw_unit();
	const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget() const { return widget_; }

	std::string image() const;
	void set_click_dismiss(bool val) { cell_.window.click_dismiss = val; }
	gui2::tcell_setting& cell() { return cell_; }
	const gui2::tcell_setting& cell() const { return cell_; }
	void set_cell(const gui2::tcell_setting& cell) { cell_ = cell; }

	void set_child(int number, const tchild& child);
	bool has_child() const { return !children_.empty(); }
	const tchild& child(int index) const { return children_[index]; }
	const std::vector<tchild>& children() const { return children_; }
	int children_layers() const;

	bool is_spacer() const { return type_ == WIDGET && widget_.first == "spacer"; }
	bool is_grid() const { return type_ == WIDGET && widget_.first == "grid"; }
	bool is_stacked_widget() const { return type_ == WIDGET && widget_.first == "stacked_widget"; }
	bool is_drawing() const { return type_ == WIDGET && widget_.first == "drawing"; }
	bool is_listbox() const { return type_ == WIDGET && widget_.first == "listbox"; }
	bool is_scroll_label() const { return type_ == WIDGET && widget_.first == "scroll_label"; }
	bool is_scroll_text_box() const { return type_ == WIDGET && widget_.first == "scroll_text_box"; }
	bool is_scrollbar_panel() const { return type_ == WIDGET && widget_.first == "scrollbar_panel"; }
	bool is_tree_view() const { return type_ == WIDGET && widget_.first == "tree_view"; }
	bool is_scroll() const { return is_listbox() || is_scroll_label() || is_scroll_text_box() || is_scrollbar_panel() || is_tree_view(); }
	bool is_panel() const { return type_ == WIDGET && (widget_.first == "panel" || widget_.first == "toggle_panel"); }
	bool has_special_setting() const { return is_scroll() || is_drawing(); }

	unit* parent() const { return parent_; }
	void set_parent(unit* parent) { parent_ = parent; }
	int type() const { return type_; }

	void insert_child(int w, int h);
	void erase_child(int index);
	std::string child_tag(int index) const;

	void generate(config& cfg) const;
	void from(const config& cfg);

private:
	void generate_window(config& cfg) const;
	void generate_row(config& cfg) const;
	void generate_column(config& cfg) const;
	void generate_widget(config& cfg) const;
	void generate_grid(config& cfg) const;
	void generate_stacked_widget(config& cfg) const;
	void generate_listbox(config& cfg) const;
	void generate_toggle_panel(config& cfg) const;
	void generate_scrollbar_panel(config& cfg) const;
	void generate_tree_view(config& cfg) const;
	void generate_drawing(config& cfg) const;

	void from_window(const config& cfg);
	void from_row(const config& cfg);
	void from_column(const config& cfg);
	void from_widget(const config& cfg);
	void from_grid(const config& cfg);
	void from_stacked_widget(const config& cfg);
	void from_listbox(const config& cfg);
	void from_toggle_panel(const config& cfg);
	void from_scrollbar_panel(const config& cfg);
	void from_tree_view(const config& cfg);
	void from_drawing(const config& cfg);

private:
	mkwin_controller& controller_;
	mkwin_display& disp_;

	int type_;
	unit* parent_;
	std::pair<std::string, gui2::tcontrol_definition_ptr> widget_;
	gui2::tcell_setting cell_;
	std::vector<tchild> children_;
};

std::string form_widget_png(const std::string& type, const std::string& definition);
std::string formual_extract_str(const std::string& str);

#endif
