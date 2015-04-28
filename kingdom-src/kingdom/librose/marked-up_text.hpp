/* $Id: marked-up_text.hpp 46186 2010-09-01 21:12:38Z silene $ */
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

#ifndef MARKED_UP_TEXT_HPP_INCLUDED
#define MARKED_UP_TEXT_HPP_INCLUDED


class CVideo;
struct surface;
#include <SDL_video.h>
#include <string>

namespace font {

/**
 * Determine if a wchar_t is a CJK character
 *
 * @retval true                   Input-char is a CJK char
 * @retval false                  Input-char is a not CJK char.
 */
bool is_cjk_char(const wchar_t ch);

/** Create string of color-markup, such as "<255,255,0>" for yellow. */
std::string color2markup(const SDL_Color &color);

/** Creates the hexadecimal string of a color, such as "#ffff00" for yellow. */
std::string color2hexa(const SDL_Color &color);

/**
 * Wrap text.
 *
 * - If the text exceedes the specified max width, wrap it one a word basis.
 * - If this is not possible, e.g. the word is too big to fit, wrap it on a
 * - char basis.
 */
std::string word_wrap_text(const std::string& unwrapped_text, int font_size,
	int max_width, int max_height = -1, int max_lines = -1, bool partial_line = false);

bool can_break(const wchar_t previous, const wchar_t ch);

} // end namespace font

#endif // MARKED_UP_TEXT_HPP_INCLUDED

