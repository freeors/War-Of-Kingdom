/* $Id: loadscreen.hpp 47261 2010-10-28 21:06:14Z mordante $ */
/*
   Copyright (C) 2005 - 2010 by Joeri Melis <joeri_melis@hotmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/** @file */

#ifndef JM_LOADSCREEN_HPP
#define JM_LOADSCREEN_HPP

class CVideo;
class config;

#include "sdl_utils.hpp"

class loadscreen {
	public:
		// Preferred constructor
		explicit loadscreen(CVideo &screen, const int &percent = 0);
		// Keep default copy constructor
		// Keep default copy assignment
		// Destructor, dumps the counter values to stderr
		~loadscreen()
		{
			dump_counters();
		}

	/**
	 * Starts the stage with identifier @a id.
	 */
	static void start_stage(char const *id);

	/**
	 * Increments the current stage for the progress bar.
	 */
	static void increment_progress();

		/** Function to draw a blank screen. */
		void clear_screen();

		/**
		 * A global loadscreen instance that can be used to avoid
		 * passing it on to functions that are many levels deep.
		 */
		static loadscreen *global_loadscreen;

		struct global_loadscreen_manager {
			explicit global_loadscreen_manager(CVideo& screen);
			~global_loadscreen_manager();
			static global_loadscreen_manager* get()
			{ return manager; }
			void reset();
private:
			static global_loadscreen_manager* manager;
			bool owns;
		};
private:
	/**
	 * Displays a load progress bar.
	 */
	void draw_screen(const std::string &text);

		// Prohibit default constructor
		loadscreen();

		// Data members
		CVideo &screen_;
		SDL_Rect textarea_;
		surface logo_surface_;
		bool logo_drawn_;
		int pby_offset_;
		int prcnt_;

		void dump_counters() const;
};

class set_increment_progress 
{
public:
	typedef void (* fn_increment_progress)(std::string const &name, uint32_t param1, void* ctx);

	static fn_increment_progress increment_progress;
	static void* ctx;

	set_increment_progress(fn_increment_progress fn, void* ctx);
	~set_increment_progress();
private:
	fn_increment_progress old_;
};

void increment_preprocessor_progress(std::string const &name, bool is_file);

void wml_config_to_file(const std::string &fname, config &cfg, uint32_t nfiles = 0, uint32_t sum_size = 0, uint32_t modified = 0);
void wml_config_from_file(const std::string &fname, config &cfg, uint32_t* nfiles = NULL, uint32_t* sum_size = NULL, uint32_t* modified = NULL);
bool wml_checksum_from_file(const std::string &fname, uint32_t* nfiles = NULL, uint32_t* sum_size = NULL, uint32_t* modified = NULL);
unsigned char calcuate_xor_from_file(const std::string &fname);

#endif
