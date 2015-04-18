/* $Id: game_display.cpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
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
 * During a game, show map & info-panels at top+right.
 */

#include "global.hpp"

#include "game_display.hpp"

#include "wesconfig.h"

#ifdef HAVE_LIBDBUS
#include <dbus/dbus.h>
#endif

#ifdef HAVE_GROWL
#include <Growl/GrowlApplicationBridge-Carbon.h>
#include <Carbon/Carbon.h>
Growl_Delegate growl_obj;
#endif

#include "halo.hpp"
#include "log.hpp"
#include "map.hpp"
#include "map_label.hpp"
#include "marked-up_text.hpp"
#include "game_preferences.hpp"
#include "resources.hpp"
#include "tod_manager.hpp"
#include "sound.hpp"
#include "artifical.hpp"
#include "actions.hpp"
#include "wml_exception.hpp"
#include "gettext.hpp"
#include "builder.hpp"
#include "play_controller.hpp"
#include "formula_string_utils.hpp"
#include <minimap.hpp>
#include "gui/widgets/button.hpp"
#include "gui/dialogs/theme2.hpp"

#include <boost/foreach.hpp>
#include <boost/bind.hpp>

static lg::log_domain log_display("display");
#define ERR_DP LOG_STREAM(err, log_display)
#define LOG_DP LOG_STREAM(info, log_display)

static lg::log_domain log_engine("engine");
#define ERR_NG LOG_STREAM(err, log_engine)

static std::string miss_anim_err_str = "logic can only process map or canvas animation!";
std::map<map_location,fixed_t> game_display::debugHighlights_;

game_display::taccess_list::taccess_list(unit_map& units, int type)
	: units_(units)
	, type_(type)
	, bar(false, true, "surface")
	, reloaded(false)
	, hide(false)
{
}

void game_display::taccess_list::reset()
{
	reloaded = false;
	hide = false;
}

void game_display::taccess_list::calculate_require_count(int side)
{
	if (type_ != TROOP) {
		return;
	}

	const gui2::tgrid::tchild* children = bar.report()->content_grid()->children();
	int childs = gui2::ttabbar::front_childs + bar.childs();

	const int max_disp_their = preferences::developer()? 200: (tent::turn_based? 0: 6);
	const bool disp_other_city = preferences::developer(); // include their and ourside
	int disp_their = 0;
	int disp_city = 0;

	for (int i = gui2::ttabbar::front_childs; i < childs; i ++) {
		gui2::tcontrol* widget = dynamic_cast<gui2::tcontrol*>(children[i].widget_);
		unit* u = reinterpret_cast<unit*>(widget->cookie());
		int drawn_ticks = u->drawn_ticks();
		if (!tent::turn_based) {
			if (u->side() != side + 1) {
				if (disp_their < max_disp_their && (disp_other_city || !u->is_city())) {
					disp_their ++;
					if (drawn_ticks == HIDDEN_TICKS) {
						drawn_ticks = NONE_TICKS;
					}
				} else {
					drawn_ticks = HIDDEN_TICKS;
				}
			} else if (u->is_city()) {
				if (disp_other_city || u->is_capital(*resources::teams)) {
					if (drawn_ticks == HIDDEN_TICKS) {
						drawn_ticks = NONE_TICKS;
					}
				} else {
					drawn_ticks = HIDDEN_TICKS;
				}
			} else {
				if (drawn_ticks == HIDDEN_TICKS) {
					drawn_ticks = NONE_TICKS;
				}
			}
			
		} else {
			if (u->side() != side + 1) {
				if (disp_their < max_disp_their) {
					disp_their ++;
					if (drawn_ticks == HIDDEN_TICKS) {
						drawn_ticks = NONE_TICKS;
					}
				} else {
					drawn_ticks = HIDDEN_TICKS;
				}
			} else {
				if (u->is_city() && !disp_other_city && !u->is_capital(*resources::teams)) {
					drawn_ticks = HIDDEN_TICKS;

				} else if (drawn_ticks == HIDDEN_TICKS) {
					drawn_ticks = NONE_TICKS;
				}
			}
		}

		u->set_drawn_ticks(drawn_ticks);

		bar.set_visible(i - gui2::ttabbar::front_childs, drawn_ticks != HIDDEN_TICKS);
	}
}

void set_unit_image(void* cookie, surface& image_, int& integer)
{
	const int bar_vtl_ticks_width = 6;
	SDL_Rect dst_clip = create_rect(0, 0, 0, 0);
	int width = image_->w;
	int height = image_->h;
	const unit* u = reinterpret_cast<const unit*>(cookie);

	if (u->get_state(ustate_tag::REVIVALED)) {
		image_ = adjust_surface_color(image_, 255, 0, 0);
	}

	int degree = u->percent_ticks();
	SDL_Color color = font::GOOD_COLOR;
	if (degree < 50) {
		color = font::BAD_COLOR;
	} else if (degree < 100) {
		color = font::YELLOW_COLOR;
	}

	std::string img_name = "misc/bar-vtl-ticks.png";
	if (current_can_action(*u)) {
		img_name = "misc/bar-vtl-ticks-hot.png";
	}
	if (!tent::turn_based || preferences::developer()) {
		draw_bar_to_surf(img_name, image_, 0, 12, height - 4 - (12 + 1), 1.0 * degree / 100, color, ftofxp(0.8), true);
	}

	if (u->is_city()) {
		// city name
		int font_size = 12;
		surface text_surf = font::get_rendered_text2(u->name(), -1, font_size, font::BIGMAP_COLOR);
		surface back_surf = font::get_rendered_text2(u->name(), -1, font_size, font::BLACK_COLOR);

		dst_clip.x = bar_vtl_ticks_width;
		dst_clip.y = height - 16;
		SDL_Rect dst;
		for (int dy=-1; dy <= 1; ++dy) {
			for (int dx=-1; dx <= 1; ++dx) {
				if (dx!=0 || dy!=0) {
					dst.x = dst_clip.x + dx;
					dst.y = dst_clip.y + dy;
					sdl_blit(back_surf, NULL, image_, &dst);
				}
			}
		}
		sdl_blit(text_surf, NULL, image_, &dst_clip);
	}

	// top-middle
	dst_clip.x = 8;
	dst_clip.y = 0;
	if (u->get_state(ustate_tag::DEPUTE)) {
		sdl_blit(image::get_image("misc/depute.png"), NULL, image_, &dst_clip);
	}
	if (u->has_mayor()) {
		sdl_blit(image::get_image("misc/special-unit.png"), NULL, image_, &dst_clip);
	}
	integer = preferences::developer()? u->ticks(): u->level();
}

surface generate_surface(int width, int height, void* cookie, const std::string& stem, int integer, bool greyscale, bool special, const std::string& icon, const std::string& lb_icon)
{
	const std::string button_image_file = stem;
	surface stem_image;

	stem_image = image::get_image(stem);
	if (greyscale) {
		stem_image = greyscale_image(stem_image);
	}
	stem_image = scale_surface(stem_image, width, height);

	surface image_ = stem_image;

	const int bar_vtl_ticks_width = 6;
	if (cookie) {
		set_unit_image(cookie, image_, integer);
	}

	blit_integer_surface(integer, image_, 0, 0);

	std::stringstream text;
	SDL_Rect dst_clip = create_rect(0, 0, 0, 0);

	if (!icon.empty()) {
		text.str("");
		text << icon << "~SCALE(16, 16)";
		dst_clip.x = 0;
		dst_clip.y = 12;
		sdl_blit(image::get_image(text.str()), NULL, image_, &dst_clip);
	}

	if (!lb_icon.empty()) {
		text.str("");
		text << lb_icon << "~SCALE(16, 16)";
		dst_clip.x = bar_vtl_ticks_width;
		dst_clip.y = image_->h - 16;
		sdl_blit(image::get_image(text.str()), NULL, image_, &dst_clip);
	}
	return image_;
}

void game_display::taccess_list::reload(game_display& gui, int side)
{
	size_t i;
	gui2::tcontrol* widget;
	gui2::tpoint unit_size = bar.report()->get_unit_size();

	i = 1;
	const std::vector<artifical*>& holden_cities = gui.teams_[side].holden_cities();
	if (type_ == TROOP) {
		for (unit_map::const_iterator it = units_.begin(); it != units_.end(); ++ it) {
			unit* u = dynamic_cast<unit*>(&*it);
			if (!u->consider_ticks()) {
				continue;
			}
			u->set_drawn_ticks(NONE_TICKS);

			widget = bar.create_child(null_str, null_str, u, null_str);
			bar.insert_child(*widget);

			i ++;
		}
	} else if (type_ == HERO && !holden_cities.empty()) {
		for (std::vector<artifical*>::const_iterator c_it = holden_cities.begin(); c_it != holden_cities.end(); ++ c_it) {
			artifical& city = **c_it;
			const std::vector<hero*>& fresh = city.fresh_heros();
			for (std::vector<hero*>::const_iterator it = fresh.begin(); it != fresh.end(); ++ it) {
				hero* h = *it;

				widget = bar.create_child(null_str, null_str, h, null_str);
				bar.insert_child(*widget);

				surface surf;
				if (h->utype_ != HEROS_NO_UTYPE) {
					const unit_type* ut = unit_types.keytype(h->utype_);
					if (h->noble_ == HEROS_NO_NOBLE || !unit_types.noble(h->noble_).formation()) {
						surf = generate_surface(unit_size.x, unit_size.y, NULL, h->image(), ut->cost(), false, false, ut->icon(), null_str);
					} else {
						surf = generate_surface(unit_size.x, unit_size.y, NULL, h->image(), ut->cost(), false, false, ut->icon(), "misc/formation.png");
					}
				} else {
					surf = generate_surface(unit_size.x, unit_size.y, NULL, h->image(), -1, false, false, null_str, null_str);
				}
				widget->set_surface(surf, unit_size.x, unit_size.y);

				i ++;
			}
			const std::vector<hero*>& finish = city.finish_heros();
			for (std::vector<hero*>::const_iterator it = finish.begin(); it != finish.end(); ++ it) {
				hero* h = *it;

				widget = bar.create_child(null_str, null_str, h, null_str);
				bar.insert_child(*widget);

				surface surf = generate_surface(unit_size.x, unit_size.y, NULL, h->image(), -1, true, false, null_str, null_str);
				widget->set_surface(surf, unit_size.x, unit_size.y);

				i ++;
			}
		}
	}

	i ++;

	calculate_require_count(side);
	bar.replacement_children();
	reloaded = true;
}

void game_display::taccess_list::insert(game_display& gui, int side, void* cookie)
{
	int at;
	gui2::tpoint unit_size = bar.report()->get_unit_size();

	const unit* u = reinterpret_cast<unit*>(cookie);
	hero* h = reinterpret_cast<hero*>(cookie);

	if (type_ == TROOP) {
		at = 0;
		for (unit_map::const_iterator it = units_.begin(); it != units_.end(); ++ it) {
			unit* u1 = dynamic_cast<unit*>(&*it);
			if (!u1->consider_ticks()) {
				continue;
			}
			if (u1 == u) {
				break;
			}
			at ++;
		}
	} else if (type_ == HERO) {
		at = -1;
	}

	gui2::tcontrol* widget = bar.create_child(null_str, null_str, cookie, null_str);
	bar.insert_child(*widget, at);


	if (type_ == TROOP) {

	} else if (type_ == HERO) {
		surface surf;
		if (h->utype_ != HEROS_NO_UTYPE) {
			const unit_type* ut = unit_types.keytype(h->utype_);
			if (h->noble_ == HEROS_NO_NOBLE || !unit_types.noble(h->noble_).formation()) {
				surf = generate_surface(unit_size.x, unit_size.y, NULL, h->image(), ut->cost(), false, false, ut->icon(), null_str);
			} else {
				surf = generate_surface(unit_size.x, unit_size.y, NULL, h->image(), ut->cost(), false, false, ut->icon(), "misc/formation.png");
			}
		} else {
			surf = generate_surface(unit_size.x, unit_size.y, NULL, h->image(), -1, false, false, null_str, null_str);
		}
		widget->set_surface(surf, unit_size.x, unit_size.y);
	}

	calculate_require_count(side);
	bar.replacement_children();
}

void game_display::taccess_list::erase(game_display& gui, int side, void* cookie)
{
	int i, dirty_btn_idx;
	const gui2::tgrid::tchild* children = bar.report()->content_grid()->children();
	int childs = gui2::ttabbar::front_childs + bar.childs();

	// 0 is reserved by previous button
	for (dirty_btn_idx = 0, i = gui2::ttabbar::front_childs; i < childs; i ++) {
		if (children[i].widget_->cookie() == cookie) {
			dirty_btn_idx = i;
			bar.erase_child(i - gui2::ttabbar::front_childs);
			
			if (type_ == TROOP) {
				// when ai troop belong to player, form ERASE to INSERT.
				// in order to keep INSERT right, should set NONE_TICKS when ERASE.
				unit* u = reinterpret_cast<unit*>(cookie);
				u->set_drawn_ticks(NONE_TICKS);
			}
			break;
		}
	}
	if (!dirty_btn_idx) {
		// cannot find.
		return;
	}

	calculate_require_count(side);
	bar.replacement_children();
}

void game_display::taccess_list::enable(game_display& gui, int side, void* cookie)
{
	gui2::tbutton* widget = NULL;

	gui2::tpoint unit_size = bar.report()->get_unit_size();
	const gui2::tgrid::tchild* children = bar.report()->content_grid()->children();
	int childs = gui2::ttabbar::front_childs + bar.childs();

	hero* h = reinterpret_cast<hero*>(cookie);
	for (int i = gui2::ttabbar::front_childs; i < childs && !widget; i ++) {
		if (children[i].widget_->cookie() == cookie) {
			widget = dynamic_cast<gui2::tbutton*>(children[i].widget_);
		}
	}
	if (!widget) {
		return;
	}

	if (type_ == TROOP) {

	} else if (type_ == HERO) {
		surface surf;
		if (h->utype_ != HEROS_NO_UTYPE) {
			const unit_type* ut = unit_types.keytype(h->utype_);
			if (h->noble_ == HEROS_NO_NOBLE || !unit_types.noble(h->noble_).formation()) {
				surf = generate_surface(unit_size.x, unit_size.y, NULL, h->image(), ut->cost(), false, false, ut->icon(), null_str);
			} else {
				surf = generate_surface(unit_size.x, unit_size.y, NULL, h->image(), ut->cost(), false, false, ut->icon(), "misc/formation.png");
			}
		} else {
			surf = generate_surface(unit_size.x, unit_size.y, NULL, h->image(), -1, false, false, null_str, null_str);
		}
		widget->set_surface(surf, unit_size.x, unit_size.y);
	}
}

