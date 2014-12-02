/* $Id: unit_display.cpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/** @file */

#include "global.hpp"
#include "area_anim.hpp"
#include "anim_display.hpp"

#include "display.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"

#include "sound.hpp"
#include <boost/foreach.hpp>

namespace anim_display {

void title_screen_finish(animation* anim)
{
}

int start_title_screen_anim()
{
	display& disp = *display::get_singleton();
	float_animation* anim = start_float_anim_th(disp, area_anim::UPGRADE);
	anim->set_scale(800, 600, true, false);
	anim->set_callback_finish(title_screen_finish);

	std::stringstream strstr;
	anim->replace_image_name("__bg.png", "tactic/bg-single-increase.png");
	anim->replace_image_name("__id.png", "hero-256/116.png");
	anim->replace_static_text("__formation_name", "");
	anim->replace_static_text("__formation_description", "Upgrade");

	start_float_anim_bh(*anim);

	return 0;
}

} // end anim_display namespace
