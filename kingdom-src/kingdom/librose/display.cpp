/* $Id: display.cpp 47842 2010-12-05 18:10:07Z mordante $ */
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
 * Routines to set up the display, scroll and zoom the map.
 */

#include "builder.hpp"
#include "cursor.hpp"
#include "display.hpp"
#include "preferences.hpp"
#include "gettext.hpp"
#include "halo.hpp"
#include "hotkeys.hpp"
#include "language.hpp"
#include "log.hpp"
#include "marked-up_text.hpp"
#include "map.hpp"
#include "map_label.hpp"
#include "text.hpp"
#include "time_of_day.hpp"
#include "tooltips.hpp"
#include "arrow.hpp"
#include "minimap.hpp"
#include "wml_exception.hpp"
#include "gui/widgets/minimap.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/auxiliary/event/handler.hpp"
#include "controller_base.hpp"

#include <boost/foreach.hpp>
#include "SDL_image.h"

#include <cmath>

namespace gui2 {
extern void async_draw();
}

static lg::log_domain log_display("display");
#define ERR_DP LOG_STREAM(err, log_display)
#define LOG_DP LOG_STREAM(info, log_display)
#define DBG_DP LOG_STREAM(debug, log_display)

namespace {
	const int MinZoom = 4;
	const int MaxZoom = 200;
	size_t sunset_delay = 0;

	bool benchmark = false;
}

int display::default_zoom_ = display::ZOOM_72;
int display::last_zoom_ = display::ZOOM_72;

int display::adjust_zoom(int zoom)
{
	if (zoom <= display::ZOOM_48) {
		return display::ZOOM_48;
	} else if (zoom <= display::ZOOM_56) {
		return display::ZOOM_56;
	} else if (zoom <= display::ZOOM_64) {
		return display::ZOOM_64;
	}
	return 72;
}

display* display::create_dummy_display(CVideo& video)
{
	static config dummy_cfg;
	static gamemap dummy_map(dummy_cfg, "");

	return new display(null_str, NULL, video, &dummy_map, dummy_cfg, dummy_cfg, 0);
}

#define KINGDOM_UNIT_RULES_SIZE		14

display::display(const std::string& tile, controller_base* controller, CVideo& video, const gamemap* map, const config& theme_cfg, const config& level, size_t num_reports)
	: controller_(controller)
	, screen_(video)
	, map_(map)
	, xpos_(0)
	, ypos_(0)
	, last_map_w_(0)
	, last_map_h_(0)
	, zero_(0, 0)
	, theme_cfg_()
	, main_map_area_(empty_rect)
	, border_()
	, theme_current_cfg_(NULL)
	, theme_(NULL)
	, zoom_(default_zoom_)
	, builder_(new terrain_builder(tile, map))
	, minimap_(NULL)
	, minimap_location_(empty_rect)
	, redrawMinimap_(false)
	, redraw_background_(true)
	, invalidateAll_(true)
	, grid_(false)
	, diagnostic_label_(0)
	, turbo_speed_(2)
	, turbo_(false)
	, invalidateGameStatus_(true)
	, map_labels_(new map_labels(*this, 0))
	, scroll_event_("scrolled")
	, nextDraw_(0)
	, mouseover_hex_overlay_(NULL)
	, tod_hex_mask1(NULL)
	, tod_hex_mask2(NULL)
	, fog_images_()
	, shroud_images_()
	, selectedHex_()
	, mouseoverHex_()
	, keys_()
	, animate_map_(true)
	, local_tod_light_(false)
	, drawing_buffer_()
	, canvas_drawing_buffer_()
	, to_canvas_(false)
	, map_screenshot_(false)
	, fps_handle_(0)
	, invalidated_hexes_(0)
	, drawn_hexes_(0)
	, idle_anim_rate_(1.0)
	, map_screenshot_surf_(NULL)
	, redraw_observers_()
	, draw_coordinates_(false)
	, draw_terrain_codes_(false)
	, arrows_map_()
	, draw_area_(NULL)
	, draw_area_pitch_(0)
	, draw_area_size_(0)
	, map_border_size_(0)
	, draw_area_unit_(NULL)
	, draw_area_unit_size_(0)
	, drawing_(false)
	, main_tip_handle_(0)
	, area_anims_()
	, min_zoom_(36)
	, max_zoom_(144)
	, reports_(num_reports)
{
	singleton_ = this;

	// show coordinate and terrain
	// draw_coordinates_ = true;
	// draw_terrain_codes_ = true;

	if (tile == game_config::tile_hex) {
		terrain_builder::unit_rules_size_ = KINGDOM_UNIT_RULES_SIZE;
	} else {
		terrain_builder::unit_rules_size_ = 0;
	}

	set_grid(preferences::grid());
	set_turbo(preferences::turbo());
	set_turbo_speed(preferences::turbo_speed());

	last_zoom_ = zoom_;

	fill_images_list(game_config::fog_prefix, fog_images_);
	fill_images_list(game_config::shroud_prefix, shroud_images_);

	// image::set_zoom(zoom_);

	draw_area_ = NULL;
	// allocate memory for access troops
	if (map_->w() && map_->h()) {
		draw_area_ = (uint8_t*)malloc(map_->total_width() * map_->total_height());
		last_map_w_ = map_->total_width();
		last_map_h_ = map_->total_height();
		memset(draw_area_, BOARD, last_map_w_ * last_map_h_);
		draw_area_pitch_ = last_map_w_;
		draw_area_size_ = last_map_w_ * last_map_h_;
		map_border_size_ = map_->border_size();
		draw_area_unit_ = (base_unit**)malloc(map_->w() * map_->h() * sizeof(base_unit*));
	} else {
		draw_area_ = NULL;
		last_map_w_ = -1;
		last_map_h_ = -1;
	}
}

display::~display()
{
	// at once release canvas animation.
	release_theme();

	clear_area_anims();

	if (draw_area_) {
		free(draw_area_);
	}
	if (draw_area_unit_) {
		free(draw_area_unit_);
		draw_area_unit_ = NULL;
		draw_area_unit_size_ = 0;
	}
	singleton_ = NULL;
}

void display::create_theme()
{
	const config& c = theme_current_cfg_->child("main_map");
	VALIDATE(c, "Theme must define main_map object!");
	main_map_area_ = theme::calculate_relative_loc(c, screen_.getx(), screen_.gety());

	if (const config &c = theme_current_cfg_->child("main_map_border")) {
		border_ = theme::tborder(c);
	} else {
		border_ = theme::tborder();
	}

	// create theme dialog
	static std::set<std::string> reserve_wml_tag;
	if (reserve_wml_tag.empty()) {
		reserve_wml_tag.insert("screen");
		reserve_wml_tag.insert("main_map_border");
		reserve_wml_tag.insert("main_map");
		reserve_wml_tag.insert("context_menu");
	}

	VALIDATE(!theme_, "theme_ must be NULL!");
	gui2::reload_window_builder(game_config::theme_window_id, *theme_current_cfg_, reserve_wml_tag);

	gui2::ttheme* dlg = create_theme_dlg(*theme_current_cfg_);
	// below aysn_show maybe exception, evalue dlg to theme_ before it make relase_theme can relase dlg.
	theme_ = dlg;

	dlg->asyn_show(screen_, main_map_area_);
	dlg->post_layout();
	controller_->events::handler::join();
}

void display::release_theme()
{
	if (theme_) {
		delete theme_;
		theme_ = NULL;
	}
}

const time_of_day& display::get_time_of_day(const map_location& /*loc*/) const
{
	static const time_of_day tod;
	return tod;
}

void display::fill_images_list(const std::string& prefix, std::vector<std::string>& images)
{
	// search prefix.png, prefix1.png, prefix2.png ...
	for(int i=0; ; ++i){
		std::ostringstream s;
		s << prefix;
		if(i != 0)
			s << i;
		s << ".png";
		if(image::exists(s.str()))
			images.push_back(s.str());
		else if(i>0)
			break;
	}
	if (images.empty())
		images.push_back("");
}

const std::string& display::get_variant(const std::vector<std::string>& variants, const map_location &loc) const
{
	//TODO use better noise function
	return variants[abs(loc.x + loc.y) % variants.size()];
}

void display::rebuild_all()
{
	// map editor: new/load other map, resize this map(this isn't call change_map)
	builder_->rebuild_all();
}

void display::reload_map()
{
	if (map_->total_width() != last_map_w_ || map_->total_height() != last_map_h_) {
		if (draw_area_) {
			free(draw_area_);
		}
		last_map_w_ = map_->total_width();
		last_map_h_ = map_->total_height();
		draw_area_ = (uint8_t*)malloc(last_map_w_ * last_map_h_);
		memset(draw_area_, BOARD, last_map_w_ * last_map_h_);
		draw_area_pitch_ = last_map_w_;
		draw_area_size_ = last_map_w_ * last_map_h_;
		map_border_size_ = map_->border_size();

		if (draw_area_unit_) {
			free(draw_area_unit_);
		}
		draw_area_unit_ = (base_unit**)malloc(map_->w() * map_->h() * sizeof(base_unit*));
	}
	builder_->reload_map();
}

void display::change_map(const gamemap* m)
{
	map_ = m;
	builder_->change_map(m);
}

const SDL_Rect& display::minimap_area() const
{
	gui2::twidget* widget = get_theme_object("mini-map");
	if (widget) {
		return widget->fix_rect(); 
	}
	return empty_rect;
}

const SDL_Rect& display::max_map_area() const
{
	static SDL_Rect max_area = {0, 0, 0, 0};

	// hex_size() is always a multiple of 4
	// and hex_width() a multiple of 3,
	// so there shouldn't be off-by-one-errors
	// due to rounding.
	// To display a hex fully on screen,
	// a little bit extra space is needed.
	// Also added the border two times.
	max_area.w  = static_cast<int>((get_map().w() + 2 * border_.size) * hex_width());
	max_area.h = static_cast<int>((get_map().h() + 2 * border_.size) * hex_size());

	return max_area;
}

const SDL_Rect& display::map_area() const
{
	static SDL_Rect max_area;
	max_area = max_map_area();

	// if it's for map_screenshot, maximize and don't recenter
	if (map_screenshot_) {
		return max_area;
	}

	static SDL_Rect res;
	res = map_outside_area();

	if(max_area.w < res.w) {
		// map is smaller, center
		res.x += (res.w - max_area.w)/2;
		res.w = max_area.w;
	}

	if(max_area.h < res.h) {
		// map is smaller, center
		res.y += (res.h - max_area.h)/2;
		res.h = max_area.h;
	}

	return res;
}

bool display::outside_area(const SDL_Rect& area, const int x, const int y) const
{
	const int x_thresh = hex_size();
	const int y_thresh = hex_size();
	return (x < area.x || x > area.x + area.w - x_thresh ||
		y < area.y || y > area.y + area.h - y_thresh);
}

double display::get_zoom_factor() const 
{ 
	return double(zoom_)/double(image::tile_size);
}

