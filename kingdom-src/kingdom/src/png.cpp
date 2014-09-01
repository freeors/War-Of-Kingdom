/* This is a PNG image file loading framework */

#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>

#include "sdl_utils.hpp"

#include "png.h"

int write_png_file(const char* file_name, const surface& surf)
{
	surface nsurf(make_neutral_surface(surf));
	if (nsurf == NULL) {
		return -1;
	}

	const bool has_alpha = true;
	const int depth = 8;
	int j, i, temp, pos;
	png_byte color_type; 
	png_structp png_ptr;
	png_infop info_ptr; 
	png_bytep* row_pointers;
	// create file 
	FILE *fp = fopen(file_name, "wb");
	if (fp == NULL) {
		return -1;
	}

	// initialize stuff 
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL); 
	info_ptr = png_create_info_struct(png_ptr);
	setjmp(png_jmpbuf(png_ptr));
	png_init_io(png_ptr, fp); 
	// write header 
	if (setjmp(png_jmpbuf(png_ptr))) {
		// "[write_png_file] Error during writing header";
		return -1;
	}

	if (has_alpha) {
		color_type = PNG_COLOR_TYPE_RGB_ALPHA;
	} else {
		color_type = PNG_COLOR_TYPE_RGB;
	}

	png_set_IHDR(png_ptr, info_ptr, surf->w, surf->h,
		depth, color_type, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE); 
	png_write_info(png_ptr, info_ptr); 
	// write bytes 
	if (setjmp(png_jmpbuf(png_ptr))) {
		// "[write_png_file] Error during writing bytes";
		return -1;
	}
	if (has_alpha) {
		temp = (4 * surf->w);
	} else {
		temp = (3 * surf->w);
	}

	{
		surface_lock lock(nsurf);
		Uint32* beg = lock.pixels();
		Uint32* end = beg + nsurf->w * surf->h;


		pos = 0;
		row_pointers = (png_bytep*)malloc(surf->h * sizeof(png_bytep));
		for (i = 0; i < surf->h; i++) {
			row_pointers[i] = (png_bytep)malloc(sizeof(unsigned char) * temp);

			for (j = 0; j < temp; j += 4, ++ beg) {
				row_pointers[i][j] = (*beg) >> 16; // red
				row_pointers[i][j+1] = (*beg) >> 8; // green
				row_pointers[i][j+2] = (*beg) >> 0; // blue
				if (has_alpha) {
					row_pointers[i][j+3] = (*beg) >> 24; // alpha
				}
			}
		}
	}

	png_write_image(png_ptr, row_pointers); // end write 
	if (setjmp(png_jmpbuf(png_ptr))) {
		// "[write_png_file] Error during end of write";
		return -1;
	}
	png_write_end(png_ptr, NULL); // cleanup heap allocation 
	for (j = 0; j < surf->h; j ++) {
		free(row_pointers[j]);
	}
	free(row_pointers);
	fclose(fp);
	return 0;
}

