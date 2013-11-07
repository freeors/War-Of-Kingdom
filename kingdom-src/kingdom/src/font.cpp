/* $Id: font.cpp 47191 2010-10-24 18:10:29Z mordante $ */
/* vim:set encoding=utf-8: */
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

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "global.hpp"

#include "config.hpp"
#include "filesystem.hpp"
#include "font.hpp"
#include "game_config.hpp"
#include "log.hpp"
#include "marked-up_text.hpp"
#include "text.hpp"
#include "tooltips.hpp"
#include "video.hpp"
#include "serialization/parser.hpp"
#include "serialization/preprocessor.hpp"
#include "serialization/string_utils.hpp"
#include "help.hpp"
#include "gui/widgets/helper.hpp"
#include "wml_exception.hpp"

#include <boost/foreach.hpp>
#include <list>
#include <set>
#include <stack>

static lg::log_domain log_font("font");
#define DBG_FT LOG_STREAM(debug, log_font)
#define LOG_FT LOG_STREAM(info, log_font)
#define WRN_FT LOG_STREAM(warn, log_font)
#define ERR_FT LOG_STREAM(err, log_font)

#ifdef	HAVE_FRIBIDI
#include <fribidi.h>
#endif

// Signed int. Negative values mean "no subset".
typedef int subset_id;

struct font_id
{
	font_id(subset_id subset, int size) : subset(subset), size(size) {};
	bool operator==(const font_id& o) const
	{
		return subset == o.subset && size == o.size;
	};
	bool operator<(const font_id& o) const
	{
		return subset < o.subset || (subset == o.subset && size < o.size);
	};

	subset_id subset;
	int size;
};

static std::map<font_id, TTF_Font*> font_table;
static std::vector<std::string> font_names;

struct text_chunk
{
	text_chunk(subset_id subset) :
		subset(subset),
		text()
	{
	}

	bool operator==(text_chunk const & t) const { return subset == t.subset && text == t.text; }
	bool operator!=(text_chunk const & t) const { return !operator==(t); }

	subset_id subset;
	std::string text;
};

struct char_block_map
{
	char_block_map()
		: cbmap()
	{
	}

	typedef std::pair<int, subset_id> block_t;
	typedef std::map<int, block_t> cbmap_t;
	cbmap_t cbmap;
	/** Associates not-associated parts of a range with a new font. */
	void insert(int first, int last, subset_id id)
	{
		if (first > last) return;
		cbmap_t::iterator i = cbmap.lower_bound(first);
		// At this point, either first <= i->first or i is past the end.
		if (i != cbmap.begin()) {
			cbmap_t::iterator j = i;
			--j;
			if (first <= j->second.first /* prev.last */) {
				insert(j->second.first + 1, last, id);
				return;
			}
		}
		if (i != cbmap.end()) {
			if (/* next.first */ i->first <= last) {
				insert(first, i->first - 1, id);
				return;
			}
		}
		cbmap.insert(std::make_pair(first, block_t(last, id)));
	}
	/**
	 * Compresses map by merging consecutive ranges with the same font, even
	 * if there is some unassociated ranges inbetween.
	 */
	void compress()
	{
		LOG_FT << "Font map size before compression: " << cbmap.size() << " ranges\n";
		cbmap_t::iterator i = cbmap.begin(), e = cbmap.end();
		while (i != e) {
			cbmap_t::iterator j = i;
			++j;
			if (j == e || i->second.second != j->second.second) {
				i = j;
				continue;
			}
			i->second.first = j->second.first;
			cbmap.erase(j);
		}
		LOG_FT << "Font map size after compression: " << cbmap.size() << " ranges\n";
	}
	subset_id get_id(int ch)
	{
		cbmap_t::iterator i = cbmap.upper_bound(ch);
		// At this point, either ch < i->first or i is past the end.
		if (i != cbmap.begin()) {
			--i;
			if (ch <= i->second.first /* prev.last */)
				return i->second.second;
		}
		return -1;
	}
};

static char_block_map char_blocks;

//cache sizes of small text
typedef std::map<std::string,SDL_Rect> line_size_cache_map;

//map of styles -> sizes -> cache
static std::map<int,std::map<int,line_size_cache_map> > line_size_cache;

//Splits the UTF-8 text into text_chunks using the same font.
static std::vector<text_chunk> split_text(std::string const & utf8_text) {
	text_chunk current_chunk(0);
	std::vector<text_chunk> chunks;

	if (utf8_text.empty())
		return chunks;

	try {
		utils::utf8_iterator ch(utf8_text);
		int sub = char_blocks.get_id(*ch);
		if (sub >= 0) current_chunk.subset = sub;
		for(utils::utf8_iterator end = utils::utf8_iterator::end(utf8_text); ch != end; ++ch)
		{
			sub = char_blocks.get_id(*ch);
			if (sub >= 0 && sub != current_chunk.subset) {
				chunks.push_back(current_chunk);
				current_chunk.text.clear();
				current_chunk.subset = sub;
			}
			current_chunk.text.append(ch.substr().first, ch.substr().second);
		}
		if (!current_chunk.text.empty()) {
			chunks.push_back(current_chunk);
		}
	}
	catch(utils::invalid_utf8_exception e) {
		WRN_FT << "Invalid UTF-8 string: \"" << utf8_text << "\"\n";
	}
	return chunks;
}

static TTF_Font* open_font(const std::string& fname, int size)
{
	std::string name;
	if(!game_config::path.empty()) {
		name = game_config::path + "/fonts/" + fname;
		if(!file_exists(name)) {
			name = "fonts/" + fname;
			if(!file_exists(name)) {
				name = fname;
				if(!file_exists(name)) {
					ERR_FT << "Failed opening font: '" << name << "': No such file or directory\n";
					return NULL;
				}
			}
		}

	} else {
		name = "fonts/" + fname;
		if(!file_exists(name)) {
			if(!file_exists(fname)) {
				ERR_FT << "Failed opening font: '" << name << "': No such file or directory\n";
				return NULL;
			}
			name = fname;
		}
	}

	TTF_Font* font = TTF_OpenFont(name.c_str(),size);
	if(font == NULL) {
		ERR_FT << "Failed opening font: TTF_OpenFont: " << TTF_GetError() << "\n";
		return NULL;
	}

	return font;
}

static TTF_Font* get_font(font_id id)
{
	const std::map<font_id, TTF_Font*>::iterator it = font_table.find(id);
	if(it != font_table.end())
		return it->second;

	if(id.subset < 0 || size_t(id.subset) >= font_names.size())
		return NULL;

	TTF_Font* font = open_font(font_names[id.subset], id.size);

	if(font == NULL)
		return NULL;

	TTF_SetFontStyle(font,TTF_STYLE_NORMAL);

	LOG_FT << "Inserting font...\n";
	font_table.insert(std::pair<font_id,TTF_Font*>(id, font));
	return font;
}

static void clear_fonts()
{
	for(std::map<font_id,TTF_Font*>::iterator i = font_table.begin(); i != font_table.end(); ++i) {
		TTF_CloseFont(i->second);
	}

	font_table.clear();
	font_names.clear();
	char_blocks.cbmap.clear();
	line_size_cache.clear();
}

namespace {

struct font_style_setter
{
	font_style_setter(TTF_Font* font, int style) : font_(font), old_style_(0)
	{
		if(style == 0) {
			style = TTF_STYLE_NORMAL;
		}

		old_style_ = TTF_GetFontStyle(font_);

		// I thought I had killed this. Now that we ship SDL_TTF, we
		// should fix the bug directly in SDL_ttf instead of disabling
		// features. -- Ayin 25/2/2005
#if 0
		//according to the SDL_ttf documentation, combinations of
		//styles may cause SDL_ttf to segfault. We work around this
		//here by disallowing combinations of styles

		if((style&TTF_STYLE_UNDERLINE) != 0) {
			//style = TTF_STYLE_NORMAL; //TTF_STYLE_UNDERLINE;
			style = TTF_STYLE_UNDERLINE;
		} else if((style&TTF_STYLE_BOLD) != 0) {
			style = TTF_STYLE_BOLD;
		} else if((style&TTF_STYLE_ITALIC) != 0) {
			//style = TTF_STYLE_NORMAL; //TTF_STYLE_ITALIC;
			style = TTF_STYLE_ITALIC;
		}
#endif

		TTF_SetFontStyle(font_, style);
	}

	~font_style_setter()
	{
		TTF_SetFontStyle(font_,old_style_);
	}

private:
	TTF_Font* font_;
	int old_style_;
};

}

