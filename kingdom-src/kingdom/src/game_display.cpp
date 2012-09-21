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

#include "foreach.hpp"
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

#ifdef ANDROID
#include <android/log.h>
#endif

static lg::log_domain log_display("display");
#define ERR_DP LOG_STREAM(err, log_display)
#define LOG_DP LOG_STREAM(info, log_display)

static lg::log_domain log_engine("engine");
#define ERR_NG LOG_STREAM(err, log_engine)

std::map<map_location,fixed_t> game_display::debugHighlights_;

game_display::game_display(unit_map& units, CVideo& video, const gamemap& map,
		const tod_manager& tod, const std::vector<team>& t,
		const config& theme_cfg, const config& level) :
		display(video, &map, theme_cfg, level),
		units_(units),
		temp_units_(),
		exclusive_unit_draw_requests_(),
		attack_indicator_src_(),
		attack_indicator_dst_(),
		energy_bar_rects_(),
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
		screen_anim_(NULL),
		terrain_dirty_(true),
		disctrict_(),
		disctrict_old_(),
		disctrict_changed_(true),
		draw_area_unit_(NULL),
		draw_area_unit_size_(0),
		access_unit_side_(-1),
		moving_src_loc_(),
		moving_dst_loc_()
{
	singleton_ = this;

	// Inits the flag list and the team colors used by ~TC
	// std::vector<std::string> side_colors;
	std::vector<std::string>& side_colors = image::get_team_colors();
	side_colors.clear();

	for (size_t i = 0; i != teams_.size(); ++i) {
		add_flag(i, side_colors);
/*
		std::string side_color = team::get_side_color_index(i+1);
		side_colors.push_back(side_color);
		std::string flag = teams_[i].flag();
		std::string old_rgb = game_config::flag_rgb;
		std::string new_rgb = side_color;

		if(flag.empty()) {
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
*/
	}
	// image::set_team_colors(&side_colors);

	// draw nodes for display::draw
	if (map_->w() && map_->h()) {
		draw_area_unit_ = (unit**)malloc(map_->w() * map_->h() * sizeof(unit*));
		pathfind::reallocate_pq(map_->w(), map_->h());
	}
}

game_display* game_display::create_dummy_display(CVideo& video)
{
	static unit_map dummy_umap;
	static config dummy_cfg;
	static gamemap dummy_map(dummy_cfg, "");
	static tod_manager dummy_tod(dummy_cfg, 0);
	static std::vector<team> dummy_teams;
	return new game_display(dummy_umap, video, dummy_map, dummy_tod,
			dummy_teams, dummy_cfg, dummy_cfg);
}

