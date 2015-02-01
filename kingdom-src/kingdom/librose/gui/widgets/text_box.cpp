/* $Id: text_box.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
/*
   Copyright (C) 2008 - 2012 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/widgets/text_box.hpp"

#include "font.hpp"
#include "gui/auxiliary/log.hpp"
#include "gui/auxiliary/widget_definition/text_box.hpp"
#include "gui/auxiliary/window_builder/text_box.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/auxiliary/timer.hpp"
#include "filesystem.hpp"
#include "clipboard.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#define LOG_SCOPE_HEADER get_control_type() + " [" + id() + "] " + __func__
#define LOG_HEADER LOG_SCOPE_HEADER + ':'

namespace gui2 {

REGISTER_WIDGET(text_box)

ttext_box::ttext_box()
	: tcontrol(COUNT)
	, state_(ENABLED)
	, selection_start_(::create_rect(0, 0, 0, 0))
	, selection_end_(::create_rect(-1, -1, 0, 0))
	, maximum_length_(std::string::npos)
	, src_pos_(-1)
	, cursor_timer_(0)
	, hide_cursor_(false)
	, forbid_hide_ticks_(0)
	, text_changed_callback_()
	, mouse_moved_callback_()
	, text_x_offset_(0)
	, text_y_offset_(0)
	, text_height_(0)
	, dragging_(false)
{
	text_editable_ = true;
#ifdef __unix__
		// pastes on UNIX systems.
	connect_signal<event::MIDDLE_BUTTON_CLICK>(boost::bind(
			&ttext_box::signal_handler_middle_button_click, this, _2, _3));

#endif

	connect_signal<event::SDL_KEY_DOWN>(boost::bind(
			&ttext_box::signal_handler_sdl_key_down, this, _2, _3, _5, _6, _7));

	connect_signal<event::SDL_TEXT_INPUT>(boost::bind(
			&ttext_box::signal_handler_sdl_text_input, this, _2, _3, _5));

	connect_signal<event::RECEIVE_KEYBOARD_FOCUS>(boost::bind(
			&ttext_box::signal_handler_receive_keyboard_focus, this, _2));
	connect_signal<event::LOSE_KEYBOARD_FOCUS>(boost::bind(
			&ttext_box::signal_handler_lose_keyboard_focus, this, _2));

	set_wants_mouse_left_double_click();

	connect_signal<event::MOUSE_MOTION>(boost::bind(
				&ttext_box::signal_handler_mouse_motion, this, _2, _3, _5));
	connect_signal<event::LEFT_BUTTON_DOWN>(boost::bind(
				&ttext_box::signal_handler_left_button_down, this, _2, _3));
	connect_signal<event::LEFT_BUTTON_UP>(boost::bind(
				&ttext_box::signal_handler_left_button_up, this, _2, _3));
	connect_signal<event::LEFT_BUTTON_DOUBLE_CLICK>(boost::bind(&ttext_box
				::signal_handler_left_button_double_click, this, _2, _3));
}

ttext_box::~ttext_box()
{
	if (cursor_timer_) {
		gui2::remove_timer(cursor_timer_);
	}
}

void ttext_box::set_maximum_length(const size_t maximum_length)
{
	maximum_length_ = maximum_length;
	set_label(label_);
}

void ttext_box::set_label(const std::string& text)
{
	std::string tmp_text;
	bool use_tmp_text = false;
	if (maximum_length_ != std::string::npos) {
		tmp_text = tintegrate::drop_escape(text);
		size_t size = 0;
		size_t byte_count = 0;
		utils::utf8_iterator it(tmp_text);
		for (; it != utils::utf8_iterator::end(tmp_text); ++ it) {
			if (++ size > maximum_length_) {
				break;
			}
			byte_count += it.substr().second - it.substr().first;
		}
		if (byte_count < tmp_text.size()) {
			use_tmp_text = true;
			tmp_text.erase(byte_count);
			tmp_text = tintegrate::stuff_escape(tmp_text);
		}
	}
	const std::string& result_text = use_tmp_text? tmp_text: text;

	if (result_text != label_ || use_tmp_text) {
		tcontrol::set_label(result_text);
		
		if (text_changed_callback_) {
			text_changed_callback_(this);
		}
		
		// default to put the cursor at the end of the buffer.
		update_canvas();
		set_dirty();
	}
}

void ttext_box::set_value(const std::string& label) 
{ 
	set_label(tintegrate::stuff_escape(label)); 
}

std::string ttext_box::get_value() const
{ 
	return tintegrate::drop_escape(label_);
}

void ttext_box::goto_end_of_data(const bool select)
{ 
	SDL_Rect end = integrate_->editable_at(get_width(), get_height());
	set_cursor(end, select);
}

void ttext_box::set_cursor(const SDL_Rect& offset, const bool select)
{
	if (select) {
		if (offset == selection_start_) {
			selection_end_ = ::create_rect(-1, -1, 0, 0);
		} else {
			selection_end_ = offset;
		}
#ifdef __unix__
		// selecting copies on UNIX systems.
		copy_selection(true);
#endif
	} else {
		selection_start_ = offset;
		if (selection_end_.x != -1) {
			selection_end_ = ::create_rect(-1, -1, 0, 0);
		}
	}

	if (mouse_moved_callback_) {
		mouse_moved_callback_(this);
	}
	update_canvas();
	set_dirty();
}

void ttext_box::goto_start_of_data(const bool select)
{ 
	SDL_Rect offset = integrate_->holden_rect(0, 0);
	set_cursor(offset, select); 
}

void ttext_box::select_all() 
{ 
	selection_start_ = integrate_->holden_rect(0, 0);
	goto_end_of_data(true); 
}

void ttext_box::insert_str(const std::string& str)
{
	if (str.empty()) {
		return;
	}

	delete_selection();

	if (maximum_length_ != std::string::npos && integrate_->at_end(selection_start_.x, selection_start_.y)) {
		std::string tmp_text = tintegrate::drop_escape(label_);
		if (utils::utf8str_len(tmp_text) >= maximum_length_) {
			return;
		}
	}
	std::string str2;
	if (str.size() == 1 && str[0] == '\n' && (label_.empty() || (label_[label_.size() - 1] != '\n' && integrate_->at_end(selection_start_.x, selection_start_.y)))) {
		str2 = "\n\n";
	} else {
		str2 = tintegrate::stuff_escape(str);
	}

	SDL_Rect new_start;
	const std::string& text = integrate_->insert_str(true, selection_start_.x, selection_start_.y, str2, new_start);
	// after delete, start maybe in end outer.
	selection_start_ = new_start;

	set_label(text);

	set_cursor(selection_start_, false);
}

void ttext_box::insert_img(const std::string& str)
{
	if (str.empty()) {
		return;
	}

	delete_selection();

	if (maximum_length_ != std::string::npos && integrate_->at_end(selection_start_.x, selection_start_.y)) {
		std::string tmp_text = tintegrate::drop_escape(label_);
		if (utils::utf8str_len(tmp_text) >= maximum_length_) {
			return;
		}
	}
	
	std::string str2 = tintegrate::generate_img(str);
	SDL_Rect new_start;
	const std::string& text = integrate_->insert_str(false, selection_start_.x, selection_start_.y, str2, new_start);
	// after delete, start maybe in end outer.
	selection_start_ = new_start;

	set_label(text);

	set_cursor(selection_start_, false);
}

int ttext_box::get_src_pos() const
{
	if (!integrate_) {
		return 0;
	}
	return integrate_->calculate_src_pos(selection_start_.x, selection_start_.y);
}

void ttext_box::set_src_pos(int pos)
{
	src_pos_ = pos;
	if (src_pos_ != -1 && integrate_) {
		selection_start_ = integrate_->calculate_cursor(src_pos_);
		src_pos_ = -1;
		set_cursor(selection_start_, false);
	}
}

void ttext_box::copy_selection(const bool mouse)
{
	if (!exist_selection()) {
		return;
	}

	SDL_Rect start, end;
	normalize_start_end(start, end);

	std::string text = integrate_->handle_selection(start.x, start.y, end.x, end.y, NULL);
	if (!text.empty()) {
		conv_ansi_utf8(text, false);
		copy_to_clipboard(text, mouse);
	}
}

void ttext_box::paste_selection(const bool mouse)
{
	std::string text = copy_from_clipboard(mouse);
	if (text.empty()) {
		return;
	}
	// on window: text is ansi format
	conv_ansi_utf8(text, true);

	insert_str(text);
	fire(event::NOTIFY_MODIFIED, *this, NULL);
}

void ttext_box::normalize_start_end(SDL_Rect& start, SDL_Rect& end) const
{
	bool normal = selection_end_.x < 0 || 
		(selection_end_.y > selection_start_.y || (selection_end_.y == selection_start_.y && selection_end_.x >= selection_start_.x));

	start = normal? selection_start_: selection_end_;
	end = normal? selection_end_: selection_start_;
}

void ttext_box::calculate_integrate()
{
	tcontrol::calculate_integrate();
	if (integrate_) {
		if (src_pos_ == -1) {
			selection_start_ = integrate_->editable_at(selection_start_.x, selection_start_.y);
		} else {
			selection_start_ = integrate_->calculate_cursor(src_pos_);
			src_pos_ = -1;
		}
		if (exist_selection()) {
			selection_end_ = integrate_->editable_at(selection_end_.x, selection_end_.y);
		}
	}
}

void ttext_box::set_state(const tstate state)
{
	if (!cursor_timer_ && state == FOCUSSED) {
		cursor_timer_ = add_timer(500, boost::bind(&ttext_box::cursor_timer_handler, this), true);
	}

	if (state != state_) {
		state_ = state;
		set_dirty(true);
	}
}

void ttext_box::handle_key_left_arrow(SDLMod modifier, bool& handled)
{
	/** @todo implement the ctrl key. */
	DBG_GUI_E << LOG_SCOPE_HEADER << '\n';

	handled = true;

	if (label_.empty()) {
		return;
	}
	if (!selection_start_.x && !selection_start_.y) {
		return;
	}

	SDL_Rect new_start;
	integrate_->handle_char(false, selection_start_.x, selection_start_.y, true, new_start);
	set_cursor(new_start, (modifier & KMOD_SHIFT) != 0);
}