namespace font {

manager::manager()
{
	const int res = TTF_Init();
	if(res == -1) {
		ERR_FT << "Could not initialize true type fonts\n";
		throw error();
	} else {
		LOG_FT << "Initialized true type fonts\n";
	}

	init();
}

manager::~manager()
{
	deinit();

	clear_fonts();
	TTF_Quit();
}

void manager::update_font_path() const
{
	deinit();
	init();
}

void manager::init() const
{
}

void manager::deinit() const
{
}

//structure used to describe a font, and the subset of the Unicode character
//set it covers.
struct subset_descriptor
{
	subset_descriptor() :
		name(),
		present_codepoints()
	{
	}

	std::string name;
	typedef std::pair<int, int> range;
	std::vector<range> present_codepoints;
};

//sets the font list to be used.
static void set_font_list(const std::vector<subset_descriptor>& fontlist)
{
	clear_fonts();

	std::vector<subset_descriptor>::const_iterator itor;
	for(itor = fontlist.begin(); itor != fontlist.end(); ++itor) {
		// Insert fonts only if the font file exists
		if(game_config::path.empty() == false) {
			if(!file_exists(game_config::path + "/fonts/" + itor->name)) {
				if(!file_exists("fonts/" + itor->name)) {
					if(!file_exists(itor->name)) {
					WRN_FT << "Failed opening font file '" << itor->name << "': No such file or directory\n";
					continue;
					}
				}
			}
		} else {
			if(!file_exists("fonts/" + itor->name)) {
				if(!file_exists(itor->name)) {
					WRN_FT << "Failed opening font file '" << itor->name << "': No such file or directory\n";
					continue;
				}
			}
		}
		const subset_id subset = font_names.size();
		font_names.push_back(itor->name);

		BOOST_FOREACH (const subset_descriptor::range &cp_range, itor->present_codepoints) {
			char_blocks.insert(cp_range.first, cp_range.second, subset);
		}
	}
	char_blocks.compress();
}

const SDL_Color NORMAL_COLOR = {0xDD,0xDD,0xDD,0},
                GRAY_COLOR   = {0x77,0x77,0x77,0},
                LOBBY_COLOR  = {0xBB,0xBB,0xBB,0},
                GOOD_COLOR   = {0x00,0xFF,0x00,0},
                BAD_COLOR    = {0xFF,0x00,0x00,0},
                BLACK_COLOR  = {0x00,0x00,0x00,0},
                YELLOW_COLOR = {0xFF,0xFF,0x00,0},
                BUTTON_COLOR = {0xBC,0xB0,0x88,0},
                PETRIFIED_COLOR = {0xA0,0xA0,0xA0,0},
                TITLE_COLOR  = {0xBC,0xB0,0x88,0},
				LABEL_COLOR  = {0x6B,0x8C,0xFF,0},
				BLUE_COLOR   = {0x00,0x00,0xFF,0}, 
				BIGMAP_COLOR = {0xFF,0xFF,0xFF,0};
const SDL_Color DISABLED_COLOR = inverse(PETRIFIED_COLOR);

}

static const size_t max_text_line_width = 4096;

class text_surface
{
public:
	text_surface(std::string const &str, int size, SDL_Color color, int style);
	text_surface(int size, SDL_Color color, int style);
	void set_text(std::string const &str);

	size_t measure_partial(const unsigned column);

	void measure() const;
	size_t width() const;
	size_t height() const;
#ifdef	HAVE_FRIBIDI
	bool is_rtl() const { return is_rtl_; }	// Right-To-Left alignment
#endif
	std::vector<surface> const & get_surfaces() const;

	bool operator==(text_surface const &t) const {
		return hash_ == t.hash_ && font_size_ == t.font_size_
			&& color_ == t.color_ && style_ == t.style_ && str_ == t.str_;
	}
	bool operator!=(text_surface const &t) const { return !operator==(t); }
private:
	int hash_;
	int font_size_;
	SDL_Color color_;
	int style_;
	mutable int w_, h_;
	std::string str_;
	mutable bool initialized_;
	mutable std::vector<text_chunk> chunks_;
	mutable std::vector<surface> surfs_;
#ifdef	HAVE_FRIBIDI
	bool is_rtl_;
	void bidi_cvt();
#endif
	void hash();
};

#ifdef	HAVE_FRIBIDI
void text_surface::bidi_cvt()
{
	char		*c_str = const_cast<char *>(str_.c_str());	// fribidi forgot const...
	FriBidiStrIndex	len = str_.length();
	FriBidiChar	*bidi_logical = new FriBidiChar[len + 2];
	FriBidiChar	*bidi_visual = new FriBidiChar[len + 2];
	char		*utf8str = new char[4*len + 1];	//assume worst case here (all 4 Byte characters)
	FriBidiCharType	base_dir = FRIBIDI_TYPE_ON;
	FriBidiStrIndex n;


#ifdef	OLD_FRIBIDI
	n = fribidi_utf8_to_unicode (c_str, len, bidi_logical);
#else
	n = fribidi_charset_to_unicode(FRIBIDI_CHAR_SET_UTF8, c_str, len, bidi_logical);
#endif
	fribidi_log2vis(bidi_logical, n, &base_dir, bidi_visual, NULL, NULL, NULL);
#ifdef	OLD_FRIBIDI
	fribidi_unicode_to_utf8 (bidi_visual, n, utf8str);
#else
	fribidi_unicode_to_charset(FRIBIDI_CHAR_SET_UTF8, bidi_visual, n, utf8str);
#endif
	is_rtl_ = base_dir == FRIBIDI_TYPE_RTL;
	str_ = std::string(utf8str);
	delete[] bidi_logical;
	delete[] bidi_visual;
	delete[] utf8str;
}
#endif

text_surface::text_surface(std::string const &str, int size,
		SDL_Color color, int style) :
	hash_(0),
	font_size_(size),
	color_(color),
	style_(style),
	w_(-1),
	h_(-1),
	str_(str),
	initialized_(false),
	chunks_(),
	surfs_()
#ifdef	HAVE_FRIBIDI
	,is_rtl_(false)
#endif
{
#ifdef	HAVE_FRIBIDI
	bidi_cvt();
#endif
	hash();
}

text_surface::text_surface(int size, SDL_Color color, int style) :
	hash_(0),
	font_size_(size),
	color_(color),
	style_(style),
	w_(-1),
	h_(-1),
	str_(),
	initialized_(false),
	chunks_(),
	surfs_()
#ifdef	HAVE_FRIBIDI
	,is_rtl_(false)
#endif
{
}

void text_surface::set_text(std::string const &str)
{
	initialized_ = false;
	w_ = -1;
	h_ = -1;
	str_ = str;
#ifdef	HAVE_FRIBIDI
	bidi_cvt();
#endif
	hash();
}

void text_surface::hash()
{
	int h = 0;
	for(std::string::const_iterator it = str_.begin(), it_end = str_.end(); it != it_end; ++it)
		h = ((h << 9) | (h >> (sizeof(int) * 8 - 9))) ^ (*it);
	hash_ = h;
}

void text_surface::measure() const
{
	w_ = 0;
	h_ = 0;

	BOOST_FOREACH (text_chunk const &chunk, chunks_)
	{
		TTF_Font* ttfont = get_font(font_id(chunk.subset, font_size_));
		if(ttfont == NULL)
			continue;
		font_style_setter const style_setter(ttfont, style_);

		int w, h;
		TTF_SizeUTF8(ttfont, chunk.text.c_str(), &w, &h);
		w_ += w;
		h_ = std::max<int>(h_, h);
	}
}

size_t text_surface::measure_partial(const unsigned column)
{
	size_t total_w = 0, total_h = 0;
	unsigned uncalc_column = column;
	
	if (column == 0) {
		return 0;
	}
	if (chunks_.empty()) {
		chunks_ = split_text(str_);
	}
	BOOST_FOREACH (text_chunk const &chunk, chunks_) {
		TTF_Font* ttfont = get_font(font_id(chunk.subset, font_size_));
		if (ttfont == NULL)
			continue;
		font_style_setter const style_setter(ttfont, style_);

		int w, h;
		std::string tmp;
		for (utils::utf8_iterator itor(chunk.text); uncalc_column && itor != utils::utf8_iterator::end(chunk.text); ++itor, uncalc_column --) {
			tmp.append(itor.substr().first, itor.substr().second);
		}
		TTF_SizeUTF8(ttfont, tmp.c_str(), &w, &h);
		total_w += w;
		total_h = std::max<int>(total_h, h);
	}
	return total_w;
}

size_t text_surface::width() const
{
	if (w_ == -1) {
		if(chunks_.empty())
			chunks_ = split_text(str_);
		measure();
	}
	return w_;
}

size_t text_surface::height() const
{
	if (h_ == -1) {
		if(chunks_.empty())
			chunks_ = split_text(str_);
		measure();
	}
	return h_;
}

