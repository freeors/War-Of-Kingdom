/* $Id: campaign_difficulty.hpp 49603 2011-05-22 17:56:17Z mordante $ */
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

#ifndef GUI_DIALOGS_GUIDE_HPP_INCLUDED
#define GUI_DIALOGS_GUIDE_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include <vector>

class display;

namespace gui2 {

class tcontrol;
class ttrack;

class tguide_ : public tdialog
{
public:
	explicit tguide_(display& disp, const std::vector<std::string>& items, int retval);

protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	virtual void callback_timer(ttrack& widget, surface& frame_buffer, const tpoint& offset, int state2, bool from_timer, int drag_offset_x);
	virtual void set_retval(twindow& window, int retval);

	virtual void post_blit(surface& frame_buffer, int left_sel, const SDL_Rect& left_src, const SDL_Rect& left_dst, int right_sel, const SDL_Rect& right_src, const SDL_Rect& right_dst) {}

private:
	bool callback_control_drag_detect(tcontrol* control, bool start, const twidget::tdrag_direction type);
	bool callback_set_drag_coordinate(tcontrol* control, const tpoint& first, const tpoint& last);

protected:
	display& disp_;
	std::vector<std::string> items_;
	std::vector<surface> surfs_;
	ttrack* track_;

	bool can_exit_;
	int cursel_;
	int retval_;
};


class tguide: public tguide_
{
public:
	explicit tguide(display& disp, const std::vector<std::string>& items, const std::string& startup_img, const int percent = 70);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	void post_blit(surface& frame_buffer, int left_sel, const SDL_Rect& left_src, const SDL_Rect& left_dst, int right_sel, const SDL_Rect& right_src, const SDL_Rect& right_dst);
	void set_retval(twindow& window, int retval);

private:
	const std::string startup_img_;
	const int percent_;
	SDL_Rect startup_rect_;
};

}


#endif /* ! GUI_DIALOGS_CAMPAIGN_DIFFICULTY_HPP_INCLUDED */