void ttext_box::handle_key_right_arrow(SDLMod modifier, bool& handled)
{
	/** @todo implement the ctrl key. */
	DBG_GUI_E << LOG_SCOPE_HEADER << '\n';

	handled = true;
	if (label_.empty()) {
		return;
	}

	SDL_Rect new_start;
	integrate_->handle_char(false, selection_start_.x, selection_start_.y, false, new_start);
	set_cursor(new_start, (modifier & KMOD_SHIFT) != 0);
}

void ttext_box::handle_key_up_arrow(SDLMod modifier, bool& handled)
{
	/** @todo implement the ctrl key. */
	DBG_GUI_E << LOG_SCOPE_HEADER << '\n';

	handled = true;

	if (label_.empty()) {
		return;
	}
	if (!selection_start_.y) {
		return;
	}

	SDL_Rect new_selection_start = integrate_->key_arrow(selection_start_.x, selection_start_.y, true);
	set_cursor(new_selection_start, (modifier & KMOD_SHIFT) != 0);
}

void ttext_box::handle_key_down_arrow(SDLMod modifier, bool& handled)
{
	/** @todo implement the ctrl key. */
	DBG_GUI_E << LOG_SCOPE_HEADER << '\n';

	handled = true;

	if (label_.empty()) {
		return;
	}

	SDL_Rect new_selection_start = integrate_->key_arrow(selection_start_.x, selection_start_.y, false);
	set_cursor(new_selection_start, (modifier & KMOD_SHIFT) != 0);
}
void ttext_box::handle_key_home(SDLMod modifier, bool& handled)
{
	DBG_GUI_E << LOG_SCOPE_HEADER << '\n';

	handled = true;
	if(modifier & KMOD_CTRL) {
		goto_start_of_data((modifier & KMOD_SHIFT) != 0);
	} else {
		goto_start_of_line((modifier & KMOD_SHIFT) != 0);
	}
}

