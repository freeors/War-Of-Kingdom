/* $Id: mp_create_game_set_password.hpp 48936 2011-03-19 21:04:04Z mordante $ */
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

#ifndef GUI_DIALOGS_MP_CREATE_GAME_SET_PASSWORD_HPP_INCLUDED
#define GUI_DIALOGS_MP_CREATE_GAME_SET_PASSWORD_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

namespace gui2 {

class tmp_create_game_set_password : public tdialog
{
public:

	/**
	 * Constructor.
	 *
	 * @param password [in]       The initial value for the password.
	 * @param password [out]      The password selected by the user if the
	 *                            dialog returns @ref twindow::OK undefined
	 *                            otherwise.
	 */
	explicit tmp_create_game_set_password(std::string& password);

	/** The excute function see @ref tdialog for more information. */
	static bool execute(std::string& password, CVideo& video)
	{
		return tmp_create_game_set_password(password).show(video);
	}

private:

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);
};

} // namespace gui2

#endif /* ! GUI_DIALOGS_MP_CREATE_GAME_SET_PASSWORD_HPP_INCLUDED */