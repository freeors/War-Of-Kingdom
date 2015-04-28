/* $Id: message.cpp 48440 2011-02-07 20:57:31Z mordante $ */
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

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/message2.hpp"

#include "display.hpp"
#include "gettext.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "rose_config.hpp"

#include <boost/foreach.hpp>

namespace gui2 {

REGISTER_DIALOG(message2)

tmessage2::tmessage2(display& disp, const std::string& message, const std::string& ok_caption, const std::string& cancel_caption)
	: disp_(disp)
	, message_(message)
	, ok_caption_(ok_caption.empty()? _("OK"): ok_caption)
	, cancel_caption_(cancel_caption.empty()? _("Cancel"): cancel_caption)
{}

void tmessage2::pre_show(CVideo& /*video*/, twindow& window)
{
	window.set_canvas_variable("border", variant("default-border"));

	// ***** ***** ***** ***** Set up the widgets ***** ***** ***** *****
	tcontrol& label = find_widget<tcontrol>(&window, "label", false);
	label.set_label(message_);

	// The label might not always be a scroll_label but the capturing
	// shouldn't hurt.
	window.keyboard_capture(&label);

	tcontrol& ok = find_widget<tcontrol>(&window, "ok", false);
	ok.set_label(ok_caption_);

	tcontrol& cancel = find_widget<tcontrol>(&window, "cancel", false);
	cancel.set_label(cancel_caption_);
}


int show_message2(display& disp, const std::string& message, const std::string& ok_caption, const std::string& cancel_caption)
{
	tmessage2 dlg(disp, message, ok_caption, cancel_caption);
	dlg.show(disp.video());
	return dlg.get_retval();
}


} // namespace gui2

