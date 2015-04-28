/* $Id: message.hpp 48440 2011-02-07 20:57:31Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_MESSAGE2_HPP_INCLUDED
#define GUI_DIALOGS_MESSAGE2_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gui/widgets/control.hpp"

class display;

namespace gui2 {

/**
 * Main class to show messages to the user.
 *
 * It can be used to show a message or ask a result from the user. For the
 * most common usage cases there are helper functions defined.
 */
class tmessage2 : public tdialog
{
public:
	tmessage2(display& disp, const std::string& message, const std::string& ok_caption = null_str, const std::string& cancel_caption = null_str);

protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

private:
	display& disp_;

	/** The message to show to the user. */
	std::string message_;

	std::string ok_caption_;
	std::string cancel_caption_;
};

int show_message2(display& disp, const std::string& message, const std::string& ok_caption = null_str, const std::string& cancel_caption = null_str);

} // namespace gui2

#endif