void ttext_box::handle_key_end(SDLMod modifier, bool& handled)
{
	DBG_GUI_E << LOG_SCOPE_HEADER << '\n';

	handled = true;
	if(modifier & KMOD_CTRL) {
		goto_end_of_data((modifier & KMOD_SHIFT) != 0);
	} else {
		goto_end_of_line((modifier & KMOD_SHIFT) != 0);
	}
}

void ttext_box::handle_key_backspace(SDLMod /*modifier*/, bool& handled)
{
	DBG_GUI_E << LOG_SCOPE_HEADER << '\n';

	handled = true;
	if (exist_selection()) {
		delete_selection();
	} else if (selection_start_.x || selection_start_.y){
		delete_char(true);
	}
	fire(event::NOTIFY_MODIFIED, *this, NULL);
}

void ttext_box::handle_key_delete(SDLMod /*modifier*/, bool& handled)
{
	DBG_GUI_E << LOG_SCOPE_HEADER << '\n';

	handled = true;
	if (exist_selection()) {
		delete_selection();
	} else if (!label_.empty()) {
		delete_char(false);
	}
	fire(event::NOTIFY_MODIFIED, *this, NULL);
}

Uint16 shift_character(SDLMod modifier, Uint16 c)
{
	if (c == '\r') {
		return '\n';
	}
	int shifted = !!(modifier & KMOD_SHIFT);
	int capslock = !!(modifier & KMOD_CAPS);
	if (!(shifted ^ capslock)) {
		return c;
	}

	if (c >= 'a' && c <= 'z') {
		return 'A' + (c - 'a');
	} else if (c >= 0x30 && c <= 0x39) {
		if (c == 0x30) {
			return 0x29;
		} else if (c == 0x31) {
			return 0x21;
		} else if (c == 0x32) {
			return 0x40;
		} else if (c == 0x33) {
			return 0x23;
		} else if (c == 0x34) {
			return 0x24;
		} else if (c == 0x35) {
			return 0x25;
		} else if (c == 0x36) {
			return 0x5e;
		} else if (c == 0x37) {
			return 0x26;
		} else if (c == 0x38) {
			return 0x2a;
		} else {
			return 0x28;
		}
	} else if (c == '`') {
		return 0x7e;
	} else if (c == '-') {
		return 0x5f;
	} else if (c == '=') {
		return 0x2b;
	} else if (c == '[') {
		return 0x7b;
	} else if (c == ']') {
		return 0x7d;
	} else if (c == '\\') {
		return 0x7c;
	} else if (c == ';') {
		return 0x3a;
	} else if (c == '\'') {
		return 0x22;
	} else if (c == ',') {
		return 0x3c;
	} else if (c == '.') {
		return 0x3e;
	} else if (c == '/') {
		return 0x3f;
	}
	return c;
}