void game_display::taccess_list::redraw(game_display& gui, int side, void* cookie)
{
	gui2::tbutton* widget = NULL;

	gui2::tpoint unit_size = bar.report()->get_unit_size();
	const gui2::tgrid::tchild* children = bar.report()->content_grid()->children();
	int childs = gui2::ttabbar::front_childs + bar.childs();


	const unit* u = reinterpret_cast<unit*>(cookie);
	hero* h = reinterpret_cast<hero*>(cookie);

	for (int i = gui2::ttabbar::front_childs; i < childs && !widget; i ++) {
		if (children[i].widget_->cookie() == cookie) {
			widget = dynamic_cast<gui2::tbutton*>(children[i].widget_);
		}
	}
	if (!widget) {
		return;
	}
	
	if (type_ == TROOP) {
		surface surf = u->generate_access_surface(unit_size.x, unit_size.y, false);
		widget->set_surface(surf, unit_size.x, unit_size.y);

	} else if (type_ == HERO) {
		
	}
}

game_display::game_display(unit_map& units, hero_map& heros, play_controller* controller, CVideo& video, const gamemap& map,
		const tod_manager& tod, const std::vector<team>& t,
		const config& theme_cfg, const config& level) :
		hex_display(controller, video, &map, theme_cfg, level, gui2::tgame_theme::NUM_REPORTS),
		units_(units),
		heros_(heros),
		controller_(controller),
		temp_units_(),
		viewpoint_(NULL),
		road_locs_(),
		exclusive_unit_draw_requests_(),
		attack_indicator_dst_(),
		selectable_indicator_(),
		tactic_indicator_(std::make_pair(map_location(), -1)),
		marker_indicator_dst_(),
		formation_indicator_(),
		interior_indicator_(),
		alternatable_indicator_(),
		clear_formationed_indicator_(std::make_pair(map_location(), -1)),
		intervene_move_indicator_(std::make_pair(map_location(), -1)),
		route_(),
		tod_manager_(tod),
		teams_(t),
		level_(level),
		invalidateUnit_(true),
		overlays_(),
		currentTeam_(0),
		activeTeam_(0),
		sidebarScaling_(1.0),
		first_turn_(true),
		in_game_(false),
		observers_(),
		chat_messages_(),
		reach_map_(),
		reach_map_old_(),
		reach_map_changed_(true),
		game_mode_(RUNNING),
		flags_(),
		big_flags_(),
		big_flags_cache_(),
		temp_unit_(NULL),
		expedite_city_(NULL),
		pass_scenario_anim_id_(-1),
		terrain_dirty_(true),
		disctrict_(),
		disctrict_old_(),
		disctrict_changed_(true),
		troop_list_(units, taccess_list::TROOP),
		hero_list_(units, taccess_list::HERO),
		current_list_type_(taccess_list::TROOP),
		access_list_side_(HEROS_INVALID_SIDE),
		ctrl_bar_(NULL),
		moving_src_loc_(),
		moving_dst_loc_()
{
	// singleton_ = this;
	
	if (controller) {
		SDL_Rect screen = screen_area();
		std::string patch = get_theme_patch();
		theme_current_cfg_ = theme::set_resolution(theme_cfg, screen, patch, theme_cfg_);
		create_theme();
		gui2::tgame_theme& dlg = *dynamic_cast<gui2::tgame_theme*>(theme_);
		troop_list_.bar.set_report(dynamic_cast<gui2::treport*>(dlg.get_object("access-unit")));
		troop_list_.bar.set_callback_show(boost::bind(&game_display::show_access_troop, this, _1, _2));
		hero_list_.bar.set_report(dynamic_cast<gui2::treport*>(dlg.get_object("access-hero")));
		ctrl_bar_ = dlg.context_menu("ctrl-bar");
		if (hero_list_.bar.report()) {
			hero_list_.bar.report()->parent()->set_visible(gui2::twidget::INVISIBLE);
		}
	}

	// Inits the flag list and the team colors used by ~TC
	std::vector<std::string>& side_colors = image::get_team_colors();
	side_colors.clear();

	for (size_t i = 0; i != teams_.size(); ++i) {
		add_flag(i, side_colors);
	}

	// draw nodes for display::draw
	if (map_->w() && map_->h()) {
		pathfind::reallocate_pq(map_->w(), map_->h());
	}
}

std::string game_display::get_theme_patch() const
{
	std::string id;
	if (controller_->is_replaying()) {
		id = "replay";
	} else if (tent::tower_mode()) {
		id = "tower";
	}
	return id;
}

gui2::ttheme* game_display::create_theme_dlg(const config& cfg)
{
	return new gui2::tgame_theme(*this, *controller_, cfg);
}

game_display::~game_display()
{
	// SDL_FreeSurface(minimap_);
	prune_chat_messages(true);
	// singleton_ = NULL;
}

void game_display::rebuild_all()
{
	display::rebuild_all();
}

void game_display::construct_road_locs(const std::map<std::pair<int, int>, std::vector<map_location> >& roads)
{
	for (std::map<std::pair<int, int>, std::vector<map_location> >::const_iterator it = roads.begin(); it != roads.end(); ++ it) {
		artifical* a = units_.city_from_cityno(it->first.first);
		artifical* b = units_.city_from_cityno(it->first.second);
		for (std::vector<map_location>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++ it2) {
			const map_location& loc = *it2;
			artifical* city = units_.city_from_loc(loc);
			if (!city || city->get_location() != loc) {
				if (city && (city != a && city != b)) {
					std::stringstream err;
					utils::string_map symbols;
					symbols["from"] = a->name();
					symbols["to"] = b->name();
					symbols["other"] = city->name();
					err << vgettext("map error, road that from $from to $to exists other city: $other!", symbols);
					VALIDATE(false, err.str());
				}
				std::map<map_location, std::vector<std::pair<artifical*, artifical*> > >::iterator find = road_locs_.find(loc);
				if (find == road_locs_.end()) {
					std::vector<std::pair<artifical*, artifical*> > v;
					v.push_back(std::make_pair(a, b));
					road_locs_.insert(std::make_pair(loc, v));
				} else {
					find->second.push_back(std::make_pair(a, b));
				}
			}
		}
	}
}

int game_display::road_owner(std::map<map_location, std::vector<std::pair<artifical*, artifical*> > >::const_iterator& loc) const
{
	const team& t = teams_[activeTeam_];
	int ret = OWNER_NONE;

	const std::vector<std::pair<artifical*, artifical*> >& owner = loc->second;
	for (std::vector<std::pair<artifical*, artifical*> >::const_iterator it = owner.begin(); it != owner.end(); ++ it) {
		int a_side = it->first->side();
		int b_side = it->second->side();
		if (a_side != b_side) continue;
		if (a_side == activeTeam_ + 1) {
			return OWNER_SELF;
		} else if (t.is_enemy(a_side)) {
			return OWNER_ENEMY;
		}
	}
	return ret;
}

bool game_display::overlay_road_image(const map_location& loc, std::string& color_mod) const
{
	bool roaded = false;
	std::map<map_location, std::vector<std::pair<artifical*, artifical*> > >::const_iterator find = road_locs_.find(loc);
	if (find != road_locs_.end()) {
		roaded = true;
		int owner = road_owner(find);
		if (owner == OWNER_SELF) {
			color_mod = "~L(misc/road-self.png)";
		} else if (owner == OWNER_ENEMY) {
			color_mod = "~L(misc/road-enemy.png)";
		} else {
			color_mod = "~L(misc/road.png)";
		} 
	}
	return roaded;
}

void game_display::add_flag(int side_index, std::vector<std::string>& side_colors)
{
	size_t i = side_index;

	std::string side_color = team::get_side_color_index(i + 1);
	side_colors.push_back(side_color);
	std::string flag = teams_[i].flag();
	std::string old_rgb = game_config::flag_rgb;
	std::string new_rgb = side_color;

	if (flag.empty()) {
		flag = game_config::images::flag;
	}

	// Must recolor flag image
	animated<image::locator> temp_anim;

	std::vector<std::string> items = utils::split(flag);
	std::vector<std::string>::const_iterator itor = items.begin();
	for(; itor != items.end(); ++itor) {
		const std::vector<std::string>& items = utils::split(*itor, ':');
		std::string str;
		int time;

		if(items.size() > 1) {
			str = items.front();
			time = atoi(items.back().c_str());
		} else {
			str = *itor;
			time = 100;
		}
		std::stringstream temp;
		temp << str << "~RC(" << old_rgb << ">"<< new_rgb << ")";
		image::locator flag_image(temp.str());
		temp_anim.add_frame(time, flag_image);
	}
	flags_.push_back(temp_anim);

	flags_.back().start_animation(rand()%flags_.back().get_end_time(), true);

	// Must recolor big flag image
	animated<image::locator> temp_anim2;

	flag = game_config::images::big_flag;
	items = utils::split(flag);
	itor = items.begin();
	for(; itor != items.end(); ++itor) {
		const std::vector<std::string>& items = utils::split(*itor, ':');
		std::string str;
		int time;

		if(items.size() > 1) {
			str = items.front();
			time = atoi(items.back().c_str());
		} else {
			str = *itor;
			time = 100;
		}
		std::stringstream temp;
		temp << str << "~RC(" << old_rgb << ">"<< new_rgb << ")";
		image::locator flag_image(temp.str());
		if (image::exists(flag_image)) {
			temp_anim2.add_frame(time, flag_image);
		}
	}
	VALIDATE(temp_anim2.get_frames_count(), _("Invalid [game_config][images].big_flag value."));
	big_flags_.push_back(temp_anim2);

	big_flags_.back().start_animation(rand() % big_flags_.back().get_end_time(), true);

	big_flags_cache_.push_back(std::map<std::string, surface>());
}

void game_display::new_turn()
{
	const time_of_day& tod = tod_manager_.get_time_of_day();

	if( !first_turn_) {
		const time_of_day& old_tod = tod_manager_.get_previous_time_of_day();

		if(old_tod.image_mask != tod.image_mask) {
			const surface old_mask(image::get_image(old_tod.image_mask,image::SCALED_TO_HEX));
			const surface new_mask(image::get_image(tod.image_mask,image::SCALED_TO_HEX));

			const int niterations = static_cast<int>(10/turbo_speed());
			const int frame_time = 30;
			const int starting_ticks = SDL_GetTicks();
			for(int i = 0; i != niterations; ++i) {

				if(old_mask != NULL) {
					const fixed_t proportion = ftofxp(1.0) - fxpdiv(i,niterations);
					tod_hex_mask1.assign(adjust_surface_alpha(old_mask,proportion));
				}

				if(new_mask != NULL) {
					const fixed_t proportion = fxpdiv(i,niterations);
					tod_hex_mask2.assign(adjust_surface_alpha(new_mask,proportion));
				}

				invalidate_all();
				draw();

				const int cur_ticks = SDL_GetTicks();
				const int wanted_ticks = starting_ticks + i*frame_time;
				if(cur_ticks < wanted_ticks) {
					SDL_Delay(wanted_ticks - cur_ticks);
				}
			}
		}

		tod_hex_mask1.assign(NULL);
		tod_hex_mask2.assign(NULL);
	}

	first_turn_ = false;

	image::set_color_adjustment(tod.red,tod.green,tod.blue);

	invalidate_all();
	draw();
}

void game_display::adjust_colors(int r, int g, int b)
{
	const time_of_day& tod = tod_manager_.get_time_of_day();
	image::set_color_adjustment(tod.red+r,tod.green+g,tod.blue+b);
}

void game_display::select_hex(map_location hex)
{
	if(hex.valid() && fogged(hex)) {
		return;
	}
	display::select_hex(hex);

	display_unit_hex(hex);
}

void game_display::highlight_hex(map_location hex)
{
	// wb::scoped_planned_pathfind_map future; //< Lasts for whole method.
	const unit *u = get_visible_unit(hex, teams_[viewing_team()], !viewpoint_);
	if (u) {
		invalidate_unit();
	} else {
		u = get_visible_unit(mouseoverHex_, teams_[viewing_team()], !viewpoint_);
		if (u) {
			// mouse moved from unit hex to non-unit hex
			if (units_.valid2(selectedHex_, true)) {
				invalidate_unit();
			}
		}
	}

	display::highlight_hex(hex);
	invalidate_game_status();
}


void game_display::display_unit_hex(map_location hex)
{
	// wb::scoped_planned_pathfind_map future; //< Lasts for whole method.

	const unit *u = get_visible_unit(hex, teams_[viewing_team()], !viewpoint_);
	if (u) {
		invalidate_unit();
	}
}

void game_display::invalidate_unit_after_move(const map_location& src, const map_location& dst)
{
	invalidate_unit();
}

void game_display::resort_access_troops(unit& u)
{
	refresh_access_troops(u.side() - 1, game_display::REFRESH_ERASE, &u);
	units_.sort_map(u);
	refresh_access_troops(u.side() - 1, game_display::REFRESH_INSERT, &u);
}

void game_display::verify_access_troops() const
{
	const taccess_list* list = &troop_list_;
	const gui2::tgrid::tchild* children = list->bar.report()->content_grid()->children();
	int childs = gui2::ttabbar::front_childs + list->bar.childs();
	const std::string miss_state_str = "u's state dismiss!";
	const std::string u_dismatch_str = "u does't match!";
	const std::string order_dismatch_str = "order does't match!";
	size_t flips = 0;

	int btn_index = 1;
	unit* previous = NULL;
	for (unit_map::const_iterator it = units_.begin(); it != units_.end(); ++ it, btn_index ++) {
		unit& a = *dynamic_cast<unit*>(&*it);
		VALIDATE(!a.get_state(ustate_tag::EXPEDITED), miss_state_str);
		if (btn_index < childs) {
			gui2::twidget* btn = children[btn_index].widget_;
			unit* b = reinterpret_cast<unit*>(btn->cookie());
			VALIDATE(&a == b, u_dismatch_str);

			bool match = true;
			if (previous) {
				if (!tent::turn_based) {
					match = previous->compare_action_order(a);
				} else {
					if (previous->side() > a.side()) {
						flips ++;
					}
					match = flips < 2;
				}
			}
			if (!match) {
				resources::controller->process_oos2(order_dismatch_str);
			}

			previous = &a;

		} else {
			VALIDATE(!a.consider_ticks(), u_dismatch_str);
		}
	}
}

void game_display::refresh_access_troops(int side, refresh_reason reason, void* cookie)
{
	refresh_access_list(side, reason, cookie, taccess_list::TROOP);
}

void game_display::refresh_access_heros(int side, refresh_reason reason, void* cookie)
{
	refresh_access_list(side, reason, cookie, taccess_list::HERO);
}

bool game_display::access_is_null(int type) const
{
	const taccess_list* list = &troop_list_;
	if (type == taccess_list::HERO) {
		list = &hero_list_;
	}
	if (!list->bar.report()) {
		return true;
	}

	return list->bar.childs()? false: true;
}