std::vector<surface> const &text_surface::get_surfaces() const
{
	if(initialized_)
		return surfs_;

	initialized_ = true;

	// Impose a maximal number of characters for a text line. Do now draw
	// any text longer that that, to prevent a SDL buffer overflow
	if(width() > max_text_line_width)
		return surfs_;

	BOOST_FOREACH (text_chunk const &chunk, chunks_)
	{
		TTF_Font* ttfont = get_font(font_id(chunk.subset, font_size_));
		if (ttfont == NULL)
			continue;
		font_style_setter const style_setter(ttfont, style_);

		surface s = surface(TTF_RenderUTF8_Blended(ttfont, chunk.text.c_str(), color_));
		if(!s.null())
			surfs_.push_back(s);
	}

	return surfs_;
}

namespace font {

class text_cache
{
public:
	static text_surface &find(text_surface const &t);
	static void resize(unsigned int size);
private:
	typedef std::list< text_surface > text_list;
	static text_list cache_;
	static unsigned int max_size_;
};

text_cache::text_list text_cache::cache_;
unsigned int text_cache::max_size_ = 50;

void text_cache::resize(unsigned int size)
{
	DBG_FT << "Text cache: resize from: " << max_size_ << " to: "
		<< size << " items in cache: " << cache_.size() << '\n';

	while(size < cache_.size()) {
		cache_.pop_back();
	}
	max_size_ = size;
}


text_surface &text_cache::find(text_surface const &t)
{
	static size_t lookup_ = 0, hit_ = 0;
	text_list::iterator it_bgn = cache_.begin(), it_end = cache_.end();
	text_list::iterator it = std::find(it_bgn, it_end, t);
	if (it != it_end) {
		cache_.splice(it_bgn, cache_, it);
		++hit_;
	} else {
		if (cache_.size() >= max_size_)
			cache_.pop_back();
		cache_.push_front(t);
	}
	if (++lookup_ % 1000 == 0) {
		DBG_FT << "Text cache: " << lookup_ << " lookups, " << (hit_ / 10) << "% hits\n";
		hit_ = 0;
	}
	return cache_.front();
}

// }

static surface render_text(const std::string& text, int fontsize, const SDL_Color& color, int style, bool use_markup)
{
	ttext text_;
	text_.set_foreground_color((color.r << 24) | (color.g << 16) | (color.b << 8) | 255);
	text_.set_font_size(fontsize);
	text_.set_font_style(style);
	// text_.set_maximum_width(width_ < 0 ? clip_rect_.w : width_);
	// text_.set_maximum_height(clip_rect_.h);

	text_.set_text(text, use_markup);

	return text_.render();
}

surface get_rendered_text2(const std::string& text, int maximum_width, int font_size, const SDL_Color& color)
{
	if (maximum_width <= 0) maximum_width = 480;
	help::tintegrate integrate(text, maximum_width, -1, font_size, color);
	return integrate.get_surface();
}

gui2::tpoint get_rendered_text_size(const std::string& text, int maximum_width, int font_size, const SDL_Color& color)
{
	if (maximum_width <= 0) maximum_width = 480;
	help::tintegrate integrate(text, maximum_width, -1, font_size, color);
	SDL_Rect rc = integrate.get_size();
	return gui2::tpoint(rc.w, rc.h);
}

// it is called by tintegrate
surface get_rendered_text(const std::string& str, int size, const SDL_Color& color, int style)
{
	return render_text(str, size, color, style, false);
}

SDL_Rect draw_text_line(surface gui_surface, const SDL_Rect& area, int size,
		   const SDL_Color& color, const std::string& text,
		   int x, int y, bool use_tooltips, int style)
{
	if (gui_surface.null()) {
		text_surface const &u = text_cache::find(text_surface(text, size, color, style));
		return create_rect(0, 0, u.width(), u.height());
	}

	if(area.w == 0) {  // no place to draw
		return create_rect(0, 0, 0, 0);
	}

	const std::string etext = make_text_ellipsis(text, size, area.w);

	// for the main current use, we already parsed markup
	surface surface(render_text(etext,size,color,style,false));
	if(surface == NULL) {
		return create_rect(0, 0, 0, 0);
	}

	SDL_Rect dest;
	if(x!=-1) {
		dest.x = x;
#ifdef	HAVE_FRIBIDI
		// Oron -- Conditional, until all draw_text_line calls have fixed area parameter
		if(getenv("NO_RTL") == NULL) {
			bool is_rtl = text_cache::find(text_surface(text, size, color, style)).is_rtl();
			if(is_rtl)
				dest.x = area.x + area.w - surface->w - (x - area.x);
		}
#endif
	} else
		dest.x = (area.w/2)-(surface->w/2);
	if(y!=-1)
		dest.y = y;
	else
		dest.y = (area.h/2)-(surface->h/2);
	dest.w = surface->w;
	dest.h = surface->h;

	if(line_width(text, size) > area.w) {
		tooltips::add_tooltip(dest,text);
	}

	if(dest.x + dest.w > area.x + area.w) {
		dest.w = area.x + area.w - dest.x;
	}

	if(dest.y + dest.h > area.y + area.h) {
		dest.h = area.y + area.h - dest.y;
	}

	if(gui_surface != NULL) {
		SDL_Rect src = dest;
		src.x = 0;
		src.y = 0;
		sdl_blit(surface,&src,gui_surface,&dest);
	}

	if(use_tooltips) {
		tooltips::add_tooltip(dest,text);
	}

	return dest;
}

int get_max_height(int size)
{
	// Only returns the maximal size of the first font
	TTF_Font* const font = get_font(font_id(0, size));
	if(font == NULL)
		return 0;
	return TTF_FontHeight(font);
}

int line_width(const std::string& line, int font_size, int style)
{
	return line_size(line,font_size,style).w;
}

SDL_Rect line_size(const std::string& line, int font_size, int style)
{
	line_size_cache_map& cache = line_size_cache[style][font_size];

	const line_size_cache_map::const_iterator i = cache.find(line);
	if(i != cache.end()) {
		return i->second;
	}

	SDL_Rect res;

	const SDL_Color col = { 0, 0, 0, 0 };
	text_surface s(line, font_size, col, style);

	res.w = s.width();
	res.h = s.height();
	res.x = res.y = 0;

	cache.insert(std::pair<std::string,SDL_Rect>(line,res));
	return res;
}

std::string make_text_ellipsis(const std::string &text, int font_size,
	int max_width, int style)
{
	static const std::string ellipsis = "...";

	if (line_width(text, font_size, style) <= max_width)
		return text;
	if (line_width(ellipsis, font_size, style) > max_width)
		return "";

	std::string current_substring;

	utils::utf8_iterator itor(text);

	for(; itor != utils::utf8_iterator::end(text); ++itor) {
		std::string tmp = current_substring;
		tmp.append(itor.substr().first, itor.substr().second);

		if (line_width(tmp + ellipsis, font_size, style) > max_width) {
			return current_substring + ellipsis;
		}

		current_substring.append(itor.substr().first, itor.substr().second);
	}

	return text; // Should not happen
}

std::string make_text_hide(const std::string& text, bool always, size_t front_should_hide_width, size_t hiden_width, int font_size, int max_showable_width, bool with_tags, bool parse_for_style)
{
	static const std::string single_char = " ";

	SDL_Color unused_color;
	int unused_int;
	int style = TTF_STYLE_NORMAL;
	std::string text1 = with_tags? text: del_tags(text);
	if (parse_for_style) parse_markup(text.begin(), text.end(), &unused_int, &unused_color, &style);
	
	int text_width = line_width(text1, font_size, style);
	if (always) {
		if (text_width <= max_showable_width) {
			return text;
		}
		if (line_width(single_char, font_size, style) > max_showable_width) {
			return "";
		}
	} else {
		if (hiden_width + text_width <= front_should_hide_width) {
			return "";
		}
		if (hiden_width >= front_should_hide_width && line_width(single_char, font_size, style) > max_showable_width) {
			return "";
		}
	}
	
	std::string current_substring, hide_substring;

	// get markup substring
	size_t pos = text.rfind(text1);
	std::string markup_substring;
	if (pos != std::string::npos) {
		markup_substring = text.substr(0, pos);
	}

	utils::utf8_iterator itor(text1);

	for (; itor != utils::utf8_iterator::end(text1); ++itor) {
		std::string tmp = current_substring;
		tmp.append(itor.substr().first, itor.substr().second);

		if (always || hiden_width >= front_should_hide_width) {
			if (line_width(tmp, font_size, style) > max_showable_width) {
				markup_substring.insert(markup_substring.size(), current_substring);
				return markup_substring;
			}
			current_substring.append(itor.substr().first, itor.substr().second);
		} else {
			hide_substring.append(itor.substr().first, itor.substr().second);
			int hide_substring_width = line_width(hide_substring, font_size, style);
			if (hiden_width + hide_substring_width > front_should_hide_width) {
				current_substring.append(itor.substr().first, itor.substr().second);
				// set hiden_width equal to front_should_hide_width
				hiden_width = front_should_hide_width;
			}
		}
	}
	markup_substring.insert(markup_substring.size(), current_substring);
	return markup_substring;
}

}

