/* $Id: saes.cpp 46186 2010-09-01 21:12:38Z silene $ */
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

#include "saes.hpp"

// Round keys: K0 = w0 + w1; K1 = w2 + w3; K2 = w4 + w5

// ##################### S-Box ######################
unsigned char sBox[] = {0x9, 0x4, 0xa, 0xb, 0xd, 0x1, 0x8, 0x5,
	0x6, 0x2, 0x0, 0x3, 0xc, 0xe, 0xf, 0x7};
// ##################################################

// ################ Inverse S-Box ###################
unsigned char sBoxI[] = {0xa, 0x5, 0x9, 0xb, 0x1, 0x7, 0x8, 0xf,
	0x6, 0x0, 0x2, 0x3, 0xc, 0x4, 0xd, 0xe};
// ################################################## 


// ################ Multiplcation function ################# 
unsigned char mult(unsigned char p1, unsigned char p2)
{
	// Multiply two polynomials in GF(2^4)/x^4 + x + 1
	unsigned char x = 0;
	while (p2) {
		if (p2 & 0x1) {
			x ^= p1;
		}
		p1 <<= 1;
		if (p1 & 0x10) {
			p1 ^= 0x3;
		}
		p2 >>= 1;
	}
	return x & 0xf;
}

unsigned char* intToVec(unsigned short n, unsigned char* vec)
{
	// Convert a 2-byte integer into a 4-element vector
	vec[0] = n >> 12; vec[1] = (n >> 4) & 0xf; vec[2] = (n >> 8) & 0xf; vec[3] = n & 0xf;
	return vec;
}

unsigned short vecToInt(unsigned char* m)
{
    // Convert a 4-element vector into 2-byte integer
    return (m[0] << 12) + (m[2] << 8) + (m[1] << 4) + m[3];
}

// #######################Adds Key########################## 
unsigned char* addKey(unsigned char* s1, unsigned char* s2)
{
	// Add two keys in GF(2^4)"""
	int i;
	for (i = 0; i < 4; i ++) {
		s2[i] = s1[i] ^ s2[i];
	}
	return s2;
}

// ############## Byte substitution function ###############     
unsigned char* sub4NibList(unsigned char* sbox, unsigned char* s)
{
	// Nibble substitution function
	int i = 0;
	for (i = 0; i < 4; i ++) {
		s[i] = sbox[s[i]];
	}
	return s;
}

// ###################### Shift Rows #######################
unsigned char* shiftRow(unsigned char* s)
{
	// ShiftRow function
	unsigned char t = s[2];
	s[2] = s[3];
	s[3] = t;
	return s;
}

// ##################### Key Expansion ##################### 
// This macro swaps and substitutes using sBox
#define sBoxNib(b)		sBox[(b) >> 4] + (sBox[(b) & 0x0f] << 4)
void keyExpansion(unsigned short key, unsigned char* w)
{
	// Generate the three round keys
	unsigned char Rcon1 = 0x80;
	unsigned char Rcon2 = 0x30;
	w[0] = (key & 0xff00) >> 8;
	w[1] = key & 0x00ff;
	w[2] = w[0] ^ Rcon1 ^ sBoxNib(w[1]);
	w[3] = w[2] ^ w[1];
	w[4] = w[2] ^ Rcon2 ^ sBoxNib(w[3]);
	w[5] = w[4] ^ w[3];
} 

unsigned char* mixCol(unsigned char* s, unsigned char* s1)
{
	s1[0] = s[0] ^ mult(4, s[2]);
	s1[1] = s[1] ^ mult(4, s[3]);
	s1[2] = s[2] ^ mult(4, s[0]);
	s1[3] = s[3] ^ mult(4, s[1]);
	return s1;
}

// ##################### ENCRYPTION #########################
unsigned short saes_encrypt16b(unsigned short ptext, unsigned char* w)
{
	unsigned char state[4] = {0, 0, 0, 0}, state1[4] = {0, 0, 0, 0};
	// Encrypt plaintext block
	intToVec(((w[0] << 8) + w[1]) ^ ptext, state);
	mixCol(shiftRow(sub4NibList(sBox, state)), state1);
	addKey(intToVec((w[2] << 8) + w[3], state), state1);
	shiftRow(sub4NibList(sBox, state1));
	return vecToInt(addKey(intToVec((w[4] << 8) + w[5], state), state1));
}

// ###################### DECRYPTION ########################
unsigned char* iMixCol(unsigned char* s, unsigned char* s1)
{
	s1[0] = mult(9, s[0]) ^ mult(2, s[2]);
	s1[1] = mult(9, s[1]) ^ mult(2, s[3]);
	s1[2] = mult(9, s[2]) ^ mult(2, s[0]);
	s1[3] = mult(9, s[3]) ^ mult(2, s[1]);
	return s1;
}

unsigned short saes_decrypt16b(unsigned short ctext, unsigned char* w)
{
	unsigned char state[4] = {0, 0, 0, 0}, state1[4] = {0, 0, 0, 0};
	// Decrypt ciphertext block
	intToVec(((w[4] << 8) + w[5]) ^ ctext, state);
	sub4NibList(sBoxI, shiftRow(state));
	iMixCol(addKey(intToVec((w[2] << 8) + w[3], state1), state), state1);
	sub4NibList(sBoxI, shiftRow(state1));
	return vecToInt(addKey(intToVec((w[0] << 8) + w[1], state), state1));
}

void saes_encrypt_stream(const unsigned char* ptext, int size, unsigned char* w, char* ctext)
{
	unsigned short t;
	if (size == 0) {
		return;
	}
	for (int i = 0; i < size; i +=2) {
		if (i + 2 <= size) {
			t = saes_encrypt16b((ptext[i] << 8) + ptext[i + 1], w);
		} else {
			t = saes_encrypt16b((ptext[i] << 8) + 0x0, w);
		}
		ctext[i] = t >> 8;
		ctext[i + 1] = t & 0xff;
	}
}

void saes_decrypt_stream(const unsigned char* ctext, int size, unsigned char* w, char* ptext)
{
	unsigned short t;
	if (size == 0 || (size & 1)) {
		return;
	}
	for (int i = 0; i < size; i += 2) {
		t = saes_decrypt16b((ctext[i] << 8) + ctext[i + 1], w);
		ptext[i] = t >> 8;
		ptext[i + 1] = t & 0xff;
	}
}

void saes_encrypt_stream_key(const unsigned char* ptext, int size, unsigned short key, char* ctext)
{
	unsigned char w[6];
	unsigned short t;
	if (size == 0) {
		return;
	}
	keyExpansion(key, w);
	for (int i = 0; i < size; i +=2) {
		if (i + 2 <= size) {
			t = saes_encrypt16b((ptext[i] << 8) + ptext[i + 1], w);
		} else {
			t = saes_encrypt16b((ptext[i] << 8) + 0x0, w);
		}
		ctext[i] = t >> 8;
		ctext[i + 1] = t & 0xff;
	}
}

void saes_decrypt_stream_key(const unsigned char* ctext, int size, unsigned short key, char* ptext)
{
	unsigned char w[6];
	unsigned short t;
	if (size == 0 || (size & 1)) {
		return;
	}
	keyExpansion(key, w);
	for (int i = 0; i < size; i += 2) {
		t = saes_decrypt16b((ctext[i] << 8) + ctext[i + 1], w);
		ptext[i] = t >> 8;
		ptext[i + 1] = t & 0xff;
	}
}