// This function use the screen as reference
const map_location display::hex_clicked_on(int xclick, int yclick) const
{
	const SDL_Rect& rect = map_area();
	if (point_in_rect(xclick,yclick,rect) == false) {
		return map_location();
	}

	xclick -= rect.x;
	yclick -= rect.y;

	return pixel_position_to_hex(xpos_ + xclick, ypos_ + yclick);
}


// This function use the rect of map_area as reference
const map_location display::pixel_position_to_hex(int x, int y) const
{
	// adjust for the border
	x -= static_cast<int>(border_.size * zoom_);
	y -= static_cast<int>(border_.size * zoom_);

	map_location result(x / zoom_, y / zoom_);

	if (x < 0) {
		result.x = -1;
	}
	if (y < 0) {
		result.y = -1;
	}

	return result;
}

void rect_of_hexes::iterator::operator++()
{
	if (loc_.y < rect_.bottom[loc_.x & 1])
		++loc_.y;
	else {
		++loc_.x;
		loc_.y = rect_.top[loc_.x & 1];
	}
}

// begin is top left, and end is after bottom right
rect_of_hexes::iterator rect_of_hexes::begin() const
{
	return iterator(map_location(left, top[left & 1]), *this);
}
rect_of_hexes::iterator rect_of_hexes::end() const
{
	return iterator(map_location(right+1, top[(right+1) & 1]), *this);
}

const rect_of_hexes display::hexes_under_rect(const SDL_Rect& r) const
{
	rect_of_hexes res;

	if (r.w<=0 || r.h<=0) {
		// empty rect, return dummy values giving begin=end
		res.left = 0;
		res.right = -1; // end is right+1
		res.top[0] = 0;
		res.top[1] = 0;
		res.bottom[0] = 0;
		res.bottom[1] = 0;
		return res;
	}

	SDL_Rect map_rect = map_area();
	// translate rect coordinates from screen-based to map_area-based
	int x = xpos_ - map_rect.x + r.x;
	int y = ypos_ - map_rect.y + r.y;
	// we use the "double" type to avoid important rounding error (size of an hex!)
	// we will also need to use std::floor to avoid bad rounding at border (negative values)
	double tile_width = hex_width();
	double tile_size = hex_size();
	double border = border_.size;

	// the "-0.25" is for the horizontal imbrication of hexes (1/4 overlaps).
	// res.left = static_cast<int>(std::floor(-border + x / tile_width - 0.25));
	res.left = static_cast<int>(std::floor(-border + x / tile_width));
	// we remove 1 pixel of the rectangle dimensions
	// (the rounded division take one pixel more than needed)
	res.right = static_cast<int>(std::floor(-border + (x + r.w-1) / tile_width));

	// for odd x, we must shift up one half-hex. Since x will vary along the edge,
	// we store here the y values for even and odd x, respectively
	res.top[0] = static_cast<int>(std::floor(-border + y / tile_size));
	res.top[1] = res.top[0];
	res.bottom[0] = static_cast<int>(std::floor(-border + (y + r.h-1) / tile_size));
	res.bottom[1] = res.bottom[0];

	// TODO: in some rare cases (1/16), a corner of the big rect is on a tile
	// (the 72x72 rectangle containing the hex) but not on the hex itself
	// Can maybe be optimized by using pixel_position_to_hex

	return res;
}

int display::get_location_x(const map_location& loc) const
{
	return static_cast<int>(map_area().x + (loc.x + border_.size) * zoom_ - xpos_);
}

int display::get_location_y(const map_location& loc) const
{
	return static_cast<int>(map_area().y + (loc.y + border_.size) * zoom_ - ypos_);
}

int display::get_scroll_pixel_x(int x) const
{
	return x - xpos_;
}

int display::get_scroll_pixel_y(int y) const
{
	return y - ypos_;
}

map_location display::minimap_location_on(int x, int y)
{
	//TODO: don't return location for this,
	// instead directly scroll to the clicked pixel position
	gui2::tminimap* minimap = dynamic_cast<gui2::tminimap*>(get_theme_object("mini-map"));
	if (!minimap) {
		return map_location();
	}
	SDL_Rect area = minimap->get_rect();
	if (!point_in_rect(x, y, area)) {
		return map_location();
	}
	x -= area.x;
	y -= area.y;

	// we transfom the coordinates from minimap to the full map image
	// probably more adjustements to do (border, minimap shift...)
	// but the mouse and human capacity to evaluate the rectangle center
	// is not pixel precise.
	int px = (x - minimap_location_.x) * get_map().w()*hex_width() / minimap_location_.w;
	int py = (y - minimap_location_.y) * get_map().h()*hex_size() / minimap_location_.h;

	map_location loc = pixel_position_to_hex(px, py);
	if (loc.x < 0)
		loc.x = 0;
	else if (loc.x >= get_map().w())
		loc.x = get_map().w() - 1;

	if (loc.y < 0)
		loc.y = 0;
	else if (loc.y >= get_map().h())
		loc.y = get_map().h() - 1;

	return loc;
}

int display::screenshot(std::string filename, bool map_screenshot)
{
	int size = 0;
	if (!map_screenshot) {
		surface screenshot_surf = screen_.getSurface();
		SDL_SaveBMP(screenshot_surf, filename.c_str());
		size = screenshot_surf->w * screenshot_surf->h;
	} else {
		if (get_map().empty()) {
			// Map Screenshot are big, abort and warn the user if he does strange things
			std::cerr << "No map, can't do a Map Screenshot. If it was not wanted, check your hotkey.\n";
			return -1;
		}

		SDL_Rect area = max_map_area();
		map_screenshot_surf_ = create_compatible_surface(screen_.getSurface(), area.w, area.h);

		if (map_screenshot_surf_ == NULL) {
			// Memory problem ?
			std::cerr << "Can't create the screenshot surface. Maybe too big, try dezooming.\n";
			return -1;
		}
		size = map_screenshot_surf_->w * map_screenshot_surf_->h;

		// back up the current map view position and move to top-left
		int old_xpos = xpos_;
		int old_ypos = ypos_;
		xpos_ = 0;
		ypos_ = 0;

		// we reroute render output to the screenshot surface and invalidate all
		map_screenshot_= true ;
		invalidateAll_ = true;
		DBG_DP << "draw() with map_screenshot\n";
		draw(true,true);

		// finally save the image on disk
		SDL_SaveBMP(map_screenshot_surf_, filename.c_str());

		//NOTE: need to be sure that we free this huge surface (is it enough?)
		map_screenshot_surf_ = NULL;

		// restore normal rendering
		map_screenshot_= false;
		xpos_ = old_xpos;
		ypos_ = old_ypos;
		// some drawing functions are confused by the temporary change
		// of the map_area and thus affect the UI outside of the map
		redraw_everything();
	}

	// convert pixel size to BMP size
	size = (2048 + size*3);
	return size;
}

void display::widget_set_pip_image(const std::string& id, const std::string& bg, const std::string& fg)
{
	gui2::tcontrol* widget = dynamic_cast<gui2::tcontrol*>(get_theme_object(id));
	if (widget) {
		int w = widget->fix_width();
		int h = widget->fix_height();
		if (!w || !h) {
			w = widget->get_width();
			h = widget->get_height();
		}
		surface surf = generate_pip_surface(w, h, bg, fg);
		widget->set_surface(surf, w, h);
	}
}

void display::widget_set_image(const std::string& id, const std::string& image)
{
	widget_set_surface(id, image::get_image(image));
}

void display::widget_set_surface(const std::string& id, const surface& surf)
{
	gui2::tcontrol* widget = dynamic_cast<gui2::tcontrol*>(get_theme_object(id));
	if (widget) {
		int w = widget->fix_width();
		int h = widget->fix_height();
		if (!w || !h) {
			w = widget->get_width();
			h = widget->get_height();
		}
		widget->set_surface(surf, w, h);
	}
}

static const std::string& get_direction(size_t n)
{
	static std::string const dirs[6] = { "-n", "-ne", "-se", "-s", "-sw", "-nw" };
	return dirs[n >= sizeof(dirs)/sizeof(*dirs) ? 0 : n];
}

std::vector<surface> display::get_fog_shroud_images(const map_location& loc, image::TYPE image_type)
{
	std::vector<std::string> names;

	map_location adjacent[6];
	get_adjacent_tiles(loc,adjacent);

	enum visibility {FOG=0, SHROUD=1, CLEAR=2};
	visibility tiles[6];

	const std::string* image_prefix[] =
		{ &game_config::fog_prefix, &game_config::shroud_prefix};

	for(int i = 0; i != 6; ++i) {
		if(shrouded(adjacent[i])) {
			tiles[i] = SHROUD;
		} else if(!fogged(loc) && fogged(adjacent[i])) {
			tiles[i] = FOG;
		} else {
			tiles[i] = CLEAR;
		}
	}

	for(int v = FOG; v != CLEAR; ++v) {
		// Find somewhere that doesn't have overlap to use as a starting point
		int start;
		for(start = 0; start != 6; ++start) {
			if(tiles[start] != v) {
				break;
			}
		}

		if(start == 6) {
			start = 0;
		}

		// Find all the directions overlap occurs from
		for(int i = (start+1)%6, n = 0; i != start && n != 6; ++n) {
			if(tiles[i] == v) {
				std::ostringstream stream;
				std::string name;
				stream << *image_prefix[v];

				for(int n = 0; v == tiles[i] && n != 6; i = (i+1)%6, ++n) {
					stream << get_direction(i);

					if(!image::exists(stream.str() + ".png")) {
						// If we don't have any surface at all,
						// then move onto the next overlapped area
						if(name.empty()) {
							i = (i+1)%6;
						}
						break;
					} else {
						name = stream.str();
					}
				}

				if(!name.empty()) {
					names.push_back(name + ".png");
				}
			} else {
				i = (i+1)%6;
			}
		}
	}

	// now get the surfaces
	std::vector<surface> res;

	BOOST_FOREACH (std::string& name, names) {
		const surface surf(image::get_image(name, image_type));
		if (surf)
			res.push_back(surf);
	}

	return res;
}

