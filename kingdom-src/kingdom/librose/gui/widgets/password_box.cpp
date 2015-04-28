/* $Id: password_box.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2009 - 2012 by Thomas Baumhauer <thomas.baumhauer@NOSPAMgmail.com>
   Copyright (C) 2009 - 2012 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/widgets/password_box.hpp"

#include "gui/auxiliary/widget_definition/text_box.hpp"
#include "gui/auxiliary/window_builder/password_box.hpp"
#include "gui/widgets/settings.hpp"
#include "serialization/string_utils.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_WIDGET3(ttext_box_definition, password_box, "text_box_definition")

void tpassword_box::set_label(const std::string& text) 
{
	std::string str = text;
	str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
	ttext_box::set_label(str);
	if (!in_operator_) {
		real_value_ = label();
		ttext_box::set_label(std::string(utils::utf8str_len(real_value_), '*'));
	}
}

const std::string& tpassword_box::label() const
{
	value_ = ttext_box::label();
	if (!in_operator_) {
		value_ = tintegrate::drop_escape(value_);
	}
	return value_;
}

class tin_operator_lock
{
public:
	tin_operator_lock(tpassword_box& widget)
		: widget_(widget)
	{
		in_operator_ = widget_.get_in_operator();
		if (!in_operator_) {
			widget_.set_in_operator(true);
		}
	}
	~tin_operator_lock()
	{
		widget_.set_in_operator(in_operator_);
	}

private:
	tpassword_box& widget_;
	bool in_operator_;
};

void tpassword_box::insert_str(const std::string& str) 
{
	pre_function();
	{
		tin_operator_lock lock(*this);
		ttext_box::insert_str(str);
	}
	post_function();
}

void tpassword_box::delete_char(const bool before_cursor) 
{
	pre_function();
	{
		tin_operator_lock lock(*this);
		ttext_box::delete_char(before_cursor);
	}
	post_function();
}

void tpassword_box::handle_key_backspace(SDLMod modifier, bool& handled) 
{
	pre_function();
	{
		tin_operator_lock lock(*this);
		ttext_box::handle_key_backspace(modifier, handled);
	}
	post_function();
}

void tpassword_box::handle_key_delete(SDLMod modifier, bool& handled)
{
	pre_function();
	{
		tin_operator_lock lock(*this);
		ttext_box::handle_key_delete(modifier, handled);
	}
	post_function();
}

void tpassword_box::paste_selection(const bool mouse) 
{
	pre_function();
	{
		tin_operator_lock lock(*this);
		ttext_box::paste_selection(mouse);
	}
	post_function();
}

void tpassword_box::set_maximum_length(const size_t maximum_length)
{
	pre_function();
	{
		tin_operator_lock lock(*this);
		ttext_box::set_maximum_length(maximum_length);
	}
	post_function();
}

void tpassword_box::pre_function() 
{
	if (in_operator_ || !integrate_) {
		return;
	}
	// ttext_box::set_label() will reset the selection,
	// we therefore have to remember it
	const std::string before = integrate_->before_str(selection_start_.x, selection_start_.y);
	std::string selection;
	bool normal = selection_end_.x < 0 || 
		(selection_end_.y > selection_start_.y || (selection_end_.y == selection_start_.y && selection_end_.x >= selection_start_.x));
	if (exist_selection()) {
		SDL_Rect start, end;
		normalize_start_end(start, end);
		selection = integrate_->handle_selection(start.x, start.y, end.x, end.y, NULL);
	}

	// Tell ttext_box the actual input of this box
	std::string str = tintegrate::stuff_escape(real_value_);
	ttext_box::set_label(str);

	// Restore the selection
	size_t before_chars = utils::utf8str_len(before);
	selection_start_ = integrate_->editable_at2(before_chars);
	if (!selection.empty()) {
		size_t selection_chars = utils::utf8str_len(selection);
		size_t end = normal? before_chars + selection_chars: before_chars - selection_chars;
		selection_end_ = integrate_->editable_at2(end);
	}
}

void tpassword_box::post_function() 
{
	if (in_operator_ || !integrate_) {
		return;
	}

	// See above
	std::string before = integrate_->before_str(selection_start_.x, selection_start_.y);
	std::string selection;
	bool normal = selection_end_.x < 0 || 
		(selection_end_.y > selection_start_.y || (selection_end_.y == selection_start_.y && selection_end_.x >= selection_start_.x));
	if (exist_selection()) {
		SDL_Rect start, end;
		normalize_start_end(start, end);
		selection = integrate_->handle_selection(start.x, start.y, end.x, end.y, NULL);
	}

	// Get the input back and make ttext_box forget it
	real_value_ = label();
	ttext_box::set_label(std::string(utils::utf8str_len(real_value_), '*'));

	// maybe exist default-text in text_box.
	// See above
	size_t before_chars = utils::utf8str_len(before);
	selection_start_ = integrate_->editable_at2(before_chars);
	if (!selection.empty()) {
		size_t selection_chars = utils::utf8str_len(selection);
		size_t end = normal? before_chars + selection_chars: before_chars - selection_chars;
		selection_end_ = integrate_->editable_at2(end);
	}

	if (real_value_.empty()) {
		// FIXBUG!! backspace delete all text, but exist default text.
		int ii = 0;
		goto_start_of_data();
	}

	// Why do the selection functions not update
	// the canvas?
	update_canvas();
	set_dirty();
}

const std::string& tpassword_box::get_control_type() const
{
	static const std::string type = "password_box";
	return type;
}

} //namespace gui2