void ttext_box::handle_key_default(
		bool& handled, SDLKey key, SDLMod modifier, Uint16)
{
#if defined(_WIN32)
	if (!(modifier & KMOD_SHIFT) || key != '\r') {
		return;
	}
#endif

	if (key == '\r' || (key >= 32 && key < 127)) {
		handled = true;

		// sdl.dll can shift 'a' to 'z', other don't.
		int modified = shift_character(modifier, key);
		std::string str(1, modified);
		insert_str(str);
		fire(event::NOTIFY_MODIFIED, *this, NULL);
	}
}

void ttext_box::signal_handler_middle_button_click(
			const event::tevent event, bool& handled)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";

	paste_selection(true);

	handled = true;
}

void ttext_box::signal_handler_sdl_key_down(const event::tevent event
		, bool& handled
		, const SDLKey key
		, SDLMod modifier
		, const Uint16 unicode)
{

	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";

// For copy/paste we use a different key on the MAC. Other ctrl modifiers won't
// be modifed seems not to be required when I read the comment in
// widgets/textbox.cpp:516. Would be nice if somebody on a MAC would test it.
#ifdef __APPLE__
	const unsigned copypaste_modifier = SDLK_LGUI |SDLK_RGUI;
#else
	const unsigned copypaste_modifier = KMOD_CTRL;
#endif

	switch(key) {

		case SDLK_LEFT :
			handle_key_left_arrow(modifier, handled);
			break;

		case SDLK_RIGHT :
			handle_key_right_arrow(modifier, handled);
			break;

		case SDLK_UP :
			handle_key_up_arrow(modifier, handled);
			break;

		case SDLK_DOWN :
			handle_key_down_arrow(modifier, handled);
			break;

		case SDLK_PAGEUP :
			handle_key_page_up(modifier, handled);
			break;

		case SDLK_PAGEDOWN :
			handle_key_page_down(modifier, handled);
			break;

		case SDLK_a :
			if(!(modifier & KMOD_CTRL)) {
				handle_key_default(handled, key, modifier, unicode);
				break;
			}

			// If ctrl-a is used for home drop the control modifier
			modifier = static_cast<SDLMod>(modifier &~ KMOD_CTRL);
			/* FALL DOWN */

		case SDLK_HOME :
			handle_key_home(modifier, handled);
			break;

		case SDLK_e :
			if(!(modifier & KMOD_CTRL)) {
				handle_key_default(handled, key, modifier, unicode);
				break;
			}

			// If ctrl-e is used for end drop the control modifier
			modifier = static_cast<SDLMod>(modifier &~ KMOD_CTRL);
			/* FALL DOWN */

		case SDLK_END :
			handle_key_end(modifier, handled);
			break;

		case SDLK_BACKSPACE :
			handle_key_backspace(modifier, handled);
			break;

		case SDLK_u :
			if(modifier & KMOD_CTRL) {
				handle_key_clear_line(modifier, handled);
			} else {
				handle_key_default(handled, key, modifier, unicode);
			}
			break;

		case SDLK_DELETE :
#if defined(__APPLE__) && TARGET_OS_IPHONE
			handle_key_backspace(modifier, handled);
#else
			handle_key_delete(modifier, handled);
#endif
			break;

#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
        case SDLK_RETURN:
            SDL_StopTextInput();
            break;
#endif
            
		case SDLK_c :
			if(!(modifier & copypaste_modifier)) {
				handle_key_default(handled, key, modifier, unicode);
				break;
			}

			// atm we don't care whether there is something to copy or paste
			// if nothing is there we still don't want to be chained.
			copy_selection(false);
			handled = true;
			break;

		case SDLK_x :
			if(!(modifier & copypaste_modifier)) {
				handle_key_default(handled, key, modifier, unicode);
				break;
			}

			copy_selection(false);
			delete_selection();
			handled = true;
			break;

		case SDLK_v :
			if(!(modifier & copypaste_modifier)) {
				handle_key_default(handled, key, modifier, unicode);
				break;
			}

			paste_selection(false);
			handled = true;
			break;

		default :
			handle_key_default(handled, key, modifier, unicode);

	}

	hide_cursor_ = false;
	forbid_hide_ticks_ = SDL_GetTicks() + 200;
	cursor_timer_handler();
}