std::vector<surface> display::get_terrain_images(const map_location &loc,
						     const std::string& timeid,
		image::TYPE image_type,
		TERRAIN_TYPE terrain_type)
{
	std::vector<surface> res;
	terrain_builder::TERRAIN_TYPE builder_terrain_type =
	      (terrain_type == FOREGROUND ?
		  terrain_builder::FOREGROUND : terrain_builder::BACKGROUND);

	const terrain_builder::imagelist* const terrains = builder_->get_terrain_at(loc,
			timeid, builder_terrain_type);

	std::string color_mod;
	bool use_lightmap = false;
	bool use_local_light = local_tod_light_;
	if (use_local_light){
		const time_of_day& tod = get_time_of_day(loc);

		map_location adjs[6];
		get_adjacent_tiles(loc,adjs);
		static const std::string dir[6] = {"n","ne","se","s","sw","nw"};

		//get all the light transitions
		std::ostringstream light_trans;
		for(int d=0; d<6; ++d){
			const time_of_day& atod = get_time_of_day(adjs[d]);
			if(atod.red == tod.red && atod.green == tod.green && atod.blue == tod.blue)
				continue;

			light_trans
				<< "~BLIT("
					<< "terrain/light-" << dir[d] << ".png"
					<< "~CS("
						<< atod.red << ","
						<< atod.green << ","
						<< atod.blue
					<< ")" // CS
				<< ")"; //BLIT
			use_lightmap = true;
		}

		std::ostringstream mod;
		if (use_lightmap) {
			//generate the base of the lightmap
			//and add light transitions on it
			mod	<< "~L("
					<< "terrain/light.png"
					<< "~CS("
						<< tod.red << ","
						<< tod.green << ","
						<< tod.blue
					<< ")" // CS
					<< light_trans.str()
				<< ")"; // L
		} else {
			// no light map needed, but still need to color the hex
			const time_of_day& global_tod =
				get_time_of_day(map_location::null_location);
			if(tod.red == global_tod.red && tod.green == global_tod.green && tod.blue == global_tod.blue) {
				// It's the same as global ToD, don't use local light
				use_local_light = false;
			} else if (tod.red != 0 || tod.green != 0 || tod.blue != 0) {
				// simply color it if needed
				mod << "~CS("
						<< tod.red << ","
						<< tod.green << ","
						<< tod.blue
					<< ")"; // CS
			}
		}
		color_mod = mod.str();
	}

	if (terrains != NULL) {
		bool roaded = overlay_road_image(loc, color_mod);
		
		// Cache the offmap name.
		// Since it is themabel it can change,
		// so don't make it static.
		const std::string off_map_name = image::terrain_prefix + border_.tile_image;
		for (std::vector<animated<image::locator> >::const_iterator it =
				terrains->begin(); it != terrains->end(); ++it) {

			const image::locator &image = animate_map_ ?
				it->get_current_frame() : it->get_first_frame();

			// We prevent ToD coloring and brightening of off-map tiles,
			// except if we are not in_game and so in the editor.
			// We need to test for the tile to be rendered and
			// not the location, since the transitions are rendered
			// over the offmap-terrain and these need a ToD coloring.

			surface surf;

			if (!use_local_light && !roaded) {
				const bool off_map = (image.get_filename() == off_map_name);
				surf = image::get_image(image, off_map ? image::SCALED_TO_HEX : image_type);
			} else if (color_mod.empty()) {
				surf = image::get_image(image, image::SCALED_TO_HEX);
			} else {
				image::locator colored_image(
						image.get_filename(),
						image.get_loc(),
						image.get_center_x(), image.get_center_y(),
						image.get_modifications() + color_mod
					);

				const bool off_map = (image.get_filename() == off_map_name);
				surf = image::get_image(colored_image, off_map? image::SCALED_TO_HEX : image_type);
			}

			if (!surf.null()) {
				res.push_back(surf);
			}
		}
	}

	return res;
}

display::tcanvas_drawing_buffer_lock::tcanvas_drawing_buffer_lock(display& disp)
	: disp_(disp)
	, to_canvas_(disp.to_canvas_)
{
	disp_.to_canvas_ = true;
}

display::tcanvas_drawing_buffer_lock::~tcanvas_drawing_buffer_lock()
{
	disp_.to_canvas_ = to_canvas_;
}

void display::drawing_buffer_add(const tdrawing_layer layer,
		const map_location& loc, int x, int y, const surface& surf,
		const SDL_Rect &clip)
{
	tdrawing_buffer& drawing_buffer = to_canvas_? canvas_drawing_buffer_: drawing_buffer_;
	drawing_buffer.push_back(tblit(layer, loc, x, y, surf, clip));
}

void display::drawing_buffer_add(const tdrawing_layer layer,
		const map_location& loc, int x, int y,
		const std::vector<surface> &surf,
		const SDL_Rect &clip)
{
	tdrawing_buffer& drawing_buffer = to_canvas_? canvas_drawing_buffer_: drawing_buffer_;
	drawing_buffer.push_back(tblit(layer, loc, x, y, surf, clip));
}

// FIXME: temporary method. Group splitting should be made
// public into the definition of tdrawing_layer
//
// The drawing is done per layer_group, the range per group is [low, high).
const display::tdrawing_layer display::drawing_buffer_key::layer_groups[] = {
	LAYER_TERRAIN_BG,
	LAYER_UNIT_FIRST,
	LAYER_UNIT_MOVE_DEFAULT,
	// Make sure the movement doesn't show above fog and reachmap.
	LAYER_REACHMAP,
	LAYER_LAST_LAYER
};

// no need to change this if layer_groups above is changed
const unsigned int display::drawing_buffer_key::max_layer_group = sizeof(display::drawing_buffer_key::layer_groups) / sizeof(display::tdrawing_layer) - 2;

enum {
	// you may adjust the following when needed:

	// maximum border. 3 should be safe even if a larger border is in use somewhere
	MAX_BORDER           = 3,

	// store x, y, and layer in one 32 bit integer
	// 4 most significant bits == layer group   => 16
	BITS_FOR_LAYER_GROUP = 4,

	// 10 second most significant bits == y     => 1024
	BITS_FOR_Y           = 10,

	// 1 third most significant bit == x parity => 2
	BITS_FOR_X_PARITY    = 1,

	// 8 fourth most significant bits == layer   => 256
	BITS_FOR_LAYER       = 8,

	// 9 least significant bits == x / 2        => 512 (really 1024 for x)
	BITS_FOR_X_OVER_2    = 9
};

inline display::drawing_buffer_key::drawing_buffer_key(const map_location &loc, tdrawing_layer layer)
	: key_(0)
{
	// max_layer_group + 1 is the last valid entry in layer_groups, but it is always > layer
	// thus the first --g is a given => start with max_layer_groups right away
	unsigned int g = max_layer_group;
	while (layer < layer_groups[g]) {
		--g;
	}

	enum {
		SHIFT_LAYER          = BITS_FOR_X_OVER_2,
		SHIFT_X_PARITY       = BITS_FOR_LAYER + SHIFT_LAYER,
		SHIFT_Y              = BITS_FOR_X_PARITY + SHIFT_X_PARITY,
		SHIFT_LAYER_GROUP    = BITS_FOR_Y + SHIFT_Y
	};
	BOOST_STATIC_ASSERT(SHIFT_LAYER_GROUP + BITS_FOR_LAYER_GROUP == sizeof(key_) * 8);

	// the parity of x must be more significant than the layer but less significant than y.
	// Thus basically every row is split in two: First the row containing all the odd x
	// then the row containing all the even x. Since thus the least significant bit of x is
	// not required for x ordering anymore it can be shifted out to the right.
	const unsigned int x_parity = static_cast<unsigned int>(loc.x) & 1;
	key_  = (g << SHIFT_LAYER_GROUP) | (static_cast<unsigned int>(loc.y + MAX_BORDER) << SHIFT_Y);
	key_ |= (x_parity << SHIFT_X_PARITY);
	key_ |= (static_cast<unsigned int>(layer) << SHIFT_LAYER) | static_cast<unsigned int>(loc.x + MAX_BORDER) / 2;
}

SDL_Rect display::clip_rect_commit() const
{
	return in_theme()? map_area(): area_anim::rt.rect;
}

void display::drawing_buffer_commit(surface& screen)
{
	tdrawing_buffer& drawing_buffer = to_canvas_? canvas_drawing_buffer_: drawing_buffer_;
	// std::list::sort() is a stable sort
	drawing_buffer.sort();

	SDL_Rect clip_rect;
	if (screen.get() == get_screen_surface().get()) {
		clip_rect = clip_rect_commit();
	} else {
		clip_rect = area_anim::rt.rect;
	}
	clip_rect_setter set_clip_rect(screen, &clip_rect);

	/*
	 * Info regarding the rendering algorithm.
	 *
	 * In order to render a hex properly it needs to be rendered per row. On
	 * this row several layers need to be drawn at the same time. Mainly the
	 * unit and the background terrain. This is needed since both can spill
	 * in the next hex. The foreground terrain needs to be drawn before to
	 * avoid decapitation a unit.
	 *
	 * This ended in the following priority order:
	 * layergroup > location > layer > 'tblit' > surface
	 */

	BOOST_FOREACH (const tblit &blit, drawing_buffer) {
		BOOST_FOREACH (const surface& surf, blit.surf()) {
			// Note that dstrect can be changed by sdl_blit
			// and so a new instance should be initialized
			// to pass to each call to sdl_blit.
			SDL_Rect dstrect = create_rect(blit.x(), blit.y(), 0, 0);
			SDL_Rect srcrect = blit.clip();
			SDL_Rect *srcrectArg = (srcrect.x | srcrect.y | srcrect.w | srcrect.h)
				? &srcrect : NULL;
			sdl_blit(surf, srcrectArg, screen, &dstrect);
			//NOTE: the screen part should already be marked as 'to update'
		}
	}
	drawing_buffer.clear();
}

void display::sunset(const size_t delay)
{
	// This allow both parametric and toggle use
	sunset_delay = (sunset_delay == 0 && delay == 0) ? 3 : delay;
}

void display::toggle_benchmark()
{
	benchmark = !benchmark;
}

void display::flip()
{
	surface frameBuffer = get_video_surface();

	// This is just the debug function "sunset" to progressively darken the map area
	static size_t sunset_timer = 0;
	if (sunset_delay && ++sunset_timer > sunset_delay) {
		sunset_timer = 0;
		SDL_Rect r = map_outside_area(); // Use frameBuffer to also test the UI
		const Uint32 color =  SDL_MapRGBA(video().getSurface()->format,0,0,0,255);
		// Adjust the alpha if you want to balance cpu-cost / smooth sunset
		fill_rect_alpha(r, color, 1, frameBuffer);
	}

	font::draw_floating_labels(frameBuffer);
	events::raise_volatile_draw_event();
	cursor::draw(frameBuffer);

	video().flip();

	cursor::undraw(frameBuffer);
	events::raise_volatile_undraw_event();
	font::undraw_floating_labels(frameBuffer);
}

void display::update_display()
{
	if (screen_.update_locked()) {
		return;
	}

	if (benchmark) {
		static int last_sample = SDL_GetTicks();
		static int frames = 0;
		++frames;
		const int sample_freq = 10;
		if(frames == sample_freq) {
			const int this_sample = SDL_GetTicks();

			const int fps = (frames*1000)/(this_sample - last_sample);
			last_sample = this_sample;
			frames = 0;

			if(fps_handle_ != 0) {
				font::remove_floating_label(fps_handle_);
				fps_handle_ = 0;
			}
			std::ostringstream stream;
			stream << "fps: " << fps;
			drawn_hexes_ = 0;
			invalidated_hexes_ = 0;

			font::floating_label flabel(stream.str());
			flabel.set_font_size(12);
			flabel.set_color(benchmark ? font::BAD_COLOR : font::NORMAL_COLOR);
			flabel.set_position(10, 100);

			fps_handle_ = font::add_floating_label(flabel);
		}
	} else if(fps_handle_ != 0) {
		font::remove_floating_label(fps_handle_);
		fps_handle_ = 0;
		drawn_hexes_ = 0;
		invalidated_hexes_ = 0;
	}

	flip();
}

