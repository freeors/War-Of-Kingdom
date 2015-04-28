/* $Id: scroll_label.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2008 - 2012 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/widgets/text_box2.hpp"

#include "gui/auxiliary/widget_definition/text_box2.hpp"
#include "gui/auxiliary/window_builder/text_box2.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/window.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_WIDGET(text_box2)

ttext_box2::ttext_box2()
	: tcontainer_(COUNT)
	, state_(ENABLED)
	, text_box_(NULL)
	, button_(NULL)
	, may_clear_(true)
{
	connect_signal<event::LEFT_BUTTON_DOWN>(
			  boost::bind(
				    &ttext_box2::signal_handler_left_button_down
				  , this
				  , _2)
			, event::tdispatcher::back_pre_child);
}

void ttext_box2::initial_subclass()
{
	VALIDATE(!text_box_ && !button_, null_str);

	text_box_ = dynamic_cast<ttext_box*>(grid_.find("_text_box", false));
	text_box_->set_text_changed_callback(boost::bind(&ttext_box2::text_changed_callback, this, _1));

	button_ = dynamic_cast<tbutton*>(grid_.find("_button", false));
	
	if (may_clear_) {
		connect_signal_mouse_left_click(
			*button_
			, boost::bind(
				&ttext_box2::clear_text
				, this));

		button_->set_label("misc/delete2.png");
		button_->set_visible(twidget::HIDDEN);
	}
}

bool ttext_box2::content_empty() const
{
	return text_box_->label().empty();
}

void ttext_box2::set_text_editable(bool editable)
{
	ttext_box* widget = find_widget<ttext_box>(&grid_, "_text_box", false, true);
	widget->set_text_editable(editable);
}

const std::string& ttext_box2::label() const
{
	return real_label_;
}

void ttext_box2::set_label(const std::string& text)
{
	if (text_box_) {
		if (text == text_box_->get_value()) {
			return;
		}
		text_box_->set_value(text);
	}
}

void ttext_box2::set_button_label(const std::string& text)
{
	if (button_) {
		button_->set_label(text);
	}
}

void ttext_box2::clear_text()
{
	text_box_->set_label(null_str);
	get_window()->keyboard_capture(text_box_);
}

void ttext_box2::text_changed_callback(ttext_box* widget)
{
	if (may_clear_) {
		real_label_ = widget->get_value();
		button_->set_visible(real_label_.empty()? twidget::HIDDEN: twidget::VISIBLE);
	}
	if (text_changed_callback_) {
		text_changed_callback_(this);
	}
}

const std::string& ttext_box2::get_control_type() const
{
	static const std::string type = "text_box2";
	return type;
}

void ttext_box2::signal_handler_left_button_down(const event::tevent event)
{
	get_window()->keyboard_capture(this);
}

} // namespace gui2

