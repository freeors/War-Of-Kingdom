/* $Id: scroll_label.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#ifndef GUI_WIDGETS_TEXT_BOX2_HPP_INCLUDED
#define GUI_WIDGETS_TEXT_BOX2_HPP_INCLUDED

#include "gui/widgets/container.hpp"
#include "gui/widgets/text_box.hpp"

namespace gui2 {

class ttext_box;
class tbutton;

namespace implementation {
	struct tbuilder_text_box2;
}

/**
 * Label showing a text.
 *
 * This version shows a scrollbar if the text gets too long and has some
 * scrolling features. In general this widget is slower as the normal label so
 * the normal label should be preferred.
 */
class ttext_box2 : public tcontainer_
{
	friend struct implementation::tbuilder_text_box2;
public:
	ttext_box2();

	/** Inherited from tcontrol. */
	void set_text_editable(bool editable);

	/** Inherited from tcontainer_. */
	void set_self_active(const bool active)
		{ state_ = active ? ENABLED : DISABLED; }

	/** Inherited from ttext_box. */
	const std::string& label() const;
	void set_label(const std::string& text);

	void set_button_label(const std::string& text);

	void insert_img(const std::string& str);

	/** Inherited from tscrollbar_container. */
	bool content_empty() const;

	/***** ***** ***** setters / getters for members ***** ****** *****/

	bool get_active() const { return state_ != DISABLED; }

	unsigned get_state() const { return state_; }

	ttext_box& text_box() { return *text_box_; }
	const ttext_box& text_box() const { return *text_box_; }

	tbutton& button() { return *button_; }
	const tbutton& button() const { return *button_; }

	void set_text_changed_callback(boost::function< void (ttext_box2* widget) > cb)
	{
		text_changed_callback_ = cb;
	}

	void set_may_clear(bool value) { may_clear_ = value; }

private:
	void clear_text();
	void text_changed_callback(ttext_box* widget);
	void initial_subclass();

private:

	ttext_box* text_box_;
	tbutton* button_;

	boost::function< void (ttext_box2* widget) > text_changed_callback_;

	mutable std::string real_label_;
	bool may_clear_;

	/**
	 * Possible states of the widget.
	 *
	 * Note the order of the states must be the same as defined in settings.hpp.
	 */
	enum tstate { ENABLED, DISABLED, COUNT };

//  It's not needed for now so keep it disabled, no definition exists yet.
//	void set_state(const tstate state);

	/**
	 * Current state of the widget.
	 *
	 * The state of the widget determines what to render and how the widget
	 * reacts to certain 'events'.
	 */
	tstate state_;

	/***** ***** ***** inherited ****** *****/

	/** Inherited from tcontrol. */
	const std::string& get_control_type() const;

	/***** ***** ***** signal handlers ***** ****** *****/

	void signal_handler_left_button_down(const event::tevent event);
};

} // namespace gui2

#endif