void ttext_box::signal_handler_sdl_text_input(const event::tevent event
		, bool& handled
		, const char* text)
{
	// for windows, ascii char will send WM_KEYDOWN and WM_CHAR,
	// WM_KEYDOWN result to call signal_handler_sdl_key_down,
	// WM_CHAR result to call signal_handler_sdl_text_input,
	// so ascii will generate two input. 
	const std::string str = text;
	bool inserted = false;
	utils::utf8_iterator ch(str);
	for (utils::utf8_iterator end = utils::utf8_iterator::end(str); ch != end; ++ch) {
		if (*ch <= 0x7f) {
#if defined(ANDROID) || defined(_WIN32)
			// on Android, some ASCII is receive by textinput insteal keydown.
			// but some controller char is by keydown, rt, del, etc.
#else
			continue;
#endif
		}
		if (*ch == '\r') {
			insert_str("\n");
		} else {
			insert_str(utils::wchar_to_string(*ch));
		}
		inserted = true;
	}
	if (inserted) {
		fire(event::NOTIFY_MODIFIED, *this, NULL);
	}

	handled = true;
}

void ttext_box::signal_handler_receive_keyboard_focus(const event::tevent event)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";

	set_state(FOCUSSED);
}

void ttext_box::signal_handler_lose_keyboard_focus(const event::tevent event)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";

	set_state(ENABLED);
}

