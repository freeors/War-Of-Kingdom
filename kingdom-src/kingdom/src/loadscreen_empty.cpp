/* $Id: loadscreen_empty.cpp 46186 2010-09-01 21:12:38Z silene $ */
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

#ifndef ANDROID

#include "loadscreen.hpp"

void loadscreen::start_stage(char const *) {}
void loadscreen::increment_progress() {}
void loadscreen::clear_screen() {}

loadscreen *loadscreen::global_loadscreen = 0;

void increment_set_config_progress () {}

void increment_parser_progress () {}

set_increment_progress::fn_increment_progress set_increment_progress::increment_progress = NULL;
void* set_increment_progress::ctx = NULL;

set_increment_progress::set_increment_progress(fn_increment_progress fn, void* ctx) :
	old_(increment_progress)
{
	set_increment_progress::increment_progress = fn;
	set_increment_progress::ctx = ctx;
}

set_increment_progress::~set_increment_progress()
{
	set_increment_progress::increment_progress = old_;
}

void increment_preprocessor_progress(std::string const &name, bool is_file)
{
	if (set_increment_progress::increment_progress) {
		set_increment_progress::increment_progress(name, is_file, set_increment_progress::ctx);
	}
}

#endif