static void draw_background(surface screen, const SDL_Rect& area, const std::string& image)
{
	const surface background(image::get_image(image));
	if(background.null()) {
		return;
	}
	const unsigned int width = background->w;
	const unsigned int height = background->h;

	const unsigned int w_count = static_cast<int>(std::ceil(static_cast<double>(area.w) / static_cast<double>(width)));
	const unsigned int h_count = static_cast<int>(std::ceil(static_cast<double>(area.h) / static_cast<double>(height)));

	for(unsigned int w = 0, w_off = area.x; w < w_count; ++w, w_off += width) {
		for(unsigned int h = 0, h_off = area.y; h < h_count; ++h, h_off += height) {
			SDL_Rect clip = create_rect(w_off, h_off, 0, 0);
			sdl_blit(background, NULL, screen, &clip);
		}
	}
}

void display::draw_text_in_hex(const map_location& loc,
		const tdrawing_layer layer, const std::string& text,
		size_t font_size, SDL_Color color, double x_in_hex, double y_in_hex)
{
	if (text.empty()) return;

	const size_t font_sz = static_cast<size_t>(font_size * get_zoom_factor());

	surface text_surf = font::get_rendered_text2(text, -1, font_sz, color);
	surface back_surf = font::get_rendered_text2(text, -1, font_sz, font::BLACK_COLOR);
	const int x = get_location_x(loc) - text_surf->w/2
	              + static_cast<int>(x_in_hex* hex_size());
	const int y = get_location_y(loc) - text_surf->h/2
	              + static_cast<int>(y_in_hex* hex_size());

	for (int dy=-1; dy <= 1; ++dy) {
		for (int dx=-1; dx <= 1; ++dx) {
			if (dx!=0 || dy!=0) {
				drawing_buffer_add(layer, loc, x + dx, y + dy, back_surf);
			}
		}
	}
	drawing_buffer_add(layer, loc, x, y, text_surf);
}

void display::draw_text_in_hex2(const map_location& loc,
		const tdrawing_layer layer, const std::string& text,
		size_t font_size, SDL_Color color, int x, int y, fixed_t alpha, bool center)
{
	if (text.empty()) return;

	const size_t font_sz = static_cast<size_t>(font_size * get_zoom_factor());

	surface	text_surf = font::get_rendered_text2(text, -1, font_sz, color);
	surface	back_surf = font::get_rendered_text2(text, -1, font_sz, font::BLACK_COLOR);

	if (center) {
		x = x - text_surf->w/2;
		y = y - text_surf->h/2;
	}

	for (int dy=-1; dy <= 1; ++dy) {
		for (int dx=-1; dx <= 1; ++dx) {
			if (dx!=0 || dy!=0) {
				drawing_buffer_add(layer, loc, x + dx, y + dy, back_surf);
			}
		}
	}

	drawing_buffer_add(layer, loc, x, y, text_surf);
}

void display::render_image(int x, int y, const display::tdrawing_layer drawing_layer,
		const map_location& loc, surface image,
		bool hreverse, bool greyscale, fixed_t alpha,
		Uint32 blendto, double blend_ratio, double submerged, bool vreverse)
{

	if (image==NULL) {
		return;
	}
	surface surf(image);

	if(hreverse) {
		surf = image::reverse_image(surf);
	}
	if(vreverse) {
		surf = flop_surface(surf, false);
	}

	if(greyscale) {
		surf = greyscale_image(surf, false);
	}

	if(blend_ratio != 0) {
		surf = blend_surface(surf, blend_ratio, blendto, false);
	}
	if(alpha > ftofxp(1.0)) {
		surf = brighten_image(surf, alpha, false);
	//} else if(alpha != 1.0 && blendto != 0) {
	//	surf.assign(blend_surface(surf,1.0-alpha,blendto));
	} else if(alpha != ftofxp(1.0)) {
		surf = adjust_surface_alpha(surf, alpha, false);
	}

	if(surf == NULL) {
		ERR_DP << "surface lost...\n";
		return;
	}

	if(submerged > 0.0) {
		// divide the surface into 2 parts
		const int submerge_height = std::max<int>(0, surf->h*(1.0-submerged));
		const int depth = surf->h - submerge_height;
		SDL_Rect srcrect = create_rect(0, 0, surf->w, submerge_height);
		drawing_buffer_add(drawing_layer, loc, x, y, surf, srcrect);

		if(submerge_height != surf->h) {
			//the lower part will be transparent
			float alpha_base = 0.3; // 30% alpha at surface of water
			float alpha_delta = 0.015; // lose 1.5% per pixel depth
			alpha_delta *= zoom_ / default_zoom_; // adjust with zoom
			surf = submerge_alpha(surf, depth, alpha_base, alpha_delta, false);

			srcrect.y = submerge_height;
			srcrect.h = surf->h-submerge_height;
			y += submerge_height;

			drawing_buffer_add(drawing_layer, loc, x, y, surf, srcrect);
		}
	} else {
		// simple blit
		drawing_buffer_add(drawing_layer, loc, x, y, surf);
	}

}

void display::select_hex(map_location hex)
{
	invalidate(selectedHex_);
	selectedHex_ = hex;
	invalidate(selectedHex_);
}

void display::highlight_hex(map_location hex)
{
	invalidate(mouseoverHex_);
	mouseoverHex_ = hex;
	invalidate(mouseoverHex_);
}

void display::set_diagnostic(const std::string& msg)
{
	if(diagnostic_label_ != 0) {
		font::remove_floating_label(diagnostic_label_);
		diagnostic_label_ = 0;
	}

	if(msg != "") {
		font::floating_label flabel(msg);
		flabel.set_font_size(font::SIZE_NORMAL);
		flabel.set_color(font::YELLOW_COLOR);
		flabel.set_position(300, 50);
		flabel.set_clip_rect(map_outside_area());

		diagnostic_label_ = font::add_floating_label(flabel);
	}
}

void display::draw_init()
{
	if (get_map().empty()) {
		return;
	}

	if(benchmark) {
		invalidateAll_ = true;
	}

	if(redraw_background_) {
		// Full redraw of the background
		const SDL_Rect clip_rect = map_outside_area();
		const surface screen = get_screen_surface();
		clip_rect_setter set_clip_rect(screen, &clip_rect);
		draw_background(screen, clip_rect, border_.background_image);

		redraw_background_ = false;

		// Force a full map redraw
		invalidateAll_ = true;
	}

	if(invalidateAll_) {
		DBG_DP << "draw() with invalidateAll\n";

		// toggle invalidateAll_ first to allow regular invalidations
		invalidateAll_ = false;
		invalidate_locations_in_rect(map_area());

		redrawMinimap_ = true;
	}
}

void display::draw_wrap(bool update, bool force)
{
	static const int time_between_draws = 20;
	const int current_time = SDL_GetTicks();
	const int wait_time = nextDraw_ - current_time;

	if(redrawMinimap_) {
		redrawMinimap_ = false;
		draw_minimap();
	}

	if(update) {
		update_display();
		if(!force && !benchmark && wait_time > 0) {
			// If it's not time yet to draw, delay until it is
			SDL_Delay(wait_time);
		}

		// Set the theortical next draw time
		nextDraw_ += time_between_draws;

		// If the next draw already should have been finished,
		// we'll enter an update frenzy, so make sure that the
		// too late value doesn't keep growing.
		// Note: if force is used too often,
		// we can also get the opposite effect.
		nextDraw_ = std::max<int>(nextDraw_, SDL_GetTicks());
	}
}

void display::delay(unsigned int milliseconds) const
{
	if (!game_config::no_delay)
		SDL_Delay(milliseconds);
}

// @flags: which items will be display/hide
void display::goto_main_context_menu()
{
}

void display::announce(const std::string& message, const SDL_Color& color)
{
	font::floating_label flabel(message);
	flabel.set_font_size(font::SIZE_XLARGE);
	flabel.set_color(color);
	flabel.set_position(map_outside_area().w/2, map_outside_area().h/3);
	flabel.set_lifetime(100);
	flabel.set_clip_rect(map_outside_area());

	font::add_floating_label(flabel);
}

void display::draw_border(const map_location& loc, const int xpos, const int ypos)
{
	/**
	 * at the moment the border must be between 0.0 and 0.5
	 * and the image should always be prepared for a 0.5 border.
	 * This way this code doesn't need modifications for other border sizes.
	 */

	// First handle the corners :
	if(loc.x == -1 && loc.y == -1) { // top left corner
		drawing_buffer_add(LAYER_BORDER, loc, xpos + zoom_/4, ypos,
			image::get_image(border_.corner_image_top_left, image::SCALED_TO_ZOOM));
	} else if(loc.x == get_map().w() && loc.y == -1) { // top right corner
		// We use the map idea of odd and even, and map coords are internal coords + 1
		drawing_buffer_add(LAYER_BORDER, loc, xpos, ypos,
			image::get_image(border_.corner_image_top_right_even, image::SCALED_TO_ZOOM));
	} else if(loc.x == -1 && loc.y == get_map().h()) { // bottom left corner
		drawing_buffer_add(LAYER_BORDER, loc, xpos + zoom_/4, ypos,
			image::get_image(border_.corner_image_bottom_left, image::SCALED_TO_ZOOM));

	} else if(loc.x == get_map().w() && loc.y == get_map().h()) { // bottom right corner
		// We use the map idea of odd and even, and map coords are internal coords + 1
		drawing_buffer_add(LAYER_BORDER, loc, xpos, ypos,
			image::get_image(border_.corner_image_bottom_right_even, image::SCALED_TO_ZOOM));
		// Now handle the sides:
	} else if(loc.x == -1) { // left side
		drawing_buffer_add(LAYER_BORDER, loc, xpos + zoom_/4, ypos,
			image::get_image(border_.border_image_left, image::SCALED_TO_ZOOM));
	} else if(loc.x == get_map().w()) { // right side
		drawing_buffer_add(LAYER_BORDER, loc, xpos + zoom_/4, ypos,
			image::get_image(border_.border_image_right, image::SCALED_TO_ZOOM));
	} else if(loc.y == -1) { // top side
		// We use the map idea of odd and even, and map coords are internal coords + 1
		drawing_buffer_add(LAYER_BORDER, loc, xpos, ypos + zoom_/2,
			image::get_image(border_.border_image_top_odd, image::SCALED_TO_ZOOM));

	} else if(loc.y == get_map().h()) { // bottom side
		// We use the map idea of odd and even, and map coords are internal coords + 1
		drawing_buffer_add(LAYER_BORDER, loc, xpos, ypos + zoom_/2,
			image::get_image(border_.border_image_bottom_odd, image::SCALED_TO_ZOOM));
	}
}

gui2::twidget* display::get_theme_object(const std::string& id) const
{
	return theme_->get_object(id);
}

void display::set_theme_object_active(const std::string& id, bool active) const
{
	theme_->set_object_active(id, active);
}