// refresh field troop buttons
// @side: base 0
void game_display::refresh_access_list(int side, refresh_reason reason, void* cookie, int type)
{
	taccess_list* list = &troop_list_;
	if (type == taccess_list::HERO) {
		list = &hero_list_;
	}
	if (!list->bar.report()) {
		return;
	}
	if (reason != REFRESH_RELOAD && !list->reloaded) {
		return;
	}

	if (reason == REFRESH_RELOAD) {
		// destroy all existed button
		VALIDATE(!list->reloaded, "program error");

		access_list_side_ = side;

		list->reload(*this, side);

	} else if ((type == taccess_list::TROOP || access_list_side_ == side) && (reason == REFRESH_INSERT)) {
		list->insert(*this, access_list_side_, cookie);

	} else if ((type == taccess_list::TROOP || access_list_side_ == side) && reason == REFRESH_ERASE) {
		list->erase(*this, access_list_side_, cookie);

	}  else if ((type == taccess_list::TROOP || access_list_side_ == side) && (reason == REFRESH_ENABLE)) {
		list->enable(*this, access_list_side_, cookie);

	} else if ((type == taccess_list::TROOP || access_list_side_ == side) && (reason == REFRESH_DRAW)) {
		if (!cookie) {
			list->bar.replacement_children();
		} else if (!list->hide) {
			list->redraw(*this, access_list_side_, cookie);
		}

	} else if ((type == taccess_list::TROOP || access_list_side_ == side) && (reason == REFRESH_HIDE)) {
		// in order to hide, must use stuff-widget, cannot use set_visible(INVISIBLE)
		list->hide = cookie? true: false;
		if (!list->hide) {
			list->bar.replacement_children();
		} else {
			list->bar.hide_children();
		}

	} else if (reason == REFRESH_CLEAR) {
		list->bar.erase_children();
		list->reloaded = false;
	}
}

void game_display::show_access_troop(gui2::ttabbar* bar, const gui2::tgrid::tchild& child)
{
	const gui2::tpoint& size = bar->report()->get_unit_size();

	gui2::tcontrol* widget = dynamic_cast<gui2::tcontrol*>(child.widget_);
	// troop image is large amount. refresh when requrie.
	unit* u = reinterpret_cast<unit*>(widget->cookie());
	int drawn_ticks = u->drawn_ticks();

	if (!u->ticks_adjusting && drawn_ticks != unit_map::main_ticks) {
		u->set_drawn_ticks(unit_map::main_ticks);
		bool greyscale = tent::turn_based? !u->can_move(): (u->side() != access_list_side_ + 1);
		surface surf = u->generate_access_surface(size.x, size.y, greyscale);
		widget->set_surface(surf, size.x, size.y);
	}
}

int game_display::next_list_type() const
{
	return (current_list_type_ + 1) % taccess_list::TYPE_COUNT;
}

game_display::taccess_list& game_display::type_2_list(int type)
{
	if (current_list_type_ == taccess_list::HERO) {
		return hero_list_;
	} else {
		return troop_list_;
	}
}

void game_display::set_current_list_type(int type) 
{ 
	if (current_list_type_ == type) {
		return;
	}
	game_display::taccess_list* list = &type_2_list(current_list_type_);
	if (list->bar.report()) {
		list->bar.hide_children();
		list->bar.report()->parent()->set_visible(gui2::twidget::INVISIBLE);
	}

	current_list_type_ = type; 

	list = &type_2_list(current_list_type_);
	if (list->bar.report()) {
		list->bar.report()->parent()->set_visible(gui2::twidget::VISIBLE);
		list->bar.replacement_children();
	}
}

void game_display::goto_main_context_menu()
{
	show_context_menu();
}

void game_display::pre_draw(rect_of_hexes& hexes) 
{
	// generate all units in current win.
	if (resources::units) {
		draw_area_unit_size_ = units_.units_from_rect(draw_area_unit_, draw_area_rect_);
	} else {
		draw_area_unit_size_ = 0;
	}

	// generate possible tactical formation.
	std::vector<tformation> formations;
	std::set<const unit*> uncalculated;
	for (size_t i = 0; i < draw_area_unit_size_; i ++) {
		unit* u = dynamic_cast<unit*>(draw_area_unit_[i]);
		if (u->is_artifical() || u->is_commoner()) {
			continue;
		}
		u->set_formation_flag(unit::FORMATION_NONE);
		uncalculated.insert(u);
	}

	calculate_formations(teams_, units_, activeTeam_ + 1, uncalculated, &draw_area_rect_, false, formations);

	// generate tactical formation's animation
	for (std::vector<tformation>::iterator it = formations.begin(); it != formations.end(); ++ it) {
		tformation& formation = *it;
		int flag2 = 0;
		for (std::map<int, unit*>::iterator it2 = formation.adjacent_.begin(); it2 != formation.adjacent_.end(); ++ it2) {
			unit* adj = it2->second;
			adj->set_formation_flag(unit::FORMATION_SECOND);
			flag2 |= 1 << it2->first;
		}
		if (!formation.disable_) {
			formation.u_->set_formation_flag(unit::FORMATION_MASTER + (formation.profile_->index() + 1) * 100, flag2);
		} else {
			formation.u_->set_formation_flag(unit::FORMATION_MASTER_DISABLE + (formation.profile_->index() + 1) * 100, flag2);
		}
	}

	process_reachmap_changes();
	process_disctrict_changes();
	/**
	 * @todo FIXME: must modify changed, but best to do it at the
	 * floating_label level
	 */
	prune_chat_messages();
}

surface generate_turn_surface(const std::string& text, int w, int h)
{
	std::vector<std::string> vstr = utils::split(text);
	if (vstr.size() != 4) {
		return create_neutral_surface(w, h);
	}

	surface screen = create_neutral_surface(w, h);

	const int bar_turn_png_valid_width = 76;
	const int left_gap_with = 2;
	int main_ticks = lexical_cast_default<int>(vstr[0]);
	int autosave_ticks = lexical_cast_default<int>(vstr[1]);

	double filled = 1.0 * (main_ticks % game_config::ticks_per_turn) / game_config::ticks_per_turn;
	draw_bar_to_surf("misc/bar-turn.png", screen, 0, 1, w - 4, filled, font::GOOD_COLOR, ftofxp(0.8), false);

	SDL_Rect dst_clip;
	if (autosave_ticks >= 0) {
		// auto save point
		int pos = (autosave_ticks % game_config::ticks_per_turn) * bar_turn_png_valid_width / game_config::ticks_per_turn;
		dst_clip.x = 0 + left_gap_with + pos;
		dst_clip.y = 0;

		surface filled_surf = create_compatible_surface(screen, 2, h);
		SDL_FillRect(filled_surf, NULL, SDL_MapRGBA(filled_surf->format, 255, 0, 0, 1.0));
		sdl_blit(filled_surf, NULL, screen, &dst_clip);
	}

	std::string turn_str = vstr[2];
	if (lexical_cast_default<int>(vstr[3]) > 0) {
		turn_str.append("/");
		turn_str.append(vstr[3]);
	}
	dst_clip.x = (w - (turn_str.size() * 8)) / 2;
	dst_clip.y = 2;
	std::stringstream img;
	for (std::string::const_iterator it = turn_str.begin(); it != turn_str.end(); ++ it) {
		char ch = *it;
		img.str("");
		if (isdigit(ch)) {
			img << "misc/digit.png~CROP(" << 8 * (ch - 0x30) << ", 0, 8, 12)";
		} else if (ch == '/') {
			img << "misc/digit.png~CROP(" << 8 * 10 << ", 0, 8, 12)";
		}
		if (!img.str().empty()) {
			sdl_blit(image::get_image(img.str()), NULL, screen, &dst_clip);
			dst_clip.x += 8;
		}
	}

	return screen;
}

surface generate_unit_image_surface(const unit_map& units, const std::string& text, int w, int h)
{
	std::vector<std::string> vstr = utils::split(text);
	if (vstr.size() != 2) {
		return create_neutral_surface(w, h);
	}

	const map_location loc(lexical_cast_default<int>(vstr[0]), lexical_cast_default<int>(vstr[1]));
	const unit* u = units.find_unit(loc);

	surface surf = image::get_image(image::locator(u->absolute_image(), u->image_mods()));
	surface screen = scale_surface(surf, w, h);

	if (!u->is_artifical() && u->especial() != NO_ESPECIAL) {
		SDL_Rect dst_clip = create_rect(0, 0, 0, 0);
		const std::string& img = unit_types.especial(u->especial()).image_;
		surf = scale_surface(image::get_image(img), 16, 16);
		sdl_blit(surf, NULL, screen, &dst_clip);
	}
	return screen;
}

surface game_display::refresh_surface_report(int num, const reports::report& r, gui2::twidget& widget)
{
	surface surf;
	const SDL_Rect rect = widget.get_rect();
	if (num == gui2::tgame_theme::TURN) {
		surf = generate_turn_surface(r.text, rect.w, rect.h);

	} else if (num == gui2::tgame_theme::UNIT_IMAGE) {
		surf = generate_unit_image_surface(units_, r.text, rect.w, rect.h);
	}

	return surf;
}

image::TYPE game_display::get_image_type(const map_location& loc) {
	// We highlight hex under the mouse, or under a selected unit.
	if (get_map().on_board(loc)) {
		if (loc == mouseoverHex_) {
			return image::BRIGHTENED;
		} else if (loc == selectedHex_) {
			const unit *un = get_visible_unit(loc, teams_[currentTeam_], !viewpoint_);
			if (un && !un->get_hidden()) {
				return image::BRIGHTENED;
			}
		}
	}
	return image::TOD_COLORED;
}

void game_display::draw_invalidated()
{
	halo::unrender();
	
	map_location loc_n, loc_ne;
	std::vector<map_location> unit_invals;

	for (size_t i = 0; i < draw_area_unit_size_; i ++) {
		const base_unit* u = draw_area_unit_[i];
		const std::set<map_location>& draw_locs = u->get_draw_locations();
		invalidate(draw_locs);

		const map_location& loc = u->get_location();
		draw_area_val(loc.x, loc.y) = INVALIDATE_UNIT;
		unit_invals.push_back(loc);
	}

	if (temp_unit_) {
		const map_location& loc = temp_unit_->get_location();

		loc_n = loc.get_direction(map_location::NORTH);
		if (point_in_rect_of_hexes(loc_n.x, loc_n.y, draw_area_rect_)) {
			draw_area_val(loc_n.x, loc_n.y) = INVALIDATE;
		}
		loc_ne = loc.get_direction(map_location::NORTH_EAST);
		if (point_in_rect_of_hexes(loc_ne.x, loc_ne.y, draw_area_rect_)) {
			draw_area_val(loc_ne.x, loc_ne.y) = INVALIDATE;
		}

		if (draw_area_val(loc.x, loc.y) != INVALIDATE_UNIT) {
			draw_area_val(loc.x, loc.y) = INVALIDATE_UNIT;
			unit_invals.push_back(loc);
		}
	}
	if (expedite_city_ && point_in_rect_of_hexes(expedite_city_->get_location().x, expedite_city_->get_location().y, draw_area_rect_)) {
		const std::set<map_location>& touch_locs = expedite_city_->get_touch_locations();
		for (std::set<map_location>::const_iterator itor = touch_locs.begin(); itor != touch_locs.end(); ++ itor) {
			draw_area_val(itor->x, itor->y) = INVALIDATE_UNIT;
		}

		const map_location& loc = expedite_city_->get_location();
		if (draw_area_val(loc.x, loc.y) != INVALIDATE_UNIT) {
			draw_area_val(loc.x, loc.y) = INVALIDATE_UNIT;
			unit_invals.push_back(loc);
		}
	}

	display::draw_invalidated();

	redraw_units(unit_invals);
}

void game_display::post_commit()
{
	halo::render();
}

void surface_2_file(SDL_Surface* surf)
{
	if (surf->pitch != 288 || surf->h != 72 || !surf->pixels) {
		return;
	}
	posix_file_t fp = INVALID_FILE;
	uint32_t bytertd;
	posix_fopen("c:\\surface.dat", GENERIC_WRITE, CREATE_ALWAYS, fp);
	if (fp == INVALID_FILE) {
		return;
	}
	posix_fwrite(fp, surf->pixels, surf->pitch * surf->h, bytertd);
	posix_fclose(fp);
}

void file_2_surface(SDL_Surface* surf)
{
	posix_file_t fp = INVALID_FILE;
	uint32_t bytertd;
	posix_fopen("c:\\surface.dat.osx.1xbgr", GENERIC_READ, OPEN_EXISTING, fp);
	if (fp == INVALID_FILE) {
		return;
	}
	posix_fread(fp, surf->pixels, surf->pitch * surf->h, bytertd);
	posix_fclose(fp);
}

std::map<map_location, std::string> tip_locs_;

