/* $Id: button.cpp 40489 2010-01-01 13:16:49Z mordante $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "global.hpp"

#include "widgets/button.hpp"
#include "game_config.hpp"
#include "font.hpp"
#include "marked-up_text.hpp"
#include <image.hpp>
#include "log.hpp"
#include "serialization/string_utils.hpp"
#include "sound.hpp"
#include "video.hpp"
#include "wml_separators.hpp"
#include "hero.hpp"
#include "wml_exception.hpp"
#include <preferences.hpp>
#include "display.hpp"

namespace gui {

const int default_font_size = font::SIZE_SMALL;
const int horizontal_padding = font::SIZE_SMALL;
const int checkbox_horizontal_padding = font::SIZE_SMALL / 2;
const int vertical_padding = 5;

button::button(CVideo& video, const std::string& label, button::TYPE type,
               std::string button_image_name, SPACE_CONSUMPTION spacing, const bool auto_join, 
			   button_hook* hook, const void* menu, size_t btnidx, SDL_Rect* clip, int base_width, int base_height, int font_size)
	: widget(video, auto_join), type_(type), label_(label),
	  image_(NULL), pressedImage_(NULL), activeImage_(NULL), pressedActiveImage_(NULL),
	  button_(true), state_(NORMAL), pressed_(false),
	  font_size_(font_size? font_size: default_font_size),
	  spacing_(spacing), 
	  base_height_(base_height), 
	  base_width_(base_width),
	  hook_(hook), 
	  menu_(menu), 
	  btnidx_(btnidx), 
	  cookie_(NULL), 
	  cookie_type(COOKIE_NONE), 
	  color_(font::BUTTON_COLOR), 
	  tactic_hero_(NULL)
{
	if (button_image_name.empty() && type == TYPE_PRESS) {
		button_image_name = "button";
	} else if(button_image_name.empty() && type == TYPE_CHECK) {
		button_image_name = "checkbox";
	}

	std::string button_image_file;
	surface button_image, pressed_image, active_image;
	
	if (button_image_name.find("/") != std::string::npos) {
		button_image_file = button_image_name;
	} else {
		button_image_file = "buttons/" + button_image_name + ".png";
	}
	if (clip) {
		button_image = cut_surface(image::get_image(button_image_file), *clip);
	} else {
		button_image = image::get_image(button_image_file);
	}

	std::stringstream err;
	err << "button::button, Could not construct button object: " << button_image_file << ", mybe no corresponding images";
	VALIDATE(button_image, err.str());

	surface pressed_active_image;
	// Get active/pressed_image from button_image
	if (type != TYPE_IMAGE) {
		active_image = adjust_surface_color(button_image, 40, 40, 40);

		if (type == TYPE_CHECK) {
			pressed_image = image::get_image("buttons/" + button_image_name + "-pressed.png");

			pressed_active_image = adjust_surface_color(pressed_image, 40, 40, 40);
		} else {
			SDL_Rect src_clip = ::create_rect(0, 0, active_image->w - 2, active_image->h - 2);
			SDL_Rect dst_clip = ::create_rect(2, 2, 0, 0);
			pressed_image.assign(create_neutral_surface(active_image->w, active_image->h));
			blit_surface(active_image, &src_clip, pressed_image, &dst_clip);
		}
	} else {
		pressed_image = image::get_image("buttons/" + button_image_name + "-pressed.png");
		active_image = image::get_image("buttons/" + button_image_name + "-active.png");
		if (pressed_image.null())
			pressed_image.assign(button_image);

		if (active_image.null())
			active_image.assign(button_image);
	}

	if (!base_height_) {
		base_height_ = button_image->h;
	}
	if (!base_width_) {
		base_width_ = button_image->w;
	}

	if (type_ != TYPE_IMAGE){
		set_label(label);
	}

	if (type == TYPE_PRESS) {
		image_.assign(scale_surface(button_image,location().w,location().h));
		pressedImage_.assign(scale_surface(pressed_image,location().w,location().h));
		activeImage_.assign(scale_surface(active_image,location().w,location().h));
	} else {
		image_.assign(scale_surface(button_image,button_image->w,button_image->h));
		pressedImage_.assign(scale_surface(pressed_image,button_image->w,button_image->h));
		activeImage_.assign(scale_surface(active_image,button_image->w,button_image->h));
		if (type == TYPE_CHECK)
			pressedActiveImage_.assign(scale_surface(pressed_active_image, button_image->w, button_image->h));
	}

	if (type_ == TYPE_IMAGE) {
		calculate_size();
	}
}

void button::set_rpg_image(hero* h, bool greyscale)
{
	surface genus_surf = get_genus_surface();

	surface hero_sur = image::get_image(h->image());
	surface masked_sur = mask_surface(hero_sur, image::get_image("buttons/photo-mask.png"));

	if (greyscale) {
		masked_sur = greyscale_image(masked_sur);
	}
	masked_sur = scale_surface(masked_sur, location().w, location().h);
	image_.assign(masked_sur);

	blit_surface(genus_surf, NULL, image_, NULL);

	generate_other_image();
}

void button::set_tactic_image(hero& h, const std::string& label)
{
	int width = location().w;
	int height = location().h;

	image_.assign(NULL);
	image_ = make_neutral_surface(image::get_image("themes/tactic-bg.png"));

	if (h.valid()) {
		tactic_hero_ = &h;
		
		std::stringstream strstr;
		strstr << h.image() << "~SCALE(36, 45)";

		surface hero_sur = image::get_image(strstr.str());

		SDL_Rect clip = ::create_rect(1, 1, 0, 0);
		blit_surface(hero_sur, NULL, image_, &clip);

		image_ = scale_surface(image_, location().w, location().h);
		label_ = label;
	} else {
		tactic_hero_ = NULL;
		label_ = "";
	}

	generate_other_image();
}

void button::set_bomb_image(int bomb_turns)
{
	std::stringstream strstr;
	int width = location().w;
	int height = location().h;

	image_.assign(NULL);
	if (bomb_turns < game_config::max_bomb_turns) {
		image_ = make_neutral_surface(image::get_image("themes/bomb-bg.png"));
	} else {
		image_ = make_neutral_surface(image::get_image("themes/bomb-full-bg.png"));
	}
	
	
	SDL_Rect dst_clip = create_rect(26, 20, 0, 0);
	// if (bomb_turns < game_config::max_bomb_turns) {
		strstr.str("");
		strstr << "misc/digit.png~CROP(" << 8 * bomb_turns << ", 0, 8, 12)";
		blit_surface(image::get_image(strstr.str()), NULL, image_, &dst_clip);

		strstr.str("");
		strstr << "misc/digit.png~CROP(" << 8 * 10 << ", 0, 8, 12)";
		dst_clip.x += 8;
		blit_surface(image::get_image(strstr.str()), NULL, image_, &dst_clip);

		strstr.str("");
		strstr << "misc/digit.png~CROP(" << 8 * game_config::max_bomb_turns << ", 0, 8, 12)";
		dst_clip.x += 8;
		blit_surface(image::get_image(strstr.str()), NULL, image_, &dst_clip);
	// }
	image_ = scale_surface(image_, width, height);
	
	generate_other_image();
}

void button::set_image(const std::string& stem, int integer, bool greyscale, bool special, const std::string& icon, const std::string& lb_icon, int)
{
	// const unit* u = reinterpret_cast<const unit*>(cookie_);

	int width = location().w;
	int height = location().h;
	
	// const std::string button_image_file = "buttons/" + stem + ".png";
	const std::string button_image_file = stem;
	surface stem_image;

	stem_image = image::get_image(stem);
	if (greyscale) {
		stem_image = greyscale_image(stem_image);
	}
	stem_image = scale_surface(stem_image, width, height);

	image_.assign(stem_image);

	const int bar_vtl_ticks_width = 6;
	if (cookie_type == COOKIE_UNIT) {
		set_unit_image(cookie_, image_, integer);
	}

	std::stringstream text;
	SDL_Rect dst_clip = create_rect(0, 0, 0, 0);

	// overlay digit
	if (integer >= 0) {
		dst_clip.x = 0;
		dst_clip.y = 0;
		integer = integer % 100000;
		if (integer >= 10000) {
			text.str("");
			text << "misc/digit.png~CROP(" << 8 * (integer / 10000) << ", 0, 8, 12)";
			blit_surface(image::get_image(text.str()), NULL, image_, &dst_clip);
			dst_clip.x += 8;
		}
		if (integer >= 1000) {
			text.str("");
			text << "misc/digit.png~CROP(" << 8 * ((integer % 10000) / 1000) << ", 0, 8, 12)";
			blit_surface(image::get_image(text.str()), NULL, image_, &dst_clip);
			dst_clip.x += 8;
		}
		if (integer >= 100) {
			text.str("");
			text << "misc/digit.png~CROP(" << 8 * ((integer % 1000) / 100) << ", 0, 8, 12)";
			blit_surface(image::get_image(text.str()), NULL, image_, &dst_clip);
			dst_clip.x += 8;
		}
		if (integer >= 10) {
			text.str("");
			text << "misc/digit.png~CROP(" << 8 * ((integer % 100) / 10) << ", 0, 8, 12)";
			blit_surface(image::get_image(text.str()), NULL, image_, &dst_clip);
			dst_clip.x += 8;
		}
		text.str("");
		text << "misc/digit.png~CROP(" << 8 * (integer % 10) << ", 0, 8, 12)";
		blit_surface(image::get_image(text.str()), NULL, image_, &dst_clip);
	}

	if (!icon.empty()) {
		text.str("");
		text << icon << "~SCALE(16, 16)";
		dst_clip.x = 0;
		dst_clip.y = 12;
		blit_surface(image::get_image(text.str()), NULL, image_, &dst_clip);
	}

	if (!lb_icon.empty()) {
		text.str("");
		text << lb_icon << "~SCALE(16, 16)";
		dst_clip.x = COOKIE_UNIT? bar_vtl_ticks_width: 0;
		dst_clip.y = image_->h - 16;
		blit_surface(image::get_image(text.str()), NULL, image_, &dst_clip);
	}

	generate_other_image();
	set_dirty();
}

void button::set_pip_image(const std::string& bg, const std::string& fg)
{
	std::stringstream strstr;
	int width = location().w;
	int height = location().h;

	std::string bg2 = bg;
	if (bg.rfind(".") != bg.size() - 4) {
		bg2 = bg2 + ".png";
	}
	std::string fg2 = fg;
	if (fg.rfind(".") != fg.size() - 4) {
		fg2 = fg2 + ".png";
	}

	image_.assign(NULL);
	image_ = make_neutral_surface(image::get_image(bg2));
	
	surface fg_surf = image::get_image(fg2);
	if (fg_surf) {
		SDL_Rect dst_clip = create_rect(0, 0, 0, 0);
		if (image_->w > fg_surf->w) {
			dst_clip.x = (image_->w - fg_surf->w) / 2;
		}
		if (image_->h > fg_surf->h) {
			dst_clip.y = (image_->h - fg_surf->h) / 2;
		}
		blit_surface(fg_surf, NULL, image_, &dst_clip);
	}

	image_ = scale_surface(image_, width, height);
	
	generate_other_image();
	set_dirty();
}

void button::generate_other_image()
{
	// Get active/pressed_image from button_image
	activeImage_ = adjust_surface_color(image_, 40, 40, 40);

	SDL_Rect src_clip = ::create_rect(0, 0, activeImage_->w - 2, activeImage_->h - 2);
	SDL_Rect dst_clip = ::create_rect(2, 2, 0, 0);
	pressedImage_.assign(create_neutral_surface(activeImage_->w, activeImage_->h));
	blit_surface(activeImage_, &src_clip, pressedImage_, &dst_clip);
}

void button::calculate_size()
{
	if (type_ == TYPE_IMAGE) {
		SDL_Rect loc_image = location();
		loc_image.h = image_->h;
		loc_image.w = image_->w;
		set_location(loc_image);
		return;
	}
	SDL_Rect const &loc = location();
	bool change_size = loc.h == 0 || loc.w == 0;

	if (!change_size) {
		unsigned w = loc.w - (type_ == TYPE_PRESS ? horizontal_padding : checkbox_horizontal_padding + base_width_);
		if (type_ != TYPE_IMAGE) {
			label_ = font::make_text_ellipsis(label_, font_size_, w, false);
		}
	}

	if (type_ != TYPE_IMAGE) {
		textRect_ = font::draw_text(NULL, screen_area(), font_size_, font::BUTTON_COLOR, label_, 0, 0);
	}

	if (!change_size)
		return;

	if (!label_.empty()) {
		set_height(std::max(textRect_.h+vertical_padding,base_height_));
	} else {
		set_height(base_height_);
	}

	if (type_ == TYPE_PRESS) {
		if (spacing_ == MINIMUM_SPACE) {
			set_width(textRect_.w + horizontal_padding);
		} else {
			set_width(std::max(textRect_.w+horizontal_padding,base_width_));
		}
	} else {
		if (label_.empty()) {
			set_width(base_width_);
		} else {
			set_width(checkbox_horizontal_padding + textRect_.w + base_width_);
		}
	}
}

void button::set_check(bool check)
{
	if (type_ != TYPE_CHECK)
		return;
	STATE new_state = check ? PRESSED : NORMAL;
	if (state_ != new_state) {
		state_ = new_state;
		set_dirty();
	}
}

void button::set_active(bool active)
{
	if ((state_ == NORMAL) && active) {
		state_ = ACTIVE;
		set_dirty();
	} else if ((state_ == ACTIVE) && !active) {
		state_ = NORMAL;
		set_dirty();
	}
}

bool button::checked() const
{
	return state_ == PRESSED || state_ == PRESSED_ACTIVE;
}

void button::enable(bool new_val)
{
	if(new_val != enabled())
	{
		pressed_ = false;
		// check buttons should keep their state
		if(type_ != TYPE_CHECK) {
			state_ = NORMAL;
		}
		widget::enable(new_val);
	}
}

void button::draw_contents()
{
	SDL_Rect const &loc = location();

	if (type_ != TYPE_CHECK && (image_->w != loc.w || image_->h != loc.h)) {
		image_ = scale_surface(image_, loc.w, loc.h);
		activeImage_ = scale_surface(activeImage_, loc.w, loc.h);
		pressedImage_ = scale_surface(pressedImage_, loc.w, loc.h);
		pressedActiveImage_ = scale_surface(pressedActiveImage_, loc.w, loc.h);
	}

	surface image = image_;
	const int image_w = image_->w;

	int offset = 0;
	switch(state_) {
	case ACTIVE:
		image = activeImage_;
		break;
	case PRESSED:
		image = pressedImage_;
		if (type_ == TYPE_PRESS)
			offset = 1;
		break;
	case PRESSED_ACTIVE:
		image = pressedActiveImage_;
		break;
	default:
		break;
	}

	SDL_Rect clipArea = loc;
	const int texty = loc.y + loc.h / 2 - textRect_.h / 2 + offset;
	int textx;

	if (type_ != TYPE_CHECK)
		textx = loc.x + image->w / 2 - textRect_.w / 2 + offset;
	else {
		clipArea.w += image_w + checkbox_horizontal_padding;
		textx = loc.x + image_w + checkbox_horizontal_padding / 2;
	}

	if (tactic_hero_) {
		textx = 36;
	}

	SDL_Color button_color = color_;

	if (!enabled()) {
		static const Uint32 disabled_btn_color = 0xAAAAAA;
		static const double disabled_btn_adjust = 0.18;
		image = blend_surface(greyscale_image(image), disabled_btn_adjust, disabled_btn_color);
		button_color = font::GRAY_COLOR;
	}

	video().blit_surface(loc.x, loc.y, image);
	if (type_ != TYPE_IMAGE) {
		clipArea.x += offset;
		clipArea.y += offset;
		clipArea.w -= 2*offset;
		clipArea.h -= 2*offset;
		font::draw_text(&video(), clipArea, font_size_, button_color, label_, textx, texty);
	}
}

bool button::hit(int x, int y) const
{
	return point_in_rect(x,y,location());
}

static bool not_image(const std::string& str) { return !str.empty() && str[0] != IMAGE_PREFIX; }

void button::set_label(const std::string& val)
{
	label_ = val;

	//if we have a list of items, use the first one that isn't an image
	if (std::find(label_.begin(), label_.end(), COLUMN_SEPARATOR) != label_.end()) {
		const std::vector<std::string>& items = utils::split(label_, COLUMN_SEPARATOR);
		const std::vector<std::string>::const_iterator i = std::find_if(items.begin(),items.end(),not_image);
		if(i != items.end()) {
			label_ = *i;
		}
	}

	calculate_size();

	set_dirty(true);
}

void button::set_color(const SDL_Color& color)
{
	color_ = color;
}

void button::mouse_motion(SDL_MouseMotionEvent const &event)
{
	if (hit(event.x, event.y)) {
		// the cursor is over the widget
		if (state_ == NORMAL)
			state_ = ACTIVE;
		else if (state_ == PRESSED && type_ == TYPE_CHECK)
			state_ = PRESSED_ACTIVE;
	} else {
		// the cursor is not over the widget
		if (state_ == PRESSED_ACTIVE)
			state_ = PRESSED;
		else if ((type_ != TYPE_CHECK && type_ != TYPE_IMAGE) || state_ != PRESSED)
			state_ = NORMAL;
	}
}

void button::mouse_down(SDL_MouseButtonEvent const &event)
{
	if (hit(event.x, event.y) && event.button == SDL_BUTTON_LEFT && type_ != TYPE_CHECK){
		state_ = PRESSED;
		sound::play_UI_sound(game_config::sounds::button_press);
	}
}

void button::release()
{
	state_ = NORMAL;
	draw_contents();
}

void button::mouse_up(SDL_MouseButtonEvent const &event)
{
	if (!(hit(event.x, event.y) && event.button == SDL_BUTTON_LEFT))
		return;
	// the user has stopped pressing the mouse left button while on the widget
	switch (type_) {
	case TYPE_CHECK:
		if (hook_) {
			if (!hook_->before_press(menu_, btnidx_)) {
				break;
			}
		}
		state_ = state_ == ACTIVE ? PRESSED_ACTIVE : ACTIVE;
		pressed_ = true;
		if (hook_) {
			hook_->pressed(menu_, btnidx_);
		}
		sound::play_UI_sound(game_config::sounds::checkbox_release);
		break;
	case TYPE_PRESS:
		if (state_ == PRESSED) {
			state_ = ACTIVE;
			pressed_ = true;
			if (hook_) {
				hook_->pressed(menu_, btnidx_);
			}
		}
		break;
	case TYPE_TURBO:
		state_ = ACTIVE;
		break;
	case TYPE_IMAGE:
		pressed_ = true;
		if (hook_) {
			hook_->pressed(menu_, btnidx_);
		}
		break;
	}
}

void button::handle_event(const SDL_Event& event)
{
	if (hidden() || !enabled())
		return;

	STATE start_state = state_;

	if (!mouse_locked())
	{
		switch(event.type) {
			case SDL_MOUSEBUTTONDOWN:
				mouse_down(event.button);
				break;
			case SDL_MOUSEBUTTONUP:
				mouse_up(event.button);
				break;
			case SDL_MOUSEMOTION:
				mouse_motion(event.motion);
				break;
			default:
				return;
		}
	}

	if (start_state != state_)
		set_dirty(true);
}

bool button::pressed()
{
	if (type_ != TYPE_TURBO) {
		const bool res = pressed_;
		pressed_ = false;
		return res;
	} else
		return state_ == PRESSED || state_ == PRESSED_ACTIVE;
}

}
