/* $Id: title_screen.cpp 48740 2011-03-05 10:01:34Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/dialogs/prelude.hpp"

#include "game_display.hpp"
#include "game_config.hpp"
#include "gettext.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/hexmap.hpp"
#include "gui/widgets/window.hpp"
#include "gui/auxiliary/timer.hpp"

#include <boost/bind.hpp>

#include <algorithm>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_title_screen
 *
 * == Title screen ==
 *
 * This shows the title screen.
 *
 * @begin{table}{dialog_widgets}
 * tutorial & & button & m &
 *         The button to start the tutorial. $
 *
 * campaign & & button & m &
 *         The button to start a campaign. $
 *
 * multiplayer & & button & m &
 *         The button to start multiplayer mode. $
 *
 * load & & button & m &
 *         The button to load a saved game. $
 *
 * editor & & button & m &
 *         The button to start the editor. $
 *
 * addons & & button & m &
 *         The button to start managing the addons. $
 *
 * language & & button & m &
 *         The button to select the game language. $
 *
 * credits & & button & m &
 *         The button to show Wesnoth's contributors. $
 *
 * quit & & button & m &
 *         The button to quit Wesnoth. $
 *
 * tips & & multi_page & m &
 *         A multi_page to hold all tips, when this widget is used the area of
 *         the tips doesn't need to be resized when the next or previous button
 *         is pressed. $
 *
 * -tip & & label & o &
 *         Shows the text of the current tip. $
 *
 * -source & & label & o &
 *         The source (the one who's quoted or the book referenced) of the
 *         current tip. $
 *
 * next_tip & & button & m &
 *         The button show the next tip of the day. $
 *
 * previous_tip & & button & m &
 *         The button show the previous tip of the day. $
 *
 * logo & & progress_bar & o &
 *         A progress bar to "animate" the Wesnoth logo. $
 *
 * revision_number & & control & o &
 *         A widget to show the version number when the version number is
 *         known. $
 *
 * @end{table}
 */

REGISTER_DIALOG(prelude)

std::map<const std::string, tprelude::ttype> tprelude::tags;

void tprelude::fill_tags()
{
	if (!tags.empty()) return;

	tags.insert(std::make_pair("message", MESSAGE));
	tags.insert(std::make_pair("animation", ANIM));
}

tprelude::ttype tprelude::find(const std::string& tag)
{
	std::map<const std::string, ttype>::const_iterator it = tags.find(tag);
	if (it != tags.end()) {
		return it->second;
	}
	return NONE;
}

tprelude::tprelude(game_display& disp, hero_map& heros, const config& game_config, const config& cfg)
	: disp_(disp)
	, heros_(heros)
	, game_config_(game_config)
	, cfg_(cfg)
	, commands_()
	, current_(0)
	, ing_timer_(0)
	, active_anim_(-1)
	, next_(NULL)
{
	fill_tags();
	VALIDATE(cfg.child("prelude"), "WML config must be exist [prelude] block!");
}

tprelude::~tprelude()
{
	if (ing_timer_) {
		gui2::remove_timer(ing_timer_);
	}
	std::map<int, unit_animation>& screen_anims = disp_.screen_anims();
	screen_anims.clear();
}

void tprelude::post_build(CVideo& video, twindow& window)
{
}

int ready_outer_anim(game_display& disp, int type, const config& cfg, bool start, bool cycles)
{
	const unit_animation* tpl = unit_types.global_anim(type);
	int id = disp.insert_screen_anim(*tpl);

	unit_animation& anim = disp.screen_anim(id);
	std::string src, dst;

	BOOST_FOREACH (const config::attribute& attr, cfg.attribute_range()) {
		const std::string& name = attr.first;
		src.clear();
		if (name == "x") {
			src = "8800";
		} else if (name == "y") {
			src = "8801";
		} else if (name == "offset_x") {
			src = "8802";
		} else if (name == "offset_y") {
			src = "8803";
		} else if (name == "alpha") {
			src = "8810";
		} else if (name == "image") {
			anim.replace_image_name("__image", attr.second);

		} else if (name == "text") {
			anim.replace_static_text("__text", attr.second);
		} else {
			continue;
		}
		if (!src.empty()) {
			anim.replace_progressive(name, src, attr.second);
		}
	}

	if (start) {
		new_animation_frame();
		anim.start_animation(0, map_location::null_location, map_location::null_location, cycles);
	}

	return id;
}