namespace {
	const int title2_size = font::SIZE_NORMAL;
	const int box_width = 2;
	const int normal_font_size = font::SIZE_SMALL;
/*
	/// Thrown when the help system fails to parse something.
	struct parse_error : public game::error
	{
		parse_error(const std::string& msg) : game::error(msg) 
		{
			VALIDATE(false, std::string("tintegrate, ") + msg);
		}
	};
*/
}

namespace help {

std::string tintegrate::generate_img(const std::string& src, tintegrate::ALIGNMENT align, bool floating)
{
	std::stringstream strstr;

	strstr << "<img>src='" << src << "'";
	std::string str = align_to_string(align);
	if (!str.empty()) {
		strstr << " align='";
		strstr << str << "'";
		
	}
	if (floating) {
		strstr << " float=yes";
	}
	strstr << " box=no";

	strstr << "</img>";
/*
	if (floating) {
		strstr << "<jump>to=" << "0";
		strstr << "</jump>";
	}
*/
	return strstr.str();
}

std::string tintegrate::generate_format(const std::string& text, const std::string& color, int font_size, bool bold, bool italic)
{
	std::stringstream strstr;

	if (text.empty()) {
		return null_str;
	}
	// text maybe have sapce character.
	strstr << "<format>text='" << text << "'";
	if (!color.empty()) {
		strstr << " color=" << color;
	}
	if (font_size) {
		strstr << " font_size=" << font_size;
	}
	if (bold) {
		strstr << " bold=yes";
	}
	if (italic) {
		strstr << " italic=yes";
	}
	strstr << "</format>";

	return strstr.str();
}

std::string tintegrate::generate_format(int val, const std::string& color, int font_size, bool bold, bool italic)
{
	std::stringstream strstr;

	// text maybe have sapce character.
	strstr << "<format>text='" << val << "'";
	if (!color.empty()) {
		strstr << " color=" << color;
	}
	if (font_size) {
		strstr << " font_size=" << font_size;
	}
	if (bold) {
		strstr << " bold=yes";
	}
	if (italic) {
		strstr << " italic=yes";
	}
	strstr << "</format>";

	return strstr.str();
}

tintegrate::item::item(surface surface, int x, int y, const std::string& _text,
						   const std::string& reference_to, bool _floating,
						   bool _box, ALIGNMENT alignment) :
	rect(),
	surf(surface),
	text(_text),
	ref_to(reference_to),
	floating(_floating), box(_box),
	align(alignment)
{
	rect.x = x;
	rect.y = y;
	rect.w = box ? surface->w + box_width * 2 : surface->w;
	rect.h = box ? surface->h + box_width * 2 : surface->h;
}

tintegrate::item::item(surface surface, int x, int y, bool _floating,
						   bool _box, ALIGNMENT alignment) :
	rect(),
	surf(surface),
	text(""),
	ref_to(""),
	floating(_floating),
	box(_box), align(alignment)
{
	rect.x = x;
	rect.y = y;
	rect.w = box ? surface->w + box_width * 2 : surface->w;
	rect.h = box ? surface->h + box_width * 2 : surface->h;
}

std::string tintegrate::hero_color = "green";
std::string tintegrate::object_color = "yellow";
std::string tintegrate::tactic_color = "blue";

tintegrate::tintegrate(const std::string& src, int maximum_width, int maximum_height, int default_font_size, const SDL_Color& default_font_color)
	: items_()
	, last_row_()
	, title_spacing_(16)
	, curr_loc_(0, 0)
	, min_row_height_(font::get_max_height(normal_font_size))
	, curr_row_height_(min_row_height_)
	, contents_height_(0)
	, maximum_width_(maximum_width)
	, maximum_height_(maximum_height)
	, default_font_size_(default_font_size)
	, default_font_color_(default_font_color)
{
	// Parse and add the text.
	std::vector<std::string> parsed_items;

	try {
		parsed_items = parse_text(src);
	} 
	catch (help::parse_error& ) {
		// [see remark#30] process character: '<' 
		add_text_item(src, default_font_color_);
	}

	std::vector<std::string>::const_iterator it;
	for (it = parsed_items.begin(); it != parsed_items.end(); ++it) {
		if (*it != "" && (*it)[0] == '[') {
			// Should be parsed as WML.
			try {
				config cfg;
				std::istringstream stream(*it);
				read(cfg, stream);

#define TRY(name) do { \
				if (config &child = cfg.child(#name)) \
					handle_##name##_cfg(child); \
				} while (0)

				TRY(ref);
				TRY(img);
				TRY(bold);
				TRY(italic);
				TRY(header);
				TRY(jump);
				TRY(format);

#undef TRY

			}
			catch (config::error& e) {
				// [see remark#30] process character: '<' 
				add_text_item(*it, default_font_color_);
			}
		} else {
			add_text_item(*it, default_font_color_);
		}
	}
	down_one_line(); // End the last line.
}

void tintegrate::handle_ref_cfg(const config &cfg)
{
	const std::string dst = cfg["dst"];
	const std::string text = cfg["text"];
	bool force = cfg["force"].to_bool();

	if (dst == "") {
		std::stringstream msg;
		msg << "Ref markup must have dst attribute. Please submit a bug"
		       " report if you have not modified the game files yourself. Erroneous config: ";
		write(msg, cfg);
		throw parse_error(msg.str());
	}

	// if (find_topic(toplevel_, dst) == NULL && !force) {
	if (!force) {
		// detect the broken link but quietly silence the hyperlink for normal user
		add_text_item(text, default_font_color_, game_config::debug ? dst : "", true);

		// FIXME: workaround: if different campaigns define different
		// terrains, some terrains available in one campaign will
		// appear in the list of seen terrains, and be displayed in the
		// help, even if the current campaign does not handle such
		// terrains. This will lead to the unit page generator creating
		// invalid references.
		//
		// Disabling this is a kludgy workaround until the
		// encountered_terrains system is fixed
		//
		// -- Ayin apr 8 2005
#if 0
		if (game_config::debug) {
			std::stringstream msg;
			msg << "Reference to non-existent topic '" << dst
			    << "'. Please submit a bug report if you have not"
			       "modified the game files yourself. Erroneous config: ";
			write(msg, cfg);
			throw parse_error(msg.str());
		}
#endif
	} else {
		add_text_item(text, default_font_color_, dst);
	}

}

void tintegrate::handle_img_cfg(const config &cfg)
{
	const std::string src = cfg["src"];
	const std::string align = cfg["align"];
	bool floating = cfg["float"].to_bool();
	bool box = cfg["box"].to_bool(true);
	if (src == "") {
		throw parse_error("Img markup must have src attribute.");
	}
	add_img_item(src, align, floating, box);
}

void tintegrate::handle_bold_cfg(const config &cfg)
{
	const std::string text = cfg["text"];
	if (text == "") {
		throw parse_error("Bold markup must have text attribute.");
	}
	add_text_item(text, default_font_color_, "", false, -1, true);
}

void tintegrate::handle_italic_cfg(const config &cfg)
{
	const std::string text = cfg["text"];
	if (text == "") {
		throw parse_error("Italic markup must have text attribute.");
	}
	add_text_item(text, default_font_color_, "", false, -1, false, true);
}

void tintegrate::handle_header_cfg(const config &cfg)
{
	const std::string text = cfg["text"];
	if (text == "") {
		throw parse_error("Header markup must have text attribute.");
	}
	add_text_item(text, default_font_color_, "", false, title2_size, true);
}

void tintegrate::handle_jump_cfg(const config &cfg)
{
	const std::string amount_str = cfg["amount"];
	const std::string to_str = cfg["to"];
	if (amount_str == "" && to_str == "") {
		throw parse_error("Jump markup must have either a to or an amount attribute.");
	}
	unsigned jump_to = curr_loc_.first;
	if (amount_str != "") {
		unsigned amount;
		try {
			amount = lexical_cast<unsigned, std::string>(amount_str);
		}
		catch (bad_lexical_cast) {
			throw parse_error("Invalid amount the amount attribute in jump markup.");
		}
		jump_to += amount;
	}
	if (to_str != "") {
		unsigned to;
		try {
			to = lexical_cast<unsigned, std::string>(to_str);
		}
		catch (bad_lexical_cast) {
			throw parse_error("Invalid amount in the to attribute in jump markup.");
		}
		if (to < jump_to) {
			down_one_line();
		}
		jump_to = to;
	}
	if (jump_to != 0 && static_cast<int>(jump_to) <
            get_max_x(curr_loc_.first, curr_row_height_)) {

		curr_loc_.first = jump_to;
	}
}

void tintegrate::handle_format_cfg(const config &cfg)
{
	const std::string text = cfg["text"];
	if (text == "") {
		// sorry, cannot avoid empty text
		// throw parse_error("Format markup must have text attribute.");
		return;
	}
	bool bold = cfg["bold"].to_bool();
	bool italic = cfg["italic"].to_bool();
	int font_size = cfg["font_size"].to_int(default_font_size_);
	SDL_Color color = string_to_color(cfg["color"]);
	add_text_item(text, color, "", false, font_size, bold, italic);
}

void tintegrate::add_text_item(const std::string& text, const SDL_Color& text_color, const std::string& ref_dst,
								   bool broken_link, int _font_size, bool bold, bool italic)
{
	const int font_size = _font_size < 0 ? default_font_size_ : _font_size;
	if (text.empty())
		return;
	const int remaining_width = get_remaining_width();
	size_t first_word_start = text.find_first_not_of(" ");
	if (first_word_start == std::string::npos) {
		first_word_start = 0;
	}
	if (text[first_word_start] == '\n') {
		down_one_line();
		std::string rest_text = text;
		rest_text.erase(0, first_word_start + 1);
		add_text_item(rest_text, text_color, ref_dst, broken_link, _font_size, bold, italic);
		return;
	}
	const std::string first_word = get_first_word(text);
	int state = ref_dst == "" ? 0 : TTF_STYLE_UNDERLINE;
	state |= bold ? TTF_STYLE_BOLD : 0;
	state |= italic ? TTF_STYLE_ITALIC : 0;
	if (curr_loc_.first != get_min_x(curr_loc_.second, curr_row_height_)
		&& remaining_width < font::line_width(first_word, font_size, state)) {
		// The first word does not fit, and we are not at the start of
		// the line. Move down.
		down_one_line();
		std::string s = remove_first_space(text);
		add_text_item(s, text_color, ref_dst, broken_link, _font_size, bold, italic);
	}
	else {
		std::vector<std::string> parts = split_in_width(text, font_size, remaining_width, state);
		std::string first_part = parts.front();
		// Always override the color if we have a cross reference.
		SDL_Color color;
		if(ref_dst.empty())
			color = text_color;
		else if(broken_link)
			color = font::BAD_COLOR;
		else
			color = font::YELLOW_COLOR;

		surface surf(font::get_rendered_text(first_part, font_size, color, state));
		if (!surf.null()) {
			// [See remark#22]
			SDL_SetAlpha(surf, 0, 0); // direct blit without alpha blending
			add_item(item(surf, curr_loc_.first, curr_loc_.second, first_part, ref_dst));
		}
		if (parts.size() > 1) {

			std::string& s = parts.back();

			const std::string first_word_before = get_first_word(s);
			const std::string first_word_after = get_first_word(remove_first_space(s));
			if (get_remaining_width() >= font::line_width(first_word_after, font_size, state)
				&& get_remaining_width()
				< font::line_width(first_word_before, font_size, state)) {
				// If the removal of the space made this word fit, we
				// must move down a line, otherwise it will be drawn
				// without a space at the end of the line.
				s = remove_first_space(s);
				down_one_line();
			}
			else if (!(font::line_width(first_word_before, font_size, state)
					   < get_remaining_width())) {
				s = remove_first_space(s);
			}
			add_text_item(s, text_color, ref_dst, broken_link, _font_size, bold, italic);

		}
	}
}

void tintegrate::add_img_item(const std::string& path, const std::string& alignment,
								  const bool floating, const bool box)
{
	surface surf(image::get_image(path));
	if (surf.null())
		return;
	ALIGNMENT align = str_to_align(alignment);
	if (align == BACK && items_.empty()) {
		align = HERE;
	}
	if (align == HERE && floating) {
		align = LEFT;
	}
	const int width = surf->w + (box ? box_width * 2 : 0);
	int xpos;
	int ypos = curr_loc_.second;
	int text_width = maximum_width_;
	switch (align) {
	case HERE:
		xpos = curr_loc_.first;
		break;
	case LEFT:
	default:
		xpos = 0;
		break;
	case MIDDLE:
		xpos = text_width / 2 - width / 2 - (box ? box_width : 0);
		break;
	case RIGHT:
		xpos = text_width - width - (box ? box_width * 2 : 0);
		break;
	case BACK:
		xpos = items_.back().rect.x;
		ypos = items_.back().rect.y;
		break;
	}
	if (align != BACK && curr_loc_.first != get_min_x(curr_loc_.second, curr_row_height_)
		&& (xpos < curr_loc_.first || xpos + width > text_width)) {
		down_one_line();
		add_img_item(path, alignment, floating, box);
	}
	else {
		if (!floating) {
			curr_loc_.first = xpos;
		}
		else {
			ypos = get_y_for_floating_img(width, xpos, ypos);
		}
		add_item(item(surf, xpos, ypos, floating, box, align));
	}
}

int tintegrate::get_y_for_floating_img(const int width, const int x, const int desired_y)
{
	int min_y = desired_y;
	for (std::list<item>::const_iterator it = items_.begin(); it != items_.end(); ++it) {
		const item& itm = *it;
		if (itm.floating) {
			if ((itm.rect.x + itm.rect.w > x && itm.rect.x < x + width)
				|| (itm.rect.x > x && itm.rect.x < x + width)) {
				min_y = std::max<int>(min_y, itm.rect.y + itm.rect.h);
			}
		}
	}
	return min_y;
}

int tintegrate::get_min_x(const int y, const int height)
{
	int min_x = 0;
	for (std::list<item>::const_iterator it = items_.begin(); it != items_.end(); ++it) {
		const item& itm = *it;
		if (itm.floating) {
			if (itm.rect.y < y + height && itm.rect.y + itm.rect.h > y && itm.align == LEFT) {
				min_x = std::max<int>(min_x, itm.rect.w + 5);
			}
		}
	}
	return min_x;
}

int tintegrate::get_max_x(const int y, const int height)
{
	int text_width = maximum_width_;
	int max_x = text_width;
	for (std::list<item>::const_iterator it = items_.begin(); it != items_.end(); ++it) {
		const item& itm = *it;
		if (itm.floating) {
			if (itm.rect.y < y + height && itm.rect.y + itm.rect.h > y) {
				if (itm.align == RIGHT) {
					max_x = std::min<int>(max_x, text_width - itm.rect.w - 5);
				} else if (itm.align == MIDDLE) {
					max_x = std::min<int>(max_x, text_width / 2 - itm.rect.w / 2 - 5);
				}
			}
		}
	}
	return max_x;
}

void tintegrate::add_item(const item &itm)
{
	items_.push_back(itm);
	if (!itm.floating) {
		curr_loc_.first += itm.rect.w;
		curr_row_height_ = std::max<int>(itm.rect.h, curr_row_height_);
		contents_height_ = std::max<int>(contents_height_, curr_loc_.second + curr_row_height_);
		last_row_.push_back(&items_.back());
	}
	else {
		if (itm.align == LEFT) {
			curr_loc_.first = itm.rect.w + 5;
		}
		contents_height_ = std::max<int>(contents_height_, itm.rect.y + itm.rect.h);
	}
}


tintegrate::ALIGNMENT tintegrate::str_to_align(const std::string &cmp_str)
{
	if (cmp_str == "left") {
		return LEFT;
	} else if (cmp_str == "middle") {
		return MIDDLE;
	} else if (cmp_str == "right") {
		return RIGHT;
	} else if (cmp_str == "back") {
		return BACK;
	} else if (cmp_str == "here" || cmp_str == "") { // Make the empty string be "here" alignment.
		return HERE;
	}
	std::stringstream msg;
	msg << "Invalid alignment string: '" << cmp_str << "'";
	throw parse_error(msg.str());
}

std::string tintegrate::align_to_string(tintegrate::ALIGNMENT align)
{
	if (align == LEFT) return "left";
	if (align == MIDDLE) return "middle";
	if (align == RIGHT) return "right";
	if (align == BACK) return "back";
	if (align == HERE) return "";
	return "";
}

void tintegrate::down_one_line()
{
	adjust_last_row();
	last_row_.clear();
	// curr_loc_.second += curr_row_height_ + (curr_row_height_ == min_row_height_ ? 0 : 2);
	curr_loc_.second += curr_row_height_ + (curr_row_height_ == min_row_height_ ? 0 : 0);
	// [see remark#25]
	if (curr_loc_.first > maximum_width_) {
		maximum_width_ = curr_loc_.first;
	}
	curr_row_height_ = min_row_height_;
	contents_height_ = std::max<int>(curr_loc_.second + curr_row_height_, contents_height_);
	curr_loc_.first = get_min_x(curr_loc_.second, curr_row_height_);
}

void tintegrate::adjust_last_row()
{
	for (std::list<item *>::iterator it = last_row_.begin(); it != last_row_.end(); ++it) {
		item &itm = *(*it);
		const int gap = curr_row_height_ - itm.rect.h;
		itm.rect.y += gap / 2;
	}
}

int tintegrate::get_remaining_width()
{
	const int total_w = get_max_x(curr_loc_.second, curr_row_height_);
	return total_w - curr_loc_.first;
}

SDL_Rect tintegrate::get_size() const
{
	int max_x = 0;
	int max_y = 0;

	for(std::list<item>::const_iterator it = items_.begin(), end = items_.end(); it != end; ++it) {
		SDL_Rect dst = it->rect;
		max_x = std::max<int>(max_x, dst.x + dst.w);
		max_y = std::max<int>(max_y, dst.y + dst.h);
	}
	SDL_Rect ret;
	ret.x = ret.y = 0;
	ret.w = max_x;
	ret.h = max_y;
	return ret;
}

surface tintegrate::get_surface() const
{
	SDL_Rect size = get_size();
	surface screen = create_neutral_surface(size.w, size.h);

	for(std::list<item>::const_iterator it = items_.begin(), end = items_.end(); it != end; ++it) {
		SDL_Rect dst = it->rect;
		if (it->box) {
			for (int i = 0; i < box_width; ++i) {
				draw_rectangle(dst.x, dst.y, it->rect.w - i * 2, it->rect.h - i * 2,
				                    0, screen);
				++dst.x;
				++dst.y;
			}
		}
		sdl_blit(it->surf, NULL, screen, &dst);
	}
	return screen;
}

void tintegrate::draw_contents()
{
/*
	SDL_Rect const &loc = inner_location();
	bg_restore();
	surface screen = video().getSurface();
	clip_rect_setter clip_rect_set(screen, &loc);
	for(std::list<item>::const_iterator it = items_.begin(), end = items_.end(); it != end; ++it) {
		SDL_Rect dst = it->rect;
		dst.y -= get_position();
		if (dst.y < static_cast<int>(loc.h) && dst.y + it->rect.h > 0) {
			dst.x += loc.x;
			dst.y += loc.y;
			if (it->box) {
				for (int i = 0; i < box_width; ++i) {
					draw_rectangle(dst.x, dst.y, it->rect.w - i * 2, it->rect.h - i * 2,
					                    0, screen);
					++dst.x;
					++dst.y;
				}
			}
			sdl_blit(it->surf, NULL, screen, &dst);
		}
	}
	update_rect(loc);
*/
}

bool tintegrate::item_at::operator()(const item& item) const {
	return point_in_rect(x_, y_, item.rect);
}

std::string tintegrate::ref_at(const int x, const int y)
{
/*	const int local_x = x - location().x;
	const int local_y = y - location().y;
	if (local_y < static_cast<int>(height()) && local_y > 0) {
		const int cmp_y = local_y + get_position();
		const std::list<item>::const_iterator it =
			std::find_if(items_.begin(), items_.end(), item_at(local_x, cmp_y));
		if (it != items_.end()) {
			if ((*it).ref_to != "") {
				return ((*it).ref_to);
			}
		}
	}
*/
	return "";
}

}

namespace {

typedef std::map<int, font::floating_label> label_map;
label_map labels;
int label_id = 1;

std::stack<std::set<int> > label_contexts;
}


namespace font {

floating_label::floating_label(const std::string& text)
		: surf_(NULL), buf_(NULL), text_(text),
		font_size_(SIZE_NORMAL),
		color_(NORMAL_COLOR),	bgcolor_(), bgalpha_(0),
		xpos_(0), ypos_(0),
		xmove_(0), ymove_(0), lifetime_(-1),
		width_(-1),
#if defined(_KINGDOM_EXE) || !defined(_WIN32)
		clip_rect_(screen_area()),
#else
		clip_rect_(create_rect(0, 0, 800, 600)),
#endif
		alpha_change_(0), visible_(true), align_(CENTER_ALIGN),
		border_(0), scroll_(ANCHOR_LABEL_SCREEN), use_markup_(true)
{}

void floating_label::move(double xmove, double ymove)
{
	xpos_ += xmove;
	ypos_ += ymove;
}

int floating_label::xpos(size_t width) const
{
	int xpos = int(xpos_);
	if(align_ == font::CENTER_ALIGN) {
		xpos -= width/2;
	} else if(align_ == font::RIGHT_ALIGN) {
		xpos -= width;
	}

	return xpos;
}

surface floating_label::create_surface()
{
	if (surf_.null()) {
/*
		font::ttext text;
		text.set_foreground_color((color_.r << 24) | (color_.g << 16) | (color_.b << 8) | 255);
		text.set_font_size(font_size_);
		text.set_maximum_width(width_ < 0 ? clip_rect_.w : width_);
		text.set_maximum_height(clip_rect_.h);

		//ignore last '\n'
		if(!text_.empty() && *(text_.rbegin()) == '\n'){
			text.set_text(std::string(text_.begin(), text_.end()-1), use_markup_);
		} else {
			text.set_text(text_, use_markup_);
		}

		// Reset the maximum width, as we want the smallest bounding box.
		// don't call text.set_maximum_width, word_wrap_text is not accurate!
		// text.set_maximum_width(text.get_width());
		surface foreground = text.render();
*/
		help::tintegrate integrate(text_, width_ < 0 ? clip_rect_.w : width_, clip_rect_.h, font_size_, color_);
		surface foreground = integrate.get_surface();

		if(foreground == NULL) {
			ERR_FT << "could not create floating label's text" << std::endl;
			return NULL;
		}

		// combine foreground text with its background
		if(bgalpha_ != 0) {
			// background is a dark tootlip box
			surface background = create_neutral_surface(foreground->w + border_*2, foreground->h + border_*2);

			if (background == NULL) {
				ERR_FT << "could not create tooltip box" << std::endl;
				surf_ = create_optimized_surface(foreground);
				return surf_;
			}

			Uint32 color = SDL_MapRGBA(foreground->format, bgcolor_.r,bgcolor_.g, bgcolor_.b, bgalpha_);
			sdl_fill_rect(background,NULL, color);

			// we make the text less transparent, because the blitting on the
			// dark background will darken the anti-aliased part.
			// This 1.13 value seems to restore the brightness of version 1.4
			// (where the text was blitted directly on screen)
			foreground = adjust_surface_alpha(foreground, ftofxp(1.13), false);

			SDL_Rect r = create_rect( border_, border_, 0, 0);
			SDL_SetAlpha(foreground,SDL_SRCALPHA,SDL_ALPHA_OPAQUE);
			blit_surface(foreground, NULL, background, &r);

			surf_ = create_optimized_surface(background);
			// RLE compression seems less efficient for big semi-transparent area
			// so, remove it for this case, but keep the optimized display format
			SDL_SetAlpha(surf_,SDL_SRCALPHA,SDL_ALPHA_OPAQUE);
		}
		else {
			// background is blurred shadow of the text
			surface background = create_neutral_surface
				(foreground->w + 4, foreground->h + 4);
			sdl_fill_rect(background, NULL, 0);
			SDL_Rect r = { 2, 2, 0, 0 };
			blit_surface(foreground, NULL, background, &r);
			background = shadow_image(background, false);

			if (background == NULL) {
				ERR_FT << "could not create floating label's shadow" << std::endl;
				surf_ = create_optimized_surface(foreground);
				return surf_;
			}
			SDL_SetAlpha(foreground,SDL_SRCALPHA,SDL_ALPHA_OPAQUE);
			blit_surface(foreground, NULL, background, &r);
			surf_ = create_optimized_surface(background);
		}
	}

	return surf_;
}

void floating_label::draw(surface screen)
{
	if(!visible_) {
		buf_.assign(NULL);
		return;
	}

	create_surface();
	if(surf_ == NULL) {
		return;
	}

	if(buf_ == NULL) {
		buf_.assign(create_compatible_surface(screen, surf_->w, surf_->h));
		if(buf_ == NULL) {
			return;
		}
	}

	if(screen == NULL) {
		return;
	}

	SDL_Rect rect = create_rect(xpos(surf_->w), ypos_, surf_->w, surf_->h);
	const clip_rect_setter clip_setter(screen, &clip_rect_);
	sdl_blit(screen,&rect,buf_,NULL);
	sdl_blit(surf_,NULL,screen,&rect);

	update_rect(rect);
}

void floating_label::undraw(surface screen)
{
	if(screen == NULL || buf_ == NULL) {
		return;
	}

	SDL_Rect rect = create_rect(xpos(surf_->w), ypos_, surf_->w, surf_->h);
	const clip_rect_setter clip_setter(screen, &clip_rect_);
	sdl_blit(buf_,NULL,screen,&rect);

	update_rect(rect);

	move(xmove_,ymove_);
	if(lifetime_ > 0) {
		--lifetime_;
		if(alpha_change_ != 0 && (xmove_ != 0.0 || ymove_ != 0.0) && surf_ != NULL) {
			// fade out moving floating labels
			// note that we don't optimize these surfaces since they will always change
			surf_.assign(adjust_surface_alpha_add(surf_,alpha_change_,false));
		}
	}
}

int add_floating_label(const floating_label& flabel)
{
	if(label_contexts.empty()) {
		return 0;
	}

	++label_id;
	labels.insert(std::pair<int, floating_label>(label_id, flabel));
	label_contexts.top().insert(label_id);
	return label_id;
}

void move_floating_label(int handle, double xmove, double ymove)
{
	const label_map::iterator i = labels.find(handle);
	if(i != labels.end()) {
		i->second.move(xmove,ymove);
	}
}

void scroll_floating_labels(double xmove, double ymove)
{
	for(label_map::iterator i = labels.begin(); i != labels.end(); ++i) {
		if(i->second.scroll() == ANCHOR_LABEL_MAP) {
			i->second.move(xmove,ymove);
		}
	}
}

void remove_floating_label(int handle)
{
	const label_map::iterator i = labels.find(handle);
	if(i != labels.end()) {
		if(label_contexts.empty() == false) {
			label_contexts.top().erase(i->first);
		}

		labels.erase(i);
	}
}

void show_floating_label(int handle, bool value)
{
	const label_map::iterator i = labels.find(handle);
	if(i != labels.end()) {
		i->second.show(value);
	}
}

SDL_Rect get_floating_label_rect(int handle)
{
	const label_map::iterator i = labels.find(handle);
	if(i != labels.end()) {
		const surface surf = i->second.create_surface();
		if(surf != NULL) {
			return create_rect(0, 0, surf->w, surf->h);
		}
	}

	return empty_rect;
}

floating_label_context::floating_label_context()
{
	surface const screen = SDL_GetVideoSurface();
	if(screen != NULL) {
		draw_floating_labels(screen);
	}

	label_contexts.push(std::set<int>());
}

floating_label_context::~floating_label_context()
{
	const std::set<int>& labels = label_contexts.top();
	for(std::set<int>::const_iterator i = labels.begin(); i != labels.end(); ) {
		remove_floating_label(*i++);
	}

	label_contexts.pop();

	surface const screen = SDL_GetVideoSurface();
	if(screen != NULL) {
		undraw_floating_labels(screen);
	}
}

void draw_floating_labels(surface screen)
{
	if(label_contexts.empty()) {
		return;
	}

	const std::set<int>& context = label_contexts.top();

	//draw the labels in the order they were added, so later added labels (likely to be tooltips)
	//are displayed over earlier added labels.
	for(label_map::iterator i = labels.begin(); i != labels.end(); ++i) {
		if(context.count(i->first) > 0) {
			i->second.draw(screen);
		}
	}
}

void undraw_floating_labels(surface screen)
{
	if(label_contexts.empty()) {
		return;
	}

	std::set<int>& context = label_contexts.top();

	//undraw labels in reverse order, so that a LIFO process occurs, and the screen is restored
	//into the exact state it started in.
	for(label_map::reverse_iterator i = labels.rbegin(); i != labels.rend(); ++i) {
		if(context.count(i->first) > 0) {
			i->second.undraw(screen);
		}
	}

	//remove expired labels
	for(label_map::iterator j = labels.begin(); j != labels.end(); ) {
		if(context.count(j->first) > 0 && j->second.expired()) {
			context.erase(j->first);
			labels.erase(j++);
		} else {
			++j;
		}
	}
}

}

static bool add_font_to_fontlist(config &fonts_config,
	std::vector<font::subset_descriptor>& fontlist, const std::string& name)
{
	config &font = fonts_config.find_child("font", "name", name);
	if (!font)
		return false;

		fontlist.push_back(font::subset_descriptor());
		fontlist.back().name = name;
		std::vector<std::string> ranges = utils::split(font["codepoints"]);

		for(std::vector<std::string>::const_iterator itor = ranges.begin();
				itor != ranges.end(); ++itor) {

			std::vector<std::string> r = utils::split(*itor, '-');
			if(r.size() == 1) {
				size_t r1 = lexical_cast_default<size_t>(r[0], 0);
				fontlist.back().present_codepoints.push_back(std::pair<size_t, size_t>(r1, r1));
			} else if(r.size() == 2) {
				size_t r1 = lexical_cast_default<size_t>(r[0], 0);
				size_t r2 = lexical_cast_default<size_t>(r[1], 0);

				fontlist.back().present_codepoints.push_back(std::pair<size_t, size_t>(r1, r2));
			}
		}

		return true;
	}

namespace font {


bool load_font_config()
{
	//read font config separately, so we do not have to re-read the whole
	//config when changing languages
	config cfg;
	try {
		scoped_istream stream = preprocess_file(get_wml_location("hardwired/fonts.cfg"));
		read(cfg, *stream);
	} catch(config::error &e) {
		ERR_FT << "could not read fonts.cfg:\n"
		       << e.message << '\n';
		return false;
	}

	config &fonts_config = cfg.child("fonts");
	if (!fonts_config)
		return false;

	std::set<std::string> known_fonts;
	BOOST_FOREACH (const config &font, fonts_config.child_range("font")) {
		known_fonts.insert(font["name"]);
	}

	const std::vector<std::string> font_order = utils::split(fonts_config["order"]);
	std::vector<font::subset_descriptor> fontlist;
	std::vector<std::string>::const_iterator font;
	for(font = font_order.begin(); font != font_order.end(); ++font) {
		add_font_to_fontlist(fonts_config, fontlist, *font);
		known_fonts.erase(*font);
	}
	std::set<std::string>::const_iterator kfont;
	for(kfont = known_fonts.begin(); kfont != known_fonts.end(); ++kfont) {
		add_font_to_fontlist(fonts_config, fontlist, *kfont);
	}

	if(fontlist.empty())
		return false;

	font::set_font_list(fontlist);
	return true;
}

void cache_mode(CACHE mode)
{
	if(mode == CACHE_LOBBY) {
		text_cache::resize(1000);
	} else {
		text_cache::resize(50);
	}
}


}

namespace font {

const unsigned ttext::STYLE_NORMAL = TTF_STYLE_NORMAL;
const unsigned ttext::STYLE_BOLD = TTF_STYLE_BOLD;
const unsigned ttext::STYLE_ITALIC = TTF_STYLE_ITALIC;
const unsigned ttext::STYLE_UNDERLINE = TTF_STYLE_UNDERLINE;

ttext::ttext() :
	rect_(),
	surface_(),
	text_(),
	markedup_text_(false),
	font_size_(14),
	font_style_(STYLE_NORMAL),
	foreground_color_(0xFFFFFFFF), // solid white
	maximum_width_(-1),
	maximum_height_(-1),
	ellipse_mode_(PANGO_ELLIPSIZE_NONE),
	maximum_length_(std::string::npos),
	calculation_dirty_(true),
	length_(0),
	surface_dirty_(true),
	cached_surfs_()
{

}

ttext::~ttext()
{

}

int ttext::get_width()
{
	return get_size().x;
}

int ttext::get_height()
{
	return get_size().y;
}

gui2::tpoint ttext::get_size()
{
	if (text_.empty()) {
		return gui2::tpoint(0, 0);
	}
	recalculate();

	return gui2::tpoint(rect_.w, rect_.h);
}

bool ttext::is_truncated() const
{
	return false;
}

unsigned ttext::insert_text(const unsigned offset, const std::string& text)
{
	if(text.empty()) {
		return 0;
	}

	return insert_unicode(offset, utils::string_to_wstring(text));
}

bool ttext::insert_unicode(const unsigned offset, const wchar_t unicode)
{
	return (insert_unicode(offset, wide_string(1, unicode)) == 1);
}

unsigned ttext::insert_unicode(const unsigned offset, const wide_string& unicode)
{
	assert(offset <= length_);

	if(length_ == maximum_length_) {
		return 0;
	}

	const unsigned len = length_ + unicode.size() > maximum_length_
		? maximum_length_ - length_  : unicode.size();

	wide_string tmp = utils::string_to_wstring(text_);
	tmp.insert(tmp.begin() + offset, unicode.begin(), unicode.begin() + len);

	set_text(utils::wstring_to_string(tmp), false);

	return len;
}

// chars --> pixel
// Only support one line!
gui2::tpoint ttext::get_cursor_position(const unsigned column, const unsigned line)
{
	recalculate();
	if (cached_surfs_.size() <= line) {
		return gui2::tpoint(0, 0);
	}
	if (column == 0) {
		return gui2::tpoint(0, 0);
	}
	return gui2::tpoint(cached_surfs_[line].measure_partial(column), 0);
}

// pixels --> chars(index) 
// Only support one line!
gui2::tpoint ttext::get_column_line(const gui2::tpoint& position)
{
	int line, offset;
	offset = position.x;
	line = 0;

	int pos = 0, last_pos;
	for (size_t i = 1; ; ++i) {
		last_pos = pos;
		pos = get_cursor_position(i, line).x;

		if (last_pos == pos) {
			return  gui2::tpoint(i - 1, line);
		}
		if (pos >= offset) {
			return  gui2::tpoint(i - 1, line);
		}
	}
}

bool ttext::set_text(const std::string& text, const bool markedup)
{
	// markedup decide markedup whether or not
	if (markedup != markedup_text_ || text != text_) {
		text_ = text;
		markedup_text_ = markedup;

		calculation_dirty_ = true;
		surface_dirty_ = true;
	}
	return true;
}
/*
const std::string& ttext::text() const 
{ 
	return text_; 
}
*/
size_t ttext::get_length() 
{ 
	if (calculation_dirty_) {
		recalculate();
	}
	return length_;
}

ttext& ttext::set_font_size(const unsigned font_size)
{
	if(font_size != font_size_) {
		font_size_ = font_size;
		calculation_dirty_ = true;
		surface_dirty_ = true;
	}

	return *this;
}

ttext& ttext::set_font_style(const unsigned font_style)
{
	if(font_style != font_style_) {
		font_style_ = font_style;
		calculation_dirty_ = true;
		surface_dirty_ = true;
	}

	return *this;
}

ttext& ttext::set_foreground_color(const Uint32 color)
{
	if(color != foreground_color_) {
		foreground_color_ = color;
		calculation_dirty_ = true;
		surface_dirty_ = true;
	}

	return *this;
}

ttext& ttext::set_maximum_width(int width)
{
	if (width <= 0) {
		width = -1;
	}

	if (width != maximum_width_) {
		maximum_width_ = width;
		calculation_dirty_ = true;
		surface_dirty_ = true;
	}

	return *this;
}

ttext& ttext::set_characters_per_line(const unsigned characters_per_line)
{
	// todo: impletement it in future.

	return *this;
}

ttext& ttext::set_maximum_height(int height, bool multiline)
{
	if (height <= 0) {
		height = -1;
		multiline = false;
	}

	if (height != maximum_height_) {
		maximum_height_ = height;
		calculation_dirty_ = true;
		surface_dirty_ = true;
	}

	return *this;
}

ttext& ttext::set_ellipse_mode(const PangoEllipsizeMode ellipse_mode)
{
	if(ellipse_mode != ellipse_mode_) {
		ellipse_mode_ = ellipse_mode;
		calculation_dirty_ = true;
		surface_dirty_ = true;
	}

	return *this;
}

ttext &ttext::set_alignment(const PangoAlignment alignment)
{
	// todo: impletement it in future.

	return *this;
}

ttext& ttext::set_maximum_length(const size_t maximum_length)
{
	if (maximum_length != maximum_length_) {
		maximum_length_ = maximum_length;
		if (length_ > maximum_length_) {

			wide_string tmp = utils::string_to_wstring(text_);
			tmp.resize(maximum_length_);
			set_text(utils::wstring_to_string(tmp), false);
		}
	}

	return *this;
}

void ttext::set_dirty(bool dirty)
{
	calculation_dirty_ = dirty;
	surface_dirty_ = dirty;
}

void ttext::recalculate()
{
	if (!calculation_dirty_) {
		return;
	}

	// we keep blank lines and spaces (may be wanted for indentation)
	const std::vector<std::string> lines = utils::split(text_, '\n', 0);
	size_t width = 0, height = 0, length = 0;

	calculation_dirty_ = false;
	
	cached_surfs_.clear();

	for (std::vector< std::string >::const_iterator ln = lines.begin(), ln_end = lines.end(); ln != ln_end; ++ln) {

		int sz = font_size_;
		int text_style = font_style_;
		SDL_Color color = int_to_color(foreground_color_ >> 8);

		std::string::const_iterator after_markup = markedup_text_? 
			parse_markup(ln->begin(), ln->end(), &sz, &color, &text_style): ln->begin();

		std::vector<std::string> wrapped_lines;
		
		if (after_markup == ln->end()) {
			wrapped_lines.push_back("");
		} else if (maximum_width_ > 0) {
			// Width of chars calling TTF_SizeUTF8 in statement is less than accumulative total.
			// word_wrap_text's method is accumulative total, so it maybe large than maximum_width_.
			std::string unwrapped_text(after_markup, ln->end());
			const int unwrapped_text_width = line_size(unwrapped_text, sz, text_style).w;

			if (unwrapped_text_width <= maximum_width_) {
				wrapped_lines.push_back(unwrapped_text);
			} else if (ellipse_mode_ == PANGO_ELLIPSIZE_NONE) {
				// this will result tow or more item.
				wrapped_lines = utils::split(word_wrap_text(std::string(after_markup, ln->end()), sz, maximum_width_), '\n', 0);
			} else {
				wrapped_lines.push_back(make_text_ellipsis(std::string(after_markup, ln->end()), sz, maximum_width_, text_style));
			}
			if (!unwrapped_text.empty() && !wrapped_lines.empty() && wrapped_lines[0].empty()) {
				// this is mod bug, but not result to ACCESS EXCEPTION, update it. progream must attion it!
				wrapped_lines[0] = unwrapped_text;
			}
		} else {
			wrapped_lines.push_back(std::string(after_markup, ln->end()));
		}
		
		for (std::vector< std::string >::const_iterator wrapped_ln = wrapped_lines.begin(), wrapped_ln_end = wrapped_lines.end(); wrapped_ln != wrapped_ln_end; ++ wrapped_ln) {
			text_surface txt_surf(sz, color, text_style);

			if (after_markup == ln->end() && (ln+1 != ln_end || lines.begin() + 1 == ln_end)) {
				// we replace empty line by a space (to have a line height)
				// except for the last line if we have several
				txt_surf.set_text(" ");
			} else {
				txt_surf.set_text(*wrapped_ln);
			
				// length is length of all chars
				for (utils::utf8_iterator itor(*wrapped_ln); itor != utils::utf8_iterator::end(*wrapped_ln); ++itor) {
					length ++;
				}
			}

			text_surface* const cached_surf = &text_cache::find(txt_surf);
			const std::vector<surface>&res = cached_surf->get_surfaces();

			if (!res.empty()) {
				cached_surfs_.push_back(*cached_surf);
				width = std::max<size_t>(cached_surf->width(), width);
				height += cached_surf->height();
			}
		}
	}

	rect_.w = width;
	if (maximum_width_ > 0 && maximum_width_ < rect_.w) {
		rect_.w = maximum_width_;
	}
	rect_.h = height;
	rect_.x = rect_.y = 0;
	length_ = length;
}

surface ttext::render()
{
	// when text_ is empty, don' return null, replace space. it is necessary for tintegrate.
	if (!surface_dirty_) {
		return surface_;
	}
	recalculate();

	surface_dirty_ = true;
	// we keep blank lines and spaces (may be wanted for indentation)
	std::vector<std::vector<surface> > surfaces;
	surfaces.reserve(cached_surfs_.size());

	for(std::vector<text_surface>::const_iterator text_surf = cached_surfs_.begin(), text_surf_end = cached_surfs_.end(); text_surf != text_surf_end; ++ text_surf) {

		const text_surface& cached_surf = *text_surf;
		const std::vector<surface>&res = cached_surf.get_surfaces();

		if (!res.empty()) {
			surfaces.push_back(res);
		}
	}

	if (surfaces.empty()) {
		surface_.assign(NULL);
	} else if (surfaces.size() == 1 && surfaces.front().size() == 1) {
		surface surf = surfaces.front().front();
		SDL_SetAlpha(surf, SDL_SRCALPHA | SDL_RLEACCEL, SDL_ALPHA_OPAQUE);
		surface_.assign(surf);
	} else {

		surface res(create_neutral_surface(rect_.w, rect_.h));
		if (res.null()) {
			surface_.assign(NULL);
			return surface_;
		}

		size_t ypos = 0;
		for (std::vector<std::vector<surface> >::const_iterator i = surfaces.begin(), i_end = surfaces.end(); i != i_end; ++i) {
			size_t xpos = 0;
			size_t height = 0;

			for (std::vector<surface>::const_iterator j = i->begin(), j_end = i->end(); j != j_end; ++j) {
				SDL_SetAlpha(*j, 0, 0); // direct blit without alpha blending
				SDL_Rect dstrect = create_rect(xpos, ypos, 0, 0);
				sdl_blit(*j, NULL, res, &dstrect);
				xpos += (*j)->w;
				height = std::max<size_t>((*j)->h, height);
			}
			ypos += height;
		}

		SDL_SetAlpha(res, SDL_SRCALPHA | SDL_RLEACCEL, SDL_ALPHA_OPAQUE);
		surface_.assign(res);
	}

	return surface_;
}

} // namespace font

