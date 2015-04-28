/* $Id: editor_display.hpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
/*
   Copyright (C) 2008 - 2010 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef LIBROSE_BASE_INSTANCE_HPP_INCLUDED
#define LIBROSE_BASE_INSTANCE_HPP_INCLUDED

#include "rose_config.hpp"
#include "display.hpp"
#include "hero.hpp"
#include "preferences.hpp"
#include "sound.hpp"
#include "filesystem.hpp"
#include "serialization/preprocessor.hpp"
#include "config_cache.hpp"

class animation;

class base_instance
{
public:
	base_instance(int argc, char** argv, int sample_rate = 44100, size_t sound_buffer_size = 4096);
	virtual ~base_instance();

	bool init_language();
	bool init_video();
	virtual bool init_config(const bool force);

	display& disp();
	const config& game_config() const { return game_config_; }
	bool is_loading() { return false; }

	bool change_language();
	virtual int show_preferences_dialog(display& disp, bool first);

	virtual void regenerate_heros(hero_map& heros, bool allow_empty);
	hero_map& heros() { return heros_; }

	virtual void fill_anim_tags(std::map<const std::string, int>& tags) {};
	virtual void fill_anim(int at, const std::string& id, bool area, bool tpl, const config& cfg);

	void clear_anims();
	const std::multimap<std::string, const config>& utype_anim_tpls() const { return utype_anim_tpls_; }
	const animation* anim(int at) const;

	void init_locale();

protected:
	virtual void load_config() {}
	virtual void load_config2(const config& cfg) {}
	virtual void load_game_cfg(const bool force);

private:
	virtual void app_init_locale(const std::string& intl_dir) {}

protected:
	//this should get destroyed *after* the video, since we want
	//to clean up threads after the display disappears.
	const threading::manager thread_manager;

	surface icon_;
	CVideo video_;
	hero_map heros_;

	int force_bpp_;
	const font::manager font_manager_;
	const preferences::base_manager prefs_manager_;
	const image::manager image_manager_;
	const events::event_context main_event_context_;
	sound::music_thinker music_thinker_;
	binary_paths_manager paths_manager_;

	util::scoped_ptr<display> disp_;

	config game_config_;
	config game_config_core_;
	preproc_map old_defines_map_;
	game_config::config_cache& cache_;

	std::map<int, animation*> anims_;
	std::multimap<std::string, const config> utype_anim_tpls_;
};

extern base_instance* instance;

// Don't call virtual function during class construct function.
template<class T>
T& create_instance(int argc, char** argv)
{
	static T game(argc, argv);
	instance = &game;
	game.init_locale();
	return game;
}

#endif