void tprelude::pre_show(CVideo& video, twindow& window)
{
	const config& prelude_cfg = cfg_.child("prelude");

	tlabel& title = find_widget<tlabel>(&window, "title", false);
	title.set_label(cfg_["name"]);

	thexmap& hexmap = find_widget<thexmap>(&window, "hexmap", false);
	hexmap.set_config(&game_config_);
	hexmap.set_map_data(thexmap::IMG, prelude_cfg["map_data"].str());
	hexmap.set_callback_size_change(boost::bind(&tprelude::size_change, this, _1));

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "previous", false)
		, boost::bind(
			&tprelude::previous
			, this
			, boost::ref(window)));
	find_widget<tbutton>(&window, "previous", false).set_visible(twidget::INVISIBLE);

	next_ = &find_widget<tbutton>(&window, "next", false);
	connect_signal_mouse_left_click(
		*next_
		, boost::bind(
			&tprelude::next
			, this
			, boost::ref(window)));

	form_commands(prelude_cfg);
	if (!commands_.empty()) {
		execute(window);
	} else {
		tbutton& b = find_widget<tbutton>(&window, "next", false);
		b.set_label(_("Battle"));
	}

	if (const config& start_cfg = prelude_cfg.child("start")) {
		BOOST_FOREACH (const config& anim, start_cfg.child_range("animation")) {
			global_anim_tag::ttype id = global_anim_tag::find(anim["id"].str());
			VALIDATE(id != global_anim_tag::NONE, "[prelude], invalidad id: " + anim["id"].str());
			anims_.insert(std::make_pair(ready_outer_anim(disp_, id, anim, true, true), &anim));
		}
	}
}

void tprelude::size_change(thexmap* widget)
{
	tpoint standard(640, 360);
	tpoint size = widget->get_size();

	outer_anim::zoom = std::make_pair(1.0 * size.x / standard.x, 1.0 * size.y / standard.y);
	outer_anim::rect = widget->get_rect();
/*
	std::string src;
	for (std::map<int, const config*>::iterator it = anims_.begin(); it != anims_.end(); ++ it) {
		unit_animation& anim = disp_.screen_anim(it->first);

		if (!anim.started()) {
			new_animation_frame();
			anim.start_animation(0, map_location::null_location, map_location::null_location, true);
		}
	}
*/
}

void tprelude::form_commands(const config& cfg)
{
	BOOST_FOREACH (const config::any_child& c, cfg.all_children_range()) {
		ttype t = find(c.key);
		if (t == NONE) {
			continue;
		}
		commands_.push_back(tcommand(t, c.cfg));
	}
}

void tprelude::timer_handler()
{
	if (active_anim_ < 0) {
		return;
	}
	std::map<int, unit_animation>& screen_anims = disp_.screen_anims();
	std::map<int, unit_animation>::iterator it = screen_anims.find(active_anim_);
	if (it == screen_anims.end() || it->second.animation_finished_potential()) {
		if (it != screen_anims.end()) {
			disp_.erase_screen_anim(active_anim_);
		}
		active_anim_ = -1;
		next_->set_active(true);
	}
}

void tprelude::execute(twindow& window)
{
	tcontrol& portrait = find_widget<tcontrol>(&window, "portrait", false);
	tcontrol& msg = find_widget<tcontrol>(&window, "msg", false);

	VALIDATE(active_anim_ < 0, "active_anim must be < 0!");
	if (ing_timer_) {
		gui2::remove_timer(ing_timer_);
		ing_timer_ = 0;
	}

	const tcommand& c = commands_[current_];
	const config& cfg = c.cfg;
	if (c.type == MESSAGE) {
		int number = cfg["hero"].to_int(HEROS_INVALID_NUMBER);
		hero& h = heros_[number];
		if (h.valid()) {
			portrait.set_label(h.image(true));
		} else {
			portrait.set_label(null_str);
		}
		msg.set_label(cfg["message"]);
	} else if (c.type == ANIM) {
		global_anim_tag::ttype id = global_anim_tag::find(cfg["id"].str());
		if (id != global_anim_tag::NONE) {
			bool persist = cfg["persist"].to_bool();
			active_anim_ = ready_outer_anim(disp_, id, cfg, true, persist);
			if (persist) {
				active_anim_ = -1;
			} else {
				next_->set_active(false);
				ing_timer_ = add_timer(100, boost::bind(&tprelude::timer_handler, this), true);
			}
		}
	}
	current_ ++;
}

void tprelude::previous(twindow& window)
{
}

void tprelude::next(twindow& window)
{
	if (commands_.empty() || current_ >= commands_.size()) {
		window.set_retval(twindow::OK);
	} else {
		execute(window);
		if (current_ == commands_.size()) {
			tbutton& b = find_widget<tbutton>(&window, "next", false);
			b.set_label(_("Battle"));
		}
	}
}

} // namespace gui2

