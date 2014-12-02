/* $Id: text.hpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
/*
   Copyright (C) 2008 - 2010 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef TEXT_HPP_INCLUDED
#define TEXT_HPP_INCLUDED

#include "sdl_utils.hpp"

#include <boost/noncopyable.hpp>

#include <string>

struct language_def;
class text_surface;

namespace gui2 {
	struct tpoint;
} // namespace gui2;

namespace font {

// add background color and also font markup.

/**
 * Text class.
 *
 * This class stores the text to draw and uses pango with the cairo backend to
 * render the text. See http://pango.org for more info.
 */
class ttext
	: private boost::noncopyable
{
public:

	ttext();

	~ttext();

	/**
	 * Returns the rendered text.
	 *
	 * Before rendering it tests whether a redraw is needed and if so it first
	 * redraws the surface before returning it.
	 */
	surface render();

	/** Returns the width needed for the text. */
	int get_width();

	/** Returns the height needed for the text. */
	int get_height();

	/** Returns the size needed for the text. */
	gui2::tpoint get_size();

	/** Has the text been truncated? */
	bool is_truncated() const;

	/**
	 * Inserts utf 8 text.
	 *
	 * @param offset              The position to insert the text.
	 * @param text                The utf-8 text to insert.
	 *
	 * @returns                   The number of characters inserted.
	 */
	unsigned insert_text(const unsigned offset, const std::string& text);

	/**
	 * Inserts a unicode char.
	 *
	 * @param offset              The position to insert the char.
	 * @param unicode             The character to insert.
	 *
	 * @returns                   True upon success, false otherwise.
	 */
	bool insert_unicode(const unsigned offset, const wchar_t unicode);

	/**
	 * Inserts unicode text.
	 *
	 * @param offset              The position to insert the text.
	 * @param unicode             Vector with characters to insert.
	 *
	 * @returns                   The number of characters inserted.
	 */
	unsigned insert_unicode(
		const unsigned offset, const std::vector<wchar_t>& unicode);

	/***** ***** ***** ***** Font flags ***** ***** ***** *****/

	/**
	 * The flags have the same values as the ones in SDL_TTF so it's easy to mix
	 * them for now. To avoid including SDL_TTF in the header they're only
	 * declared here. Once SDL_TTF is removed they can be moved in the header.
	 */

	static const unsigned STYLE_NORMAL;     /**< Normal text. */
	static const unsigned STYLE_BOLD;       /**< Bold text. */
	static const unsigned STYLE_ITALIC;     /**< Italicized text. */
	static const unsigned STYLE_UNDERLINE;  /**< Underlined text. */

	/***** ***** ***** ***** Query details ***** ***** ***** *****/

	/**
	 * Gets the location for the cursor.
	 *
	 * @param column              The column offset of the cursor.
	 * @param line                The line offset of the cursor.
	 *
	 * @returns                   The position of the top of the cursor. It the
	 *                            requested location is out of range 0,0 is
	 *                            returned.
	 */
	gui2::tpoint get_cursor_position(
		const unsigned column, const unsigned line = 0);

	/**
	 * Gets the column of line of the character at the position.
	 *
	 * @param position            The pixel position in the text area.
	 *
	 * @returns                   A point with the x value the column and the y
	 *                            value the line of the character found (or last
	 *                            character if not found.
	 */
	gui2::tpoint get_column_line(const gui2::tpoint& position);

	/**
	 * Gets the length of the text in characters.
	 *
	 * The text set is utf-8 so the length of the string might not be the length
	 * of the text.
	 */
	size_t get_length();

	/**
	 * Sets the text to render.
	 *
	 * @param text                The text to render.
	 * @param markedup            Should the text be rendered with pango
	 *                            markup. If the markup is invalid it's
	 *                            rendered as text without markup.
	 *
	 * @returns                   The status, if rendered as markup and the
	 *                            markup contains errors, false is returned
	 *                            else true.
	 */
	bool set_text(const std::string& text, const bool markedup);

	/***** ***** ***** ***** Setters / getters ***** ***** ***** *****/

	const std::string& text() const { return text_; }

	ttext& set_font_size(const unsigned font_size);

	ttext& set_font_style(const unsigned font_style);

	ttext& set_foreground_color(const Uint32 color);

	ttext& set_maximum_width(int width);

	ttext& set_characters_per_line(const unsigned characters_per_line);

	ttext& set_maximum_height(int height, bool multiline = true);

	ttext& set_ellipse_mode(const PangoEllipsizeMode ellipse_mode);

	ttext &set_alignment(const PangoAlignment alignment);

	ttext& set_maximum_length(const size_t maximum_length);

	void set_dirty(bool dirty = true);

private:

	std::vector<text_surface> cached_surfs_;
	/***** ***** ***** *****  Pango variables ***** ***** ***** *****/
	mutable SDL_Rect rect_;

	/** The surface to render upon used as a cache. */
	mutable surface surface_;

	/** The text to draw (stored as utf-8). */
	std::string text_;

	/** Is the text markedup if so the markedup render routines need to be used. */
	bool markedup_text_;

	/** The font size to draw. */
	unsigned font_size_;

	/** The style of the font, this is an orred mask of the font flags. */
	unsigned font_style_;

	/** The foreground color. */
	Uint32 foreground_color_;

	/**
	 * The maximum width of the text.
	 *
	 * Values less or equal to 0 mean no maximum and are internally stored as
	 * -1, since that's the value pango uses for it.
	 */
	int maximum_width_;

	/**
	 * The maximum height of the text.
	 *
	 * Values less or equal to 0 mean no maximum and are internally stored as
	 * -1, since that's the value pango uses for it.
	 */
	int maximum_height_;

	/** The way too long text is shown depends on this mode. */
	PangoEllipsizeMode ellipse_mode_;

	/** The maximum length of the text. */
	size_t maximum_length_;

	/**
	 * The text has two dirty states:
	 * - The setting of the state and the size calculations.
	 * - The rendering of the surface.
	 */

	/** The dirty state of the calculations. */
	mutable bool calculation_dirty_;

	/** Length of the text. */
	mutable size_t length_;

	/**
	 * Recalculates the text.
	 *
	 * When the text is recalculated the surface is dirtied.
	 *
	 * @param force               Recalculate even if not dirty?
	 */
	void recalculate();

	/** The dirty state of the surface. */
	mutable bool surface_dirty_;
};

} // namespace font

#endif