void game_display::draw_hex(const map_location& loc)
{
	std::string str;
	const bool on_map = get_map().on_board(loc);
	const bool is_shrouded = shrouded(loc);
	const bool is_fogged = fogged(loc);
	const int xpos = get_location_x(loc);
	const int ypos = get_location_y(loc);
	const unit* selected_unit = units_.find_unit(selectedHex_);

	image::TYPE image_type = get_image_type(loc);

	display::draw_hex(loc);

	if (!is_shrouded) {
		typedef overlay_map::const_iterator Itor;
		std::pair<Itor,Itor> overlays = overlays_.equal_range(loc);
		for( ; overlays.first != overlays.second; ++overlays.first) {
			if ((overlays.first->second.team_name == "" ||
			overlays.first->second.team_name.find(teams_[playing_team()].team_name()) != std::string::npos)
			&& !(is_fogged && !overlays.first->second.visible_in_fog))
			{
				drawing_buffer_add(LAYER_TERRAIN_BG, loc, xpos, ypos,
					image::get_image(overlays.first->second.image,image_type));
			}
		}
		// village-control flags.
		drawing_buffer_add(LAYER_TERRAIN_BG, loc, xpos, ypos, get_flag(loc));

		// city-controll flags
		if (surface big_flag_surf = get_big_flag(loc)) {
			if (default_zoom_ == ZOOM_48) {
				drawing_buffer_add(LAYER_REACHMAP, loc, xpos, ypos - 20, big_flag_surf);
			} else if (default_zoom_ == ZOOM_56) {
				drawing_buffer_add(LAYER_REACHMAP, loc, xpos, ypos - 24, big_flag_surf);
			} else if (default_zoom_ == ZOOM_64) {
				drawing_buffer_add(LAYER_REACHMAP, loc, xpos, ypos - 28, big_flag_surf);
			} else {
				drawing_buffer_add(LAYER_REACHMAP, loc, xpos, ypos - 32, big_flag_surf);
			}
		}
	}

	if (on_map) {
		if (selected_unit && selected_unit->task() == unit::TASK_GUARD) {
			const guard_cache::tguard& guard = guard_cache::find(*selected_unit);
			if ((int)distance_between(guard.loc, loc) <= guard.radius) {
				drawing_buffer_add(LAYER_GRID_BOTTOM,
					loc, xpos, ypos, image::get_image("misc/guard-hex.png", image::SCALED_TO_ZOOM));
			}
			if (guard.loc == loc) {
				if (get_current_animation_tick() % 400 < 200) {
					drawing_buffer_add(LAYER_ATTACK_INDICATOR,
						loc, xpos, ypos, image::get_image("misc/guard-loc1.png", image::SCALED_TO_ZOOM));
				} else {
					drawing_buffer_add(LAYER_ATTACK_INDICATOR,
						loc, xpos, ypos, image::get_image("misc/guard-loc2.png", image::SCALED_TO_ZOOM));
				}
			}
		}
		std::map<map_location, std::string>::const_iterator find = tip_locs_.find(loc);
		if (find != tip_locs_.end() && !find->second.empty()) {
			drawing_buffer_add(LAYER_GRID_BOTTOM, // LAYER_GRID_BOTTOM, LAYER_UNIT_MOVE_DEFAULT
				loc, xpos, ypos, image::get_image(find->second, image::SCALED_TO_ZOOM));
		}
	}

	// Draw reach_map information.
	// We remove the reachability mask of the unit
	// that we want to attack.
	if (!is_shrouded && !reach_map_.empty() && reach_map_.find(loc) != reach_map_.end()) {
		drawing_buffer_add(LAYER_GRID_BOTTOM, loc, xpos, ypos, image::get_image(game_config::terrain::unreachable, image::SCALED_TO_HEX));
	}

	if (!disctrict_.empty() && disctrict_.find(loc) != disctrict_.end()) {
		drawing_buffer_add(LAYER_MOVE_INFO, loc, xpos, ypos, image::get_image(game_config::terrain::disctrict, image::SCALED_TO_HEX));
	}

	// Footsteps indicating a movement path
	const std::vector<surface>& footstepImages = footsteps_images(loc);
	if (footstepImages.size() != 0) {
		drawing_buffer_add(LAYER_FOOTSTEPS, loc, xpos, ypos, footstepImages);
	}

	// Draw the selectable indicator
	// selectable indicator overlapped with attack indicator, privilidge display selectable.
	if (on_map && selectable_indicator_.empty()) {
		if (formation_indicator_.second.valid() && marker_indicator_dst_.find(loc) != marker_indicator_dst_.end()) {
			// Draw the makrer indicator
			drawing_buffer_add(LAYER_ATTACK_INDICATOR, loc, xpos, ypos, image::get_image("misc/attack-marker.png", image::SCALED_TO_HEX));
		}
		if (attack_indicator_dst_.find(loc) != attack_indicator_dst_.end()) {
			// Draw the attack direction indicator
			if (alternatable_indicator_.find(loc) == alternatable_indicator_.end()) {
				drawing_buffer_add(LAYER_ATTACK_INDICATOR, loc, xpos, ypos, image::get_image("misc/attack-indicator-dst.png", image::SCALED_TO_HEX));
			} else if (!preferences::default_move()) {
				drawing_buffer_add(LAYER_ATTACK_INDICATOR, loc, xpos, ypos, image::get_image("misc/selectable-indicator.png", image::SCALED_TO_HEX));
			}
		}
	} else if (on_map) {
		if (selectable_indicator_.find(loc) != selectable_indicator_.end()) {
			drawing_buffer_add(LAYER_ATTACK_INDICATOR, loc, xpos, ypos, image::get_image("misc/selectable-indicator.png", image::SCALED_TO_HEX));
		}
	}

	// placable indicator
	if (on_map && placable_indicator_.find(loc) != placable_indicator_.end()) {
		drawing_buffer_add(LAYER_TERRAIN_FG, loc, xpos, ypos, image::get_image("misc/placable-indicator.png", image::SCALED_TO_HEX));
	}
	// joinable indicator
	if (on_map && joinable_indicator_.find(loc) != joinable_indicator_.end()) {
		drawing_buffer_add(LAYER_ATTACK_INDICATOR, loc, xpos, ypos, image::get_image("misc/joinable-indicator.png", image::SCALED_TO_HEX));
	}

	// Draw the tactic indicator
	if (on_map && loc == tactic_indicator_.first) {
		if (get_current_animation_tick() % 800 < 400) {
			str = "misc/tactic-indicator1.png";
		} else {
			str = "misc/tactic-indicator2.png";
		}
		drawing_buffer_add(LAYER_ATTACK_INDICATOR, loc, xpos, ypos, image::get_image(str, image::SCALED_TO_HEX));
		if (tactic_indicator_.second > 0) {
			str = multi_digit_image_str(tactic_indicator_.second);
			SDL_Point size = multi_digit_image_size(tactic_indicator_.second);
			drawing_buffer_add(LAYER_ATTACK_INDICATOR, loc, xpos + (zoom_ - size.x) / 2, ypos + (zoom_ - size.y) - 4, font::get_rendered_text2(str, -1, font::SIZE_NORMAL, font::BLACK_COLOR));
		}
	}
	// Draw the clear_formated indicator
	if (on_map && loc == clear_formationed_indicator_.first) {
		if (get_current_animation_tick() % 800 < 400) {
			str = "misc/clear-formationed-indicator1.png";
		} else {
			str = "misc/clear-formationed-indicator2.png";
		}
		if (clear_formationed_indicator_.second < 0) {
			str.append("~GS()");
		}
		drawing_buffer_add(LAYER_ATTACK_INDICATOR, loc, xpos, ypos, image::get_image(str, image::SCALED_TO_HEX));
		if (clear_formationed_indicator_.second) {
			str = multi_digit_image_str(abs(clear_formationed_indicator_.second));
			SDL_Point size = multi_digit_image_size(abs(clear_formationed_indicator_.second));
			drawing_buffer_add(LAYER_ATTACK_INDICATOR, loc, xpos + (zoom_ - size.x) / 2, ypos + zoom_ / 3, font::get_rendered_text2(str, -1, font::SIZE_NORMAL, font::BLACK_COLOR));
		}
	}
	// Draw the intervene_move indicator
	if (on_map && loc == intervene_move_indicator_.first) {
		if (get_current_animation_tick() % 800 < 400) {
			str = "misc/intervene-move-indicator1.png";
		} else {
			str = "misc/intervene-move-indicator2.png";
		}
		if (intervene_move_indicator_.second < 0) {
			str.append("~GS()");
		}
		drawing_buffer_add(LAYER_ATTACK_INDICATOR, loc, xpos, ypos, image::get_image(str, image::SCALED_TO_HEX));
		if (intervene_move_indicator_.second) {
			str = multi_digit_image_str(abs(intervene_move_indicator_.second));
			SDL_Point size = multi_digit_image_size(abs(intervene_move_indicator_.second));
			drawing_buffer_add(LAYER_ATTACK_INDICATOR, loc, xpos + (zoom_ - size.x) / 2, ypos + zoom_ / 3, font::get_rendered_text2(str, -1, font::SIZE_NORMAL, font::BLACK_COLOR));
		}
	}
	// Draw the interior indicator
	if (on_map && !events::mouse_handler::get_singleton()->is_moving() && loc == interior_indicator_) {
		drawing_buffer_add(LAYER_ATTACK_INDICATOR, loc, xpos, ypos, image::get_image("misc/interior-indicator.png", image::SCALED_TO_HEX));
	}
	// Draw the build direction indicator
	if (on_map && std::find(build_indicator_dst_.begin(), build_indicator_dst_.end(), loc) != build_indicator_dst_.end()) {
		drawing_buffer_add(LAYER_ATTACK_INDICATOR, loc, xpos, ypos, image::get_image("misc/build-indicator-dst.png", image::SCALED_TO_HEX));
	}

	if (on_map && loc == moving_src_loc_) {
		drawing_buffer_add(LAYER_ATTACK_INDICATOR, loc, xpos, ypos, image::get_image("misc/move-indicator-src.png", image::SCALED_TO_HEX));
	}

	if (on_map && loc == moving_dst_loc_) {
		drawing_buffer_add(LAYER_ATTACK_INDICATOR, loc, xpos, ypos, image::get_image("misc/move-indicator-dst.png", image::SCALED_TO_HEX));
	}

	// Linger overlay unconditionally otherwise it might give glitches
	// so it's drawn over the shroud and fog.
	if (game_mode_ != RUNNING) {
		static const image::locator linger(game_config::terrain::linger);
		drawing_buffer_add(LAYER_LINGER_OVERLAY, loc, xpos, ypos,
			image::get_image(linger, image::TOD_COLORED));
	}

	if(on_map && loc == selectedHex_ && !game_config::terrain::selected.empty()) {
		static const image::locator selected(game_config::terrain::selected);
		drawing_buffer_add(LAYER_SELECTED_HEX, loc, xpos, ypos,
				image::get_image(selected, image::SCALED_TO_HEX));
	}

	// Draw the fomration indicator
	if (on_map && loc == formation_indicator_.second) {
		drawing_buffer_add(LAYER_ATTACK_INDICATOR, loc, xpos, ypos, image::get_image("misc/formation-indicator.png", image::SCALED_TO_HEX));
	} else {
		// Show def% and turn to reach info
		if (!is_shrouded && on_map && !units_.city_from_loc(loc)) {
			draw_movement_info(loc);
		}
	}

	//simulate_delay += 1;
}

void game_display::redraw_units(const std::vector<map_location>& invalidated_unit_locations)
{
	// Units can overlap multiple hexes, so we need
	// to redraw them last and in the good sequence.
	BOOST_FOREACH (map_location loc, invalidated_unit_locations) {
		if (expedite_city_ && expedite_city_->get_location() == loc) {
			expedite_city_->redraw_unit();
			//simulate_delay += 1;
		}
		unit_map::iterator u_it = units_.find(loc, false);
		if (u_it != units_.end()) {
			u_it->redraw_unit();
		}
		u_it = units_.find(loc);
		if (u_it != units_.end()) {
			u_it->redraw_unit();
		}
		if (temp_unit_ && temp_unit_->get_location() == loc) {
			temp_unit_->redraw_unit();
		}
	}
	for (std::map<int, animation*>::iterator it = area_anims_.begin(); it != area_anims_.end(); ++ it) {
		tanim_type type = it->second->type();
		VALIDATE(type == anim_map || type == anim_canvas, miss_anim_err_str);
		if (type != anim_map) {
			continue;
		}

		it->second->update_last_draw_time();
		area_anim::rt.type = it->second->type();
		it->second->redraw(screen_.getSurface(), empty_rect);
	}
}

const time_of_day& game_display::get_time_of_day(const map_location& loc) const
{
	return tod_manager_.get_time_of_day(loc);
}

bool game_display::has_time_area() const
{
	return tod_manager_.has_time_area();
}

void game_display::draw_report(int report_num)
{
	if(!team_valid()) {
		return;
	}

	reports::report report = reports::generate_report(report_num,
		teams_[viewing_team()], currentTeam_ + 1, activeTeam_ + 1,
		selectedHex_, mouseoverHex_,
		observers_, level_, !viewpoint_);

	refresh_report(report_num, report);
}

void game_display::draw_game_status()
{
	if(teams_.empty()) {
		return;
	}

	for (int r = gui2::tgame_theme::STATUS_REPORTS_BEGIN; r != gui2::tgame_theme::STATUS_REPORTS_END; ++r) {
		draw_report(r);
	}
}

void game_display::show_unit_tip(const unit& troop, const map_location& loc)
{
	if (!game_config::tiny_gui) {
		// hide tactic slots
		std::stringstream name;
		for (int n = 0; n < game_config::active_tactic_slots; n ++) {
			name.str("");
			name << "tactic" << n;
			gui2::tbutton* widget = dynamic_cast<gui2::tbutton*>(get_theme_object(name.str()));
			widget->set_visible(gui2::twidget::INVISIBLE);
		}
		invalidate_all();
	}
	
	if (main_tip_handle_) {
		for (std::map<map_location, std::string>::const_iterator it = tip_locs_.begin(); it != tip_locs_.end(); ++ it) {
			invalidate(it->first);
		}
		tip_locs_.clear();

		font::remove_floating_label(main_tip_handle_);
		main_tip_handle_ = 0;
	}

	for (size_t i = 0; i < troop.adjacent_size_ && !troop.is_artifical(); i ++) {
		invalidate(troop.adjacent_[i]);
		tip_locs_.insert(std::make_pair(troop.adjacent_[i], ""));
	}
	for (size_t i = 0; i < troop.adjacent_size_2_ && !troop.is_artifical(); i ++) {
		const map_location& loc1 = troop.adjacent_2_[i];
		if (loc1.x == loc.x - 2) {
			if (loc1.y < loc.y) {
				tip_locs_.insert(std::make_pair(loc1, "misc/light2-nw2.png"));
			} else if (loc1.y == loc.y) {
				tip_locs_.insert(std::make_pair(loc1, "misc/light2-w.png"));
			} else {
				tip_locs_.insert(std::make_pair(loc1, "misc/light2-sw2.png"));
			}
		} else if (loc1.x == loc.x - 1) {
			if (loc1.y < loc.y) {
				tip_locs_.insert(std::make_pair(loc1, "misc/light2-nw.png"));
			} else {
				tip_locs_.insert(std::make_pair(loc1, "misc/light2-sw.png"));
			}
		} else if (loc1.x == loc.x) {
			if (loc1.y < loc.y) {
				tip_locs_.insert(std::make_pair(loc1, "misc/light2-n.png"));
			} else {
				tip_locs_.insert(std::make_pair(loc1, "misc/light2-s.png"));
			}
		} else if (loc1.x == loc.x + 1) {
			if (loc1.y < loc.y) {
				tip_locs_.insert(std::make_pair(loc1, "misc/light2-ne.png"));
			} else {
				tip_locs_.insert(std::make_pair(loc1, "misc/light2-se.png"));
			}
		} else if (loc1.x == loc.x + 2) {
			if (loc1.y < loc.y) {
				tip_locs_.insert(std::make_pair(loc1, "misc/light2-ne2.png"));
			} else if (loc1.y == loc.y) {
				tip_locs_.insert(std::make_pair(loc1, "misc/light2-e.png"));
			} else {
				tip_locs_.insert(std::make_pair(loc1, "misc/light2-se2.png"));
			}
		}
		invalidate(loc1);
	}
	for (size_t i = 0; i < troop.adjacent_size_3_ && !troop.is_artifical(); i ++) {
		invalidate(troop.adjacent_3_[i]);
		tip_locs_.insert(std::make_pair(troop.adjacent_3_[i], ""));
	}

	show_tip(troop.form_tip(), loc, false);
}

void game_display::show_tip(const std::string& message, const map_location& loc, bool clear)
{
	int16_t xoffset_main, yoffset_main;
	SDL_Color tip_color = font::NORMAL_COLOR;
	int font_size = font::SIZE_SMALL;

	const SDL_Color bgcolor = {0, 0, 0, 160};
	SDL_Rect area = screen_area();

	if (clear && main_tip_handle_) {
		for (std::map<map_location, std::string>::const_iterator it = tip_locs_.begin(); it != tip_locs_.end(); ++ it) {
			invalidate(it->first);
		}
		tip_locs_.clear();

		font::remove_floating_label(main_tip_handle_);
		main_tip_handle_ = 0;
	}

	// const std::string wrapped_message = font::word_wrap_text(tip_message, font_size, text_width);
	// font::word_wrap_text don't now tag of tintegate. but tip_message is tintegate format message.
	const std::string wrapped_message = message;

	font::floating_label flabel(wrapped_message);
	flabel.set_font_size(font_size);
	flabel.set_color(tip_color);
	flabel.set_bg_color(bgcolor);
	flabel.set_position(0, 0);
	flabel.set_alignment(font::LEFT_ALIGN);
	flabel.set_clip_rect(area);
	main_tip_handle_ = font::add_floating_label(flabel);

	SDL_Rect rect_main = font::get_floating_label_rect(main_tip_handle_);

	// one instance of map_area: (0, 26, 800, 471)
	const SDL_Rect& map_area = map_outside_area();
	int xpos = loc.valid()? get_location_x(loc): rect_main.w;

	// 单位提示放置规则:
	// 1. 当x方向上，窗口距单位所在格子足够放置提示宽度时，提示放在左下角，否则放去右下解；
	if (game_config::tiny_gui || xpos >= rect_main.w) {
		xoffset_main = map_area.x;
	} else  {
		xoffset_main = map_area.x + map_area.w - rect_main.w;
	}

	yoffset_main = map_area.y + map_area.h - rect_main.h;
	font::move_floating_label(main_tip_handle_, xoffset_main, yoffset_main);
}