void display::set_theme_object_visible(const std::string& id, const gui2::twidget::tvisible visible) const
{
	theme_->set_object_visible(id, visible);
}

void display::set_theme_object_surface(const std::string& id, const surface& surf) const
{
	theme_->set_object_surface(id, surf);
}

gui2::twidget* display::get_theme_report(int num) const
{
	return theme_->get_report(num);
}

void display::set_theme_report_label(int num, const std::string& label) const
{
	return theme_->set_report_label(num, label);
}

void display::set_theme_report_surface(int num, const surface& surf) const
{
	return theme_->set_report_surface(num, surf);
}

surface display::minimap_surface(int w, int h) 
{ 
	return image::getMinimap(w, h, get_map(), NULL);
}

double display::minimap_shift_x(const SDL_Rect& map_rect, const SDL_Rect& map_out_rect) const
{
	return - border_.size * hex_width() - (map_out_rect.w - map_rect.w) / 2;
}

double display::minimap_shift_y(const SDL_Rect& map_rect, const SDL_Rect& map_out_rect) const
{
	return - border_.size * hex_size() - (map_out_rect.h - map_rect.h) / 2;
}

void display::draw_minimap()
{
	gui2::tminimap* widget = dynamic_cast<gui2::tminimap*>(get_theme_object("mini-map"));
	if (!widget) {
		return;
	}
	SDL_Rect area = widget->get_rect();

	// const SDL_Rect& area = minimap_area();
	if (minimap_ == NULL || minimap_->w > area.w || minimap_->h > area.h) {
		minimap_ = minimap_surface(area.w, area.h);
		if (minimap_ == NULL) {
			return;
		}
	}

	surface screen = create_neutral_surface(area.w, area.h);
	area.x = area.y = 0;

	SDL_Color back_color = {31,31,23,255};
	draw_centered_on_background(minimap_, area, back_color, screen);

	//update the minimap location for mouse and units functions
	minimap_location_.x = area.x + (area.w - minimap_->w) / 2;
	minimap_location_.y = area.y + (area.h - minimap_->h) / 2;
	minimap_location_.w = minimap_->w;
	minimap_location_.h = minimap_->h;

	draw_minimap_units(screen);

	// calculate the visible portion of the map:
	// scaling between minimap and full map images
	double xscaling = 1.0*minimap_->w / (get_map().w()*hex_width());
	double yscaling = 1.0*minimap_->h / (get_map().h()*hex_size());

	// we need to shift with the border size
	// and the 0.25 from the minimap balanced drawing
	// and the possible difference between real map and outside off-map
	SDL_Rect map_rect = map_area();
	SDL_Rect map_out_rect = map_outside_area();
	double border = border_.size;
	double shift_x = minimap_shift_x(map_rect, map_out_rect);
	double shift_y = minimap_shift_y(map_rect, map_out_rect);

	int view_x = static_cast<int>((xpos_ + shift_x) * xscaling);
	int view_y = static_cast<int>((ypos_ + shift_y) * yscaling);
	int view_w = static_cast<int>(map_out_rect.w * xscaling);
	int view_h = static_cast<int>(map_out_rect.h * yscaling);

	const Uint32 box_color = SDL_MapRGB(minimap_->format, border_.view_rectange_color.r, border_.view_rectange_color.g, border_.view_rectange_color.b);
	draw_rectangle(minimap_location_.x + view_x - 1,
                   minimap_location_.y + view_y - 1,
                   view_w + 2, view_h + 2,
                   box_color, screen);

	widget->set_surface(screen, area.w, area.h);
}

bool display::scroll(int xmove, int ymove)
{
	const int orig_x = xpos_;
	const int orig_y = ypos_;
	xpos_ += xmove;
	ypos_ += ymove;
	bounds_check_position();
	const int dx = orig_x - xpos_; // dx = -xmove
	const int dy = orig_y - ypos_; // dy = -ymove

	// Only invalidate if we've actually moved
	if(dx == 0 && dy == 0)
		return false;

	font::scroll_floating_labels(dx, dy);

	surface screen(screen_.getSurface());

	SDL_Rect dstrect = map_area();
	dstrect.x += dx;
	dstrect.y += dy;
	dstrect = intersect_rects(dstrect, map_area());

	SDL_Rect srcrect = dstrect;
	srcrect.x -= dx;
	srcrect.y -= dy;
	if (!screen_.update_locked()) {
		sdl_blit(screen, &srcrect, screen, &dstrect);
	}
/*
//This is necessary to avoid a crash in some SDL versions on some systems
//see http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=462794
//FIXME remove this once the latest stable SDL release doesn't crash as 1.2.13 does
#ifdef _MSC_VER
    __asm{cld};
#elif defined(__GNUG__) && (defined(__i386__) || defined(__x86_64__))
    asm("cld");
#endif
*/
	// Invalidate locations in the newly visible rects

	if (dy != 0) {
		SDL_Rect r = map_area();
		if(dy < 0)
			r.y = r.y + r.h + dy;
		r.h = abs(dy);
		invalidate_locations_in_rect(r);
	}
	if (dx != 0) {
		SDL_Rect r = map_area();
		if (dx < 0)
			r.x = r.x + r.w + dx;
		r.w = abs(dx);
		invalidate_locations_in_rect(r);
	}
	scroll_event_.notify_observers();

	redrawMinimap_ = true;
	return true;
}

void display::set_zoom(int amount)
{
	int new_zoom = zoom_ + amount;
	if (new_zoom < MinZoom) {
		new_zoom = MinZoom;
	}
	if (new_zoom > MaxZoom) {
		new_zoom = MaxZoom;
	}
	if (new_zoom != zoom_) {
		SDL_Rect const &area = map_area();
		xpos_ += (xpos_ + area.w / 2) * amount / zoom_;
		ypos_ += (ypos_ + area.h / 2) * amount / zoom_;

		zoom_ = new_zoom;
		bounds_check_position();
		if (zoom_ != default_zoom_) {
			last_zoom_ = zoom_;
		}
		image::set_zoom(zoom_);

		labels().recalculate_labels();
		redraw_background_ = true;
		invalidate_all();

		// Forces a redraw after zooming.
		// This prevents some graphic glitches from occurring.
		draw();
	}
}

void display::set_default_zoom()
{
	if (zoom_ != default_zoom_) {
		last_zoom_ = zoom_;
		set_zoom(default_zoom_ - zoom_ );
	} else {
		// When we are already at the default zoom,
		// switch to the last zoom used
		set_zoom(last_zoom_ - zoom_);
	}
}

bool display::tile_fully_on_screen(const map_location& loc)
{
	int x = get_location_x(loc);
	int y = get_location_y(loc);
	return !outside_area(map_area(), x, y);
}

bool display::tile_nearly_on_screen(const map_location& loc) const
{
	int x = get_location_x(loc);
	int y = get_location_y(loc);
	const SDL_Rect &area = map_area();
	int hw = hex_width(), hs = hex_size();
	return x + hs >= area.x - hw && x < area.x + area.w + hw &&
	       y + hs >= area.y - hs && y < area.y + area.h + hs;
}

void display::scroll_to_xy(int screenxpos, int screenypos, SCROLL_TYPE scroll_type, bool force)
{
	if (!force && !preferences::scroll_to_action()) return;
	if (screen_.update_locked()) {
		return;
	}
	const SDL_Rect area = map_area();
	const int xmove_expected = screenxpos - (area.x + area.w/2);
	const int ymove_expected = screenypos - (area.y + area.h/2);

	int xpos = xpos_ + xmove_expected;
	int ypos = ypos_ + ymove_expected;
	bounds_check_position(xpos, ypos);
	int xmove = xpos - xpos_;
	int ymove = ypos - ypos_;

	if (scroll_type == WARP || turbo_speed() > 2.0 || preferences::scroll_speed() > 99) {
		scroll(xmove,ymove);
		draw();
		return;
	}

	// Doing an animated scroll, with acceleration etc.

	int x_old = 0;
	int y_old = 0;

	const double dist_total = hypot(xmove, ymove);
	double dist_moved = 0.0;

	int t_prev = SDL_GetTicks();

	double velocity = 0.0;
	while (dist_moved < dist_total) {
		events::pump();

		int t = SDL_GetTicks();
		double dt = (t - t_prev) / 1000.0;
		if (dt > 0.200) {
			// Do not skip too many frames on slow PCs
			dt = 0.200;
		}
		t_prev = t;

		//std::cout << t << " " << hypot(x_old, y_old) << "\n";

		/** @todo Those values might need some fine-tuning: */
		const double accel_time = 0.3 / turbo_speed(); // seconds until full speed is reached
		const double decel_time = 0.4 / turbo_speed(); // seconds from full speed to stop

		double velocity_max = preferences::scroll_speed() * 60.0;
		velocity_max *= turbo_speed();
		double accel = velocity_max / accel_time;
		double decel = velocity_max / decel_time;

		// If we started to decelerate now, where would we stop?
		double stop_time = velocity / decel;
		double dist_stop = dist_moved + velocity*stop_time - 0.5*decel*stop_time*stop_time;
		if (dist_stop > dist_total || velocity > velocity_max) {
			velocity -= decel * dt;
			if (velocity < 1.0) velocity = 1.0;
		} else {
			velocity += accel * dt;
			if (velocity > velocity_max) velocity = velocity_max;
		}

		dist_moved += velocity * dt;
		if (dist_moved > dist_total) dist_moved = dist_total;

		int x_new = round_double(xmove * dist_moved / dist_total);
		int y_new = round_double(ymove * dist_moved / dist_total);

		int dx = x_new - x_old;
		int dy = y_new - y_old;

		scroll(dx,dy);
		x_old += dx;
		y_old += dy;
		draw();
	}
}

void display::scroll_to_tile(const map_location& loc, SCROLL_TYPE scroll_type, bool check_fogged, bool force)
{
	if(get_map().on_board(loc) == false) {
		ERR_DP << "Tile at " << loc << " isn't on the map, can't scroll to the tile.\n";
		return;
	}

	std::vector<map_location> locs;
	locs.push_back(loc);
	scroll_to_tiles(locs, scroll_type, check_fogged,false,0.0,force);
}

void display::scroll_to_tiles(map_location loc1, map_location loc2,
                              SCROLL_TYPE scroll_type, bool check_fogged,
			      double add_spacing, bool force)
{
	std::vector<map_location> locs;
	locs.push_back(loc1);
	locs.push_back(loc2);
	scroll_to_tiles(locs, scroll_type, check_fogged, false, add_spacing,force);
}