void ttext_box::place(const tpoint& origin, const tpoint& size)
{
	// Inherited.
	tcontrol::place(origin, size);

	update_offsets();
}

void ttext_box::cursor_timer_handler()
{
	if (SDL_GetTicks() > forbid_hide_ticks_) {
		hide_cursor_ = !hide_cursor_;
	}
	BOOST_FOREACH(tcanvas& tmp, canvas()) {
		tmp.set_variable("cursor_height", variant(hide_cursor_? 0: text_height_));
	}
	set_dirty();
}

unsigned ttext_box::cursor_height() const 
{ 
	return text_height_;
}

void ttext_box::update_canvas()
{
	/***** Gather the info *****/
	const int max_width = get_text_maximum_width();
	const int max_height = get_text_maximum_height();

	// Set the cursor info.
	SDL_Rect start, end;
	normalize_start_end(start, end);

	int cursor_offset_y = 0;
	const SDL_Rect& cursor_rect = selection_end_.x >= 0? selection_end_: selection_start_;

	int tmp = (cursor_rect.h - (int)text_height_) / 2;
	int tmp2 = cursor_rect.y + tmp;

	if (!integrate_ || integrate_->align_bottom()) {
		cursor_offset_y = cursor_rect.y + (cursor_rect.h - (int)text_height_);
	} else {
		cursor_offset_y = cursor_rect.y + (cursor_rect.h - (int)text_height_) / 2;
	}
	
	if (cursor_offset_y < 0) {
		cursor_offset_y = 0;
	}

	int selection_width_i = 0;
	int selection_height_i = start.h;
	if (end.x >= 0) {
		if (start.y == end.y) {
			selection_width_i = end.x - start.x;
		} else {
			selection_width_i = max_width - start.x; 
		}
	}

	int selection_height_ii = 0;

	int selection_width_iii = 0;
	int selection_height_iii = 0;
	if (end.x >= 0) {
		if (start.y != end.y) {
			selection_height_ii = end.y - start.y - selection_height_i;

			selection_width_iii = end.x;

			selection_height_iii = end.h;
		}
	}

	/***** Set in all canvases *****/
	
	BOOST_FOREACH(tcanvas& tmp, canvas()) {

		tmp.set_variable("text", variant(label()));
		tmp.set_variable("text_x_offset", variant(text_x_offset_));
		tmp.set_variable("text_y_offset", variant(text_y_offset_));
		tmp.set_variable("text_maximum_width", variant(max_width));
		tmp.set_variable("text_maximum_height", variant(max_height));
		tmp.set_variable("text_editable", variant(get_text_editable()));

		tmp.set_variable("cursor_offset_x", variant(selection_end_.x >= 0? selection_end_.x: selection_start_.x));
		tmp.set_variable("cursor_offset_y", variant(cursor_offset_y));
		tmp.set_variable("cursor_height", variant(hide_cursor_? 0: text_height_));

		tmp.set_variable("selection_offset_x_i", variant(start.x));
		tmp.set_variable("selection_offset_y_i", variant(start.y));
		tmp.set_variable("selection_width_i", variant(selection_width_i));
		tmp.set_variable("selection_height_i", variant(selection_height_i));

		tmp.set_variable("selection_height_ii", variant(selection_height_ii));

		tmp.set_variable("selection_width_iii", variant(selection_width_iii));
		tmp.set_variable("selection_height_iii", variant(selection_height_iii));
	}
}