void game_display::hide_tip()
{
	if (!in_theme()) {
		return;
	}
	if (main_tip_handle_) {
		for (std::map<map_location, std::string>::const_iterator it = tip_locs_.begin(); it != tip_locs_.end(); ++ it) {
			invalidate(it->first);
		}
		tip_locs_.clear();

		font::remove_floating_label(main_tip_handle_);
		main_tip_handle_ = 0;
	}
	teams_[playing_team()].refresh_tactic_slots(*this);
}

void game_display::draw_sidebar()
{
	// wb::scoped_planned_pathfind_map future; //< Lasts for whole method.

	draw_report(gui2::tgame_theme::REPORT_CLOCK);
	draw_report(gui2::tgame_theme::REPORT_COUNTDOWN);

	if(teams_.empty()) {
		return;
	}

	if (invalidateUnit_) {
		// We display the unit the mouse is over if it is over a unit,
		// otherwise we display the unit that is selected.
		for (int r = gui2::tgame_theme::UNIT_REPORTS_BEGIN; r != gui2::tgame_theme::UNIT_REPORTS_END; ++r) {
			draw_report(r);
		}

		invalidateUnit_ = false;

		if (resources::controller->is_recovering(currentTeam_ + 1) || tent::mode == mode_tag::SIEGE || !events::commands_disabled) {
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
			const unit* selected_it = find_visible_unit(selectedHex_, teams_[currentTeam_]);
			if (selected_it && !fogged(selectedHex_)) {
				show_unit_tip(*selected_it, selected_it->get_location());
			}
#else
			const unit* mouseover_it = find_visible_unit(mouseoverHex_, teams_[currentTeam_]);
			if (mouseover_it && !fogged(mouseoverHex_)) {
				show_unit_tip(*mouseover_it, mouseover_it->get_location());
			}
#endif
		}
	}

	if (invalidateGameStatus_) {
		draw_game_status();
		invalidateGameStatus_ = false;
	}
}

surface game_display::minimap_surface(int w, int h)
{
	return image::getMinimap(w, h, get_map(), viewpoint_);
}

void game_display::draw_minimap_units(surface& screen)
{
	double xscaling = 1.0 * minimap_location_.w / get_map().w();
	double yscaling = 1.0 * minimap_location_.h / get_map().h();

	for(unit_map::const_iterator it = units_.begin(); it != units_.end(); ++ it) {
		const unit* u = dynamic_cast<const unit*>(&*it);
		if (fogged(u->get_location()) ||
		    (teams_[currentTeam_].is_enemy(u->side()) &&
		     u->invisible(u->get_location())) ||
			 u->get_hidden()) {
			continue;
		}

		int side = u->side();
		const SDL_Color col = team::get_minimap_color(side);
		const Uint32 mapped_col = SDL_MapRGB(screen->format, col.r, col.g, col.b);

		double u_x = u->get_location().x * xscaling;
		double u_y = (u->get_location().y + (is_odd(u->get_location().x) ? 1 : -1)/4.0) * yscaling;
 		// use 4/3 to compensate the horizontal hexes imbrication
		double u_w = 4.0 / 3.0 * xscaling;
		double u_h = yscaling;

		SDL_Rect r = create_rect(minimap_location_.x + round_double(u_x)
				, minimap_location_.y + round_double(u_y)
				, round_double(u_w)
				, round_double(u_h));

		sdl_fill_rect(screen, &r, mapped_col);

		if (unit_is_city(u)) {
			draw_rectangle(r.x - 2, r.y - 2, r.w + 4, r.h + 4, mapped_col, screen);
		}
	}

	if (moving_dst_loc_.valid()) {
		static surface indicator_dst = image::get_image("misc/move-indicator-dst-mini.png");
		SDL_Rect dstrect = create_rect(minimap_location_.x + std::min(minimap_location_.w - indicator_dst->w, std::max(0, round_double(moving_dst_loc_.x * xscaling) - indicator_dst->w / 2)), 
			minimap_location_.y + std::min(minimap_location_.h - indicator_dst->h, std::max(0, round_double(moving_dst_loc_.y * yscaling) - indicator_dst->h / 2)), 0, 0);
		sdl_blit(indicator_dst, NULL, screen, &dstrect);
	}
}

void game_display::draw_bar(const std::string& image, int xpos, int ypos, const map_location& loc, int size, double filled, const SDL_Color& col, fixed_t alpha, bool vtl)
{
	filled = std::min<double>(std::max<double>(filled, 0.0), 1.0);
	if (!vtl) {
		// now vertical bar is tactic bar only, it is calcualted by outer well and truly. 
		size = static_cast<size_t>(size * get_zoom_factor());
	}

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
	const SDL_Rect& unscaled_bar_loc = calculate_energy_bar(bar_surf);

	SDL_Rect bar_loc;
	if (surf->w == bar_surf->w && surf->h == bar_surf->h) {
		bar_loc = unscaled_bar_loc;
	} else {
		// don't support scale again.
		return;

		const fixed_t xratio = fxpdiv(surf->w,bar_surf->w);
		const fixed_t yratio = fxpdiv(surf->h,bar_surf->h);
		const SDL_Rect scaled_bar_loc = {fxptoi(unscaled_bar_loc. x * xratio),
					   fxptoi(unscaled_bar_loc. y * yratio + 127),
					   fxptoi(unscaled_bar_loc. w * xratio + 255),
					   fxptoi(unscaled_bar_loc. h * yratio + 255)};
		bar_loc = scaled_bar_loc;
	}

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
	SDL_Rect bot = {0, bar_loc.y + skip_rows, surf->w, 0};
	
	if (vtl) {
		bot.h = surf->h - bot.y;
		drawing_buffer_add(LAYER_UNIT_BAR, loc, xpos, ypos, surf, top);
		drawing_buffer_add(LAYER_UNIT_BAR, loc, xpos, ypos + top.h, surf, bot);
	} else {
		top.w = bar_loc.x;
		top.h = surf->h;
		bot.x = bar_loc.x + skip_rows;
		bot.y = 0;
		bot.w = surf->w - bot.x;
		bot.h = surf->h;
		drawing_buffer_add(LAYER_UNIT_BAR, loc, xpos, ypos, surf, top);
		drawing_buffer_add(LAYER_UNIT_BAR, loc, xpos + top.w, ypos, surf, bot);
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
			drawing_buffer_add(LAYER_UNIT_BAR, loc, xpos + bar_loc.x, ypos + bar_loc.y + unfilled, filled_surf);
		} else {
			drawing_buffer_add(LAYER_UNIT_BAR, loc, xpos + bar_loc.x, ypos + bar_loc.y, filled_surf);
		}
	}
}

void game_display::set_game_mode(const tgame_mode game_mode)
{
	if(game_mode != game_mode_) {
		game_mode_ = game_mode;
		invalidate_all();
	}
}

SDL_Rect game_display::clip_rect_commit() const
{
	return in_game_? map_area(): area_anim::rt.rect;
}

void game_display::draw_movement_info(const map_location& loc)
{
	// Search if there is a mark here
	pathfind::marked_route::mark_map::iterator w = route_.marks.find(loc);

	// Don't use empty route or the first step (the unit will be there)
	if(w != route_.marks.end()
				&& !route_.steps.empty() && route_.steps.front() != loc) {
		const unit* un = units_.find_unit(route_.steps.front(), true);
		if (un) {
			// Display the def% of this terrain
			int def =  100 - un->defense_modifier(get_map().get_terrain(loc));
			std::stringstream def_text;
			def_text << def << "%";

			SDL_Color color = int_to_color(game_config::red_to_green(def, false));

			// simple mark (no turn point) use smaller font
			int def_font = w->second.turns > 0 ? 18 : 16;
			draw_text_in_hex(loc, LAYER_MOVE_INFO, def_text.str(), def_font, color);

			int xpos = get_location_x(loc);
			int ypos = get_location_y(loc);

            if (w->second.invisible) {
				drawing_buffer_add(LAYER_MOVE_INFO, loc, xpos, ypos,
					image::get_image("misc/hidden.png", image::SCALED_TO_HEX));
			}

			if (w->second.zoc) {
				drawing_buffer_add(LAYER_MOVE_INFO, loc, xpos, ypos,
					image::get_image("misc/zoc.png", image::SCALED_TO_HEX));
			}

			if (w->second.capture) {
				drawing_buffer_add(LAYER_MOVE_INFO, loc, xpos, ypos,
					image::get_image("misc/capture.png", image::SCALED_TO_HEX));
			}

			if (w->second.pass_here) {
				drawing_buffer_add(LAYER_MOVE_INFO, loc, xpos, ypos,
					image::get_image("misc/waypoint.png", image::SCALED_TO_HEX));
			}

			//we display turn info only if different from a simple last "1"
			if (w->second.turns > 1 || (w->second.turns == 1 && loc != route_.steps.back())) {
				std::stringstream turns_text;
				turns_text << w->second.turns;
				draw_text_in_hex(loc, LAYER_MOVE_INFO, turns_text.str(), 17, font::NORMAL_COLOR, 0.5,0.8);
			}

			// The hex is full now, so skip the "show enemy moves"
			return;
		}
	}
	// When out-of-turn, it's still interesting to check out the terrain defs of the selected unit
	else if (selectedHex_.valid() && loc == mouseoverHex_)
	{
		const unit* selectedUnit = units_.find_unit(selectedHex_, true);
		const unit* mouseoveredUnit = units_.find_unit(mouseoverHex_, true);
		if (selectedUnit && !mouseoveredUnit) {
			// Display the def% of this terrain
			int def =  100 - selectedUnit->defense_modifier(get_map().get_terrain(loc));
			std::stringstream def_text;
			def_text << def << "%";

			SDL_Color color = int_to_color(game_config::red_to_green(def, false));

			// use small font
			int def_font = 16;
			draw_text_in_hex(loc, LAYER_MOVE_INFO, def_text.str(), def_font, color);
		}
	}

	if (!reach_map_.empty()) {
		reach_map::iterator reach = reach_map_.find(loc);
		if (reach != reach_map_.end() && reach->second > 1) {
			const std::string num = lexical_cast<std::string>(reach->second);
			draw_text_in_hex(loc, LAYER_MOVE_INFO, num, 16, font::YELLOW_COLOR);
		}
	}
}

std::vector<surface> game_display::footsteps_images(const map_location& loc)
{
	std::vector<surface> res;

	if (route_.steps.size() < 2) {
		return res; // no real "route"
	}

	std::vector<map_location>::const_iterator i =
	         std::find(route_.steps.begin(),route_.steps.end(),loc);

	if( i == route_.steps.end()) {
		return res; // not on the route
	}

	// Check which footsteps images of game_config we will use
	int move_cost = 1;
	const unit* u = units_.find_unit(route_.steps.front(), true);
	if (u) {
		move_cost = u->movement_cost(get_map().get_terrain(loc));
	}
	int image_number = std::min<int>(move_cost, game_config::foot_speed_prefix.size());
	if (image_number < 1) {
		return res; // Invalid movement cost or no images
	}
	const std::string foot_speed_prefix = game_config::foot_speed_prefix[image_number-1];

	surface teleport = NULL;

	// We draw 2 half-hex (with possibly different directions),
	// but skip the first for the first step.
	const int first_half = (i == route_.steps.begin()) ? 1 : 0;
	// and the second for the last step
	const int second_half = (i+1 == route_.steps.end()) ? 0 : 1;

	for (int h = first_half; h <= second_half; ++h) {
		const std::string sense( h==0 ? "-in" : "-out" );

		if (!tiles_adjacent(*(i+(h-1)), *(i+h))) {
			std::string teleport_image =
			h==0 ? game_config::foot_teleport_enter : game_config::foot_teleport_exit;
			teleport = image::get_image(teleport_image, image::SCALED_TO_HEX);
			continue;
		}

		// In function of the half, use the incoming or outgoing direction
		map_location::DIRECTION dir = (i+(h-1))->get_relative_dir(*(i+h));

		std::string rotate;
		if (dir > map_location::SOUTH_EAST) {
			// No image, take the opposite direction and do a 180 rotation
			dir = i->get_opposite_dir(dir);
			rotate = "~FL(horiz)~FL(vert)";
		}

		const std::string image = foot_speed_prefix
			+ sense + "-" + i->write_direction(dir)
			+ ".png" + rotate;

		res.push_back(image::get_image(image, image::SCALED_TO_HEX));
	}

	// we draw teleport image (if any) in last
	if (teleport != NULL) res.push_back(teleport);

	return res;
}

surface game_display::get_flag(const map_location& loc)
{
	t_translation::t_terrain terrain = get_map().get_terrain(loc);

	size_t side = 0;
	const unit* u_itor = units_.find_unit(loc, true);
	if (u_itor && u_itor->type() == unit_types.find_keep()) {
		side = u_itor->side();
	}
	if (!side && !get_map().is_village(terrain)) {
		return surface(NULL);
	}

	if (!side) {
		side = player_teams::village_owner(loc) + 1;
		const unit* it = units_.find_unit(loc, false);
		if (it) {
			side = it->side();
		}
	}
	if (side && (!fogged(loc) || teams_[currentTeam_].is_enemy(side))) {
		flags_[side - 1].update_last_draw_time();
		const image::locator &image_flag = animate_map_? flags_[side - 1].get_current_frame() : flags_[side - 1].get_first_frame();
		return image::get_image(image_flag, image::TOD_COLORED);
	}

	return surface(NULL);
}

void game_display::pre_change_resolution(std::map<const std::string, bool>& actives)
{
	static const char* items[] = {
		"chat", "card", "undo",	"endturn"
	};
	static int nb_items = sizeof(items) / sizeof(items[0]);

	for (int n = 0; n < nb_items; n ++) {
		gui2::tcontrol* widget = dynamic_cast<gui2::tcontrol*>(get_theme_object(items[n]));
		if (widget) {
			actives.insert(std::make_pair(items[n], widget->get_active()));
		} else {
			actives.insert(std::make_pair(items[n], true));
		}
	}
}

