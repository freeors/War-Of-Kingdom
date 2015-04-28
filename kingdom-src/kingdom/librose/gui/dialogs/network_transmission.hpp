/* $Id$ */
/*
   Copyright (C) 2011 by Sergey Popov <loonycyborg@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_NETWORK_RECEIVE_HPP_INCLUDED
#define GUI_DIALOGS_NETWORK_RECEIVE_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gui/widgets/control.hpp"
#include "lobby.hpp"
#include <boost/optional.hpp>

class game_display;

namespace network_asio {

class connection;

}

namespace gui2 {

class tbutton;

class ttransfer : public tlobby::thandler
{
public:
	void handle_status(int at, tsock::ttype type);
	bool handle_raw2(int at, tsock::ttype type, const char* data, int len);
	
	explicit ttransfer(network_asio::connection& connection, int hidden_ms);

	void set_track_upload(bool track_upload) { track_upload_ = track_upload; }

protected:
	void pre_show(twindow& window);

	void cancel(twindow& window);

private:
	network_asio::connection& connection_;
	boost::optional<twindow&> window_;
	bool track_upload_;
	tbutton* cancel_;

	Uint32 start_ticks_;
	Uint32 hidden_ticks_;
};

class tnetwork_transmission;

/**
 * Dialog that tracks network transmissions
 *
 * It shows upload/download progress and allows the user
 * to cancel the transmission.
 */
class tnetwork_transmission : public tdialog, public ttransfer
{
public:
	tnetwork_transmission(network_asio::connection& connection, const std::string& title, const std::string& subtitle, int hidden_ms = 4);

	void set_subtitle(const std::string&);

protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

private:
	/** The title for the dialog. */
	std::string title_;

	/**
	 * The subtitle for the dialog.
	 *
	 * This field commenly shows the action in progress eg connecting,
	 * uploading, downloading etc..
	 */
	std::string subtitle_;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;
};

} // namespace gui2

#endif