void ttext_box::delete_char(const bool backspace)
{
	if (label_.empty()) {
		return;
	}

	SDL_Rect start, end;
	normalize_start_end(start, end);

	SDL_Rect new_start;
	const std::string& text = integrate_->handle_char(true, start.x, start.y, backspace, new_start);
	if (text != label()) {
		selection_start_ = new_start;
		set_label(text);
		// repoint
		set_cursor(selection_start_, false);
	}
}

void ttext_box::delete_selection()
{
	if (!exist_selection()) {
		return;
	}

	SDL_Rect start, end;
	normalize_start_end(start, end);

	SDL_Rect new_start;
	const std::string& text = integrate_->handle_selection(start.x, start.y, end.x, end.y, &new_start);
	// after delete, start maybe in end outer.
	selection_start_ = new_start;
	set_label(text);

	set_cursor(selection_start_, false);
}

void ttext_box::handle_mouse_selection(tpoint mouse, const bool start_selection)
{
	if (label_.empty()) {
        return;
    }

	hide_cursor_ = false;
	forbid_hide_ticks_ = SDL_GetTicks() + 200;
	cursor_timer_handler();

	mouse.x -= get_x();
	mouse.y -= get_y();
	// FIXME we don't test for overflow in width
	if (mouse.x < static_cast<int>(text_x_offset_) || mouse.y < static_cast<int>(text_y_offset_)) {
		return;
	}

	SDL_Rect offset = integrate_->editable_at(mouse.x - text_x_offset_, mouse.y - text_y_offset_);
	if (offset.x < 0 || offset.y < 0) {
		return;
	}

	set_cursor(offset, !start_selection);
	dragging_ |= start_selection;
}

void ttext_box::update_offsets()
{
	assert(config());

	boost::intrusive_ptr<const ttext_box_definition::tresolution> conf =
		boost::dynamic_pointer_cast
		<const ttext_box_definition::tresolution>(config());

	assert(conf);

	text_height_ = font::get_max_height(conf->text_font_size);

	game_logic::map_formula_callable variables;
	variables.add("height", variant(get_height()));
	variables.add("width", variant(get_width()));
	variables.add("text_font_height", variant(text_height_));

	text_x_offset_ = conf->text_x_offset(variables);
	text_y_offset_ = conf->text_y_offset(variables);

	// Since this variable doesn't change set it here instead of in
	// update_canvas().
	BOOST_FOREACH(tcanvas& tmp, canvas()) {
		tmp.set_variable("text_font_height", variant(text_height_));
	}

 	// Force an update of the canvas since now text_font_height is known.
	update_canvas();
}

void ttext_box::handle_key_clear_line(SDLMod /*modifier*/, bool& handled)
{
	handled = true;

	set_label("");
}

void ttext_box::load_config_extra()
{
	assert(config());

	boost::intrusive_ptr<const ttext_box_definition::tresolution> conf =
		boost::dynamic_pointer_cast
		<const ttext_box_definition::tresolution>(config());

	assert(conf);

	update_offsets();
}

const std::string& ttext_box::get_control_type() const
{
	static const std::string type = "text_box";
	return type;
}

void ttext_box::signal_handler_mouse_motion(
		const event::tevent event, bool& handled, const tpoint& coordinate)
{
	DBG_GUI_E << get_control_type() << "[" << id() << "]: " << event << ".\n";

	if(dragging_) {
		handle_mouse_selection(coordinate, false);
	}

	handled = true;
}

void ttext_box::signal_handler_left_button_down(
		const event::tevent event, bool& handled)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";

	/*
	 * Copied from the base class see how we can do inheritance with the new
	 * system...
	 */
	get_window()->keyboard_capture(this);
	get_window()->mouse_capture();

	handle_mouse_selection(get_mouse_position(), true);

	handled = true;

#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	SDL_StartTextInput();
#endif

}

void ttext_box::signal_handler_left_button_up(
		const event::tevent event, bool& handled)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";

	dragging_ = false;
	handled = true;
}

void ttext_box::signal_handler_left_button_double_click(
		const event::tevent event, bool& handled)
{
	return;

	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";

	select_all();

	handled = true;
}

} //namespace gui2