void game_display::post_change_resolution(const std::map<const std::string, bool>& actives)
{
	gui2::tgame_theme& dlg = *dynamic_cast<gui2::tgame_theme*>(theme_);
	troop_list_.bar.set_report(dynamic_cast<gui2::treport*>(dlg.get_object("access-unit")));
	hero_list_.bar.set_report(dynamic_cast<gui2::treport*>(dlg.get_object("access-hero")));
	ctrl_bar_ = dlg.context_menu("ctrl-bar");

	gui2::treport* require_invisible = NULL;
	if (current_list_type_ == taccess_list::TROOP) {
		require_invisible = hero_list_.bar.report();
	} else {
		require_invisible = troop_list_.bar.report();
	}
	if (require_invisible) {
		require_invisible->parent()->set_visible(gui2::twidget::INVISIBLE);
	}

	troop_list_.reset();
	hero_list_.reset();

	if (!teams_.empty() && teams_[activeTeam_].is_human() && !controller_->in_browse()) {
		refresh_access_troops(activeTeam_, REFRESH_RELOAD);
		refresh_access_heros(activeTeam_, REFRESH_RELOAD);
		show_context_menu();
	}
	if (tent::mode == mode_tag::SIEGE && !controller_->is_linger_mode()) {
		if (controller_->in_browse()) {
			if (controller_->pause_when_human()) {
				widget_set_image(game_config::theme_object_id_endturn, "buttons/ctrl-pause2.png");
			} else {
				widget_set_image(game_config::theme_object_id_endturn, "buttons/ctrl-pause.png");
			}
		} else {
			widget_set_image(game_config::theme_object_id_endturn, "buttons/ctrl-play.png");
		}
	}
	refresh_card_button(teams_[currentTeam_], *this);
	widget_set_surface("rpg", generate_rpg_surface(*rpg::h));
	show_context_menu();
}

surface game_display::get_big_flag(const map_location& loc)
{
	if (artifical* city = units_.city_from_loc(loc)) {
/*
		if (moving_dst_loc_.valid() && units_.city_from_loc(moving_dst_loc_) == city) {
			return surface(NULL);
		}
*/
		const map_location& center = city->get_location();
		if (center.get_direction(map_location::SOUTH) == loc) {
			size_t i = city->side() - 1;
			if (!fogged(loc) || !teams_[currentTeam_].is_enemy(i+1)) {
				big_flags_[i].update_last_draw_time();
				const image::locator &image_flag = animate_map_ ?
					big_flags_[i].get_current_frame() : big_flags_[i].get_first_frame();

				std::map<std::string, surface>::iterator itor = big_flags_cache_[i].find(image_flag.get_filename());
				if (itor == big_flags_cache_[i].end()) {
					surface surf = image::get_image(image_flag, image::SCALED_TO_HEX);
					// must use a copy. or result to overlapped!
					surface flag_surf = make_neutral_surface(surf);

					std::string surname = "--";
					if (teams_[i].side() != team::empty_side) {
						surname = teams_[i].leader()->surname();
					}
					size_t chars = utils::utf8str_len(surname);

					surface text_surf;
					surface back_surf;
					int font_size;
					int xstart, ystart;
					if (default_zoom_ == ZOOM_48) {
						if (chars == 1) {
							font_size = 10;
							xstart = 21;
							ystart = 6;
						} else if (chars == 2) {
							font_size = 8;
							xstart = 18;
							ystart = 8;
						} else if (chars == 3) {
							font_size = 8;
							xstart = 18;
							ystart = 8;
						} else {
							font_size = 8;
							xstart = 16;
							ystart = 8;
						}
					} else if (default_zoom_ == ZOOM_56) {
						if (chars == 1) {
							font_size = 12;
							xstart = 24;
							ystart = 7;
						} else if (chars == 2) {
							font_size = 9;
							xstart = 21;
							ystart = 8;
						} else if (chars == 3) {
							font_size = 8;
							xstart = 18;
							ystart = 8;
						} else {
							font_size = 8;
							xstart = 16;
							ystart = 8;
						}
					} else if (default_zoom_ == ZOOM_64) {
						if (chars == 1) {
							font_size = 14;
							xstart = 28;
							ystart = 9;
						} else if (chars == 2) {
							font_size = 10;
							xstart = 24;
							ystart = 10;
						} else if (chars == 3) {
							font_size = 9;
							xstart = 24;
							ystart = 9;
						} else {
							font_size = 9;
							xstart = 16;
							ystart = 8;
						}
					} else {
						if (chars == 1) {
							font_size = font::SIZE_NORMAL;
							xstart = 30;
							ystart = 12;
						} else if (chars == 2) {
							font_size = font::SIZE_SMALL;
							xstart = 26;
							ystart = 11;
						} else if (chars == 3) {
							font_size = font::SIZE_TINY;
							xstart = 28;
							ystart = 12;
						} else {
							font_size = font::SIZE_TINY;
							xstart = 22;
							ystart = 12;
						}
					}
					text_surf = font::get_rendered_text2(surname, -1, font_size, font::BIGMAP_COLOR);
					back_surf = font::get_rendered_text2(surname, -1, font_size, font::BLACK_COLOR);

					SDL_Rect dst;
					for (int dy=-1; dy <= 1; ++dy) {
						for (int dx=-1; dx <= 1; ++dx) {
							if (dx!=0 || dy!=0) {
								dst.x = xstart + dx;
								dst.y = ystart + dy;
								sdl_blit(back_surf, NULL, flag_surf, &dst);
							}
						}
					}
					dst.x = xstart;
					dst.y = ystart;
					sdl_blit(text_surf, NULL, flag_surf, &dst);
					
					big_flags_cache_[i][image_flag.get_filename()] = flag_surf;

					return flag_surf;	
				}
				return itor->second;
			}
		}
	}
	return surface(NULL);
}
	
void game_display::highlight_reach(const pathfind::paths &paths_list)
{
	unhighlight_reach();
	highlight_another_reach(paths_list);
}

void game_display::highlight_another_reach(const pathfind::paths &paths_list)
{
	// Fold endpoints of routes into reachability map.
	BOOST_FOREACH (const pathfind::paths::step &dest, paths_list.destinations) {
		reach_map_[dest.curr]++;
	}
	reach_map_changed_ = true;
}

void game_display::unhighlight_reach()
{
	reach_map_ = reach_map();
	reach_map_changed_ = true;
}

void game_display::process_reachmap_changes()
{
	if (!reach_map_changed_) return;
	if (reach_map_.empty() != reach_map_old_.empty()) {
		// Invalidate everything except the non-darkened tiles
		reach_map &full = reach_map_.empty() ? reach_map_old_ : reach_map_;

		for (reach_map::iterator reach = full.begin(); reach != full.end(); ++reach) {
			if (point_in_rect_of_hexes(reach->first.x, reach->first.y, draw_area_rect_)) {
				// Location needs to be darkened or brightened
				draw_area_val(reach->first.x, reach->first.y) = INVALIDATE;
			}
		}
	} else if (!reach_map_.empty()) {
		// Invalidate only changes
		reach_map::iterator reach, reach_old;
		for (reach = reach_map_.begin(); reach != reach_map_.end(); ++reach) {
			reach_old = reach_map_old_.find(reach->first);
			if (reach_old == reach_map_old_.end()) {
				invalidate(reach->first);
			} else {
				if (reach_old->second != reach->second) {
					invalidate(reach->first);
				}
				reach_map_old_.erase(reach_old);
			}
		}
		for (reach_old = reach_map_old_.begin(); reach_old != reach_map_old_.end(); ++reach_old) {
			invalidate(reach_old->first);
		}
	}
	reach_map_old_ = reach_map_;
	reach_map_changed_ = false;
}

void game_display::process_disctrict_changes()
{
	if (!disctrict_changed_) return;

	invalidate(disctrict_old_);
	invalidate(disctrict_);

	disctrict_old_ = disctrict_;
	disctrict_changed_ = false;
}

void game_display::highlight_disctrict(const artifical& art)
{
	unhighlight_disctrict();
	disctrict_ = art.district_locs();
	reach_map_changed_ = true;
}

void game_display::unhighlight_disctrict()
{
	disctrict_ = std::set<map_location>();
	disctrict_changed_ = true;
}

void game_display::invalidate_route()
{
	for(std::vector<map_location>::const_iterator i = route_.steps.begin();
	    i != route_.steps.end(); ++i) {
		invalidate(*i);
	}
}

void game_display::set_route(const pathfind::marked_route *route)
{
	invalidate_route();

	if(route != NULL) {
		route_ = *route;
	} else {
		route_.steps.clear();
		route_.marks.clear();
	}

	invalidate_route();
}

void game_display::invalidate_animations_location(const map_location& loc) 
{
	if (get_map().is_village(loc)) {
		int owner = player_teams::village_owner(loc);
		const unit* it = units_.find_unit(loc, false);
		if (it) {
			owner = it->side() - 1;
		}
		if (owner >= 0 && flags_[owner].need_update()
		&& (!fogged(loc) || !teams_[currentTeam_].is_enemy(owner+1))) {
			invalidate(loc);
		}
	}
}

void game_display::invalidate_animations()
{
	if (clear_formationed_indicator_.first.valid()) {
		invalidate(clear_formationed_indicator_.first);
	}
	if (intervene_move_indicator_.first.valid()) {
		invalidate(intervene_move_indicator_.first);
	}

	for (std::map<map_location, std::string>::const_iterator it = tip_locs_.begin(); it != tip_locs_.end(); ++ it) {
		invalidate(it->first);
	}

	// new_animation_frame();
	display::invalidate_animations();

	if (terrain_dirty_) {
		get_builder().rebuild_terrain();
		terrain_dirty_ = false;
	}
	for (std::map<int, animation*>::iterator it = area_anims_.begin(); it != area_anims_.end(); ++ it) {
		tanim_type type = it->second->type();
		VALIDATE(type == anim_map || type == anim_canvas, miss_anim_err_str);
		if (type != anim_map) {
			continue;
		}
		it->second->invalidate(screen_.getSurface());
	}
	if (expedite_city_) {
		expedite_city_->refresh();
		expedite_city_->invalidate(expedite_city_->get_location());
	}
	for (size_t i = 0; i < draw_area_unit_size_; i ++) {
		unit* u = dynamic_cast<unit*>(draw_area_unit_[i]);
		u->refresh();
		u->invalidate(u->get_location());
	}
	if (temp_unit_) {
		temp_unit_->refresh();
		temp_unit_->invalidate(temp_unit_->get_location());
	}
}

int& game_display::debug_highlight(const map_location& loc)
{
	assert(game_config::debug);
	return debugHighlights_[loc];
}

void game_display::place_temporary_unit(unit *u)
{
	temp_unit_ = u;
	invalidate(u->get_location());
}

void game_display::remove_temporary_unit()
{
	if(!temp_unit_) return;

	invalidate(temp_unit_->get_location());
	// Redraw with no location to get rid of haloes
	temp_unit_->clear_haloes();
	temp_unit_ = NULL;
}

bool game_display::add_exclusive_draw(const map_location& loc, unit& unit)
{
	if (loc.valid() && exclusive_unit_draw_requests_.find(loc) == exclusive_unit_draw_requests_.end())
	{
		exclusive_unit_draw_requests_[loc] = unit.id();
		return true;
	}
	else
	{
		return false;
	}
}

std::string game_display::remove_exclusive_draw(const map_location& loc)
{
	std::string id = "";
	if(loc.valid())
	{
		id = exclusive_unit_draw_requests_[loc];
		//id will be set to the default "" string by the [] operator if the map doesn't have anything for that loc.
		exclusive_unit_draw_requests_.erase(loc);
	}
	return id;
}

void game_display::place_expedite_city(artifical& city)
{
	expedite_city_ = &city;
	invalidate(city.get_location());
}

void game_display::remove_expedite_city()
{
	if (!expedite_city_) return;

	invalidate(expedite_city_->get_location());
	// Redraw with no location to get rid of haloes
	expedite_city_->clear_haloes();
	expedite_city_ = NULL;
}

std::set<map_location> placable_locs(const std::vector<team>& teams, const unit_map& units, const gamemap& map, unit& u)
{
	std::set<map_location> ret;
	const team& current_team = teams[u.side() - 1];

	std::vector<artifical*> cities;
	for (size_t i = 0; i < teams.size(); i ++) {
		if (!current_team.is_enemy(i + 1)) {
			continue;
		}
		const std::vector<artifical*>& side_cities = teams[i].holden_cities();
		cities.insert(cities.end(), side_cities.begin(), side_cities.end());
	}

	const unit_movement_resetter move_resetter(u);
	pathfind::paths paths(map, units, u.get_location(), teams, false, false, current_team);
	BOOST_FOREACH (const pathfind::paths::step& dest, paths.destinations) {
		const map_location& dst = dest.curr;
		unit* target = units.find_unit(dst);
		if (target && !current_team.is_enemy(target->side()) && !target->base()) {
			continue;
		}
		bool restricted_zone = false;
		for (std::vector<artifical*>::const_iterator it = cities.begin(); it != cities.end(); ++ it) {
			const artifical& city = **it;
			if (distance_between(city.get_location(), dst) < 4) {
				restricted_zone = true;
			}
		}
		if (restricted_zone) {
			continue;
		}
		ret.insert(dst);
	}
	return ret;
}

