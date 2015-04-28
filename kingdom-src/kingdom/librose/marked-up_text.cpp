/* $Id: marked-up_text.cpp 46186 2010-09-01 21:12:38Z silene $ */
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

/**
 * @file
 * Support for simple markup in text (fonts, colors, images).
 * E.g. "@Victory" will be shown in green.
 */

#define GETTEXT_DOMAIN "rose-lib"

#include "global.hpp"

#include "font.hpp"
#include "gettext.hpp"
#include "marked-up_text.hpp"
#include "serialization/string_utils.hpp"
#include "video.hpp"
#include "wml_exception.hpp"

namespace font {

std::string color2markup(const SDL_Color &color)
{
	std::stringstream markup;
	// The RGB of SDL_Color are Uint8, we need to cast them to int.
	// Without cast, it gives their char equivalent.
	markup << static_cast<int>(color.r) << ","
		   << static_cast<int>(color.g) << ","
		   << static_cast<int>(color.b);
	return markup.str();
}

std::string color2hexa(const SDL_Color &color)
{
	char buf[7];
	sprintf(buf, "%02x%02x%02x", color.r, color.g, color.b);
	return buf;
}

bool is_cjk_char(const wchar_t c)
{
	/**
	 * You can check these range at http://unicode.org/charts/
	 * see the "East Asian Scripts" part.
	 * Notice that not all characters in that part is still in use today, so don't list them all here.
	 * Below are characters that I guess may be used in wesnoth translations.
	 */

	// cast to silence a windows warning (uses only 16bit for wchar_t)
	const unsigned int ch = static_cast<unsigned int>(c);

	//FIXME add range from Japanese-specific and Korean-specific section if you know the characters are used today.

	if (ch < 0x2e80) return false; // shorcut for common non-CJK

	return
		//Han Ideographs: all except Supplement
		(ch >= 0x4e00 && ch < 0x9fcf) ||
		(ch >= 0x3400 && ch < 0x4dbf) ||
		(ch >= 0x20000 && ch < 0x2a6df) ||
		(ch >= 0xf900 && ch < 0xfaff) ||
		(ch >= 0x3190 && ch < 0x319f) ||

		//Radicals: all except Ideographic Description
		(ch >= 0x2e80 && ch < 0x2eff) ||
		(ch >= 0x2f00 && ch < 0x2fdf) ||
		(ch >= 0x31c0 && ch < 0x31ef) ||

		//Chinese-specific: Bopomofo
		(ch >= 0x3000 && ch < 0x303f) ||

		//Japanese-specific: Halfwidth Katakana
		(ch >= 0xff00 && ch < 0xffef) ||

		//Japanese-specific: Hiragana, Katakana
		(ch >= 0x3040 && ch <= 0x309f) ||
		(ch >= 0x30a0 && ch <= 0x30ff) ||

		//Korean-specific: Hangul Syllables, Halfwidth Jamo
		(ch >= 0xac00 && ch < 0xd7af) ||
		(ch >= 0xff00 && ch < 0xffef);
}

static void cut_word(std::string& line, std::string& word, int font_size, int style, int max_width)
{
	std::string tmp = line;
	utils::utf8_iterator tc(word);
	bool first = true;

	for(;tc != utils::utf8_iterator::end(word); ++tc) {
		tmp.append(tc.substr().first, tc.substr().second);
		SDL_Rect tsize = line_size(tmp, font_size, style);
		if(tsize.w > max_width) {
			const std::string& w = word;
			if(line.empty() && first) {
				line += std::string(w.begin(), tc.substr().second);
				word = std::string(tc.substr().second, w.end());
			} else {
				line += std::string(w.begin(), tc.substr().first);
				word = std::string(tc.substr().first, w.end());
			}
			break;
		}
		first = false;
	}
}

namespace {

/*
 * According to Kinsoku-Shori, Japanese rules about line-breaking:
 *
 * * the following characters cannot begin a line (so we will never break before them):
 * 、。，．）〕］｝〉》」』】’”ゝゞヽヾ々？！：；ぁぃぅぇぉゃゅょゎァィゥェォャュョヮっヵッヶ?…ー
 *
 * * the following characters cannot end a line (so we will never break after them):
 * （〔［｛〈《「『【‘“
 *
 * Unicode range that concerns word wrap for Chinese:
 *   全角ASCII、全角中英文标点 (Fullwidth Character for ASCII, English punctuations and part of Chinese punctuations)
 *   http://www.unicode.org/charts/PDF/UFF00.pdf
 *   CJK 标点符号 (CJK punctuations)
 *   http://www.unicode.org/charts/PDF/U3000.pdf
 */
inline bool no_break_after(const wchar_t ch)
{
	return
		/**
		 * don't break after these Japanese characters
		 */
		ch == 0x2018 || ch == 0x201c || ch == 0x3008 || ch == 0x300a || ch == 0x300c ||
		ch == 0x300e || ch == 0x3010 || ch == 0x3014 || ch == 0xff08 || ch == 0xff3b ||
		ch == 0xff5b ||

		/**
		 * FIXME don't break after these Korean characters
		 */

		/**
		 * don't break after these Chinese characters
		 * contains left side of different kinds of brackets and quotes
		 */
		ch == 0x3014 || ch == 0x3016 || ch == 0x3008 || ch == 0x301a || ch == 0x3008 ||
		ch == 0x300a || ch == 0x300c || ch == 0x300e || ch == 0x3010 || ch == 0x301d;
}

inline bool no_break_before(const wchar_t ch)
{
	return
		/**
		 * don't break before these Japanese characters
		 */
		ch == 0x2019 || ch == 0x201d || ch == 0x2026 || ch == 0x3001 || ch == 0x3002 ||
		ch == 0x3005 || ch == 0x3009 || ch == 0x300b || ch == 0x300d || ch == 0x300f ||
		ch == 0x3011 || ch == 0x3015 || ch == 0x3041 || ch == 0x3043 || ch == 0x3045 ||
		ch == 0x3047 || ch == 0x3049 || ch == 0x3063 || ch == 0x3083 || ch == 0x3085 ||
		ch == 0x3087 || ch == 0x308e || ch == 0x309d || ch == 0x309e || ch == 0x30a1 ||
		ch == 0x30a3 || ch == 0x30a5 || ch == 0x30a7 || ch == 0x30a9 || ch == 0x30c3 ||
		ch == 0x30e3 || ch == 0x30e5 || ch == 0x30e7 || ch == 0x30ee || ch == 0x30f5 ||
		ch == 0x30f6 || ch == 0x30fb || ch == 0x30fc || ch == 0x30fd || ch == 0x30fe ||
		ch == 0xff01 || ch == 0xff09 || ch == 0xff0c || ch == 0xff0e || ch == 0xff1a ||
		ch == 0xff1b || ch == 0xff1f || ch == 0xff3d || ch == 0xff5d ||

		/**
		 * FIXME don't break before these Korean characters
		 */

		/**
		 * don't break before these Chinese characters
		 * contains
		 *   many Chinese punctuations that should not start a line
		 *   and right side of different kinds of brackets, quotes
		 */
		ch == 0x3001 || ch == 0x3002 || ch == 0x301c || ch == 0xff01 || ch == 0xff0c ||
		ch == 0xff0d || ch == 0xff0e || ch == 0xff1a || ch == 0xff1b || ch == 0xff1f ||
		ch == 0xff64 || ch == 0xff65 || ch == 0x3015 || ch == 0x3017 || ch == 0x3009 ||
		ch == 0x301b || ch == 0x3009 || ch == 0x300b || ch == 0x300d || ch == 0x300f ||
		ch == 0x3011 || ch == 0x301e;
}

inline bool break_before(const wchar_t ch)
{
	if(no_break_before(ch))
		return false;

	return is_cjk_char(ch);
}

inline bool break_after(const wchar_t ch)
{
	if(no_break_after(ch))
		return false;

	return is_cjk_char(ch);
}

} // end of anon namespace

std::string word_wrap_text(const std::string& unwrapped_text, int font_sz,
	int max_width, int max_height, int max_lines, bool partial_line)
{
	VALIDATE(max_width > 0, _("The maximum text width is less than 1."));

	const int attention_min_chars = 15;
	utils::utf8_iterator ch(unwrapped_text);
	std::string current_word;
	std::string current_line;
	size_t line_width = 0;
	size_t current_height = 0;
	bool current_word_result_cut;
	bool line_break = false;
	bool first = true;
	bool start_of_line = true;
	std::string wrapped_text;
	int style = TTF_STYLE_NORMAL;
	utils::utf8_iterator end = utils::utf8_iterator::end(unwrapped_text);

	while (1) {
		if (start_of_line) {
			line_width = 0;
			start_of_line = false;
		}

		// If there is no current word, get one
		if (current_word.empty() && ch == end) {
			break;
		} else if (current_word.empty()) {
			current_word_result_cut = false;
			if (*ch == ' ' || *ch == '\n') {
				current_word = *ch;
				++ ch;
			} else {
				wchar_t previous = 0;
				int chars = 0;
				for (; ch != utils::utf8_iterator::end(unwrapped_text) && *ch != ' ' && *ch != '\n'; ++ch) {

					if (!current_word.empty() && break_before(*ch) && !no_break_after(previous)) {
						break;
					}

					if (!current_word.empty() && break_after(previous) && !no_break_before(*ch)) {
						break;
					}

					current_word.append(ch.substr().first, ch.substr().second);

					previous = *ch;

					chars ++;
					if (chars > attention_min_chars) {
						if (line_size(current_word, font_sz, style).w + (int)line_width > max_width) {
							size_t last_char_size = ch.substr().second - ch.substr().first;
							current_word.erase(current_word.size() - last_char_size);

							// current_word maybe require git rid of. so don't forbit end.

							// current_line += current_word;
							// current_word_result_cut = true;
							break;
						}
					}
				}
			}
		}

		if (current_word == "\n" || current_word_result_cut) {
			line_break = true;
			current_word.clear();
			start_of_line = true;
		} else {

			const size_t word_width = line_size(current_word, font_sz, style).w;

			line_width += word_width;

			if (static_cast<long>(line_width) > max_width) {
				if (!partial_line && static_cast<long>(word_width) > max_width) {
					cut_word(current_line, current_word, font_sz, style, max_width);
				}
				if (current_word == " ") {
					current_word = "";
				}
				line_break = true;

			} else {
				current_line += current_word;
				current_word = "";

			}
		}

		if (line_break || (current_word.empty() && ch == end)) {

			SDL_Rect size = line_size(current_line, font_sz, style);
			if (max_height > 0 && current_height + size.h >= size_t(max_height)) {
				return wrapped_text;
			}

			if (!first) {
				wrapped_text += '\n';
			}

			wrapped_text += current_line;
			current_line.clear();
			line_width = 0;
			current_height += size.h;
			line_break = false;
			first = false;

			if (--max_lines == 0) {
				return wrapped_text;
			}
		}
	}
	return wrapped_text;
}

bool can_break(const wchar_t previous, const wchar_t ch)
{
	if (break_before(ch) && !no_break_after(previous))
		return true;

	if (break_after(previous) && !no_break_before(ch))
		return true;

	return false;
}

} // end namespace font

