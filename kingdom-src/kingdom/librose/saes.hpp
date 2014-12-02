/* $Id: unit_id.hpp 46186 2010-09-01 21:12:38Z silene $ */
/*
   Copyright (C) 2008 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/


#ifndef SAES_HPP_INCLUDED
#define SAES_HPP_INCLUDED

void saes_encrypt_stream(const unsigned char* ptext, int size, unsigned char* w, char* ctext);
void saes_decrypt_stream(const unsigned char* ctext, int size, unsigned char* w, char* ptext);

#endif