void game_display::set_attack_indicator(unit* attack, bool browse)
{
	invalidate(attack_indicator_dst_);
	attack_indicator_dst_.clear();
	attack_indicator_each_dst_.clear();
	// alternatable_indicator_ must be included in attack_indicator_dst_.
	alternatable_indicator_.clear();

	invalidate(tactic_indicator_.first);
	tactic_indicator_ = std::make_pair(map_location(), -1);

	invalidate(formation_indicator_.second);
	marker_indicator_dst_.clear();
	formation_indicator_ = std::make_pair(map_location(), map_location());

	invalidate(interior_indicator_);
	interior_indicator_ = map_location();

	invalidate(clear_formationed_indicator_.first);
	clear_formationed_indicator_ = std::make_pair(map_location(), -1);

	invalidate(intervene_move_indicator_.first);
	intervene_move_indicator_ = std::make_pair(map_location(), -1);

	if (!attack) {
		return;
	}

	const team& current_team = teams_[attack->side() - 1];
	std::vector<unit*> actives = current_team.active_tactics();

	if (!tent::tower_mode()) {
		if (!attack->is_artifical() && !attack->attacks_left()) {
			return;
		}
	}

	map_offset* adjacent_ptr;

	const map_location& loc = attack->get_location();
	map_location relative_loc;
	std::set<map_location> each_dst;
	size_t size;
	int range;
	bool find;
	
	if (!attack->is_artifical()) {
		if (browse) {
			return;
		}
		
		int cost;
		if (attack->master().tactic_ != HEROS_NO_TACTIC ||
			(attack->second().valid() && attack->second().tactic_ != HEROS_NO_TACTIC) ||
			(attack->third().valid() && attack->third().tactic_ != HEROS_NO_TACTIC)) {
			cost = 0;
			tactic_indicator_ = std::make_pair(loc, cost);
		}

		if (current_team.allow_intervene) {
			for (size_t adj = 0; adj < attack->adjacent_size_; adj ++) {
				if (adj == 0) {
					cost = attack->calculate_clear_formated_cost();
					if (current_team.gold() < cost) {
						cost *= -1;
					}
					if (cost > 0) {
						clear_formationed_indicator_ = std::make_pair(attack->adjacent_[adj], cost);
					}

				} else if (adj == attack->adjacent_size_ / 2) {
					if (current_team.gold() >= game_config::cost_intervene_move) {
						cost = game_config::cost_intervene_move;
					} else {
						cost = -1 * game_config::cost_intervene_move;
					}
					if (cost >= 0 && placable_locs(teams_, units_, *map_, *attack).empty()) {
						cost *= -1;
					}
					if (cost >= 0) {
						intervene_move_indicator_ = std::make_pair(attack->adjacent_[adj], cost);
					}
				}
			}
			return;
		}

		tformation formation;
		if (!tent::tower_mode()) {
			formation = calculate_attack_formation(teams_, units_, *attack, false);
		}
		if (formation.valid() && !formation.disable_) {
			for (int adj = attack->adjacent_size_ - 1; adj >= 0; adj --) {
				const unit* that = units_.find_unit(attack->adjacent_[adj]);
				if (that && !current_team.is_enemy(that->side()) && !that->base()) {
					// don't use vacant, it will can moveable.
					formation_indicator_ = std::make_pair(attack->get_location(), attack->adjacent_[adj]);
					invalidate(formation_indicator_.second);
					break;
				}
			}
		}

		std::vector<attack_type>& attacks = attack->attacks();
		for (std::vector<attack_type>::const_iterator at_it = attacks.begin(); at_it != attacks.end(); ++ at_it) {
			if (tent::tower_mode()) {
				continue;
			}
			if (at_it->range() == "melee") {
				// range: 1
				range = 1;
			} else if (at_it->range() == "ranged") {
				// range: 2
				range = 2;
			} else {
				// other to range: 3
				range = 3;
			}
			each_dst.clear();

			if (!at_it->attack_weight()) {
				// although cannot use, place null item into.
				attack_indicator_each_dst_.push_back(std::pair<int, std::set<map_location> >(range, each_dst));
				continue;
			}
			
			// 检查此种range在此次是否已遇到过
			find = false;
			for (std::vector<range_locs_pair>::iterator itor = attack_indicator_each_dst_.begin(); itor != attack_indicator_each_dst_.end(); ++ itor) {
				if (itor->first == range) {
					attack_indicator_each_dst_.push_back(*itor);
					find = true;
					break;
				}
			}
			if (find) {
				// This range has been in this indicator.
				continue;
			}
			each_dst.clear();
			if (range == 1) {
				size = (sizeof(adjacent_1) / sizeof(map_offset)) >> 1;
				adjacent_ptr = adjacent_1[loc.x & 0x1];
			} else if (range == 2) {
				size = (sizeof(adjacent_2) / sizeof(map_offset)) >> 1;
				adjacent_ptr = adjacent_2[loc.x & 0x1];
			} else if (range == 3) {
				size = (sizeof(adjacent_3) / sizeof(map_offset)) >> 1;
				adjacent_ptr = adjacent_3[loc.x & 0x1];
			}
			for (size_t i = 0; i < size; i ++) {
				relative_loc.x = loc.x + adjacent_ptr[i].x;
				relative_loc.y = loc.y + adjacent_ptr[i].y;
				if (units_.valid2(relative_loc, true) || units_.valid2(relative_loc, false)) {
					unit* other = units_.find_unit(relative_loc);
					if (!current_team.fogged(relative_loc) && current_team.is_enemy(other->side()) && !other->invisible(relative_loc)) {
						if (formation.valid() && !formation.disable_) {
							const tformation_profile& profile = *formation.profile_;
							if (attack->weapon(profile.arms_type_, profile.arms_range_id_) >= 0 && profile.arms_range_ == range - 1) {
								marker_indicator_dst_.insert(relative_loc);
							}
						}

						if (!tent::turn_based || !(formation.valid() && !formation.disable_)) {
							// attack
							each_dst.insert(relative_loc);
							attack_indicator_dst_.insert(relative_loc);
						}

						const unit& enemy = *other;
						if (enemy.wall() && teams_[currentTeam_].land_enemy_wall_ && attack->land_wall()) {
							alternatable_indicator_.insert(relative_loc);
						}
					}
				}
			}
			attack_indicator_each_dst_.push_back(std::pair<int, std::set<map_location> >(range, each_dst));
		}
		invalidate(attack_indicator_dst_);

	} else if (attack->is_city()) {
		interior_indicator_ = loc;
	}
	invalidate(loc);
}

void game_display::clear_attack_indicator()
{
	set_attack_indicator(NULL);
}

void game_display::set_hero_indicator(const hero& h)
{
	invalidate(placable_indicator_);
	placable_indicator_.clear();
	invalidate(joinable_indicator_);
	joinable_indicator_.clear();
	if (!h.valid()) {
		return;
	}

	const team& current_team = teams_[h.side_];
	const gamemap& map = *map_;
	int gold = current_team.gold();
	int cost_exponent = current_team.cost_exponent();
	int holden_troops = current_team.holden_troops();

	int n;
	map_location adjs[6];

	int border_size = map.border_size();
	int x_start = 3, y_start = 0;
	int x_end = map.w(), y_end = map.h();
	if (tent::mode == mode_tag::LAYOUT) {
		x_start = 0;
		x_end = map.w() / 2;
	}
	for (int y_off = y_start; y_off < y_end; ++ y_off) {
		for (int x_off = x_start; x_off < x_end; ++ x_off) {
			const map_location loc = map_location(x_off, y_off);

			if (tent::mode == mode_tag::LAYOUT) {
				if (loc == game_config::inforce_attacker_loc || loc == game_config::inforce_defender_loc) {
					continue;
				}
			}

			const t_translation::t_terrain terrain = map.get_terrain(loc);
			const unit* u = units_.find_unit(loc);
			if (u && !u->base()) {
				if (!u->is_artifical() && !u->third().valid() && h.side_ + 1 == u->side()) {
					joinable_indicator_.insert(loc);
					invalidate(loc);
				}
			} else if (h.utype_ != HEROS_NO_UTYPE) {
				if (current_team.max_troops > 0 && holden_troops >= current_team.max_troops) {
					continue;
				}

				const unit_type* ut = unit_types.keytype(h.utype_);
				const unit_movement_type& mtype = ut->movement_type();
				if ((tent::mode == mode_tag::LAYOUT || gold >= ut->cost() * cost_exponent / 100) && mtype.movement_cost(map, terrain) != unit_movement_type::UNREACHABLE) {
					get_adjacent_tiles(loc, adjs);
					for (n = 0; n < 6; n ++) {
						const unit* adj = units_.find_unit(adjs[n], true);
						if (adj && adj->is_city() && current_team.is_enemy(adj->side())) {
							break;
						}
					}
					if (n == 6) {
						placable_indicator_.insert(loc);
						invalidate(loc);
					}
				}
			}
		}
	}
}

void game_display::set_placable_indicator(unit& u)
{
	const team& current_team = teams_[u.side() - 1];
	const gamemap& map = *map_;

	if (!current_team.allow_intervene) {
		int n;
		map_location adjs[6];

		int border_size = map.border_size();
		int x_start = 3, y_start = 0;
		int x_end = map.w(), y_end = map.h();
		for (int y_off = y_start; y_off < y_end; ++ y_off) {
			for (int x_off = x_start; x_off < x_end; ++ x_off) {
				
				const map_location loc = map_location(x_off, y_off);
				const t_translation::t_terrain terrain = map.get_terrain(loc);
				const unit* find = units_.find_unit(loc);
				if (!find || find->base()) {
					const unit_type* ut = u.packee_type();
					const unit_movement_type& mtype = ut->movement_type();
					if (mtype.movement_cost(map, terrain) != unit_movement_type::UNREACHABLE) {
						get_adjacent_tiles(loc, adjs);
						for (n = 0; n < 6; n ++) {
							const unit* adj = units_.find_unit(adjs[n], true);
							if (adj && adj->is_city() && current_team.is_enemy(adj->side())) {
								break;
							}
						}
						if (n == 6) {
							placable_indicator_.insert(loc);
							invalidate(loc);
						}
					}
				}
			}
		}
	} else {
		placable_indicator_ = placable_locs(teams_, units_, map, u);
		invalidate(placable_indicator_);
	}
}

void game_display::clear_hero_indicator()
{
	set_hero_indicator(hero_invalid);
}

bool game_display::loc_in_attack_indicator(const map_location& loc) const
{
	return attack_indicator_dst_.find(loc) != attack_indicator_dst_.end();
}

void game_display::set_moving_src_loc(const map_location& loc)
{
	if (moving_src_loc_ != loc) {
		invalidate(moving_src_loc_);
		moving_src_loc_ = loc;
		invalidate(moving_src_loc_);
	}
}

void game_display::clear_moving_src_loc()
{
	set_moving_src_loc(map_location());
}

void game_display::set_moving_dst_loc(const map_location& loc)
{
	if (moving_dst_loc_ != loc) {
		invalidate(moving_dst_loc_);
		moving_dst_loc_ = loc;
		invalidate(moving_dst_loc_);
	}
}

void game_display::clear_moving_dst_loc()
{
	set_moving_dst_loc(map_location());
}

bool game_display::set_ea_build_indicator(const map_location& loc, bool set)
{
	if (set) {
		invalidate(build_indicator_dst_);
		build_indicator_dst_.clear();
	}

	if (units_.find_unit(loc)) {
		return false;
	}

	artifical* city = NULL;
	std::map<const map_location, int>::const_iterator it = unit_map::economy_areas_.find(loc);
	if (it != unit_map::economy_areas_.end()) {
		city = units_.city_from_cityno(it->second);
	}
	if (!city && !tent::tower_mode()) {
		city = loc_can_build_wall(units_, *map_, loc, teams_[activeTeam_]);
	}
	if (!city || city->side() != activeTeam_ + 1) {
		return false;
	}

	if (!actor_can_continue_action(units_, activeTeam_ + 1)) {
		return false;
	}
	if (rpg::stratum == hero_stratum_citizen || (rpg::stratum == hero_stratum_mayor && city->mayor() != rpg::h)) {
		return false;
	}

	if (set) {
		build_indicator_dst_.insert(loc);
		invalidate(build_indicator_dst_);
	}

	return true;
}

void game_display::set_build_indicator(const unit* builder, const artifical* new_art)
{
	invalidate(build_indicator_dst_);
	build_indicator_dst_.clear();
	if (!builder || !builder->attacks_left()) {
		return;
	}

	map_offset* adjacent_ptr;

	const map_location& loc = builder->get_location();
	map_location relative_loc;
	std::set<map_location> dst;
	size_t i, size;
	const team& current_team = teams_[builder->side() - 1];

	if (tent::mode == mode_tag::LAYOUT && new_art->type()->master() != hero::number_wall) {
		for (std::map<const map_location, int>::const_iterator it = unit_map::economy_areas_.begin(); it != unit_map::economy_areas_.end(); ++ it) {
			artifical& city = *units_.city_from_cityno(it->second);
			if (city.side() == current_team.side() && !units_.find_unit(it->first)) {
				build_indicator_dst_.insert(it->first);
			}
		}
		invalidate(build_indicator_dst_);
		return;
	}

	size = (sizeof(adjacent_1) / sizeof(map_offset)) >> 1;
	adjacent_ptr = adjacent_1[loc.x & 0x1];

	for (i = 0; i < size; i ++) {
		relative_loc.x = loc.x + adjacent_ptr[i].x;
		relative_loc.y = loc.y + adjacent_ptr[i].y;
		unit_map::iterator u_itor = units_.find(relative_loc);
		if (!u_itor.valid()) {
			u_itor = units_.find(relative_loc, false);
		}
		if (!u_itor.valid() && new_art->terrain_matches(map_->get_terrain(relative_loc))) {
			if (new_art->type()->master() == hero::number_wall || new_art->type()->master() == hero::number_keep) {
				map_offset* adjacent2_ptr = adjacent_1[relative_loc.x & 0x1];
				size_t i2, size2 = (sizeof(adjacent_1) / sizeof(map_offset)) >> 1;
				for (i2 = 0; i2 < size2; i2 ++) {
					artifical* city = units_.city_from_loc(map_location(relative_loc.x + adjacent2_ptr[i2].x, relative_loc.y + adjacent2_ptr[i2].y));
					if (city) {
						break;
					}
				}
				if (i2 != size2) {
					continue;
				}
				adjacent2_ptr = adjacent_2[relative_loc.x & 0x1];
				size2 = (sizeof(adjacent_2) / sizeof(map_offset)) >> 1;
				for (i2 = 0; i2 < size2; i2 ++) {
					artifical* city = units_.city_from_loc(map_location(relative_loc.x + adjacent2_ptr[i2].x, relative_loc.y + adjacent2_ptr[i2].y));
					if (city && city->side() == new_art->side() && (!new_art->wall() || calculate_keeps(units_, *city) >= 2)) {
						break;
					}
				}
				if (i2 != size2) {
					build_indicator_dst_.insert(relative_loc);
				}

			} else if (new_art->emits_zoc()) {
				map_offset* adjacent2_ptr = adjacent_1[relative_loc.x & 0x1];
				size_t i2, size2 = (sizeof(adjacent_1) / sizeof(map_offset)) >> 1;
				for (i2 = 0; i2 < size2; i2 ++) {
					map_location relative_loc2(relative_loc.x + adjacent2_ptr[i2].x, relative_loc.y + adjacent2_ptr[i2].y);
					unit* itor = units_.find_unit(relative_loc2, true);
					if (itor && itor->emits_zoc() && itor->is_artifical()) {
						break;
					}
				}
				if (i2 == size2) {
					build_indicator_dst_.insert(relative_loc);
				}
			} else if (new_art->type()->master() == hero::number_tactic) {
				std::map<const map_location, int>::const_iterator it = unit_map::economy_areas_.find(relative_loc);
				if (it != unit_map::economy_areas_.end()) {
					artifical& city = *units_.city_from_cityno(it->second);
					if (!city.tactic_on_ea()) {
						build_indicator_dst_.insert(relative_loc);
					}
				}
			} else {
				build_indicator_dst_.insert(relative_loc);
			}
		}
	}
	invalidate(build_indicator_dst_);
}

void game_display::clear_build_indicator()
{
	set_build_indicator(NULL);
}

animation& game_display::insert_pass_scenario_anim(const animation& tpl)
{
	pass_scenario_anim_id_ = insert_area_anim(tpl);
	return area_anim(pass_scenario_anim_id_);
}

void game_display::erase_area_anim(int id)
{
	if (id == pass_scenario_anim_id_) {
		invalidate_all();
	}
	display::erase_area_anim(id);
}

void game_display::set_terrain_dirty()
{
	terrain_dirty_ = true;
}

void game_display::add_overlay(const map_location& loc, const std::string& img, const std::string& halo,const std::string& team_name, bool visible_under_fog)
{
	const int halo_handle = halo::add(get_location_x(loc) + hex_size() / 2,
			get_location_y(loc) + hex_size() / 2, halo, loc);

	const overlay item(img, halo, halo_handle, team_name, visible_under_fog);
	overlays_.insert(overlay_map::value_type(loc,item));
}

void game_display::remove_overlay(const map_location& loc)
{
	typedef overlay_map::const_iterator Itor;
	std::pair<Itor,Itor> itors = overlays_.equal_range(loc);
	while(itors.first != itors.second) {
		halo::remove(itors.first->second.halo_handle);
		++itors.first;
	}

	overlays_.erase(loc);
}

