/* $Id: network_worker.hpp 47827 2010-12-05 18:09:12Z mordante $ */
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

#ifndef NETWORK_WORKER_HPP_INCLUDED
#define NETWORK_WORKER_HPP_INCLUDED

#include <map>
#include "network.hpp"

/**
 * Aligns a variable on a 4 byte boundary.
 *
 * The address needs to be aligned on a Sparc system, if it's not aligned the
 * SDLNet_Read32 call will cause a SIGBUS and the server will be terminated [1].
 * Best use this alignment for all buffers used in for the SDL_Net calls.
 *
 * [1] http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=426318
 */
#ifdef __GNUC__
#define ALIGN_4 __attribute__ ((aligned (4)))
#else
#define ALIGN_4
#endif

struct textendable_buf
{
	textendable_buf()
		: data(NULL)
		, size(0)
		, vsize(0)
	{}
	~textendable_buf()
	{
		if (data) {
			free(data);
		}
	}

	void put(const void* src, int src_size)
	{
		if (!src || src_size <= 0) {
			return;
		}

		if (src_size > size) {
			char* tmp = (char*)malloc(src_size);
			if (data) {
				if (vsize) {
					memcpy(tmp, data, vsize);
				}
				free(data);
			}
			data = tmp;
			size = src_size;
		}
		memcpy(data, src, src_size);
		vsize = src_size;
	}

	char* data;
	int size;
	int vsize;
};

namespace network_worker_pool
{

struct manager
{
	explicit manager(size_t min_threads,size_t max_threads);
	~manager();

private:
	manager(const manager&);
	void operator=(const manager&);

	bool active_;
};

network::pending_statistics get_pending_stats();

void set_raw_data_only();
void set_use_system_sendfile(bool);

/** Function to asynchronously received data to the given socket. */
void receive_data(TCPsocket sock);

TCPsocket get_received_data(TCPsocket sock, config& cfg, network::bandwidth_in_ptr&);

TCPsocket get_received_data(TCPsocket sock, std::vector<char>& buf);

void queue_file(TCPsocket sock, const std::string&);

size_t queue_data(TCPsocket sock, const config& buf, const std::string& packet_type);
bool is_locked(const TCPsocket sock);
void close_socket(TCPsocket sock);
TCPsocket detect_error();

std::pair<network::statistics, network::statistics> get_current_transfer_stats(TCPsocket sock);
}

#endif