void display::scroll_to_tiles(const std::vector<map_location>& locs,
                              SCROLL_TYPE scroll_type, bool check_fogged,
			      bool only_if_possible, double add_spacing, bool force)
{
	// basically we calculate the min/max coordinates we want to have on-screen
	int minx = 0;
	int maxx = 0;
	int miny = 0;
	int maxy = 0;
	bool valid = false;

	for(std::vector<map_location>::const_iterator itor = locs.begin(); itor != locs.end() ; ++itor) {
		if(get_map().on_board(*itor) == false) continue;
		if(check_fogged && fogged(*itor)) continue;

		int x = get_location_x(*itor);
		int y = get_location_y(*itor);

		if (!valid) {
			minx = x;
			maxx = x;
			miny = y;
			maxy = y;
			valid = true;
		} else {
			int minx_new = std::min<int>(minx,x);
			int miny_new = std::min<int>(miny,y);
			int maxx_new = std::max<int>(maxx,x);
			int maxy_new = std::max<int>(maxy,y);
			SDL_Rect r = map_area();
			r.x = minx_new;
			r.y = miny_new;
			if(outside_area(r, maxx_new, maxy_new)) {
				// we cannot fit all locations to the screen
				if (only_if_possible) return;
				break;
			}
			minx = minx_new;
			miny = miny_new;
			maxx = maxx_new;
			maxy = maxy_new;

		}
	}
	//if everything is fogged or the locs list is empty
	if(!valid) return;

	if (scroll_type == ONSCREEN) {
		SDL_Rect r = map_area();
		int spacing = round_double(add_spacing*hex_size());
		r.x += spacing;
		r.y += spacing;
		r.w -= 2*spacing;
		r.h -= 2*spacing;
		if (!outside_area(r, minx,miny) && !outside_area(r, maxx,maxy)) {
			return;
		}
	}

	// let's do "normal" rectangle math from now on
	SDL_Rect locs_bbox;
	locs_bbox.x = minx;
	locs_bbox.y = miny;
	locs_bbox.w = maxx - minx + hex_size();
	locs_bbox.h = maxy - miny + hex_size();

	// target the center
	int target_x = locs_bbox.x + locs_bbox.w/2;
	int target_y = locs_bbox.y + locs_bbox.h/2;

	if (scroll_type == ONSCREEN) {
		// when doing an ONSCREEN scroll we do not center the target unless needed
		SDL_Rect r = map_area();
		int map_center_x = r.x + r.w/2;
		int map_center_y = r.y + r.h/2;

		int h = r.h;
		int w = r.w;

		// we do not want to be only inside the screen rect, but center a bit more
		double inside_frac = 0.5; // 0.0 = always center the target, 1.0 = scroll the minimum distance
		w = static_cast<int>(w * inside_frac);
		h = static_cast<int>(h * inside_frac);

		// shrink the rectangle by the size of the locations rectangle we found
		// such that the new task to fit a point into a rectangle instead of rectangle into rectangle
		w -= locs_bbox.w;
		h -= locs_bbox.h;

		if (w < 1) w = 1;
		if (h < 1) h = 1;

		r.x = target_x - w/2;
		r.y = target_y - h/2;
		r.w = w;
		r.h = h;

		// now any point within r is a possible target to scroll to
		// we take the one with the minimum distance to map_center
		// which will always be at the border of r

		if (map_center_x < r.x) {
			target_x = r.x;
			target_y = map_center_y;
			if (target_y < r.y) target_y = r.y;
			if (target_y > r.y+r.h-1) target_y = r.y+r.h-1;
		} else if (map_center_x > r.x+r.w-1) {
			target_x = r.x+r.w-1;
			target_y = map_center_y;
			if (target_y < r.y) target_y = r.y;
			if (target_y >= r.y+r.h) target_y = r.y+r.h-1;
		} else if (map_center_y < r.y) {
			target_y = r.y;
			target_x = map_center_x;
			if (target_x < r.x) target_x = r.x;
			if (target_x > r.x+r.w-1) target_x = r.x+r.w-1;
		} else if (map_center_y > r.y+r.h-1) {
			target_y = r.y+r.h-1;
			target_x = map_center_x;
			if (target_x < r.x) target_x = r.x;
			if (target_x > r.x+r.w-1) target_x = r.x+r.w-1;
		} else {
			ERR_DP << "Bug in the scrolling code? Looks like we would not need to scroll after all...\n";
			// keep the target at the center
		}
	}

	scroll_to_xy(target_x, target_y,scroll_type,force);
}


void display::bounds_check_position()
{
	const int orig_zoom = zoom_;

	if(zoom_ < MinZoom) {
		zoom_ = MinZoom;
	}

	if(zoom_ > MaxZoom) {
		zoom_ = MaxZoom;
	}

	bounds_check_position(xpos_, ypos_);

	if(zoom_ != orig_zoom) {
		image::set_zoom(zoom_);
	}
}

void display::bounds_check_position(int& xpos, int& ypos)
{
	const int tile_width = hex_width();

	// Adjust for the border 2 times
	const SDL_Rect& max_area = max_map_area();
	const SDL_Rect& rect = map_area();

	const int xend = max_area.w;
	const int yend = max_area.h;

	if (xpos > xend - rect.w) {
		xpos = xend - rect.w;
	}

	if (ypos > yend - rect.h) {
		ypos = yend - rect.h;
	}

	if (xpos < 0) {
		xpos = 0;
	}

	if (ypos < 0) {
		ypos = 0;
	}
}

double display::turbo_speed() const
{
	bool res = turbo_;
	if(keys_[SDLK_LSHIFT] || keys_[SDLK_RSHIFT]) {
		res = !res;
	}

	if (res)
		return turbo_speed_;
	else
		return 1.0;
}

namespace hotkey {
extern void clear();
}

void display::change_resolution()
{
	if (controller_) {
		hotkey::clear();

		std::map<const std::string, bool> actives;
		pre_change_resolution(actives);

		release_theme();
		// video_.
		theme::set_resolution2(theme_cfg_, screen_.getx(), screen_.gety());
		create_theme();

		post_change_resolution(actives);
		for (std::map<const std::string, bool>::const_iterator it = actives.begin(); it != actives.end(); ++ it) {
			set_theme_object_active(it->first, it->second);
		}
		redraw_everything();
	}
}

void display::redraw_everything()
{
	if (screen_.update_locked()) {
		return;
	}

	invalidateGameStatus_ = true;

	for (std::vector<reports::report>::iterator it = reports_.begin(); it != reports_.end(); ++ it) {
		*it = reports::report();
	}

	bounds_check_position();

	tooltips::clear_tooltips();

	labels().recalculate_labels();

	redraw_background_ = true;

	int ticks1 = SDL_GetTicks();
	invalidate_all();
	int ticks2 = SDL_GetTicks();
	draw(true,true);
	int ticks3 = SDL_GetTicks();
	LOG_DP << "invalidate and draw: " << (ticks3 - ticks2) << " and " << (ticks2 - ticks1) << "\n";

	BOOST_FOREACH (boost::function<void(display&)> f, redraw_observers_) {
		f(*this);
	}
}

void display::click_context_menu(const std::string& main, const std::string& id, size_t flags)
{
	std::vector<std::string> items = utils::split(id, ':');
	size_t pos = items[0].rfind("_m");
	if (pos == items[0].size() - 2) {
		// cancel current menu, and display sub-menu
		const std::string item1 = items[0].substr(0, pos);
		show_context_menu(main, item1);
	} else {
		// execute one normal command
		if (flags & gui2::tcontext_menu::F_HIDE) {
			hide_context_menu();
		}
		if (flags & gui2::tcontext_menu::F_PARAM) {
			controller_->execute_command2(hotkey::get_hotkey(items[0]).get_id(), items[1]);

		} else {
			controller_->execute_command2(hotkey::get_hotkey(items.back()).get_id(), null_str);
		}
	}
}

void display::show_context_menu(const std::string& main_id, const std::string& id)
{
	const gui2::tcontext_menu* menu = NULL;
	if (!main_id.empty()) {
		menu = theme_->context_menu(main_id);
	} else {
		std::vector<gui2::tcontext_menu>& menus = theme_->context_menus();
		menu = &menus.front();
	}

	menu->show(*this, *controller_, id);
}

void display::hide_context_menu(const std::string& main_id)
{
	const gui2::tcontext_menu* menu = NULL;
	if (!main_id.empty()) {
		menu = theme_->context_menu(main_id);
	} else {
		std::vector<gui2::tcontext_menu>& menus = theme_->context_menus();
		menu = &menus.front();
	}

	menu->hide();
}

void display::add_redraw_observer(boost::function<void(display&)> f)
{
	redraw_observers_.push_back(f);
}

void display::clear_redraw_observers()
{
	redraw_observers_.clear();
}

rect_of_hexes& display::draw_area()
{
	return draw_area_rect_;
}

void display::draw(bool update,bool force) 
{
	if (screen_.update_locked()) {
		return;
	}

	local_tod_light_ = has_time_area();

	// enter draw, set flag
	drawing_ = true;

	//
	// recalculate draw area
	//
	draw_area_rect_ = get_visible_hexes();
	
	draw_init();
	pre_draw(draw_area_rect_);

	// invalidate all that needs to be invalidated
	invalidate_animations();

	// update_time_of_day();
	// these new invalidations can not cause any propagation because
	// if a hex was invalidated last turn but not this turn, then
	// * case of no unit in neighbour hex=> no propagation
	// * case of unit in hex but was there last turn=>its hexes are invalidated too
	// * case of unit inhex not there last turn => it moved, so was invalidated previously
	if (!get_map().empty()) {
		invalidate_theme();

		/*
		 * draw_invalidated() also invalidates the halos, so also needs to be
		 * ran if invalidated_.empty() == true.
		 */
		draw_invalidated();

		drawing_buffer_commit(screen_.getSurface());
		gui2::async_draw();

		post_commit();

		// clear flag. 
		// draw_sidebar may genrate new invalidate loc(ex. show_unit_tip), keep those dirty so next draw will update then
		memset(draw_area_, BOARD, draw_area_size_);

		draw_sidebar();
	}

	draw_wrap(update, force);
	
	drawing_ = false;
}

map_labels& display::labels()
{
	return *map_labels_;
}

const map_labels& display::labels() const
{
	return *map_labels_;
}

void display::clear_screen()
{
	surface disp(screen_.getSurface());
	SDL_Rect area = screen_area();
	sdl_fill_rect(disp, &area, SDL_MapRGB(disp->format, 0, 0, 0));
}

const SDL_Rect& display::get_clip_rect()
{
	return map_area();
}

void display::pre_draw(rect_of_hexes& hexes)
{
}

void display::draw_invalidated() 
{
	SDL_Rect clip_rect = get_clip_rect();
	surface screen = get_screen_surface();
	clip_rect_setter set_clip_rect(screen, &clip_rect);

	for (int x = draw_area_rect_.left; x <= draw_area_rect_.right; x ++) {
		for (int y = draw_area_rect_.top[x & 1]; y <= draw_area_rect_.bottom[x & 1]; y ++) {
			if (draw_area_val(x, y) < INVALIDATE) {
				continue;
			}
			const map_location loc(x, y);
			int xpos = get_location_x(loc);
			int ypos = get_location_y(loc);

			const bool on_map = get_map().on_board(loc);
			SDL_Rect hex_rect = create_rect(xpos, ypos, zoom_, zoom_);
			if(!rects_overlap(hex_rect,clip_rect)) {
				continue;
			}
			draw_hex(loc);
			drawn_hexes_+=1;
			// If the tile is at the border, we start to blend it
			if(!on_map) {
				draw_border(loc, xpos, ypos);
			}
			invalidated_hexes_ ++;
		}
	}
}

