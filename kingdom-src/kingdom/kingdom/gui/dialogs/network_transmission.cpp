/* $Id$ */
/*
   Copyright (C) 2011 Sergey Popov <loonycyborg@gmail.com>
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

#include "gui/dialogs/network_transmission.hpp"

#include "gettext.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/progress_bar.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "log.hpp"
#include "network.hpp"
#include "dialogs.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_DIALOG(network_transmission)

ttransfer::ttransfer(network_asio::connection& connection, int hidden_ms)
	: connection_(connection)
	, hidden_ticks_(hidden_ms * 1000)
	, track_upload_(false)
	, window_()
	, cancel_(NULL)
{
	start_ticks_ = SDL_GetTicks();
}

void ttransfer::pre_show(twindow& window)
{
	cancel_ = find_widget<tbutton>(&window, "_cancel", false, false);
	for (int i = 0; i < 4; i ++) {
		cancel_->canvas()[i].set_variable("image", variant("misc/delete.png"));
	}
	connect_signal_mouse_left_click(
		*cancel_
		, boost::bind(
		&ttransfer::cancel
		, this
		, boost::ref(window)));

	window_ = window;
	tlobby::thandler::join();
}

void ttransfer::handle_status(int at, tsock::ttype type)
{
	if (at != tlobby::tag_http) {
		return;
	}
	if (type == tsock::t_disconnected) {
		window_.get().set_retval(twindow::CANCEL);
	}
}

bool ttransfer::handle_raw2(int at, tsock::ttype type, const char* data, int len)
{
	if (at != tlobby::tag_http) {
		return false;
	}

	connection_.poll(data, len);
	if (!window_) return false;
	if (connection_.done()) {
		window_.get().set_retval(twindow::OK);

	} else {
		if (SDL_GetTicks() - start_ticks_ >= hidden_ticks_) {
			window_.get().set_visible(twidget::VISIBLE);
		}
		size_t completed, total;
		if (track_upload_) {
			completed = connection_.bytes_written();
			total = connection_.bytes_to_write();
		} else {
			completed = connection_.bytes_read();
			total = connection_.bytes_to_read();
		}

		if (total) {
			find_widget<tprogress_bar>(&(window_.get()), "_progress", false)
				.set_percentage((completed*100.)/total);

			std::stringstream ss;
			ss << utils::si_string(completed, true, _("unit_byte^B"))
			   << "/"
			   << utils::si_string(total, true, _("unit_byte^B"));

			find_widget<tlabel>(&(window_.get()), "_numeric_progress", false)
					.set_label(ss.str());
			window_.get().invalidate_layout();

		}
	}
	return false;
}

void ttransfer::cancel(gui2::twindow& window)
{
	window_.reset();
	connection_.set_done();

	window.set_retval(twindow::CANCEL);
}

tnetwork_transmission::tnetwork_transmission(network_asio::connection& connection, 
					const std::string& title, const std::string& subtitle, int hidden_ms)
	: ttransfer(connection, hidden_ms)
	, title_(title)
	, subtitle_(subtitle)
{
}

void tnetwork_transmission::pre_show(CVideo& /*video*/, twindow& window)
{
	// ***** ***** ***** ***** Set up the widgets ***** ***** ***** *****
	if (!title_.empty()) {
		find_widget<tlabel>(&window, "title", false).set_label(title_);
	}
	if (!subtitle_.empty()) {
		tlabel& subtitle_label = find_widget<tlabel>(&window, "subtitle", false);
		subtitle_label.set_label(subtitle_);
	}

	ttransfer::pre_show(window);

	window.set_visible(twidget::INVISIBLE);
}

void tnetwork_transmission::post_show(twindow& /*window*/)
{
}

} // namespace gui2