void game_display::remove_single_overlay(const map_location& loc, const std::string& toDelete)
{
	//Iterate through the values with key of loc
	typedef overlay_map::iterator Itor;
	overlay_map::iterator iteratorCopy;
	std::pair<Itor,Itor> itors = overlays_.equal_range(loc);
	while(itors.first != itors.second) {
		//If image or halo of overlay struct matches toDelete, remove the overlay
		if(itors.first->second.image == toDelete || itors.first->second.halo == toDelete) {
			iteratorCopy = itors.first;
			++itors.first;
			halo::remove(iteratorCopy->second.halo_handle);
			overlays_.erase(iteratorCopy);
		}
		else {
			++itors.first;
		}
	}
}

void game_display::parse_team_overlays()
{
	const team& curr_team = teams_[playing_team()];
	const team& prev_team = teams_[playing_team()-1 < teams_.size() ? playing_team()-1 : teams_.size()-1];
	BOOST_FOREACH (const game_display::overlay_map::value_type i, overlays_) {
		const overlay& ov = i.second;
		if (!ov.team_name.empty() &&
			((ov.team_name.find(curr_team.team_name()) + 1) != 0) !=
			((ov.team_name.find(prev_team.team_name()) + 1) != 0))
		{
			invalidate(i.first);
		}
	}
}

std::string game_display::current_team_name() const
{
	if (team_valid())
	{
		return teams_[currentTeam_].team_name();
	}
	return std::string();
}

#ifdef HAVE_LIBDBUS
/** Use KDE 4 notifications. */
static bool kde_style = false;

struct wnotify
{
	uint32_t id;
	std::string owner;
	std::string message;
};

static std::list<wnotify> notifications;

static DBusHandlerResult filter_dbus_signal(DBusConnection *, DBusMessage *buf, void *)
{
	if (!dbus_message_is_signal(buf, "org.freedesktop.Notifications", "NotificationClosed")) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	uint32_t id;
	dbus_message_get_args(buf, NULL,
		DBUS_TYPE_UINT32, &id,
		DBUS_TYPE_INVALID);

	std::list<wnotify>::iterator i = notifications.begin(),
		i_end = notifications.end();
	while (i != i_end && i->id != id) ++i;
	if (i != i_end)
		notifications.erase(i);

	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusConnection *get_dbus_connection()
{
	static bool initted = false;
	static DBusConnection *connection = NULL;
	if (!initted)
	{
		initted = true;
		if (preferences::get("disable_notifications", false)) {
			return NULL;
		}
		if (getenv("KDE_SESSION_VERSION")) {
			// This variable is defined for KDE 4 only.
			kde_style = true;
		}
		DBusError err;
		dbus_error_init(&err);
		connection = dbus_bus_get(DBUS_BUS_SESSION, &err);
		if (!connection) {
			ERR_DP << "Failed to open DBus session: " << err.message << '\n';
			dbus_error_free(&err);
			return NULL;
		}
		dbus_connection_add_filter(connection, filter_dbus_signal, NULL, NULL);
	}
	if (connection) {
		dbus_connection_read_write(connection, 0);
		while (dbus_connection_dispatch(connection) == DBUS_DISPATCH_DATA_REMAINS) {}
	}
	return connection;
}

static uint32_t send_dbus_notification(DBusConnection *connection, uint32_t replaces_id,
	const std::string &owner, const std::string &message)
{
	DBusMessage *buf = dbus_message_new_method_call(
		kde_style ? "org.kde.VisualNotifications" : "org.freedesktop.Notifications",
		kde_style ? "/VisualNotifications" : "/org/freedesktop/Notifications",
		kde_style ? "org.kde.VisualNotifications" : "org.freedesktop.Notifications",
		"Notify");
	const char *app_name = "Battle for Wesnoth";
	dbus_message_append_args(buf,
		DBUS_TYPE_STRING, &app_name,
		DBUS_TYPE_UINT32, &replaces_id,
		DBUS_TYPE_INVALID);
	if (kde_style) {
		const char *event_id = "";
		dbus_message_append_args(buf,
			DBUS_TYPE_STRING, &event_id,
			DBUS_TYPE_INVALID);
	}
	std::string app_icon_ = game_config::path + "/images/wesnoth-icon-small.png";
	const char *app_icon = app_icon_.c_str();
	const char *summary = owner.c_str();
	const char *body = message.c_str();
	const char **actions = NULL;
	dbus_message_append_args(buf,
		DBUS_TYPE_STRING, &app_icon,
		DBUS_TYPE_STRING, &summary,
		DBUS_TYPE_STRING, &body,
		DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &actions, 0,
		DBUS_TYPE_INVALID);
	DBusMessageIter iter, hints;
	dbus_message_iter_init_append(buf, &iter);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &hints);
	dbus_message_iter_close_container(&iter, &hints);
	int expire_timeout = kde_style ? 5000 : -1;
	dbus_message_append_args(buf,
		DBUS_TYPE_INT32, &expire_timeout,
		DBUS_TYPE_INVALID);
	DBusError err;
	dbus_error_init(&err);
	DBusMessage *ret = dbus_connection_send_with_reply_and_block(connection, buf, 1000, &err);
	dbus_message_unref(buf);
	if (!ret) {
		ERR_DP << "Failed to send visual notification: " << err.message << '\n';
		dbus_error_free(&err);
		if (kde_style) {
			ERR_DP << " Retrying with the freedesktop protocol.\n";
			kde_style = false;
			return send_dbus_notification(connection, replaces_id, owner, message);
		}
		return 0;
	}
	uint32_t id;
	dbus_message_get_args(ret, NULL,
		DBUS_TYPE_UINT32, &id,
		DBUS_TYPE_INVALID);
	dbus_message_unref(ret);
	// TODO: remove once closing signals for KDE are handled in filter_dbus_signal.
	if (kde_style) return 0;
	return id;
}
#endif

#if defined(HAVE_LIBDBUS) || defined(HAVE_GROWL)
void game_display::send_notification(const std::string& owner, const std::string& message)
#else
void game_display::send_notification(const std::string& /*owner*/, const std::string& /*message*/)
#endif
{
#if defined(HAVE_LIBDBUS) || defined(HAVE_GROWL)
	Uint8 app_state = SDL_GetAppState();

	// Do not show notifications when the window is visible...
	if ((app_state & SDL_APPACTIVE) != 0)
	{
		// ... and it has a focus.
		if ((app_state & (SDL_APPMOUSEFOCUS | SDL_APPINPUTFOCUS)) != 0) {
			return;
		}
	}
#endif

#ifdef HAVE_LIBDBUS
	DBusConnection *connection = get_dbus_connection();
	if (!connection) return;

	std::list<wnotify>::iterator i = notifications.begin(),
		i_end = notifications.end();
	while (i != i_end && i->owner != owner) ++i;

	if (i != i_end) {
		i->message += "\n";
		i->message += message;
		send_dbus_notification(connection, i->id, owner, i->message);
		return;
	}

	uint32_t id = send_dbus_notification(connection, 0, owner, message);
	if (!id) return;
	wnotify visual;
	visual.id = id;
	visual.owner = owner;
	visual.message = message;
	notifications.push_back(visual);
#endif

#ifdef HAVE_GROWL
	CFStringRef app_name = CFStringCreateWithCString(NULL, "Wesnoth", kCFStringEncodingUTF8);
	CFStringRef cf_owner = CFStringCreateWithCString(NULL, owner.c_str(), kCFStringEncodingUTF8);
	CFStringRef cf_message = CFStringCreateWithCString(NULL, message.c_str(), kCFStringEncodingUTF8);
	//Should be changed as soon as there are more than 2 types of notifications
	CFStringRef cf_note_name = CFStringCreateWithCString(NULL, owner == "Turn changed" ? "Turn changed" : "Chat message", kCFStringEncodingUTF8);

	growl_obj.applicationName = app_name;
	growl_obj.registrationDictionary = NULL;
	growl_obj.applicationIconData = NULL;
	growl_obj.growlIsReady = NULL;
	growl_obj.growlNotificationWasClicked = NULL;
	growl_obj.growlNotificationTimedOut = NULL;

	Growl_SetDelegate(&growl_obj);
	Growl_NotifyWithTitleDescriptionNameIconPriorityStickyClickContext(cf_owner, cf_message, cf_note_name, NULL, NULL, NULL, NULL);

	CFRelease(app_name);
	CFRelease(cf_owner);
	CFRelease(cf_message);
	CFRelease(cf_note_name);
#endif
}

void game_display::set_team(size_t teamindex, bool show_everything)
{
	assert(teamindex < teams_.size());
	currentTeam_ = teamindex;
	if (!show_everything)
	{
		labels().set_team(&teams_[teamindex]);
		viewpoint_ = &teams_[teamindex];
	}
	else
	{
		labels().set_team(0);
		viewpoint_ = NULL;
	}
	labels().recalculate_labels();
}

void game_display::set_playing_team(size_t teamindex)
{
	assert(teamindex < teams_.size());
	activeTeam_ = teamindex;
	invalidate_game_status();
}

void game_display::begin_game()
{
	in_game_ = true;

	invalidate_all();
}

bool game_display::unit_image_location_on(int x, int y)
{
	gui2::twidget* widget = get_theme_object("unit_image");
	const SDL_Rect& area = widget->fix_rect();
	if (!point_in_rect(x, y, area)) {
		return false;
	}
	return true;
}

namespace {
	const int chat_message_border = 5;
	const int chat_message_x = 10;
	const int chat_message_y = 10;
	const SDL_Color chat_message_color = {255,255,255,255};
	const SDL_Color chat_message_bg     = {0,0,0,140};
}

void game_display::add_chat_message(const time_t& time, const std::string& speaker,
		int side, const std::string& message, events::chat_handler::MESSAGE_TYPE type,
		bool bell)
{
	const bool whisper = speaker.find("whisper: ") == 0;
	std::string sender = speaker;
	if (whisper) {
		sender.assign(speaker, 9, speaker.size());
	}
	if (!preferences::parse_should_show_lobby_join(sender, message)) return;
	if (preferences::is_ignored(sender)) return;

	preferences::parse_admin_authentication(sender, message);

	if (bell) {
		if ((type == events::chat_handler::MESSAGE_PRIVATE && (!is_observer() || whisper))
			|| utils::word_match(message, preferences::login())) {
			sound::play_UI_sound(game_config::sounds::receive_message_highlight);
		} else if (preferences::is_friend(sender)) {
			sound::play_UI_sound(game_config::sounds::receive_message_friend);
		} else if (sender == "server") {
			sound::play_UI_sound(game_config::sounds::receive_message_server);
		} else {
			sound::play_UI_sound(game_config::sounds::receive_message);
		}
	}

	bool action = false;

	std::string msg;

	if (message.find("/me ") == 0) {
		msg.assign(message, 4, message.size());
		action = true;
	} else {
		msg += message;
	}

	try {
		// We've had a joker who send an invalid utf-8 message to crash clients
		// so now catch the exception and ignore the message.
		msg = font::word_wrap_text(msg,font::SIZE_SMALL,map_outside_area().w*3/4);
	} catch (utils::invalid_utf8_exception&) {
		ERR_NG << "Invalid utf-8 found, chat message is ignored.\n";
		return;
	}

	int ypos = chat_message_x;
	for(std::vector<chat_message>::const_iterator m = chat_messages_.begin(); m != chat_messages_.end(); ++m) {
		ypos += std::max(font::get_floating_label_rect(m->handle).h,
			font::get_floating_label_rect(m->speaker_handle).h);
	}
	SDL_Color speaker_color = {255,255,255,255};
	if(side >= 1) {
		speaker_color = int_to_color(team::get_side_color_range(side).mid());
	}

	SDL_Color message_color = chat_message_color;
	std::stringstream str;
	std::stringstream message_str;

	if(type ==  events::chat_handler::MESSAGE_PUBLIC) {
		if(action) {
			str << "\\<" << speaker << " " << msg << ">";
			message_color = speaker_color;
			message_str << " ";
		} else {
			if (!speaker.empty())
				str << "\\<" << speaker << ">";
			message_str << msg;
		}
	} else {
		if(action) {
			str << "*" << speaker << " " << msg << "*";
			message_color = speaker_color;
			message_str << " ";
		} else {
			if (!speaker.empty())
				str << "*" << speaker << "*";
			message_str << msg;
		}
	}

	// Prepend message with timestamp.
	std::stringstream message_complete;
	message_complete << preferences::get_chat_timestamp(time) << str.str();

	const SDL_Rect rect = map_outside_area();

	font::floating_label spk_flabel(message_complete.str());
	spk_flabel.set_font_size(font::SIZE_SMALL);
	spk_flabel.set_color(speaker_color);
	spk_flabel.set_position(rect.x + chat_message_x, rect.y + ypos);
	spk_flabel.set_clip_rect(rect);
	spk_flabel.set_bg_color(chat_message_bg);
	spk_flabel.set_border_size(chat_message_border);
	spk_flabel.use_markup(false);
	spk_flabel.set_alignment(font::LEFT_ALIGN);

	int speaker_handle = font::add_floating_label(spk_flabel);

	font::floating_label msg_flabel(message_str.str());
	msg_flabel.set_font_size(font::SIZE_SMALL);
	msg_flabel.set_color(message_color);
	msg_flabel.set_position(rect.x + chat_message_x + font::get_floating_label_rect(speaker_handle).w,
	rect.y + ypos);
	msg_flabel.set_clip_rect(rect);
	msg_flabel.set_bg_color(chat_message_bg);
	msg_flabel.set_border_size(chat_message_border);
	msg_flabel.use_markup(false);
	msg_flabel.set_alignment(font::LEFT_ALIGN);

	int message_handle = font::add_floating_label(msg_flabel);

	// Send system notification if appropriate.
	send_notification(speaker, message);

	chat_messages_.push_back(chat_message(speaker_handle,message_handle));

	prune_chat_messages();
}

void game_display::prune_chat_messages(bool remove_all)
{
	unsigned message_ttl = remove_all ? 0 : 1200000;
	unsigned max_chat_messages = preferences::chat_lines();
	int movement = 0;

	while (!chat_messages_.empty() &&
	       (chat_messages_.front().created_at + message_ttl < SDL_GetTicks() ||
	        chat_messages_.size() > max_chat_messages))
	{
		const chat_message &old = chat_messages_.front();
		movement += font::get_floating_label_rect(old.handle).h;
		font::remove_floating_label(old.speaker_handle);
		font::remove_floating_label(old.handle);
		chat_messages_.erase(chat_messages_.begin());
	}

	BOOST_FOREACH (const chat_message &cm, chat_messages_) {
		font::move_floating_label(cm.speaker_handle, 0, - movement);
		font::move_floating_label(cm.handle, 0, - movement);
	}
}

// game_display *game_display::singleton_ = NULL;

void set_zoom_to_default(int zoom)
{
	display::default_zoom_ = zoom;
	image::set_zoom(display::default_zoom_);
	unit_map::set_zoom();	
}