game_display::~game_display()
{
	if (draw_area_unit_) {
		free(draw_area_unit_);
		draw_area_unit_ = NULL;
		draw_area_unit_size_ = 0;
	}
	// SDL_FreeSurface(minimap_);
	prune_chat_messages(true);
	singleton_ = NULL;

#ifdef ANDROID
	__android_log_print(ANDROID_LOG_INFO, "SDL", "game_dispaly::~game_display");
#endif
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
			if (units_.count(selectedHex_)) {
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

// refresh field troop buttons
// @side: base 0
void game_display::refresh_access_troops(int side, refresh_reason reason, void* , unit* troop)
{
	if (!buttons_ctx_ || access_troops_index_ == -1) {
		return;
	}

	size_t i, unit_button_count, dirty_btn_idx;
	gui::button* btn;
	gui::button** tmp;
	theme::menu* access_unit_menu = buttons_ctx_[access_troops_index_].menu_;

	const SDL_Rect& loc = access_unit_menu->location(screen_area());
	// SDL_Rect clip = {36, 30, 48, 60};

	if (reason == REFRESH_RELOAD) {
		// destroy all existed button
		if (buttons_ctx_[access_troops_index_].buttons_) {
			posix_print_mb("game_display::refresh_access_tropps, program error");
		}
		
		// restruct all button
		buttons_ctx_[access_troops_index_].buttons_ = access_troops_buttons_;

		// previous arrow
		btn = buttons_ctx_[access_troops_index_].buttons_[0] = 
			new gui::button(screen_, "", gui::button::TYPE_PRESS, "buttons/arrow_left.png", gui::button::DEFAULT_SPACE, true, this, access_unit_menu, 0, NULL, loc.w, loc.h);
		// next arrow
		btn = buttons_ctx_[access_troops_index_].buttons_[1] = 
			new gui::button(screen_, "", gui::button::TYPE_PRESS, "buttons/arrow_right.png", gui::button::DEFAULT_SPACE, true, this, access_unit_menu, 1, NULL, loc.w, loc.h);

		i = 2;
		actable_troop_count_ = 0;
		tmp = buttons_ctx_[access_troops_index_].buttons_;
		const std::vector<artifical*>& holded_cities = teams_[side].holded_cities();
		for (std::vector<artifical*>::const_iterator itor1 = holded_cities.begin(); itor1 != holded_cities.end(); ++ itor1) {
			const std::vector<unit*>& troops = (*itor1)->field_troops();
			for (std::vector<unit*>::const_iterator itor2 = troops.begin(); itor2 != troops.end(); ++ itor2) {
				const unit* u = *itor2;
				if (!u->human()) {
					continue;
				}
				if (!unit_can_move(*u)) {
					// 部队不可移动, 直接追加在末尾
					btn = tmp[i] = new gui::button(screen_, "", gui::button::TYPE_PRESS, u->master().image(), gui::button::DEFAULT_SPACE, true, this, access_unit_menu, i, NULL, loc.w, loc.h);
					btn->cookie_ = const_cast<unit*>(u);
					btn->set_image(u->master().image(), u->level(), true);
				} else {
					// 部队可移动, 追加可移动末尾
					if (actable_troop_count_ < i - 2) {
						memcpy(&(tmp[actable_troop_count_ + 3]), &(tmp[actable_troop_count_ + 2]), (i - 2 - actable_troop_count_) * sizeof(gui::button*));
					}
					btn = tmp[actable_troop_count_ + 2] = new gui::button(screen_, "", gui::button::TYPE_PRESS, (*itor2)->master().image(), gui::button::DEFAULT_SPACE, true, this, access_unit_menu, actable_troop_count_ + 2, NULL, loc.w, loc.h);
					btn->cookie_ = const_cast<unit*>(u);
					btn->set_image(u->master().image(), u->level());
					actable_troop_count_ ++;
				}
				i ++;
			}
		}
		buttons_ctx_[access_troops_index_].button_count_ = i;
		// actable_troop_count_ = i - 2;

		// 一次性修正btnidx参数
		for (i = actable_troop_count_ + 2; i < buttons_ctx_[access_troops_index_].button_count_; i ++) {
			tmp[i]->btnidx_ = i;
		}

		// reset start_group_
		start_group_ = 0;

		redraw_access_unit();
		access_unit_side_ = side;

	} else if (buttons_ctx_[access_troops_index_].buttons_ && (access_unit_side_ == side) && (reason == REFRESH_INSERT)) {
		// 处理
		hide_access_unit();
		tmp = buttons_ctx_[access_troops_index_].buttons_;
		unit_button_count = buttons_ctx_[access_troops_index_].button_count_ - 2;
		if (unit_button_count && (actable_troop_count_ != unit_button_count)) {
			memcpy(&(tmp[actable_troop_count_ + 3]), &(tmp[actable_troop_count_ + 2]), (unit_button_count - actable_troop_count_) * sizeof(gui::button*));
		}
		buttons_ctx_[access_troops_index_].button_count_ ++;
		for (i = actable_troop_count_ + 3; i < buttons_ctx_[access_troops_index_].button_count_; i ++) {
			tmp[i]->btnidx_ = i;
		}
		// 新按钮就放在actable_troop_count_ + 2处
		btn = buttons_ctx_[access_troops_index_].buttons_[actable_troop_count_ + 2] = 
			new gui::button(screen_, "", gui::button::TYPE_PRESS, troop->master().image(), gui::button::DEFAULT_SPACE, true, this, access_unit_menu, actable_troop_count_ + 2, NULL, loc.w, loc.h);
		btn->cookie_ = troop;
		if (!unit_can_move(*troop)) {
			// 1. 城市被攻占, 城外部队可能并入了本阵营, 这时那些部队应该被灰色
			btn->set_image(troop->master().image(), troop->level(), true);
		} else {
			btn->set_image(troop->master().image(), troop->level());
			actable_troop_count_ ++;
		}
		redraw_access_unit();

	} else if (buttons_ctx_[access_troops_index_].buttons_ && (access_unit_side_ == side) && (reason == REFRESH_ERASE)) {
		// 根据cookie进行搜索
		tmp = buttons_ctx_[access_troops_index_].buttons_;
		// 在此处,unit_button_count指示单位按钮个数(包括前端两上箭头)-1, 它用于加快for中的那个if判断
		unit_button_count = buttons_ctx_[access_troops_index_].button_count_ - 1;
		// 按钮不可能是0, dirty_btn_idx置0指示无效
		for (dirty_btn_idx = 0, i = 2; i < buttons_ctx_[access_troops_index_].button_count_; i ++) {
			if (dirty_btn_idx && (i < unit_button_count)) {
				buttons_ctx_[access_troops_index_].buttons_[i] = buttons_ctx_[access_troops_index_].buttons_[i + 1];
				buttons_ctx_[access_troops_index_].buttons_[i]->btnidx_ --;
			} else if (tmp[i]->cookie_ == troop) {
				// 检查到有按钮要被erase,先进行隐藏
				hide_access_unit();

				dirty_btn_idx = i;
				// 测试证明, delete不能清除已画在窗口中的图面
				buttons_ctx_[access_troops_index_].buttons_[i]->hide(true);
				delete buttons_ctx_[access_troops_index_].buttons_[i];
				// 记住, 别忘这一处
				// 同样要判断如果是最后一个按钮, 不要执行以下这两个赋值, [i + 1]这个按钮就是不存的
				if (i < unit_button_count) {
					buttons_ctx_[access_troops_index_].buttons_[i] = buttons_ctx_[access_troops_index_].buttons_[i + 1];
					buttons_ctx_[access_troops_index_].buttons_[i]->btnidx_ --;
				}
			}
		}
		if (!dirty_btn_idx) {
			// 没有搜索到匹配项
			return;
		}
		if (dirty_btn_idx < actable_troop_count_ + 2) {
			actable_troop_count_ --;
		}
		buttons_ctx_[access_troops_index_].button_count_ --;
		redraw_access_unit();

	} else if (buttons_ctx_[access_troops_index_].buttons_ && (access_unit_side_ == side) && (reason == REFRESH_DISABLE)) {
		// 根据cookie进行搜索
		tmp = buttons_ctx_[access_troops_index_].buttons_;
		// 在此处,unit_button_count指示单位按钮个数(包括前端两上箭头)-1, 它用于加快for中的那个if判断
		unit_button_count = buttons_ctx_[access_troops_index_].button_count_ - 1;
		// 按钮不可能是0, dirty_btn_idx置0指示无效
		for (dirty_btn_idx = 0, i = 2; i < buttons_ctx_[access_troops_index_].button_count_; i ++) {
			if (dirty_btn_idx && (i < unit_button_count)) {
				buttons_ctx_[access_troops_index_].buttons_[i] = buttons_ctx_[access_troops_index_].buttons_[i + 1];
				buttons_ctx_[access_troops_index_].buttons_[i]->btnidx_ --;
			} else if (!dirty_btn_idx && tmp[i]->cookie_ == troop) {
				dirty_btn_idx = i;
				if (dirty_btn_idx >= actable_troop_count_ + 2) {
					return;
				}
				// 检查到有按钮要被erase,先进行隐藏
				hide_access_unit();

				// 测试证明, delete不能清除已画在窗口中的图面
				btn = buttons_ctx_[access_troops_index_].buttons_[i];
				btn->hide(true);
				
				// 记住, 别忘这一处
				// 同样要判断如果是最后一个按钮, 不要执行以下这两个赋值, [i + 1]这个按钮就是不存的
				if (i < unit_button_count) {
					buttons_ctx_[access_troops_index_].buttons_[i] = buttons_ctx_[access_troops_index_].buttons_[i + 1];
					buttons_ctx_[access_troops_index_].buttons_[i]->btnidx_ --;
				}
			}
		}
		if (!dirty_btn_idx) {
			// 没有搜索到匹配项
			return;
		}
		if (dirty_btn_idx < actable_troop_count_ + 2) {
			actable_troop_count_ --;
		}
		// 修改图像参数, 向未尾增加该图像
		btn->set_image(troop->master().image(), troop->level(), true);
		btn->btnidx_ = unit_button_count;
		buttons_ctx_[access_troops_index_].buttons_[unit_button_count] = btn;

		redraw_access_unit();

	} else if (buttons_ctx_[access_troops_index_].buttons_ && (access_unit_side_ == side) && (reason == REFRESH_ENABLE)) {
		// 根据cookie进行搜索
		tmp = buttons_ctx_[access_troops_index_].buttons_;
		// 在此处,unit_button_count指示单位按钮个数(包括前端两上箭头)-1, 它用于加快for中的那个if判断
		unit_button_count = buttons_ctx_[access_troops_index_].button_count_ - 1;
		// 按钮不可能是0, dirty_btn_idx置0指示无效
		for (dirty_btn_idx = 0, i = 2; i < buttons_ctx_[access_troops_index_].button_count_; i ++) {
			if (tmp[i]->cookie_ == troop) {
				dirty_btn_idx = i;
				if (dirty_btn_idx < actable_troop_count_ + 2) {
					return;
				}
				// 检查到有按钮要被erase,先进行隐藏
				hide_access_unit();

				// 测试证明, delete不能清除已画在窗口中的图面
				btn = buttons_ctx_[access_troops_index_].buttons_[i];
				btn->hide(true);
				break;
			}
		}
		if (!dirty_btn_idx) {
			// 没有搜索到匹配项
			return;
		}
		// 重新循环, 把dirty_btn_idx之前的按钮后移一个位置
		// 在此处,unit_button_count指示单位按钮个数(包括前端两上箭头)-1, 它用于加快for中的那个if判断
		// unit_button_count = std::min(dirty_btn_idx, buttons_ctx_[access_troops_index_].button_count_ - 2);
		for (i = dirty_btn_idx - 1; i >= 2; i --) {
			buttons_ctx_[access_troops_index_].buttons_[i + 1] = buttons_ctx_[access_troops_index_].buttons_[i];
			buttons_ctx_[access_troops_index_].buttons_[i + 1]->btnidx_ ++;
		}

		// 可行动按钮数增1
		actable_troop_count_ ++;

		// 修改图像参数, 向头部增加该图像
		btn->set_image(troop->master().image(), troop->level());
		btn->btnidx_ = 2;
		buttons_ctx_[access_troops_index_].buttons_[2] = btn;

		redraw_access_unit();

	} else if (buttons_ctx_[access_troops_index_].buttons_ && (reason == REFRESH_CLEAR)) {
		hide_access_unit();
		for (i = 0; i < buttons_ctx_[access_troops_index_].button_count_; i ++) {
			delete buttons_ctx_[access_troops_index_].buttons_[i];
		}
		buttons_ctx_[access_troops_index_].buttons_ = NULL;
		buttons_ctx_[access_troops_index_].button_count_ = 0; // 这个还是要置的, display清除上下文菜单时要根据它来判断
		access_unit_side_ = -1;
	}
}

// redraw access_unit menu. call refresh_access_troops before it.
// must valid parameter: start_group_
// 根据当前按钮数组和要显示组号，在快捷访问部队列表中显示该组中所有部队。它和hide_access_unit功能是相反。
// 注：start_groups_值必须被指定，但如果start_groups_是一个过大非法值（理论上不该出现），会被这函数修改。
void game_display::redraw_access_unit()
{
	size_t i, need_disp_units, can_disp_units, stop_disp_unit_plus1, groups;
	theme::menu* access_unit_menu = buttons_ctx_[access_troops_index_].menu_;
	gui::button* btn;

	const SDL_Rect& loc = access_unit_menu->location(screen_area());
	const SDL_Rect& mid_panel_loc = theme_.mid_panel_location(screen_area());

	// first tow button are previous and next button
	if (buttons_ctx_[access_troops_index_].button_count_ > 2) {
		need_disp_units = buttons_ctx_[access_troops_index_].button_count_ - 2;
	} else {
		// no unit need be displayed
		return;
	}
	if (mid_panel_loc.w / loc.w > 2) {
		can_disp_units = mid_panel_loc.w / loc.w - 2;
	} else {
		// width configuration about bottom-middle-panel is too short
		return;
	}
	// groups指示的是显示所有单位需要多少组, 以下计算时应该用buttons_ctx_[access_troops_index_].button_count_ - 2而不是buttons_ctx_[access_troops_index_].button_count_
	groups = ((buttons_ctx_[access_troops_index_].button_count_ - 2) + can_disp_units - 1) / can_disp_units;
	if (start_group_ >= groups) {
		// correct start_group_
		start_group_ = groups - 1;
	}
	// preview arrow
	btn = buttons_ctx_[access_troops_index_].buttons_[0];
	if (start_group_) {
		btn = buttons_ctx_[access_troops_index_].buttons_[0];
		btn->set_location(loc.x, loc.y);
		btn->hide(false);
	}

	// need_disp_units, can_disp_units和stop_disp_unit_plus1它不下处理上/下一组两个按钮. 像
	// need_disp_units: 指的就是要被显示的单位数
	// can_disp_units: 一次最多可显示的单位数
	// stop_disp_unit_plus1: 此次要显到的终于单位索引值+1, 这个索引值指的是在need_disp_units这个范围内索引值
	if (need_disp_units > (start_group_ + 1) * can_disp_units) {
		stop_disp_unit_plus1 = (start_group_ + 1) * can_disp_units;
	} else {
		stop_disp_unit_plus1 = need_disp_units;
	}
	// main units
	for (i = start_group_ * can_disp_units; i < stop_disp_unit_plus1; i ++) {
		btn = buttons_ctx_[access_troops_index_].buttons_[i + 2];
		btn->set_location(loc.x + (1 + i - start_group_ * can_disp_units) * loc.w, loc.y);
		btn->hide(false);
	}
	// next arrow
	btn = buttons_ctx_[access_troops_index_].buttons_[1];
	if (need_disp_units > stop_disp_unit_plus1) {
		btn->set_location(loc.x + (1 + can_disp_units) * loc.w, loc.y);
		btn->hide(false);
	}
}

// must valid parameter: start_group_
void game_display::hide_access_unit()
{
	size_t i, need_disp_units, can_disp_units, stop_disp_unit_plus1;
	theme::menu* access_unit_menu = buttons_ctx_[access_troops_index_].menu_;
	gui::button* btn;

	const SDL_Rect& loc = access_unit_menu->location(screen_area());
	const SDL_Rect& mid_panel_loc = theme_.mid_panel_location(screen_area());

	// first tow button are previous and next button
	if (buttons_ctx_[access_troops_index_].button_count_ > 2) {
		need_disp_units = buttons_ctx_[access_troops_index_].button_count_ - 2;
	} else {
		// no unit need be displayed
		return;
	}
	if (mid_panel_loc.w / loc.w > 2) {
		can_disp_units = mid_panel_loc.w / loc.w - 2;
	} else {
		// width configuration about bottom-middle-panel is too short
		return;
	}
	// preview arrow
	btn = buttons_ctx_[access_troops_index_].buttons_[0];
	if (start_group_) {
		btn->hide(true);
	}

	// need_disp_units, can_disp_units和stop_disp_unit_plus1它不下处理上/下一组两个按钮. 像
	// need_disp_units: 指的就是要被显示的单位数
	// can_disp_units: 一次最多可显示的单位数
	// stop_disp_unit_plus1: 此次要显到的终于单位索引值+1, 这个索引值指的是在need_disp_units这个范围内索引值
	if (need_disp_units > (start_group_ + 1) * can_disp_units) {
		stop_disp_unit_plus1 = (start_group_ + 1) * can_disp_units;
	} else {
		stop_disp_unit_plus1 = need_disp_units;
	}
	// main units
	for (i = start_group_ * can_disp_units; i < stop_disp_unit_plus1; i ++) {
		btn = buttons_ctx_[access_troops_index_].buttons_[i + 2];
		btn->hide(true);
	}
	// next arrow
	btn = buttons_ctx_[access_troops_index_].buttons_[1];
	if (need_disp_units > stop_disp_unit_plus1) {
		btn->hide(true);
	}
}

map_location game_display::access_unit_press(size_t btnidx)
{
	gui::button* btn;
	unit* troop;
	map_location loc;

	if (btnidx == 0) {
		hide_access_unit();
		-- start_group_;
		redraw_access_unit();
	} else if (btnidx == 1) {
		hide_access_unit();
		start_group_ ++;
		redraw_access_unit();
	} else {
		btn = buttons_ctx_[access_troops_index_].buttons_[btnidx];
		troop = static_cast<unit*>(btn->cookie_);
		// scroll_to_tile(troop->get_location(), WARP);
		// events::mouse_handler::get_singleton()->select_hex(troop->get_location(), 0);
		loc = troop->get_location();
	}
	return loc;
}

void game_display::hide_context_menu(const theme::menu* m, bool hide, uint32_t flags, uint32_t disable)
{
	const theme::menu* m_adjusted = m;

	if (!m) {
		m_adjusted = theme_.context_menu("");
	}
	if (!m_adjusted) {
		return;
	}
	display::hide_context_menu(m_adjusted, hide, flags, disable);
}

void game_display::scroll_to_leader(unit_map& units, int side, SCROLL_TYPE scroll_type,bool force)
{
	unit_map::const_iterator leader = units.find_leader(side);

	if(leader != units_.end()) {
		// YogiHH: I can't see why we need another key_handler here,
		// therefore I will comment it out :
		/*
		const hotkey::basic_handler key_events_handler(gui_);
		*/
		scroll_to_tile(leader->get_location(), scroll_type, true, force);
	}
}

void game_display::pre_draw(rect_of_hexes& hexes) 
{
	// 形成大地图窗口内所有单位列表
	if (resources::units) {
		draw_area_unit_size_ = units_.units_from_rect(draw_area_unit_, draw_area_rect_);
	} else {
		draw_area_unit_size_ = 0;
	}

	process_reachmap_changes();
	process_disctrict_changes();
	/**
	 * @todo FIXME: must modify changed, but best to do it at the
	 * floating_label level
	 */
	prune_chat_messages();
}

image::TYPE game_display::get_image_type(const map_location& loc) {
	// We highlight hex under the mouse, or under a selected unit.
	if (get_map().on_board(loc)) {
		if (loc == mouseoverHex_ || loc == attack_indicator_src_) {
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
		const map_location& loc = draw_area_unit_[i]->get_location();

		loc_n = loc.get_direction(map_location::NORTH);
		if (point_in_rect_of_hexes(loc_n.x, loc_n.y, draw_area_rect_)) {
			draw_area_[(loc_n.y + 1) * draw_area_pitch_ + (loc_n.x + 1)] = INVALIDATE;
		}
		loc_ne = loc.get_direction(map_location::NORTH_EAST);
		if (point_in_rect_of_hexes(loc_ne.x, loc_ne.y, draw_area_rect_)) {
			draw_area_[(loc_ne.y + 1) * draw_area_pitch_ + (loc_ne.x + 1)] = INVALIDATE;
		}

		draw_area_[(loc.y + 1) * draw_area_pitch_ + (loc.x + 1)] = INVALIDATE_UNIT;
		unit_invals.push_back(loc);
	}

	if (temp_unit_) {
		const map_location& loc = temp_unit_->get_location();

		loc_n = loc.get_direction(map_location::NORTH);
		if (point_in_rect_of_hexes(loc_n.x, loc_n.y, draw_area_rect_)) {
			draw_area_[(loc_n.y + 1) * draw_area_pitch_ + (loc_n.x + 1)] = INVALIDATE;
		}
		loc_ne = loc.get_direction(map_location::NORTH_EAST);
		if (point_in_rect_of_hexes(loc_ne.x, loc_ne.y, draw_area_rect_)) {
			draw_area_[(loc_ne.y + 1) * draw_area_pitch_ + (loc_ne.x + 1)] = INVALIDATE;
		}

		if (draw_area_[(loc.y + 1) * draw_area_pitch_ + (loc.x + 1)] != INVALIDATE_UNIT) {
			draw_area_[(loc.y + 1) * draw_area_pitch_ + (loc.x + 1)] = INVALIDATE_UNIT;
			unit_invals.push_back(loc);
		}
	}
	if (expedite_city_ && point_in_rect_of_hexes(expedite_city_->get_location().x, expedite_city_->get_location().y, draw_area_rect_)) {
		const std::set<map_location>& touch_locs = expedite_city_->get_touch_locations();
		for (std::set<map_location>::const_iterator itor = touch_locs.begin(); itor != touch_locs.end(); ++ itor) {
			draw_area_[(itor->y + 1) * draw_area_pitch_ + (itor->x + 1)] = INVALIDATE_UNIT;
		}

		const map_location& loc = expedite_city_->get_location();
		if (draw_area_[(loc.y + 1) * draw_area_pitch_ + (loc.x + 1)] != INVALIDATE_UNIT) {
			draw_area_[(loc.y + 1) * draw_area_pitch_ + (loc.x + 1)] = INVALIDATE_UNIT;
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

void game_display::draw_hex(const map_location& loc)
{
	const bool on_map = get_map().on_board(loc);
	const bool is_shrouded = shrouded(loc);
	const bool is_fogged = fogged(loc);
	const int xpos = get_location_x(loc);
	const int ypos = get_location_y(loc);

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

	// Draw reach_map information.
	// We remove the reachability mask of the unit
	// that we want to attack.
	if (!is_shrouded && !reach_map_.empty() && reach_map_.find(loc) != reach_map_.end()) {
		// drawing_buffer_add(LAYER_REACHMAP, loc, xpos, ypos, image::get_image(game_config::images::unreachable,image::SCALED_TO_HEX));
		drawing_buffer_add(LAYER_UNIT_MOVE_DEFAULT, loc, xpos, ypos, image::get_image(game_config::images::unreachable, image::SCALED_TO_HEX));
	}

	if (!disctrict_.empty() && disctrict_.find(loc) != disctrict_.end()) {
		drawing_buffer_add(LAYER_MOVE_INFO/*LAYER_REACHMAP*/, loc, xpos, ypos, image::get_image("terrain/disctrict.png", image::SCALED_TO_HEX));
	}

	// Footsteps indicating a movement path
	const std::vector<surface>& footstepImages = footsteps_images(loc);
	if (footstepImages.size() != 0) {
		drawing_buffer_add(LAYER_FOOTSTEPS, loc, xpos, ypos, footstepImages);
	}

	// Draw the attack direction indicator
	if (on_map && std::find(attack_indicator_dst_.begin(), attack_indicator_dst_.end(), loc) != attack_indicator_dst_.end()) {
		drawing_buffer_add(LAYER_ATTACK_INDICATOR, loc, xpos, ypos, image::get_image("misc/attack-indicator-dst.png", image::SCALED_TO_HEX));
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
	if(game_mode_ != RUNNING) {
		static const image::locator linger(game_config::images::linger);
		drawing_buffer_add(LAYER_LINGER_OVERLAY, loc, xpos, ypos,
			image::get_image(linger, image::TOD_COLORED));
	}

	if(on_map && loc == selectedHex_ && !game_config::images::selected.empty()) {
		static const image::locator selected(game_config::images::selected);
		drawing_buffer_add(LAYER_SELECTED_HEX, loc, xpos, ypos,
				image::get_image(selected, image::SCALED_TO_HEX));
	}

	// Show def% and turn to reach info
	if (!is_shrouded && on_map && !units_.city_from_loc(loc)) {
		draw_movement_info(loc);
	}

	if(game_config::debug) {
		int debugH = debugHighlights_[loc];
		if (debugH) {
			std::string txt = lexical_cast<std::string>(debugH);
			draw_text_in_hex(loc, LAYER_MOVE_INFO, txt, 18, font::BAD_COLOR);
		}
	}
	//simulate_delay += 1;
}

void game_display::redraw_units(const std::vector<map_location>& invalidated_unit_locations)
{
	// Units can overlap multiple hexes, so we need
	// to redraw them last and in the good sequence.
	foreach (map_location loc, invalidated_unit_locations) {
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
	if (screen_anim_) {
		screen_anim_->update_last_draw_time();
		screen_anim_->redraw(frame_parameters::null_param);
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

void game_display::draw_report(reports::TYPE report_num)
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

	for(size_t r = reports::STATUS_REPORTS_BEGIN; r != reports::STATUS_REPORTS_END; ++r) {
		draw_report(reports::TYPE(r));
	}
}

void game_display::show_unit_tooltip(const unit& troop, const map_location& loc)
{
	int16_t xoffset = 0;
	int16_t yoffset = 200; 
	std::string tip_message;
	SDL_Color tip_color = font::NORMAL_COLOR;
	int font_size = font::SIZE_SMALL;
	int text_width = 400;

	if (game_config::tiny_gui) {
		return;
	}

	const SDL_Color bgcolor = {0, 0, 0, 128};
	SDL_Rect area = screen_area();

	if (tooltip_handle_) {
		font::remove_floating_label(tooltip_handle_);
		tooltip_handle_ = 0;
	}

	tip_message = troop.form_tooltip();
	
	unsigned int border = 10;

	const std::string wrapped_message = font::word_wrap_text(tip_message, font_size, text_width);
/*
	tooltip_handle_ = font::add_floating_label(wrapped_message, font_size, tip_color, 
	                                          0, 0, 0, 0, -1, area, font::LEFT_ALIGN, &bgcolour, border);
*/
	font::floating_label flabel(wrapped_message);
	flabel.set_font_size(font_size);
	flabel.set_color(tip_color);
	flabel.set_bg_color(bgcolor);
	flabel.set_position(0, 0);
	flabel.set_alignment(font::LEFT_ALIGN);
	flabel.set_clip_rect(area);
	tooltip_handle_ = font::add_floating_label(flabel);

	SDL_Rect rect = font::get_floating_label_rect(tooltip_handle_);

	// one instance of map_area: (0, 26, 800, 471)
	const SDL_Rect& map_area = map_outside_area();
	int xpos = get_location_x(loc);

	// 单位提示放置规则:
	// 1. 当x方向上，窗口距单位所在格子足够放置提示宽度时，提示放在左下角，否则放去右下解；
	if (xpos >= rect.w) {
		xoffset = map_area.x;
	} else  {
		xoffset = map_area.x + map_area.w - rect.w;
	}

	yoffset = map_area.y + map_area.h - rect.h;
	font::move_floating_label(tooltip_handle_, xoffset, yoffset);
}

void game_display::show_unit_tooltip(const unit& troop, int16_t xoffset, int16_t yoffset)
{
	std::string tip_message;
	SDL_Color tip_color = font::NORMAL_COLOR;
	int font_size = font::SIZE_SMALL;
	int text_width = 400;

	// const SDL_Color bgcolour = {0, 0, 0, 128};
	const SDL_Color bgcolor = {0, 0, 0, 255};
	SDL_Rect area = screen_area();

	if (tooltip_handle_) {
		font::remove_floating_label(tooltip_handle_);
		tooltip_handle_ = 0;
	}

	tip_message = troop.form_tooltip();
	
	unsigned int border = 10;

	const std::string wrapped_message = font::word_wrap_text(tip_message, font_size, text_width);
/*
	tooltip_handle_ = font::add_floating_label(wrapped_message, font_size, tip_color, 
	                                          0, 0, 0, 0, -1, area, font::LEFT_ALIGN, &bgcolour, border);
*/
	font::floating_label flabel(wrapped_message);
	flabel.set_font_size(font_size);
	flabel.set_color(tip_color);
	flabel.set_bg_color(bgcolor);
	flabel.set_position(xoffset, yoffset);
	flabel.set_alignment(font::LEFT_ALIGN);
	flabel.set_clip_rect(area);
	tooltip_handle_ = font::add_floating_label(flabel);

	SDL_Rect rect = font::get_floating_label_rect(tooltip_handle_);

	// font::move_floating_label(tooltip_handle_, xoffset, yoffset);
}

void game_display::hide_unit_tooltip()
{
	if (tooltip_handle_) {
		font::remove_floating_label(tooltip_handle_);
		tooltip_handle_ = 0;
	}
}

void game_display::draw_sidebar()
{
	// wb::scoped_planned_pathfind_map future; //< Lasts for whole method.

	draw_report(reports::REPORT_CLOCK);
	draw_report(reports::REPORT_COUNTDOWN);

	if(teams_.empty()) {
		return;
	}

	if(invalidateUnit_) {
		// We display the unit the mouse is over if it is over a unit,
		// otherwise we display the unit that is selected.
		for(size_t r = reports::UNIT_REPORTS_BEGIN; r != reports::UNIT_REPORTS_END; ++r) {
			draw_report(reports::TYPE(r));
		}

		invalidateUnit_ = false;

		unit_map::const_iterator itor = find_visible_unit(mouseoverHex_, teams_[currentTeam_]);
		if (itor.valid() && !fogged(mouseoverHex_)) {
			show_unit_tooltip(*itor, itor->get_location());
		}
	}

	if(invalidateGameStatus_) {
		draw_game_status();
		invalidateGameStatus_ = false;
	}
}

void game_display::draw_minimap_units()
{
	double xscaling = 1.0 * minimap_location_.w / get_map().w();
	double yscaling = 1.0 * minimap_location_.h / get_map().h();

	for(unit_map::const_iterator u = units_.begin(); u != units_.end(); ++u) {
		if (fogged(u->get_location()) ||
		    (teams_[currentTeam_].is_enemy(u->side()) &&
		     u->invisible(u->get_location())) ||
			 u->get_hidden()) {
			continue;
		}

		int side = u->side();
		const SDL_Color col = team::get_minimap_color(side);
		const Uint32 mapped_col = SDL_MapRGB(video().getSurface()->format,col.r,col.g,col.b);

		double u_x = u->get_location().x * xscaling;
		double u_y = (u->get_location().y + (is_odd(u->get_location().x) ? 1 : -1)/4.0) * yscaling;
 		// use 4/3 to compensate the horizontal hexes imbrication
		double u_w = 4.0 / 3.0 * xscaling;
		double u_h = yscaling;

		SDL_Rect r = create_rect(minimap_location_.x + round_double(u_x)
				, minimap_location_.y + round_double(u_y)
				, round_double(u_w)
				, round_double(u_h));

		sdl_fill_rect(video().getSurface(), &r, mapped_col);

		if (unit_is_city(u)) {
			draw_rectangle(r.x - 2, r.y - 2, r.w + 4, r.h + 4, mapped_col, video().getSurface());
		}
	}

	if (moving_dst_loc_.valid()) {
		static surface indicator_dst = image::get_image("misc/move-indicator-dst-mini.png");
		SDL_Rect dstrect = create_rect(minimap_location_.x + std::min(minimap_location_.w - indicator_dst->w, std::max(0, round_double(moving_dst_loc_.x * xscaling) - indicator_dst->w / 2)), 
			minimap_location_.y + std::min(minimap_location_.h - indicator_dst->h, std::max(0, round_double(moving_dst_loc_.y * yscaling) - indicator_dst->h / 2)), 0, 0);
		blit_surface(indicator_dst, NULL, video().getSurface(), &dstrect);
	}
}

void game_display::draw_bar(const std::string& image, int xpos, int ypos, const map_location& loc, size_t size, double filled, const SDL_Color& col, fixed_t alpha, bool vtl)
{
	filled = std::min<double>(std::max<double>(filled, 0.0), 1.0);
	size = static_cast<size_t>(size * get_zoom_factor());

	surface surf(image::get_image(image,image::SCALED_TO_HEX));

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

	const size_t unfilled = static_cast<const size_t>(size * (1.0 - filled));

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
			// drawing_buffer_add(LAYER_UNIT_BAR, loc, tblit(xpos + bar_loc.x + unfilled, ypos + bar_loc.y, filled_surf));
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

void game_display::draw_movement_info(const map_location& loc)
{
	// Search if there is a mark here
	pathfind::marked_route::mark_map::iterator w = route_.marks.find(loc);

	// Don't use empty route or the first step (the unit will be there)
	if(w != route_.marks.end()
				&& !route_.steps.empty() && route_.steps.front() != loc) {
		const unit_map::const_iterator un = units_.find(route_.steps.front());
		if(un != units_.end()) {
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
		const unit_map::const_iterator selectedUnit = units_.find(selectedHex_);
		const unit_map::const_iterator mouseoveredUnit = units_.find(mouseoverHex_);
		if(selectedUnit != units_.end() && mouseoveredUnit == units_.end()) {
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
	const unit_map::const_iterator u = units_.find(route_.steps.front());
	if(u != units_.end()) {
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
	unit_map::const_iterator u_itor = units_.find(loc);
	if (u_itor.valid() && u_itor->type() == unit_types.find_keep()) {
		side = u_itor->side();
	}
	if (!side && !get_map().is_village(terrain)) {
		return surface(NULL);
	}

	if (side) {
		if (fogged(loc) && teams_[currentTeam_].is_enemy(side)) {
			side = 0;
		}
	} else {
		for (size_t i = 0; i != teams_.size(); ++i) {
			if (teams_[i].owns_village(loc) && (!fogged(loc) || !teams_[currentTeam_].is_enemy(i+1))) {
				side = i + 1;
				break;
			}
		}
	}
	if (side) {
		flags_[side - 1].update_last_draw_time();
		const image::locator &image_flag = animate_map_? flags_[side - 1].get_current_frame() : flags_[side - 1].get_first_frame();
		return image::get_image(image_flag, image::TOD_COLORED);
	}

	return surface(NULL);
}

bool game_display::redraw_everything()
{
	bool redrawn = display::redraw_everything();
	if (redrawn) {
		if (!teams_.empty() && teams_[activeTeam_].is_human()) {
			refresh_access_troops(activeTeam_);
			resources::controller->show_context_menu(NULL, *this);
		}
	}
	return redrawn;
}

size_t utf8str_len(std::string& utf8str)
{
	size_t size = 0;
	utils::utf8_iterator itor(utf8str);
	for (; itor != utils::utf8_iterator::end(utf8str); ++ itor) {
		size ++;
	}
	return size;
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

					std::string& surname = teams_[i].leader()->surname();
					size_t chars = utf8str_len(surname);

					surface text_surf;
					surface back_surf;
					int font_size;
					int xstart, ystart;
					if (default_zoom_ == ZOOM_48) {
						if (chars == 1) {
							font_size = 10;
							xstart = 20;
							ystart = 4;
						} else if (chars == 2) {
							font_size = 8;
							xstart = 18;
							ystart = 6;
						} else if (chars == 3) {
							font_size = 8;
							xstart = 18;
							ystart = 6;
						} else {
							font_size = 8;
							xstart = 16;
							ystart = 6;
						}
					} else if (default_zoom_ == ZOOM_56) {
						if (chars == 1) {
							font_size = 12;
							xstart = 23;
							ystart = 4;
						} else if (chars == 2) {
							font_size = 9;
							xstart = 20;
							ystart = 6;
						} else if (chars == 3) {
							font_size = 8;
							xstart = 18;
							ystart = 6;
						} else {
							font_size = 8;
							xstart = 16;
							ystart = 6;
						}
					} else if (default_zoom_ == ZOOM_64) {
						if (chars == 1) {
							font_size = 14;
							xstart = 27;
							ystart = 5;
						} else if (chars == 2) {
							font_size = 10;
							xstart = 23;
							ystart = 6;
						} else if (chars == 3) {
							font_size = 9;
							xstart = 24;
							ystart = 7;
						} else {
							font_size = 9;
							xstart = 16;
							ystart = 6;
						}
					} else {
						if (chars == 1) {
							font_size = font::SIZE_PLUS;
							xstart = 30;
							ystart = 6;
						} else if (chars == 2) {
							font_size = font::SIZE_SMALL;
							xstart = 26;
							ystart = 8;
						} else if (chars == 3) {
							font_size = font::SIZE_TINY;
							xstart = 28;
							ystart = 10;
						} else {
							font_size = font::SIZE_TINY;
							xstart = 22;
							ystart = 10;
						}
					}
					text_surf = font::get_rendered_text(teams_[i].leader()->surname(), font_size, font::BIGMAP_COLOR);
					back_surf = font::get_rendered_text(teams_[i].leader()->surname(), font_size, font::BLACK_COLOR);

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
	foreach (const pathfind::paths::step &dest, paths_list.destinations) {
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
				draw_area_[(reach->first.y + 1) * draw_area_pitch_ + (reach->first.x + 1)] = INVALIDATE;
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

void game_display::float_label(const map_location& loc, const std::string& text,
						  int red, int green, int blue)
{
	if(preferences::show_floating_labels() == false || fogged(loc)) {
		return;
	}

	font::floating_label flabel(text);
	flabel.set_font_size(font::SIZE_XLARGE);
	const SDL_Color color = create_color(red, green, blue);
	flabel.set_color(color);
	flabel.set_position(get_location_x(loc)+zoom_/2, get_location_y(loc));
	flabel.set_move(0, -2 * turbo_speed());
	flabel.set_lifetime(60/turbo_speed());
	flabel.set_scroll_mode(font::ANCHOR_LABEL_MAP);

	font::add_floating_label(flabel);
}

struct is_energy_color {
	bool operator()(Uint32 color) const { return (color&0xFF000000) > 0x10000000 &&
	                                              (color&0x00FF0000) < 0x00100000 &&
												  (color&0x0000FF00) < 0x00001000 &&
												  (color&0x000000FF) < 0x00000010; }
};

const SDL_Rect& game_display::calculate_energy_bar(surface surf)
{
	const std::map<surface,SDL_Rect>::const_iterator i = energy_bar_rects_.find(surf);
	if(i != energy_bar_rects_.end()) {
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
	energy_bar_rects_.insert(std::pair<surface,SDL_Rect>(surf,res));
	return calculate_energy_bar(surf);
}

void game_display::invalidate_animations_location(const map_location& loc) {
	if (get_map().is_village(loc)) {
		const int owner = player_teams::village_owner(loc);
		if (owner >= 0 && flags_[owner].need_update()
		&& (!fogged(loc) || !teams_[currentTeam_].is_enemy(owner+1))) {
			invalidate(loc);
		}
	}
}

void game_display::invalidate_animations()
{
	// new_animation_frame();
	display::invalidate_animations();

	if (terrain_dirty_) {
		get_builder().rebuild_terrain();
		terrain_dirty_ = false;
	}
	if (screen_anim_) {
		screen_anim_->invalidate(frame_parameters::null_param);
	}
	if (expedite_city_) {
		expedite_city_->refresh();
		expedite_city_->invalidate(expedite_city_->get_location());
	}
	for (size_t i = 0; i < draw_area_unit_size_; i ++) {
		draw_area_unit_[i]->refresh();
		draw_area_unit_[i]->invalidate(draw_area_unit_[i]->get_location());
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

void game_display::set_attack_indicator(unit* attack)
{
	invalidate(attack_indicator_dst_);
	attack_indicator_dst_.clear();
	attack_indicator_each_dst_.clear();
	if (!attack || !attack->attacks_left()) {
		return;
	}

	map_offset* adjacent_ptr;

	const map_location& loc = attack->get_location();
	map_location relative_loc;
	std::set<map_location> dst, each_dst;
	size_t i, size;
	int range;
	bool find;
	const team& current_team = teams_[attack->side() - 1];

	std::vector<attack_type>& attacks = attack->attacks();
	for (std::vector<attack_type>::const_iterator at_it = attacks.begin(); at_it != attacks.end(); ++ at_it, i ++) {
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
		for (i = 0; i < size; i ++) {
			relative_loc.x = loc.x + adjacent_ptr[i].x;
			relative_loc.y = loc.y + adjacent_ptr[i].y;
			if (units_.count(relative_loc) || units_.count(relative_loc, false)) {
				unit_map::iterator itor = units_.find(relative_loc);
				if (itor == units_.end()) {
					itor = units_.find(relative_loc, false);
				}
				if (!current_team.fogged(relative_loc) && current_team.is_enemy(itor->side()) && !itor->invisible(relative_loc)) {
					dst.insert(relative_loc);
					each_dst.insert(relative_loc);
				}
			}
		}
		attack_indicator_each_dst_.push_back(std::pair<int, std::set<map_location> >(range, each_dst));
	}
	attack_indicator_dst_ = dst;
	invalidate(attack_indicator_dst_);
}

void game_display::clear_attack_indicator()
{
	set_attack_indicator(NULL);
}

bool game_display::loc_in_attack_indicator(const map_location& loc) const
{
	return std::find(attack_indicator_dst_.begin(), attack_indicator_dst_.end(), loc) != attack_indicator_dst_.end();
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
			if (new_art->type() == unit_types.find_wall() || new_art->type() == unit_types.find_keep()) {
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
					unit_map::iterator itor = units_.find(relative_loc2);
					if (itor.valid() && itor->emits_zoc() && itor->is_artifical()) {
						break;
					}
				}
				if (i2 == size2) {
					build_indicator_dst_.insert(relative_loc);
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

void game_display::set_screen_anim(unit_animation* anim)
{
	screen_anim_ = anim;
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
	foreach (const game_display::overlay_map::value_type i, overlays_) {
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
	create_buttons();
	invalidate_all();
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
			str << "<" << speaker << " " << msg << ">";
			message_color = speaker_color;
			message_str << " ";
		} else {
			if (!speaker.empty())
				str << "<" << speaker << ">";
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

	foreach (const chat_message &cm, chat_messages_) {
		font::move_floating_label(cm.speaker_handle, 0, - movement);
		font::move_floating_label(cm.handle, 0, - movement);
	}
}

game_display *game_display::singleton_ = NULL;

