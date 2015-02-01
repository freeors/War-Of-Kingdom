/* $Id: password_box.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#ifndef GUI_WIDGETS_PASSWORD_BOX_HPP_INCLUDED
#define GUI_WIDGETS_PASSWORD_BOX_HPP_INCLUDED

#include "gui/widgets/text_box.hpp"


/**
 * A class inherited from ttext_box that displays
 * its input as stars
 *
 * @todo This implementation is quite a hack that
 * needs to be rewritten cleanly
 */
namespace gui2 {

class tpassword_box : public ttext_box {

// The hack works like this: we add the member real_value_
// that holds the actual user input.
// Overridden functions now simply
//  - call set_value() from ttext_box with real_value_,
//    which is done in prefunction()
//  - call ttext_box::overridden_function()
//  - set real_value_ to get_value() from ttext_box and
//    call set_value() from ttext_box with real_value_
//    turned into stars, which is done in post_function()
//
// and overridden function should therefore look like this:
//
// overridden_function(some parameter) {
// 	pre_function();
// 	ttext_box::overridden_function(some parameter);
// 	post_function();
// }

public:
	tpassword_box() 
		: ttext_box()
		, real_value_()
		, value_()
		, in_operator_(false)
	{}

	/** Inherited from ttext_. */
	virtual void set_label(const std::string& label);
	virtual const std::string& label() const;

	std::string get_real_value() const { return real_value_; }
	void set_maximum_length(const size_t maximum_length);

	void set_in_operator(bool val) { in_operator_ = val; }
	bool get_in_operator() const { return in_operator_; }

protected:
	// Overwritten functions must of course be virtual!
	void insert_str(const std::string& str);
	void delete_char(const bool before_cursor);

	void paste_selection(const bool mouse);

	// We do not override copy_selection because we
	// actually want it to copy just the stars

private:
	void handle_key_backspace(SDLMod modifier, bool& handled);
	void handle_key_delete(SDLMod modifier, bool& handled);

	void pre_function();
	void post_function();

	std::string real_value_;
	mutable std::string value_;
	bool in_operator_;

	/** Inherited from ttext_box. */
	const std::string& get_control_type() const;
};

} //namespace gui2

#endif