void display::draw_hex(const map_location& loc) 
{
	int xpos = get_location_x(loc);
	int ypos = get_location_y(loc);
	image::TYPE image_type = get_image_type(loc);
	const bool on_map = get_map().on_board(loc);
	const bool off_map_tile = (get_map().get_terrain(loc) == t_translation::OFF_MAP_USER);
	const time_of_day& tod = get_time_of_day(loc);

	if (!shrouded(loc)) {
		// unshrouded terrain (the normal case)
		drawing_buffer_add(LAYER_TERRAIN_BG, loc, xpos, ypos,
			get_terrain_images(loc,tod.id, image_type, BACKGROUND));

		drawing_buffer_add(LAYER_TERRAIN_FG, loc, xpos, ypos,
			get_terrain_images(loc,tod.id,image_type, FOREGROUND));

		// Draw the grid, if that's been enabled
		if (grid_ && on_map && !off_map_tile) {
			drawing_buffer_add(LAYER_GRID_TOP, loc, xpos, ypos, image::get_image(image::grid_top, image::TOD_COLORED));
			drawing_buffer_add(LAYER_GRID_BOTTOM, loc, xpos, ypos, image::get_image(image::grid_bottom, image::TOD_COLORED));
		}

	}

	// Draw the time-of-day mask on top of the terrain in the hex.
	// tod may differ from tod if hex is illuminated.
	const std::string& tod_hex_mask = tod.image_mask;
	if(tod_hex_mask1 != NULL || tod_hex_mask2 != NULL) {
		drawing_buffer_add(LAYER_TERRAIN_FG, loc, xpos, ypos, tod_hex_mask1);
		drawing_buffer_add(LAYER_TERRAIN_FG, loc, xpos, ypos, tod_hex_mask2);
	} else if(!tod_hex_mask.empty()) {
		drawing_buffer_add(LAYER_TERRAIN_FG, loc, xpos, ypos,
			image::get_image(tod_hex_mask,image::SCALED_TO_HEX));
	}

	// Paint mouseover overlays
	if (loc == mouseoverHex_ && (on_map || (in_theme() && get_map().on_board_with_border(loc))) && mouseover_hex_overlay_ != NULL) {
		// drawing_buffer_add(LAYER_MOUSEOVER_OVERLAY, loc, xpos, ypos, mouseover_hex_overlay_);
		drawing_buffer_add(LAYER_LINGER_OVERLAY, loc, xpos, ypos, mouseover_hex_overlay_);
	}

	// Paint arrows
	arrows_map_t::const_iterator arrows_in_hex = arrows_map_.find(loc);
	if(arrows_in_hex != arrows_map_.end()) {
		BOOST_FOREACH (arrow* const a, arrows_in_hex->second) {
			a->draw_hex(loc);
		}
	}

	// Apply shroud, fog and linger overlay

	if(shrouded(loc)) {
		// We apply void also on off-map tiles
		// to shroud the half-hexes too
		const std::string& shroud_image = get_variant(shroud_images_, loc);
		drawing_buffer_add(LAYER_FOG_SHROUD, loc, xpos, ypos,
			image::get_image(shroud_image, image_type));
	} else if(fogged(loc)) {
		const std::string& fog_image = get_variant(fog_images_, loc);
		drawing_buffer_add(LAYER_FOG_SHROUD, loc, xpos, ypos,
			image::get_image(fog_image, image_type));
	}

	if (!shrouded(loc)) {
		drawing_buffer_add(LAYER_FOG_SHROUD, loc, xpos, ypos, get_fog_shroud_images(loc, image_type));
	}

	if (on_map && loc == mouseoverHex_) {
		drawing_buffer_add(LAYER_UNIT_MISSILE_DEFAULT,
				loc, xpos, ypos, image::get_image(game_config::terrain::mouseover, image::SCALED_TO_HEX));
	}

	if (on_map) {
		if (draw_coordinates_) {
			int off_x = xpos + hex_size()/2;
			int off_y = ypos + hex_size()/2;
			surface text = font::get_rendered_text2(lexical_cast<std::string>(map_location(loc.x - 1, loc.y - 1)), -1, font::SIZE_SMALL, font::NORMAL_COLOR);

			surface bg = create_neutral_surface(text->w, text->h);
			SDL_Rect bg_rect = create_rect(0, 0, text->w, text->h);
			sdl_fill_rect(bg, &bg_rect, 0xaa000000);
			off_x -= text->w / 2;
			if (draw_terrain_codes_) {
				off_y -= text->h;
			} else {
				off_y -= text->h / 2;
			}
			drawing_buffer_add(LAYER_FOG_SHROUD, loc, off_x, off_y, bg);
			drawing_buffer_add(LAYER_FOG_SHROUD, loc, off_x, off_y, text);
		}
		if (draw_terrain_codes_ && !shrouded(loc)) {
			int off_x = xpos + hex_size()/2;
			int off_y = ypos + hex_size()/2;
			surface text = font::get_rendered_text2(lexical_cast<std::string>(get_map().get_terrain(loc)), -1, font::SIZE_SMALL, font::NORMAL_COLOR);
			surface bg = create_neutral_surface(text->w, text->h);
			SDL_Rect bg_rect = create_rect(0, 0, text->w, text->h);
			sdl_fill_rect(bg, &bg_rect, 0xaa000000);
			off_x -= text->w / 2;
			if (!draw_coordinates_) {
				off_y -= text->h / 2;
			}
			drawing_buffer_add(LAYER_FOG_SHROUD, loc, off_x, off_y, bg);
			drawing_buffer_add(LAYER_FOG_SHROUD, loc, off_x, off_y, text);
		}
	}
}

image::TYPE display::get_image_type(const map_location& /*loc*/) {
	return image::TOD_COLORED;
}

void display::draw_sidebar() {

}


void display::draw_image_for_report(surface& img, SDL_Rect& rect)
{
	// SDL_Rect visible_area = get_non_transparent_portion(img);
	SDL_Rect visible_area = {0, 0, img->w, img->h};
	SDL_Rect target = rect;
	if (visible_area.x != 0 || visible_area.y != 0 || visible_area.w != img->w || visible_area.h != img->h) {
		if(visible_area.w == 0 || visible_area.h == 0) {
			return;
		}

		if(visible_area.w > rect.w || visible_area.h > rect.h) {
			img.assign(get_surface_portion(img,visible_area,false));
			img.assign(scale_surface(img,rect.w,rect.h));
			visible_area.x = 0;
			visible_area.y = 0;
			visible_area.w = img->w;
			visible_area.h = img->h;
		} else {
			target.x = rect.x + (rect.w - visible_area.w)/2;
			target.y = rect.y + (rect.h - visible_area.h)/2;
			target.w = visible_area.w;
			target.h = visible_area.h;
		}

		sdl_blit(img,&visible_area,screen_.getSurface(),&target);
	} else {
		if(img->w != rect.w || img->h != rect.h) {
			img.assign(scale_surface(img,rect.w,rect.h));
		}

		sdl_blit(img,NULL,screen_.getSurface(),&target);
	}
}

void display::refresh_report(int num, const reports::report& r)
{
	gui2::twidget* widget = get_theme_report(num);
	if (!widget) {
		return;
	}

	const SDL_Rect rect = widget->get_rect();

	// Report and its location is unchanged since last time. Do nothing.
	const reports::report& orignal = reports_[num];
	if (orignal.rect == rect && orignal == r) {
		return;
	}

	tooltips::clear_tooltips(orignal.rect);
	reports_[num] = r;
	reports_[num].rect = rect;

	if (!r.valid()) {
		return;
	}

	if (r.type == reports::report::LABEL) {
		set_theme_report_label(num, r.text);

	} else {
		surface surf = refresh_surface_report(num, reports_[num], *widget);
		set_theme_report_surface(num, surf);
	}

	if (!r.tooltip.empty()) {
		tooltips::add_tooltip(rect, r.tooltip);
	}
}

void display::invalidate_all()
{
	invalidateAll_ = true;
	if (draw_area_) {
		memset(draw_area_, INVALIDATE, draw_area_size_);
	}
}

bool display::invalidate(const map_location& loc)
{
	if (invalidateAll_) {
		return false;
	}

	int pos = draw_area_index(loc.x, loc.y);
	if (pos < 0 || pos >= draw_area_size_) {
		return false;
	}
	if (draw_area_[pos] != INVALIDATE) {
		draw_area_[pos] = INVALIDATE;
		return true;
	}
	return false;
}

bool display::invalidate(const std::set<map_location>& locs)
{
	if (invalidateAll_)
		return false;

	bool ret = false;
	BOOST_FOREACH (const map_location& loc, locs) {
		int pos = draw_area_index(loc.x, loc.y);
		if (pos < 0 || pos >= draw_area_size_) {
			continue;
		}
		if (draw_area_[pos] != INVALIDATE) {
			draw_area_[pos] = INVALIDATE;
			ret = true;
		}
	}
	return ret;
}

bool display::propagate_invalidation(const std::set<map_location>& locs)
{
	if (invalidateAll_)
		return false;

	if (locs.size()<=1)
		return false; // propagation never needed

	// search the first hex invalidated (if any)
	std::set<map_location>::const_iterator i = locs.begin();
	for(; i != locs.end() && draw_area_val(i->x, i->y) != INVALIDATE; ++i) {}

	if (i == locs.end())
		return false; // no invalidation, don't propagate

	// propagate invalidation
	// 'i' is already in, but I suspect that splitting the range is bad
	// especially because locs are often adjacents
	return invalidate(locs);
}

bool display::invalidate_visible_locations_in_rect(const SDL_Rect& rect)
{
	return invalidate_locations_in_rect(intersect_rects(map_area(),rect));
}

bool display::invalidate_locations_in_rect(const SDL_Rect& rect)
{
	if (invalidateAll_)
		return false;

	bool result = false;
	BOOST_FOREACH (const map_location &loc, hexes_under_rect(rect)) {
		result |= invalidate(loc);
	}
	return result;
}

void display::invalidate_animations()
{
	new_animation_frame();
	animate_map_ = true;
	if (!animate_map_) return;

	BOOST_FOREACH (const map_location &loc, draw_area_rect_) {
		if (shrouded(loc)) continue;

		if (builder_->update_animation(loc)) {
			invalidate(loc);
		} else {
			invalidate_animations_location(loc);
		}
	}
}

void display::invalidate_theme()
{
	const map_location zero_loc(0,0);
	int zero_x = get_location_x(zero_loc);
	int zero_y = get_location_y(zero_loc);

	const std::vector<gui2::twidget*>& volatiles = theme_->volatiles();
	std::vector<SDL_Rect> rects;
	for (std::vector<gui2::twidget*>::const_iterator it = volatiles.begin(); it != volatiles.end(); ++ it) {
		gui2::twidget* widget = *it;
		gui2::twidget::tvisible visible = widget->get_visible();
		if (visible == gui2::twidget::VISIBLE || widget->get_dirty()) {
			rects.push_back(widget->get_rect());
			if (visible != gui2::twidget::VISIBLE) {
				// when visible --> invisible
				widget->set_dirty(false);
			}
		}
	}

	rect_of_hexes underlying_hex;
	for (std::vector<SDL_Rect>::const_iterator it = rects.begin(); it != rects.end(); ++ it) {
		SDL_Rect r = *it;

		if (zero_.x != -1 && zero_.y != -1) {
			r.x += zero_x - zero_.x;
			r.y += zero_y - zero_.y;
		}

		underlying_hex = hexes_under_rect(r);
		for (rect_of_hexes::iterator it2 = underlying_hex.begin(); it2 != underlying_hex.end(); ++ it2) {
			invalidate(*it2);
		}
	}

	zero_.x = zero_x;
	zero_.y = zero_y;
}

void display::float_label(const map_location& loc, const std::string& text,
						  int red, int green, int blue, bool slow)
{
	font::floating_label flabel(text);
	flabel.set_font_size(font::SIZE_XLARGE);
	const SDL_Color color = create_color(red, green, blue);
	flabel.set_color(color);
	flabel.set_position(get_location_x(loc)+zoom_/2, get_location_y(loc));
	flabel.set_move(0, -2 * turbo_speed());
	int lifetime = 60 / turbo_speed();
	flabel.set_lifetime(slow? (lifetime * 3): lifetime);
	flabel.set_scroll_mode(font::ANCHOR_LABEL_MAP);

	font::add_floating_label(flabel);
}

animation& display::area_anim(int id)
{
	return *area_anims_.find(id)->second;
}

int display::insert_area_anim(const animation& tpl)
{
	animation* clone = new animation(tpl);
	return insert_area_anim2(clone);
}

int display::insert_area_anim2(animation* anim)
{
	int id = -1;
	for (std::map<int, animation*>::reverse_iterator it = area_anims_.rbegin(); it != area_anims_.rend(); ++ it) {
		id = it->first;
		break;
	}
	id ++;
	area_anims_.insert(std::make_pair(id, anim));
	return id;
}

void display::erase_area_anim(int id)
{
	std::map<int, animation*>::iterator find = area_anims_.find(id);
	VALIDATE(find != area_anims_.end(), "display::erase_area_anim, cannot find 'id' screen anim!");

	if (find->second->type() == anim_map) {
		find->second->invalidate(screen_.getSurface());
		delete find->second;
		area_anims_.erase(find);

		draw();

	} else {
		// find->second->undraw(screen_.getSurface());
		delete find->second;
		area_anims_.erase(find);
	}
}

void display::clear_area_anims()
{
	for (std::map<int, animation*>::const_iterator it = area_anims_.begin(); it != area_anims_.end(); ++ it) {
		delete it->second;
	}
	area_anims_.clear();
}

void display::draw_float_anim()
{
	if (area_anims_.empty() || in_theme()) {
		return;
	}

	// invalidate_animations();
	new_animation_frame();
	
	for (std::map<int, animation*>::iterator it = area_anims_.begin(); it != area_anims_.end(); ++ it) {
		animation& anim = *it->second;
		if (anim.type() != anim_float || !anim.started()) {
			continue;
		}

		anim.update_last_draw_time();
		anim.redraw(screen_.getSurface(), screen_area());
	}

	drawing_buffer_commit(screen_.getSurface());
}

void display::undraw_float_anim()
{
	if (area_anims_.empty() || in_theme()) {
		return;
	}

	for (std::map<int, animation*>::reverse_iterator it = area_anims_.rbegin(); it != area_anims_.rend(); ++ it) {
		animation& anim = *it->second;
		if (anim.type() != anim_float || !anim.started()) {
			continue;
		}
		anim.undraw(screen_.getSurface());
	}

	for (std::map<int, animation*>::iterator it = area_anims_.begin(); it != area_anims_.end(); ) {
		animation& anim = *it->second;
		if (anim.type() != anim_float || !anim.started()) {
			++ it;
			continue;
		}
		if (anim.started() && !anim.cycles() && anim.animation_finished_potential()) {
			anim.require_release();
			delete it->second;
			area_anims_.erase(it ++);
		} else {
			++ it;
		}
	}
}

void display::add_arrow(arrow& arrow)
{
	const arrow_path_t & arrow_path = arrow.get_path();
	BOOST_FOREACH (const map_location& loc, arrow_path)
	{
		arrows_map_[loc].push_back(&arrow);
	}
}

void display::remove_arrow(arrow& arrow)
{
	const arrow_path_t & arrow_path = arrow.get_path();
	BOOST_FOREACH (const map_location& loc, arrow_path)
	{
		arrows_map_[loc].remove(&arrow);
	}
}

void display::update_arrow(arrow & arrow)
{
	const arrow_path_t & previous_path = arrow.get_previous_path();
	BOOST_FOREACH (const map_location& loc, previous_path)
	{
		arrows_map_[loc].remove(&arrow);
	}
	const arrow_path_t & arrow_path = arrow.get_path();
	BOOST_FOREACH (const map_location& loc, arrow_path)
	{
		arrows_map_[loc].push_back(&arrow);
	}
}

std::map<surface,SDL_Rect> energy_bar_rects;

struct is_energy_color {
	bool operator()(Uint32 color) const { return (color&0xFF000000) > 0x10000000 &&
	                                              (color&0x00FF0000) < 0x00100000 &&
												  (color&0x0000FF00) < 0x00001000 &&
												  (color&0x000000FF) < 0x00000010; }
};

/**
 * Finds the start and end rows on the energy bar image.
 *
 * White pixels are substituted for the color of the energy.
 */

const SDL_Rect& calculate_energy_bar(surface surf)
{
	const std::map<surface,SDL_Rect>::const_iterator i = energy_bar_rects.find(surf);
	if(i != energy_bar_rects.end()) {
		return i->second;
	}

	int first_row = -1, last_row = -1, first_col = -1, last_col = -1;

	surface image(make_neutral_surface(surf));

	const_surface_lock image_lock(image);
	const Uint32* const begin = image_lock.pixels();

	for(int y = 0; y != image->h; ++y) {
		const Uint32* const i1 = begin + image->w*y;
		const Uint32* const i2 = i1 + image->w;
		const Uint32* const itor = std::find_if(i1,i2,is_energy_color());
		const int count = std::count_if(itor,i2,is_energy_color());

		if(itor != i2) {
			if(first_row == -1) {
				first_row = y;
			}

			first_col = itor - i1;
			last_col = first_col + count;
			last_row = y;
		}
	}

	const SDL_Rect res = create_rect(first_col
			, first_row
			, last_col-first_col
			, last_row+1-first_row);
	energy_bar_rects.insert(std::pair<surface,SDL_Rect>(surf,res));
	return calculate_energy_bar(surf);
}

// @size: valid size, don't include outline.
void draw_bar_to_surf(const std::string& image, surface& dst_surf, int x, int y, int size, double filled, const SDL_Color& col, fixed_t alpha, bool vtl)
{
	filled = std::min<double>(std::max<double>(filled, 0.0), 1.0);

	surface surf(image::get_image(image));

	// We use UNSCALED because scaling (and bilinear interpolaion)
	// is bad for calculate_energy_bar.
	// But we will do a geometric scaling later.
	surface bar_surf(image::get_image(image));
	if (surf == NULL || bar_surf == NULL) {
		return;
	}

	// calculate_energy_bar returns incorrect results if the surface colors
	// have changed (for example, due to bilinear interpolaion)
	const SDL_Rect& bar_loc = calculate_energy_bar(bar_surf);

	if (vtl) {
		if (size > bar_loc.h) {
			size = bar_loc.h;
		}
	} else {
		if (size > bar_loc.w) {
			size = bar_loc.w;
		}
	}

	size_t skip_rows;
	
	if (vtl) {
		skip_rows = bar_loc.h - size;
	} else  {
		skip_rows = bar_loc.w - size;
	}

	SDL_Rect top = {0, 0, surf->w, bar_loc.y};
	SDL_Rect bot = {0, int(bar_loc.y + skip_rows), surf->w, 0};
	
	SDL_Rect dst_clip = create_rect(x, y, 0, 0);
	if (vtl) {
		bot.h = surf->h - bot.y;

		blit_surface(surf, &top, dst_surf, &dst_clip);
		dst_clip.y += top.h;
		blit_surface(surf, &bot, dst_surf, &dst_clip);

		// drawing_buffer_add(LAYER_UNIT_BAR, loc, xpos, ypos, surf, top);
		// drawing_buffer_add(LAYER_UNIT_BAR, loc, xpos, ypos + top.h, surf, bot);
	} else {
		top.w = bar_loc.x;
		top.h = surf->h;
		bot.x = bar_loc.x + skip_rows;
		bot.y = 0;
		bot.w = surf->w - bot.x;
		bot.h = surf->h;

		// ???I don't know why below tow blit_surface not work?? must use sdl_blit.
		// reference http://www.freeors.com/bbs/forum.php?mod=viewthread&tid=21963
		// blit_surface(surf, &top, dst_surf, &dst_clip);
		sdl_blit(surf, &top, dst_surf, &dst_clip);
		dst_clip.x += top.w;
		// blit_surface(surf, &bot, dst_surf, &dst_clip);
		sdl_blit(surf, &bot, dst_surf, &dst_clip);

		// drawing_buffer_add(LAYER_UNIT_BAR, loc, xpos, ypos, surf, top);
		// drawing_buffer_add(LAYER_UNIT_BAR, loc, xpos + top.w, ypos, surf, bot);
	}

	const int unfilled = static_cast<const int>(size * (1.0 - filled));

	if (unfilled < size && alpha >= ftofxp(0.3)) {
		const Uint8 r_alpha = std::min<unsigned>(unsigned(fxpmult(alpha,255)),255);
		surface filled_surf;
		SDL_Rect filled_area = {0, 0, bar_loc.w, size - unfilled};
		if (vtl) {
			filled_surf = create_compatible_surface(bar_surf, bar_loc.w, size - unfilled);
		} else {
			filled_surf = create_compatible_surface(bar_surf, size - unfilled, bar_loc.h);
			filled_area.w = size - unfilled;
			filled_area.h = bar_loc.h;
		}

		SDL_FillRect(filled_surf, &filled_area, SDL_MapRGBA(bar_surf->format, col.r, col.g, col.b, r_alpha));
		if (vtl) {
			dst_clip.x = x + bar_loc.x;
			dst_clip.y = y + bar_loc.y + unfilled;
			// use sdl_blit insteal of blit_surface
			sdl_blit(filled_surf, NULL, dst_surf, &dst_clip);
		} else {
			dst_clip.x = x + bar_loc.x;
			dst_clip.y = y + bar_loc.y;
			// use sdl_blit insteal of blit_surface
			// 1. right color
			// 2. this source surface is safe. it will release once used.
			sdl_blit(filled_surf, NULL, dst_surf, &dst_clip);
		}
	}
}

display* display::singleton_ = NULL;