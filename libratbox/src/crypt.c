/*
 * crypt.c: Implements unix style crypt() for platforms that don't have it
 *	    This version has MD5, DES, and SHA256/SHA512 crypt.
 *	    DES taken from uClibc, MD5 taken from BSD, SHA256/SHA512 taken from
 *	    Drepper's public domain implementation.
 */

/*
 * crypt() for uClibc
 *
 * Copyright (C) 2000 by Lineo, inc. and Erik Andersen
 * Copyright (C) 2000,2001 by Erik Andersen <andersen@uclibc.org>
 * Written by Erik Andersen <andersen@uclibc.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Library General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <libratbox_config.h>
#include <ratbox_lib.h>

static char *rb_md5_crypt(const char *pw, const char *salt);
static char *rb_des_crypt(const char *pw, const char *salt);
static char *rb_sha256_crypt(const char *key, const char *salt);
static char *rb_sha512_crypt(const char *key, const char *salt);
static char *rb_blowfish_crypt(const char *key, const char *salt);

char *
rb_crypt(const char *key, const char *salt)
{
	/* First, check if we are supposed to be using a replacement
	 * hash instead of DES...  */
	if(salt[0] == '$' && (salt[2] == '$' || salt[3] == '$'))
	{
		switch(salt[1])
		{
		case '1':
			return rb_md5_crypt(key, salt);
			break;
		case '2':
			/* Handles both 2 and 2a --Elizabeth */
			return rb_blowfish_crypt(key, salt);
			break;
		case '5':
			return rb_sha256_crypt(key, salt);
			break;
		case '6':
			return rb_sha512_crypt(key, salt);
			break;
		default:
			return NULL;
			break;
		};
	}
	else
		return rb_des_crypt(key, salt);
}

#define b64_from_24bit(B2, B1, B0, N)					\
	do								\
	{								\
		unsigned int w = ((B2) << 16) | ((B1) << 8) | (B0);	\
		int n = (N);						\
		while (n-- > 0 && buflen > 0)				\
		{							\
			*cp++ = ascii64[w & 0x3f];			\
			--buflen;					\
			w >>= 6;					\
		}							\
	} while (0)

#ifndef MAX
#	define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef MIN
#	define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

/* Here is the des crypt() stuff */

/*
 * FreeSec: libcrypt for NetBSD
 *
 * Copyright (c) 1994 David Burren
 * All rights reserved.
 *
 * Adapted for FreeBSD-2.0 by Geoffrey M. Rehmet
 *	this file should now *only* export crypt(), in order to make
 *	binaries of libcrypt exportable from the USA
 *
 * Adapted for FreeBSD-4.0 by Mark R V Murray
 *	this file should now *only* export crypt_des(), in order to make
 *	a module that can be optionally included in libcrypt.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of other contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This is an original implementation of the DES and the crypt(3) interfaces
 * by David Burren <davidb@werj.com.au>.
 *
 * An excellent reference on the underlying algorithm (and related
 * algorithms) is:
 *
 *	B. Schneier, Applied Cryptography: protocols, algorithms,
 *	and source code in C, John Wiley & Sons, 1994.
 *
 * Note that in that book's description of DES the lookups for the initial,
 * pbox, and final permutations are inverted (this has been brought to the
 * attention of the author).  A list of errata for this book has been
 * posted to the sci.crypt newsgroup by the author and is available for FTP.
 *
 * ARCHITECTURE ASSUMPTIONS:
 *	It is assumed that the 8-byte arrays passed by reference can be
 *	addressed as arrays of uint32_t's (ie. the CPU is not picky about
 *	alignment).
 */


/* Re-entrantify me -- all this junk needs to be in 
 * struct crypt_data to make this really reentrant... */
static uint8_t inv_key_perm[64];
static uint8_t inv_comp_perm[56];
static uint8_t u_sbox[8][64];
static uint8_t un_pbox[32];
static uint32_t en_keysl[16], en_keysr[16];
static uint32_t de_keysl[16], de_keysr[16];
static uint32_t ip_maskl[8][256], ip_maskr[8][256];
static uint32_t fp_maskl[8][256], fp_maskr[8][256];
static uint32_t key_perm_maskl[8][128], key_perm_maskr[8][128];
static uint32_t comp_maskl[8][128], comp_maskr[8][128];
static uint32_t saltbits;
static uint32_t old_salt;
static uint32_t old_rawkey0, old_rawkey1;


/* Static stuff that stays resident and doesn't change after 
 * being initialized, and therefore doesn't need to be made 
 * reentrant. */
static uint8_t init_perm[64], final_perm[64];
static uint8_t m_sbox[4][4096];
static uint32_t psbox[4][256];

/* A pile of data */
static const uint8_t ascii64[] = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static const uint8_t IP[64] = {
	58, 50, 42, 34, 26, 18, 10, 2, 60, 52, 44, 36, 28, 20, 12, 4,
	62, 54, 46, 38, 30, 22, 14, 6, 64, 56, 48, 40, 32, 24, 16, 8,
	57, 49, 41, 33, 25, 17, 9, 1, 59, 51, 43, 35, 27, 19, 11, 3,
	61, 53, 45, 37, 29, 21, 13, 5, 63, 55, 47, 39, 31, 23, 15, 7
};

static const uint8_t key_perm[56] = {
	57, 49, 41, 33, 25, 17, 9, 1, 58, 50, 42, 34, 26, 18,
	10, 2, 59, 51, 43, 35, 27, 19, 11, 3, 60, 52, 44, 36,
	63, 55, 47, 39, 31, 23, 15, 7, 62, 54, 46, 38, 30, 22,
	14, 6, 61, 53, 45, 37, 29, 21, 13, 5, 28, 20, 12, 4
};

static const uint8_t key_shifts[16] = {
	1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1
};

static const uint8_t comp_perm[48] = {
	14, 17, 11, 24, 1, 5, 3, 28, 15, 6, 21, 10,
	23, 19, 12, 4, 26, 8, 16, 7, 27, 20, 13, 2,
	41, 52, 31, 37, 47, 55, 30, 40, 51, 45, 33, 48,
	44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32
};

/*
 *	No E box is used, as it's replaced by some ANDs, shifts, and ORs.
 */

static const uint8_t sbox[8][64] = {
	{
	 14, 4, 13, 1, 2, 15, 11, 8, 3, 10, 6, 12, 5, 9, 0, 7,
	 0, 15, 7, 4, 14, 2, 13, 1, 10, 6, 12, 11, 9, 5, 3, 8,
	 4, 1, 14, 8, 13, 6, 2, 11, 15, 12, 9, 7, 3, 10, 5, 0,
	 15, 12, 8, 2, 4, 9, 1, 7, 5, 11, 3, 14, 10, 0, 6, 13},
	{
	 15, 1, 8, 14, 6, 11, 3, 4, 9, 7, 2, 13, 12, 0, 5, 10,
	 3, 13, 4, 7, 15, 2, 8, 14, 12, 0, 1, 10, 6, 9, 11, 5,
	 0, 14, 7, 11, 10, 4, 13, 1, 5, 8, 12, 6, 9, 3, 2, 15,
	 13, 8, 10, 1, 3, 15, 4, 2, 11, 6, 7, 12, 0, 5, 14, 9},
	{
	 10, 0, 9, 14, 6, 3, 15, 5, 1, 13, 12, 7, 11, 4, 2, 8,
	 13, 7, 0, 9, 3, 4, 6, 10, 2, 8, 5, 14, 12, 11, 15, 1,
	 13, 6, 4, 9, 8, 15, 3, 0, 11, 1, 2, 12, 5, 10, 14, 7,
	 1, 10, 13, 0, 6, 9, 8, 7, 4, 15, 14, 3, 11, 5, 2, 12},
	{
	 7, 13, 14, 3, 0, 6, 9, 10, 1, 2, 8, 5, 11, 12, 4, 15,
	 13, 8, 11, 5, 6, 15, 0, 3, 4, 7, 2, 12, 1, 10, 14, 9,
	 10, 6, 9, 0, 12, 11, 7, 13, 15, 1, 3, 14, 5, 2, 8, 4,
	 3, 15, 0, 6, 10, 1, 13, 8, 9, 4, 5, 11, 12, 7, 2, 14},
	{
	 2, 12, 4, 1, 7, 10, 11, 6, 8, 5, 3, 15, 13, 0, 14, 9,
	 14, 11, 2, 12, 4, 7, 13, 1, 5, 0, 15, 10, 3, 9, 8, 6,
	 4, 2, 1, 11, 10, 13, 7, 8, 15, 9, 12, 5, 6, 3, 0, 14,
	 11, 8, 12, 7, 1, 14, 2, 13, 6, 15, 0, 9, 10, 4, 5, 3},
	{
	 12, 1, 10, 15, 9, 2, 6, 8, 0, 13, 3, 4, 14, 7, 5, 11,
	 10, 15, 4, 2, 7, 12, 9, 5, 6, 1, 13, 14, 0, 11, 3, 8,
	 9, 14, 15, 5, 2, 8, 12, 3, 7, 0, 4, 10, 1, 13, 11, 6,
	 4, 3, 2, 12, 9, 5, 15, 10, 11, 14, 1, 7, 6, 0, 8, 13},
	{
	 4, 11, 2, 14, 15, 0, 8, 13, 3, 12, 9, 7, 5, 10, 6, 1,
	 13, 0, 11, 7, 4, 9, 1, 10, 14, 3, 5, 12, 2, 15, 8, 6,
	 1, 4, 11, 13, 12, 3, 7, 14, 10, 15, 6, 8, 0, 5, 9, 2,
	 6, 11, 13, 8, 1, 4, 10, 7, 9, 5, 0, 15, 14, 2, 3, 12},
	{
	 13, 2, 8, 4, 6, 15, 11, 1, 10, 9, 3, 14, 5, 0, 12, 7,
	 1, 15, 13, 8, 10, 3, 7, 4, 12, 5, 6, 11, 0, 14, 9, 2,
	 7, 11, 4, 1, 9, 12, 14, 2, 0, 6, 10, 13, 15, 3, 5, 8,
	 2, 1, 14, 7, 4, 10, 8, 13, 15, 12, 9, 0, 3, 5, 6, 11}
};

static const uint8_t pbox[32] = {
	16, 7, 20, 21, 29, 12, 28, 17, 1, 15, 23, 26, 5, 18, 31, 10,
	2, 8, 24, 14, 32, 27, 3, 9, 19, 13, 30, 6, 22, 11, 4, 25
};

static const uint32_t bits32[32] = {
	0x80000000, 0x40000000, 0x20000000, 0x10000000,
	0x08000000, 0x04000000, 0x02000000, 0x01000000,
	0x00800000, 0x00400000, 0x00200000, 0x00100000,
	0x00080000, 0x00040000, 0x00020000, 0x00010000,
	0x00008000, 0x00004000, 0x00002000, 0x00001000,
	0x00000800, 0x00000400, 0x00000200, 0x00000100,
	0x00000080, 0x00000040, 0x00000020, 0x00000010,
	0x00000008, 0x00000004, 0x00000002, 0x00000001
};

static const uint8_t bits8[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

static const uint32_t *bits28, *bits24;


static int
rb_ascii_to_bin(char ch)
{
	if(ch > 'z')
		return (0);
	if(ch >= 'a')
		return (ch - 'a' + 38);
	if(ch > 'Z')
		return (0);
	if(ch >= 'A')
		return (ch - 'A' + 12);
	if(ch > '9')
		return (0);
	if(ch >= '.')
		return (ch - '.');
	return (0);
}

static void
rb_des_init(void)
{
	int i, j, b, k, inbit, obit;
	uint32_t *p, *il, *ir, *fl, *fr;
	static int rb_des_initialised = 0;

	if(rb_des_initialised == 1)
		return;

	old_rawkey0 = old_rawkey1 = 0L;
	saltbits = 0L;
	old_salt = 0L;
	bits24 = (bits28 = bits32 + 4) + 4;

	/*
	 * Invert the S-boxes, reordering the input bits.
	 */
	for(i = 0; i < 8; i++)
		for(j = 0; j < 64; j++)
		{
			b = (j & 0x20) | ((j & 1) << 4) | ((j >> 1) & 0xf);
			u_sbox[i][j] = sbox[i][b];
		}

	/*
	 * Convert the inverted S-boxes into 4 arrays of 8 bits.
	 * Each will handle 12 bits of the S-box input.
	 */
	for(b = 0; b < 4; b++)
		for(i = 0; i < 64; i++)
			for(j = 0; j < 64; j++)
				m_sbox[b][(i << 6) | j] =
					(uint8_t)((u_sbox[(b << 1)][i] << 4) |
						  u_sbox[(b << 1) + 1][j]);

	/*
	 * Set up the initial & final permutations into a useful form, and
	 * initialise the inverted key permutation.
	 */
	for(i = 0; i < 64; i++)
	{
		init_perm[final_perm[i] = IP[i] - 1] = (uint8_t)i;
		inv_key_perm[i] = 255;
	}

	/*
	 * Invert the key permutation and initialise the inverted key
	 * compression permutation.
	 */
	for(i = 0; i < 56; i++)
	{
		inv_key_perm[key_perm[i] - 1] = (uint8_t)i;
		inv_comp_perm[i] = 255;
	}

	/*
	 * Invert the key compression permutation.
	 */
	for(i = 0; i < 48; i++)
	{
		inv_comp_perm[comp_perm[i] - 1] = (uint8_t)i;
	}

	/*
	 * Set up the OR-mask arrays for the initial and final permutations,
	 * and for the key initial and compression permutations.
	 */
	for(k = 0; k < 8; k++)
	{
		for(i = 0; i < 256; i++)
		{
			*(il = &ip_maskl[k][i]) = 0L;
			*(ir = &ip_maskr[k][i]) = 0L;
			*(fl = &fp_maskl[k][i]) = 0L;
			*(fr = &fp_maskr[k][i]) = 0L;
			for(j = 0; j < 8; j++)
			{
				inbit = 8 * k + j;
				if(i & bits8[j])
				{
					if((obit = init_perm[inbit]) < 32)
						*il |= bits32[obit];
					else
						*ir |= bits32[obit - 32];
					if((obit = final_perm[inbit]) < 32)
						*fl |= bits32[obit];
					else
						*fr |= bits32[obit - 32];
				}
			}
		}
		for(i = 0; i < 128; i++)
		{
			*(il = &key_perm_maskl[k][i]) = 0L;
			*(ir = &key_perm_maskr[k][i]) = 0L;
			for(j = 0; j < 7; j++)
			{
				inbit = 8 * k + j;
				if(i & bits8[j + 1])
				{
					if((obit = inv_key_perm[inbit]) == 255)
						continue;
					if(obit < 28)
						*il |= bits28[obit];
					else
						*ir |= bits28[obit - 28];
				}
			}
			*(il = &comp_maskl[k][i]) = 0L;
			*(ir = &comp_maskr[k][i]) = 0L;
			for(j = 0; j < 7; j++)
			{
				inbit = 7 * k + j;
				if(i & bits8[j + 1])
				{
					if((obit = inv_comp_perm[inbit]) == 255)
						continue;
					if(obit < 24)
						*il |= bits24[obit];
					else
						*ir |= bits24[obit - 24];
				}
			}
		}
	}

	/*
	 * Invert the P-box permutation, and convert into OR-masks for
	 * handling the output of the S-box arrays setup above.
	 */
	for(i = 0; i < 32; i++)
		un_pbox[pbox[i] - 1] = (uint8_t)i;

	for(b = 0; b < 4; b++)
		for(i = 0; i < 256; i++)
		{
			*(p = &psbox[b][i]) = 0L;
			for(j = 0; j < 8; j++)
			{
				if(i & bits8[j])
					*p |= bits32[un_pbox[8 * b + j]];
			}
		}

	rb_des_initialised = 1;
}


static void
rb_setup_salt(long salt)
{
	uint32_t obit, saltbit;
	int i;

	if(salt == (long)old_salt)
		return;
	old_salt = salt;

	saltbits = 0L;
	saltbit = 1;
	obit = 0x800000;
	for(i = 0; i < 24; i++)
	{
		if(salt & saltbit)
			saltbits |= obit;
		saltbit <<= 1;
		obit >>= 1;
	}
}

static int
rb_des_setkey(const char *key)
{
	uint32_t k0, k1, rawkey0, rawkey1;
	int shifts, round;

	rb_des_init();

	rawkey0 = ntohl(*(const uint32_t *)key);
	rawkey1 = ntohl(*(const uint32_t *)(key + 4));

	if((rawkey0 | rawkey1) && rawkey0 == old_rawkey0 && rawkey1 == old_rawkey1)
	{
		/*
		 * Already setup for this key.
		 * This optimisation fails on a zero key (which is weak and
		 * has bad parity anyway) in order to simplify the starting
		 * conditions.
		 */
		return (0);
	}
	old_rawkey0 = rawkey0;
	old_rawkey1 = rawkey1;

	/*
	 *      Do key permutation and split into two 28-bit subkeys.
	 */
	k0 = key_perm_maskl[0][rawkey0 >> 25]
		| key_perm_maskl[1][(rawkey0 >> 17) & 0x7f]
		| key_perm_maskl[2][(rawkey0 >> 9) & 0x7f]
		| key_perm_maskl[3][(rawkey0 >> 1) & 0x7f]
		| key_perm_maskl[4][rawkey1 >> 25]
		| key_perm_maskl[5][(rawkey1 >> 17) & 0x7f]
		| key_perm_maskl[6][(rawkey1 >> 9) & 0x7f]
		| key_perm_maskl[7][(rawkey1 >> 1) & 0x7f];
	k1 = key_perm_maskr[0][rawkey0 >> 25]
		| key_perm_maskr[1][(rawkey0 >> 17) & 0x7f]
		| key_perm_maskr[2][(rawkey0 >> 9) & 0x7f]
		| key_perm_maskr[3][(rawkey0 >> 1) & 0x7f]
		| key_perm_maskr[4][rawkey1 >> 25]
		| key_perm_maskr[5][(rawkey1 >> 17) & 0x7f]
		| key_perm_maskr[6][(rawkey1 >> 9) & 0x7f]
		| key_perm_maskr[7][(rawkey1 >> 1) & 0x7f];
	/*
	 *      Rotate subkeys and do compression permutation.
	 */
	shifts = 0;
	for(round = 0; round < 16; round++)
	{
		uint32_t t0, t1;

		shifts += key_shifts[round];

		t0 = (k0 << shifts) | (k0 >> (28 - shifts));
		t1 = (k1 << shifts) | (k1 >> (28 - shifts));

		de_keysl[15 - round] =
			en_keysl[round] = comp_maskl[0][(t0 >> 21) & 0x7f]
			| comp_maskl[1][(t0 >> 14) & 0x7f]
			| comp_maskl[2][(t0 >> 7) & 0x7f]
			| comp_maskl[3][t0 & 0x7f]
			| comp_maskl[4][(t1 >> 21) & 0x7f]
			| comp_maskl[5][(t1 >> 14) & 0x7f]
			| comp_maskl[6][(t1 >> 7) & 0x7f] | comp_maskl[7][t1 & 0x7f];

		de_keysr[15 - round] =
			en_keysr[round] = comp_maskr[0][(t0 >> 21) & 0x7f]
			| comp_maskr[1][(t0 >> 14) & 0x7f]
			| comp_maskr[2][(t0 >> 7) & 0x7f]
			| comp_maskr[3][t0 & 0x7f]
			| comp_maskr[4][(t1 >> 21) & 0x7f]
			| comp_maskr[5][(t1 >> 14) & 0x7f]
			| comp_maskr[6][(t1 >> 7) & 0x7f] | comp_maskr[7][t1 & 0x7f];
	}
	return (0);
}

static int
rb_do_des(uint32_t l_in, uint32_t r_in, uint32_t *l_out, uint32_t *r_out, int count)
{
	/*
	 *      l_in, r_in, l_out, and r_out are in pseudo-"big-endian" format.
	 */
	uint32_t l, r, *kl, *kr, *kl1, *kr1;
	uint32_t f, r48l, r48r;
	int round;

	if(count == 0)
	{
		return (1);
	}
	else if(count > 0)
	{
		/*
		 * Encrypting
		 */
		kl1 = en_keysl;
		kr1 = en_keysr;
	}
	else
	{
		/*
		 * Decrypting
		 */
		count = -count;
		kl1 = de_keysl;
		kr1 = de_keysr;
	}

	/*
	 *      Do initial permutation (IP).
	 */
	l = ip_maskl[0][l_in >> 24]
		| ip_maskl[1][(l_in >> 16) & 0xff]
		| ip_maskl[2][(l_in >> 8) & 0xff]
		| ip_maskl[3][l_in & 0xff]
		| ip_maskl[4][r_in >> 24]
		| ip_maskl[5][(r_in >> 16) & 0xff]
		| ip_maskl[6][(r_in >> 8) & 0xff] | ip_maskl[7][r_in & 0xff];
	r = ip_maskr[0][l_in >> 24]
		| ip_maskr[1][(l_in >> 16) & 0xff]
		| ip_maskr[2][(l_in >> 8) & 0xff]
		| ip_maskr[3][l_in & 0xff]
		| ip_maskr[4][r_in >> 24]
		| ip_maskr[5][(r_in >> 16) & 0xff]
		| ip_maskr[6][(r_in >> 8) & 0xff] | ip_maskr[7][r_in & 0xff];

	while(count--)
	{
		/*
		 * Do each round.
		 */
		kl = kl1;
		kr = kr1;
		round = 16;
		while(round--)
		{
			/*
			 * Expand R to 48 bits (simulate the E-box).
			 */
			r48l = ((r & 0x00000001) << 23)
				| ((r & 0xf8000000) >> 9)
				| ((r & 0x1f800000) >> 11)
				| ((r & 0x01f80000) >> 13) | ((r & 0x001f8000) >> 15);

			r48r = ((r & 0x0001f800) << 7)
				| ((r & 0x00001f80) << 5)
				| ((r & 0x000001f8) << 3)
				| ((r & 0x0000001f) << 1) | ((r & 0x80000000) >> 31);
			/*
			 * Do salting for crypt() and friends, and
			 * XOR with the permuted key.
			 */
			f = (r48l ^ r48r) & saltbits;
			r48l ^= f ^ *kl++;
			r48r ^= f ^ *kr++;
			/*
			 * Do sbox lookups (which shrink it back to 32 bits)
			 * and do the pbox permutation at the same time.
			 */
			f = psbox[0][m_sbox[0][r48l >> 12]]
				| psbox[1][m_sbox[1][r48l & 0xfff]]
				| psbox[2][m_sbox[2][r48r >> 12]]
				| psbox[3][m_sbox[3][r48r & 0xfff]];
			/*
			 * Now that we've permuted things, complete f().
			 */
			f ^= l;
			l = r;
			r = f;
		}
		r = l;
		l = f;
	}
	/*
	 * Do final permutation (inverse of IP).
	 */
	*l_out = fp_maskl[0][l >> 24]
		| fp_maskl[1][(l >> 16) & 0xff]
		| fp_maskl[2][(l >> 8) & 0xff]
		| fp_maskl[3][l & 0xff]
		| fp_maskl[4][r >> 24]
		| fp_maskl[5][(r >> 16) & 0xff]
		| fp_maskl[6][(r >> 8) & 0xff] | fp_maskl[7][r & 0xff];
	*r_out = fp_maskr[0][l >> 24]
		| fp_maskr[1][(l >> 16) & 0xff]
		| fp_maskr[2][(l >> 8) & 0xff]
		| fp_maskr[3][l & 0xff]
		| fp_maskr[4][r >> 24]
		| fp_maskr[5][(r >> 16) & 0xff]
		| fp_maskr[6][(r >> 8) & 0xff] | fp_maskr[7][r & 0xff];
	return (0);
}

static char *
rb_des_crypt(const char *key, const char *setting)
{
	uint32_t count, salt, l, r0, r1, keybuf[2];
	uint8_t *p, *q;
	static char output[21];

	rb_des_init();

	/*
	 * Copy the key, shifting each character up by one bit
	 * and padding with zeros.
	 */
	q = (uint8_t *)keybuf;
	while(q - (uint8_t *)keybuf - 8)
	{
		*q++ = *key << 1;
		if(*(q - 1))
			key++;
	}
	if(rb_des_setkey((char *)keybuf))
		return (NULL);
	{
		/*
		 * "old"-style:
		 *      setting - 2 bytes of salt
		 *      key - up to 8 characters
		 */
		count = 25;

		salt = (rb_ascii_to_bin(setting[1]) << 6) | rb_ascii_to_bin(setting[0]);

		output[0] = setting[0];
		/*
		 * If the encrypted password that the salt was extracted from
		 * is only 1 character long, the salt will be corrupted.  We
		 * need to ensure that the output string doesn't have an extra
		 * NUL in it!
		 */
		output[1] = setting[1] ? setting[1] : output[0];

		p = (uint8_t *)output + 2;
	}
	rb_setup_salt(salt);
	/*
	 * Do it.
	 */
	if(rb_do_des(0L, 0L, &r0, &r1, (int)count))
		return (NULL);
	/*
	 * Now encode the result...
	 */
	l = (r0 >> 8);
	*p++ = ascii64[(l >> 18) & 0x3f];
	*p++ = ascii64[(l >> 12) & 0x3f];
	*p++ = ascii64[(l >> 6) & 0x3f];
	*p++ = ascii64[l & 0x3f];

	l = (r0 << 16) | ((r1 >> 16) & 0xffff);
	*p++ = ascii64[(l >> 18) & 0x3f];
	*p++ = ascii64[(l >> 12) & 0x3f];
	*p++ = ascii64[(l >> 6) & 0x3f];
	*p++ = ascii64[l & 0x3f];

	l = r1 << 2;
	*p++ = ascii64[(l >> 12) & 0x3f];
	*p++ = ascii64[(l >> 6) & 0x3f];
	*p++ = ascii64[l & 0x3f];
	*p = 0;

	return (output);
}

/* Now md5 crypt */
/*
 * MD5C.C - RSA Data Security, Inc., MD5 message-digest algorithm
 *
 * Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
 * rights reserved.
 *
 * License to copy and use this software is granted provided that it
 * is identified as the "RSA Data Security, Inc. MD5 Message-Digest
 * Algorithm" in all material mentioning or referencing this software
 * or this function.
 *
 * License is also granted to make and use derivative works provided
 * that such works are identified as "derived from the RSA Data
 * Security, Inc. MD5 Message-Digest Algorithm" in all material
 * mentioning or referencing the derived work.
 *
 * RSA Data Security, Inc. makes no representations concerning either
 * the merchantability of this software or the suitability of this
 * software for any particular purpose. It is provided "as is"
 * without express or implied warranty of any kind.
 *
 * These notices must be retained in any copies of any part of this
 * documentation and/or software.
 *
 * This code is the same as the code published by RSA Inc.  It has been
 * edited for clarity and style only.
 */

#define MD5_BLOCK_LENGTH                64
#define MD5_DIGEST_LENGTH               16
#define MD5_DIGEST_STRING_LENGTH        (MD5_DIGEST_LENGTH * 2 + 1)
#define MD5_SIZE			16

static void
_crypt_to64(char *s, u_long v, int n)
{
	while (--n >= 0) {
	        *s++ = ascii64[v&0x3f];
	        v >>= 6;
	}
}

/* MD5 context. */
typedef struct MD5Context {
	uint32_t state[4];   /* state (ABCD) */
	uint32_t count[2];   /* number of bits, modulo 2^64 (lsb first) */
	unsigned char buffer[64];     /* input buffer */
} MD5_CTX;

static void   MD5Transform(uint32_t [4], const unsigned char [64]);
static void   MD5Init (MD5_CTX *);
static void   MD5Update (MD5_CTX *, const void *, unsigned int);
static void   MD5Final (unsigned char [16], MD5_CTX *);

#ifndef WORDS_BIGENDIAN
#define Encode memcpy
#define Decode memcpy
#else 

/*
 * Encodes input (uint32_t) into output (unsigned char). Assumes len is
 * a multiple of 4.
 */

static void
Encode (unsigned char *output, uint32_t *input, unsigned int len)
{
	unsigned int i;
	uint32_t *op = (uint32_t *)output;

	for (i = 0; i < len / 4; i++)
		op[i] = htole32(input[i]);
}

/*
 * Decodes input (unsigned char) into output (uint32_t). Assumes len is
 * a multiple of 4.
 */

static void
Decode (uint32_t *output, const unsigned char *input, unsigned int len)
{
	unsigned int i;
	const uint32_t *ip = (const uint32_t *)input;

	for (i = 0; i < len / 4; i++)
		output[i] = le32toh(ip[i]);
}
#endif

static unsigned char PADDING[64] = {
  0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* F, G, H and I are basic MD5 functions. */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits. */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/*
 * FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
 * Rotation is separate from addition to prevent recomputation.
 */
#define FF(a, b, c, d, x, s, ac) { \
	(a) += F ((b), (c), (d)) + (x) + (uint32_t)(ac); \
	(a) = ROTATE_LEFT ((a), (s)); \
	(a) += (b); \
	}
#define GG(a, b, c, d, x, s, ac) { \
	(a) += G ((b), (c), (d)) + (x) + (uint32_t)(ac); \
	(a) = ROTATE_LEFT ((a), (s)); \
	(a) += (b); \
	}
#define HH(a, b, c, d, x, s, ac) { \
	(a) += H ((b), (c), (d)) + (x) + (uint32_t)(ac); \
	(a) = ROTATE_LEFT ((a), (s)); \
	(a) += (b); \
	}
#define II(a, b, c, d, x, s, ac) { \
	(a) += I ((b), (c), (d)) + (x) + (uint32_t)(ac); \
	(a) = ROTATE_LEFT ((a), (s)); \
	(a) += (b); \
	}

/* MD5 initialization. Begins an MD5 operation, writing a new context. */

static void
MD5Init (context)
	MD5_CTX *context;
{

	context->count[0] = context->count[1] = 0;

	/* Load magic initialization constants.  */
	context->state[0] = 0x67452301;
	context->state[1] = 0xefcdab89;
	context->state[2] = 0x98badcfe;
	context->state[3] = 0x10325476;
}

/* 
 * MD5 block update operation. Continues an MD5 message-digest
 * operation, processing another message block, and updating the
 * context.
 */

static void
MD5Update (context, in, inputLen)
	MD5_CTX *context;
	const void *in;
	unsigned int inputLen;
{
	unsigned int i, idx, partLen;
	const unsigned char *input = in;

	/* Compute number of bytes mod 64 */
	idx = (unsigned int)((context->count[0] >> 3) & 0x3F);

	/* Update number of bits */
	if ((context->count[0] += ((uint32_t)inputLen << 3))
	    < ((uint32_t)inputLen << 3))
		context->count[1]++;
	context->count[1] += ((uint32_t)inputLen >> 29);

	partLen = 64 - idx;

	/* Transform as many times as possible. */
	if (inputLen >= partLen) {
		memcpy((void *)&context->buffer[idx], (const void *)input,
		    partLen);
		MD5Transform (context->state, context->buffer);

		for (i = partLen; i + 63 < inputLen; i += 64)
			MD5Transform (context->state, &input[i]);

		idx = 0;
	}
	else
		i = 0;

	/* Buffer remaining input */
	memcpy ((void *)&context->buffer[idx], (const void *)&input[i],
	    inputLen-i);
}

/*
 * MD5 padding. Adds padding followed by original length.
 */

static void
MD5Pad (context)
	MD5_CTX *context;
{
	unsigned char bits[8];
	unsigned int idx, padLen;

	/* Save number of bits */
	Encode (bits, context->count, 8);

	/* Pad out to 56 mod 64. */
	idx = (unsigned int)((context->count[0] >> 3) & 0x3f);
	padLen = (idx < 56) ? (56 - idx) : (120 - idx);
	MD5Update (context, PADDING, padLen);

	/* Append length (before padding) */
	MD5Update (context, bits, 8);
}

/*
 * MD5 finalization. Ends an MD5 message-digest operation, writing the
 * the message digest and zeroizing the context.
 */

static void
MD5Final (digest, context)
	unsigned char digest[16];
	MD5_CTX *context;
{
	/* Do padding. */
	MD5Pad (context);

	/* Store state in digest */
	Encode (digest, context->state, 16);

	/* Zeroize sensitive information. */
	memset ((void *)context, 0, sizeof (*context));
}

/* MD5 basic transformation. Transforms state based on block. */

static void
MD5Transform (state, block)
	uint32_t state[4];
	const unsigned char block[64];
{
	uint32_t a = state[0], b = state[1], c = state[2], d = state[3], x[16];

	Decode (x, block, 64);

	/* Round 1 */
#define S11 7
#define S12 12
#define S13 17
#define S14 22
	FF (a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
	FF (d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
	FF (c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
	FF (b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
	FF (a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
	FF (d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
	FF (c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
	FF (b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
	FF (a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
	FF (d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
	FF (c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
	FF (b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
	FF (a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
	FF (d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
	FF (c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
	FF (b, c, d, a, x[15], S14, 0x49b40821); /* 16 */

	/* Round 2 */
#define S21 5
#define S22 9
#define S23 14
#define S24 20
	GG (a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
	GG (d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
	GG (c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
	GG (b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
	GG (a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
	GG (d, a, b, c, x[10], S22,  0x2441453); /* 22 */
	GG (c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
	GG (b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
	GG (a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
	GG (d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
	GG (c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
	GG (b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
	GG (a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
	GG (d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
	GG (c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
	GG (b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */

	/* Round 3 */
#define S31 4
#define S32 11
#define S33 16
#define S34 23
	HH (a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
	HH (d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
	HH (c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
	HH (b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
	HH (a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
	HH (d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
	HH (c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
	HH (b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
	HH (a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
	HH (d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
	HH (c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
	HH (b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
	HH (a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
	HH (d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
	HH (c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
	HH (b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */

	/* Round 4 */
#define S41 6
#define S42 10
#define S43 15
#define S44 21
	II (a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
	II (d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
	II (c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
	II (b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
	II (a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
	II (d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
	II (c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
	II (b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
	II (a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
	II (d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
	II (c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
	II (b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
	II (a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
	II (d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
	II (c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
	II (b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;

	/* Zeroize sensitive information. */
	memset ((void *)x, 0, sizeof (x));
}

/*
 * UNIX password
 */

static char *
rb_md5_crypt(const char *pw, const char *salt)
{
	MD5_CTX	ctx,ctx1;
	unsigned long l;
	int sl, pl;
	u_int i;
	u_char final[MD5_SIZE];
	static const char *sp, *ep;
	static char passwd[120], *p;
	static const char *magic = "$1$";

	/* Refine the Salt first */
	sp = salt;

	/* If it starts with the magic string, then skip that */
	if(!strncmp(sp, magic, strlen(magic)))
		sp += strlen(magic);

	/* It stops at the first '$', max 8 chars */
	for(ep = sp; *ep && *ep != '$' && ep < (sp + 8); ep++)
		continue;

	/* get the length of the true salt */
	sl = ep - sp;

	MD5Init(&ctx);

	/* The password first, since that is what is most unknown */
	MD5Update(&ctx, (const u_char *)pw, strlen(pw));

	/* Then our magic string */
	MD5Update(&ctx, (const u_char *)magic, strlen(magic));

	/* Then the raw salt */
	MD5Update(&ctx, (const u_char *)sp, (u_int)sl);

	/* Then just as many characters of the MD5(pw,salt,pw) */
	MD5Init(&ctx1);
	MD5Update(&ctx1, (const u_char *)pw, strlen(pw));
	MD5Update(&ctx1, (const u_char *)sp, (u_int)sl);
	MD5Update(&ctx1, (const u_char *)pw, strlen(pw));
	MD5Final(final, &ctx1);
	for(pl = (int)strlen(pw); pl > 0; pl -= MD5_SIZE)
		MD5Update(&ctx, (const u_char *)final,
		    (u_int)(pl > MD5_SIZE ? MD5_SIZE : pl));

	/* Don't leave anything around in vm they could use. */
	memset(final, 0, sizeof(final));

	/* Then something really weird... */
	for (i = strlen(pw); i; i >>= 1)
		if(i & 1)
		    MD5Update(&ctx, (const u_char *)final, 1);
		else
		    MD5Update(&ctx, (const u_char *)pw, 1);

	/* Now make the output string */
	rb_strlcpy(passwd, magic, sizeof(passwd));
	strncat(passwd, sp, (u_int)sl);
	rb_strlcat(passwd, "$", sizeof(passwd));

	MD5Final(final, &ctx);

	/*
	 * and now, just to make sure things don't run too fast
	 * On a 60 Mhz Pentium this takes 34 msec, so you would
	 * need 30 seconds to build a 1000 entry dictionary...
	 */
	for(i = 0; i < 1000; i++) {
		MD5Init(&ctx1);
		if(i & 1)
			MD5Update(&ctx1, (const u_char *)pw, strlen(pw));
		else
			MD5Update(&ctx1, (const u_char *)final, MD5_SIZE);

		if(i % 3)
			MD5Update(&ctx1, (const u_char *)sp, (u_int)sl);

		if(i % 7)
			MD5Update(&ctx1, (const u_char *)pw, strlen(pw));

		if(i & 1)
			MD5Update(&ctx1, (const u_char *)final, MD5_SIZE);
		else
			MD5Update(&ctx1, (const u_char *)pw, strlen(pw));
		MD5Final(final, &ctx1);
	}

	p = passwd + strlen(passwd);

	l = (final[ 0]<<16) | (final[ 6]<<8) | final[12];
	_crypt_to64(p, l, 4); p += 4;
	l = (final[ 1]<<16) | (final[ 7]<<8) | final[13];
	_crypt_to64(p, l, 4); p += 4;
	l = (final[ 2]<<16) | (final[ 8]<<8) | final[14];
	_crypt_to64(p, l, 4); p += 4;
	l = (final[ 3]<<16) | (final[ 9]<<8) | final[15];
	_crypt_to64(p, l, 4); p += 4;
	l = (final[ 4]<<16) | (final[10]<<8) | final[ 5];
	_crypt_to64(p, l, 4); p += 4;
	l = final[11];
	_crypt_to64(p, l, 2); p += 2;
	*p = '\0';

	/* Don't leave anything around in vm they could use. */
	memset(final, 0, sizeof(final));

	return (passwd);
}


/* SHA256-based Unix crypt implementation.
   Released into the Public Domain by Ulrich Drepper <drepper@redhat.com>.  */

/* Structure to save state of computation between the single steps.  */
struct sha256_ctx
{
	uint32_t H[8];

	uint32_t total[2];
	uint32_t buflen;
	char buffer[128];	/* NB: always correctly aligned for uint32_t.  */
};

#ifndef WORDS_BIGENDIAN
#	define SHA256_SWAP(n) \
		(((n) << 24) | (((n) & 0xff00) << 8) | (((n) >> 8) & 0xff00) | ((n) >> 24))
#else
#	define SHA256_SWAP(n) (n)
#endif

/* This array contains the bytes used to pad the buffer to the next
   64-byte boundary.  (FIPS 180-2:5.1.1)  */
static const unsigned char SHA256_fillbuf[64] = { 0x80, 0 /* , 0, 0, ...  */  };


/* Constants for SHA256 from FIPS 180-2:4.2.2.  */
static const uint32_t SHA256_K[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};


/* Process LEN bytes of BUFFER, accumulating context into CTX.
   It is assumed that LEN % 64 == 0.  */
static void rb_sha256_process_block(const void *buffer, size_t len, struct sha256_ctx *ctx)
{
	const uint32_t *words = buffer;
	size_t nwords = len / sizeof(uint32_t);
	uint32_t a = ctx->H[0];
	uint32_t b = ctx->H[1];
	uint32_t c = ctx->H[2];
	uint32_t d = ctx->H[3];
	uint32_t e = ctx->H[4];
	uint32_t f = ctx->H[5];
	uint32_t g = ctx->H[6];
	uint32_t h = ctx->H[7];

	/* First increment the byte count.  FIPS 180-2 specifies the possible
	   length of the file up to 2^64 bits.  Here we only compute the
	   number of bytes.  Do a double word increment.  */
	ctx->total[0] += len;
	if (ctx->total[0] < len)
		++ctx->total[1];

	/* Process all bytes in the buffer with 64 bytes in each round of
	   the loop.  */
	while (nwords > 0)
	{
		uint32_t W[64];
		uint32_t a_save = a;
		uint32_t b_save = b;
		uint32_t c_save = c;
		uint32_t d_save = d;
		uint32_t e_save = e;
		uint32_t f_save = f;
		uint32_t g_save = g;
		uint32_t h_save = h;
		unsigned int t;

		/* Operators defined in FIPS 180-2:4.1.2.  */
		#define SHA256_Ch(x, y, z) ((x & y) ^ (~x & z))
		#define SHA256_Maj(x, y, z) ((x & y) ^ (x & z) ^ (y & z))
		#define SHA256_S0(x) (SHA256_CYCLIC (x, 2) ^ SHA256_CYCLIC (x, 13) ^ SHA256_CYCLIC (x, 22))
		#define SHA256_S1(x) (SHA256_CYCLIC (x, 6) ^ SHA256_CYCLIC (x, 11) ^ SHA256_CYCLIC (x, 25))
		#define SHA256_R0(x) (SHA256_CYCLIC (x, 7) ^ SHA256_CYCLIC (x, 18) ^ (x >> 3))
		#define SHA256_R1(x) (SHA256_CYCLIC (x, 17) ^ SHA256_CYCLIC (x, 19) ^ (x >> 10))

		/* It is unfortunate that C does not provide an operator for
		   cyclic rotation.  Hope the C compiler is smart enough.  */
		#define SHA256_CYCLIC(w, s) ((w >> s) | (w << (32 - s)))

		/* Compute the message schedule according to FIPS 180-2:6.2.2 step 2.  */
		for (t = 0; t < 16; ++t)
		{
			W[t] = SHA256_SWAP(*words);
			++words;
		}
		for (t = 16; t < 64; ++t)
			W[t] = SHA256_R1(W[t - 2]) + W[t - 7] + SHA256_R0(W[t - 15]) + W[t - 16];

		/* The actual computation according to FIPS 180-2:6.2.2 step 3.  */
		for (t = 0; t < 64; ++t)
		{
			uint32_t T1 = h + SHA256_S1(e) + SHA256_Ch(e, f, g) + SHA256_K[t] + W[t];
			uint32_t T2 = SHA256_S0(a) + SHA256_Maj(a, b, c);
			h = g;
			g = f;
			f = e;
			e = d + T1;
			d = c;
			c = b;
			b = a;
			a = T1 + T2;
		}

		/* Add the starting values of the context according to FIPS 180-2:6.2.2
		   step 4.  */
		a += a_save;
		b += b_save;
		c += c_save;
		d += d_save;
		e += e_save;
		f += f_save;
		g += g_save;
		h += h_save;

		/* Prepare for the next round.  */
		nwords -= 16;
	}

	/* Put checksum in context given as argument.  */
	ctx->H[0] = a;
	ctx->H[1] = b;
	ctx->H[2] = c;
	ctx->H[3] = d;
	ctx->H[4] = e;
	ctx->H[5] = f;
	ctx->H[6] = g;
	ctx->H[7] = h;
}


/* Initialize structure containing state of computation.
   (FIPS 180-2:5.3.2)  */
static void rb_sha256_init_ctx(struct sha256_ctx *ctx)
{
	ctx->H[0] = 0x6a09e667;
	ctx->H[1] = 0xbb67ae85;
	ctx->H[2] = 0x3c6ef372;
	ctx->H[3] = 0xa54ff53a;
	ctx->H[4] = 0x510e527f;
	ctx->H[5] = 0x9b05688c;
	ctx->H[6] = 0x1f83d9ab;
	ctx->H[7] = 0x5be0cd19;

	ctx->total[0] = ctx->total[1] = 0;
	ctx->buflen = 0;
}


/* Process the remaining bytes in the internal buffer and the usual
   prolog according to the standard and write the result to RESBUF.

   IMPORTANT: On some systems it is required that RESBUF is correctly
   aligned for a 32 bits value.  */
static void *rb_sha256_finish_ctx(struct sha256_ctx *ctx, void *resbuf)
{
	/* Take yet unprocessed bytes into account.  */
	uint32_t bytes = ctx->buflen;
	size_t pad;
	unsigned int i;

	/* Now count remaining bytes.  */
	ctx->total[0] += bytes;
	if (ctx->total[0] < bytes)
		++ctx->total[1];

	pad = bytes >= 56 ? 64 + 56 - bytes : 56 - bytes;
	memcpy(&ctx->buffer[bytes], SHA256_fillbuf, pad);

	/* Put the 64-bit file length in *bits* at the end of the buffer.  */
	*(uint32_t *) & ctx->buffer[bytes + pad + 4] = SHA256_SWAP(ctx->total[0] << 3);
	*(uint32_t *) & ctx->buffer[bytes + pad] = SHA256_SWAP((ctx->total[1] << 3) |
							(ctx->total[0] >> 29));

	/* Process last bytes.  */
	rb_sha256_process_block(ctx->buffer, bytes + pad + 8, ctx);

	/* Put result from CTX in first 32 bytes following RESBUF.  */
	for (i = 0; i < 8; ++i)
		((uint32_t *) resbuf)[i] = SHA256_SWAP(ctx->H[i]);

	return resbuf;
}


static void rb_sha256_process_bytes(const void *buffer, size_t len, struct sha256_ctx *ctx)
{
	/* When we already have some bits in our internal buffer concatenate
	   both inputs first.  */
	if (ctx->buflen != 0)
	{
		size_t left_over = ctx->buflen;
		size_t add = 128 - left_over > len ? len : 128 - left_over;

		memcpy(&ctx->buffer[left_over], buffer, add);
		ctx->buflen += add;

		if (ctx->buflen > 64)
		{
			rb_sha256_process_block(ctx->buffer, ctx->buflen & ~63, ctx);

			ctx->buflen &= 63;
			/* The regions in the following copy operation cannot overlap.  */
			memcpy(ctx->buffer, &ctx->buffer[(left_over + add) & ~63], ctx->buflen);
		}

		buffer = (const char *)buffer + add;
		len -= add;
	}

	/* Process available complete blocks.  */
	if (len >= 64)
	{
		/* To check alignment gcc has an appropriate operator.  Other
		   compilers don't.  */
		#if __GNUC__ >= 2
		#	define SHA256_UNALIGNED_P(p) (((uintptr_t) p) % __alignof__ (uint32_t) != 0)
		#else
		#	define SHA256_UNALIGNED_P(p) (((uintptr_t) p) % sizeof (uint32_t) != 0)
		#endif
		if (SHA256_UNALIGNED_P(buffer))
			while (len > 64)
			{
				rb_sha256_process_block(memcpy(ctx->buffer, buffer, 64), 64, ctx);
				buffer = (const char *)buffer + 64;
				len -= 64;
			}
		else
		{
			rb_sha256_process_block(buffer, len & ~63, ctx);
			buffer = (const char *)buffer + (len & ~63);
			len &= 63;
		}
	}

	/* Move remaining bytes into internal buffer.  */
	if (len > 0)
	{
		size_t left_over = ctx->buflen;

		memcpy(&ctx->buffer[left_over], buffer, len);
		left_over += len;
		if (left_over >= 64)
		{
			rb_sha256_process_block(ctx->buffer, 64, ctx);
			left_over -= 64;
			memcpy(ctx->buffer, &ctx->buffer[64], left_over);
		}
		ctx->buflen = left_over;
	}
}


/* Define our magic string to mark salt for SHA256 "encryption"
   replacement.  */
static const char sha256_salt_prefix[] = "$5$";

/* Prefix for optional rounds specification.  */
static const char sha256_rounds_prefix[] = "rounds=";

/* Maximum salt string length.  */
#define SHA256_SALT_LEN_MAX 16
/* Default number of rounds if not explicitly specified.  */
#define SHA256_ROUNDS_DEFAULT 5000
/* Minimum number of rounds.  */
#define SHA256_ROUNDS_MIN 1000
/* Maximum number of rounds.  */
#define SHA256_ROUNDS_MAX 999999999

static char *rb_sha256_crypt_r(const char *key, const char *salt, char *buffer, int buflen)
{
	unsigned char alt_result[32] __attribute__ ((__aligned__(__alignof__(uint32_t))));
	unsigned char temp_result[32] __attribute__ ((__aligned__(__alignof__(uint32_t))));
	struct sha256_ctx ctx;
	struct sha256_ctx alt_ctx;
	size_t salt_len;
	size_t key_len;
	size_t cnt;
	char *cp;
	char *copied_key = NULL;
	char *copied_salt = NULL;
	char *p_bytes;
	char *s_bytes;
	/* Default number of rounds.  */
	size_t rounds = SHA256_ROUNDS_DEFAULT;
	int rounds_custom = 0;

	/* Find beginning of salt string.  The prefix should normally always
	   be present.  Just in case it is not.  */
	if (strncmp(sha256_salt_prefix, salt, sizeof(sha256_salt_prefix) - 1) == 0)
		/* Skip salt prefix.  */
		salt += sizeof(sha256_salt_prefix) - 1;

	if (strncmp(salt, sha256_rounds_prefix, sizeof(sha256_rounds_prefix) - 1) == 0)
	{
		const char *num = salt + sizeof(sha256_rounds_prefix) - 1;
		char *endp;
		unsigned long int srounds = strtoul(num, &endp, 10);
		if (*endp == '$')
		{
			salt = endp + 1;
			rounds = MAX(SHA256_ROUNDS_MIN, MIN(srounds, SHA256_ROUNDS_MAX));
			rounds_custom = 1;
		}
	}

	salt_len = MIN(strcspn(salt, "$"), SHA256_SALT_LEN_MAX);
	key_len = strlen(key);

	if ((key - (char *)0) % __alignof__(uint32_t) != 0)
	{
		char *tmp = (char *)alloca(key_len + __alignof__(uint32_t));
		key = copied_key =
			memcpy(tmp + __alignof__(uint32_t)
			       - (tmp - (char *)0) % __alignof__(uint32_t), key, key_len);
	}

	if ((salt - (char *)0) % __alignof__(uint32_t) != 0)
	{
		char *tmp = (char *)alloca(salt_len + __alignof__(uint32_t));
		salt = copied_salt =
			memcpy(tmp + __alignof__(uint32_t)
			       - (tmp - (char *)0) % __alignof__(uint32_t), salt, salt_len);
	}

	/* Prepare for the real work.  */
	rb_sha256_init_ctx(&ctx);

	/* Add the key string.  */
	rb_sha256_process_bytes(key, key_len, &ctx);

	/* The last part is the salt string.  This must be at most 16
	   characters and it ends at the first `$' character (for
	   compatibility with existing implementations).  */
	rb_sha256_process_bytes(salt, salt_len, &ctx);


	/* Compute alternate SHA256 sum with input KEY, SALT, and KEY.  The
	   final result will be added to the first context.  */
	rb_sha256_init_ctx(&alt_ctx);

	/* Add key.  */
	rb_sha256_process_bytes(key, key_len, &alt_ctx);

	/* Add salt.  */
	rb_sha256_process_bytes(salt, salt_len, &alt_ctx);

	/* Add key again.  */
	rb_sha256_process_bytes(key, key_len, &alt_ctx);

	/* Now get result of this (32 bytes) and add it to the other
	   context.  */
	rb_sha256_finish_ctx(&alt_ctx, alt_result);

	/* Add for any character in the key one byte of the alternate sum.  */
	for (cnt = key_len; cnt > 32; cnt -= 32)
		rb_sha256_process_bytes(alt_result, 32, &ctx);
	rb_sha256_process_bytes(alt_result, cnt, &ctx);

	/* Take the binary representation of the length of the key and for every
	   1 add the alternate sum, for every 0 the key.  */
	for (cnt = key_len; cnt > 0; cnt >>= 1)
		if ((cnt & 1) != 0)
			rb_sha256_process_bytes(alt_result, 32, &ctx);
		else
			rb_sha256_process_bytes(key, key_len, &ctx);

	/* Create intermediate result.  */
	rb_sha256_finish_ctx(&ctx, alt_result);

	/* Start computation of P byte sequence.  */
	rb_sha256_init_ctx(&alt_ctx);

	/* For every character in the password add the entire password.  */
	for (cnt = 0; cnt < (size_t)(16 + alt_result[0]); ++cnt)
		rb_sha256_process_bytes(key, key_len, &alt_ctx);

	/* Finish the digest.  */
	rb_sha256_finish_ctx(&alt_ctx, temp_result);

	/* Create byte sequence P.  */
	cp = p_bytes = alloca(key_len);
	for (cnt = key_len; cnt >= 32; cnt -= 32)
	{
		memcpy(cp, temp_result, 32);
		cp += 32;
	}
	memcpy(cp, temp_result, cnt);

	/* Start computation of S byte sequence.  */
	rb_sha256_init_ctx(&alt_ctx);

	/* For every character in the password add the entire password.  */
	for (cnt = 0; cnt < (size_t)(16 + alt_result[0]); ++cnt)
		rb_sha256_process_bytes(salt, salt_len, &alt_ctx);

	/* Finish the digest.  */
	rb_sha256_finish_ctx(&alt_ctx, temp_result);

	/* Create byte sequence S.  */
	cp = s_bytes = alloca(salt_len);
	for (cnt = salt_len; cnt >= 32; cnt -= 32)
	{
		memcpy(cp, temp_result, 32);
		cp += 32;
	}
	memcpy(cp, temp_result, cnt);

	/* Repeatedly run the collected hash value through SHA256 to burn
	   CPU cycles.  */
	for (cnt = 0; cnt < rounds; ++cnt)
	{
		/* New context.  */
		rb_sha256_init_ctx(&ctx);

		/* Add key or last result.  */
		if ((cnt & 1) != 0)
			rb_sha256_process_bytes(p_bytes, key_len, &ctx);
		else
			rb_sha256_process_bytes(alt_result, 32, &ctx);

		/* Add salt for numbers not divisible by 3.  */
		if (cnt % 3 != 0)
			rb_sha256_process_bytes(s_bytes, salt_len, &ctx);

		/* Add key for numbers not divisible by 7.  */
		if (cnt % 7 != 0)
			rb_sha256_process_bytes(p_bytes, key_len, &ctx);

		/* Add key or last result.  */
		if ((cnt & 1) != 0)
			rb_sha256_process_bytes(alt_result, 32, &ctx);
		else
			rb_sha256_process_bytes(p_bytes, key_len, &ctx);

		/* Create intermediate result.  */
		rb_sha256_finish_ctx(&ctx, alt_result);
	}

	/* Now we can construct the result string.  It consists of three
	   parts.  */
	memset(buffer, '\0', MAX(0, buflen));
	strncpy(buffer, sha256_salt_prefix, MAX(0, buflen));
	if((cp = strchr(buffer, '\0')) == NULL)
		cp = buffer + MAX(0, buflen);
	buflen -= sizeof(sha256_salt_prefix) - 1;

	if (rounds_custom)
	{
		int n = snprintf(cp, MAX(0, buflen), "%s%zu$",
				 sha256_rounds_prefix, rounds);
		cp += n;
		buflen -= n;
	}

	memset(cp, '\0', salt_len);
	strncpy(cp, salt, MIN((size_t) MAX(0, buflen), salt_len));
	if((cp = strchr(buffer, '\0')) == NULL)
		cp += salt_len;
	buflen -= MIN((size_t) MAX(0, buflen), salt_len);

	if (buflen > 0)
	{
		*cp++ = '$';
		--buflen;
	}

	b64_from_24bit(alt_result[0], alt_result[10], alt_result[20], 4);
	b64_from_24bit(alt_result[21], alt_result[1], alt_result[11], 4);
	b64_from_24bit(alt_result[12], alt_result[22], alt_result[2], 4);
	b64_from_24bit(alt_result[3], alt_result[13], alt_result[23], 4);
	b64_from_24bit(alt_result[24], alt_result[4], alt_result[14], 4);
	b64_from_24bit(alt_result[15], alt_result[25], alt_result[5], 4);
	b64_from_24bit(alt_result[6], alt_result[16], alt_result[26], 4);
	b64_from_24bit(alt_result[27], alt_result[7], alt_result[17], 4);
	b64_from_24bit(alt_result[18], alt_result[28], alt_result[8], 4);
	b64_from_24bit(alt_result[9], alt_result[19], alt_result[29], 4);
	b64_from_24bit(0, alt_result[31], alt_result[30], 3);
	if (buflen <= 0)
	{
		errno = ERANGE;
		buffer = NULL;
	}
	else
		*cp = '\0';	/* Terminate the string.  */

	/* Clear the buffer for the intermediate result so that people
	   attaching to processes or reading core dumps cannot get any
	   information.  We do it in this way to clear correct_words[]
	   inside the SHA256 implementation as well.  */
	rb_sha256_init_ctx(&ctx);
	rb_sha256_finish_ctx(&ctx, alt_result);
	memset(temp_result, '\0', sizeof(temp_result));
	memset(p_bytes, '\0', key_len);
	memset(s_bytes, '\0', salt_len);
	memset(&ctx, '\0', sizeof(ctx));
	memset(&alt_ctx, '\0', sizeof(alt_ctx));
	if (copied_key != NULL)
		memset(copied_key, '\0', key_len);
	if (copied_salt != NULL)
		memset(copied_salt, '\0', salt_len);

	return buffer;
}


/* This entry point is equivalent to the `crypt' function in Unix
   libcs.  */
static char *rb_sha256_crypt(const char *key, const char *salt)
{
	/* We don't want to have an arbitrary limit in the size of the
	   password.  We can compute an upper bound for the size of the
	   result in advance and so we can prepare the buffer we pass to
	   `rb_sha256_crypt_r'.  */
	static char *buffer;
	static int buflen;
	int needed = (sizeof(sha256_salt_prefix) - 1
		      + sizeof(sha256_rounds_prefix) + 9 + 1 + strlen(salt) + 1 + 43 + 1);

		char *new_buffer = (char *)malloc(needed);
		if (new_buffer == NULL)
			return NULL;

		buffer = new_buffer;
		buflen = needed;

	return rb_sha256_crypt_r(key, salt, buffer, buflen);
}

/* Structure to save state of computation between the single steps.  */
struct sha512_ctx
{
	uint64_t H[8];

	uint64_t total[2];
	uint64_t buflen;
	char buffer[256];	/* NB: always correctly aligned for uint64_t.  */
};


#ifndef WORDS_BIGENDIAN
#	define SHA512_SWAP(n)			\
		(((n) << 56)			\
		| (((n) & 0xff00) << 40)	\
		| (((n) & 0xff0000) << 24)	\
		| (((n) & 0xff000000) << 8)	\
		| (((n) >> 8) & 0xff000000)	\
		| (((n) >> 24) & 0xff0000)	\
		| (((n) >> 40) & 0xff00)	\
		| ((n) >> 56))
#else
#	define SHA512_SWAP(n) (n)
#endif


/* This array contains the bytes used to pad the buffer to the next
   64-byte boundary.  (FIPS 180-2:5.1.2)  */
static const unsigned char SHA512_fillbuf[128] = { 0x80, 0 /* , 0, 0, ...  */  };


/* Constants for SHA512 from FIPS 180-2:4.2.3.  */
static const uint64_t SHA512_K[80] = {
	0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL,
	0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
	0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
	0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
	0xd807aa98a3030242ULL, 0x12835b0145706fbeULL,
	0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
	0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL,
	0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
	0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
	0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
	0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL,
	0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
	0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL,
	0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
	0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
	0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
	0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL,
	0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
	0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL,
	0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
	0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
	0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
	0xd192e819d6ef5218ULL, 0xd69906245565a910ULL,
	0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
	0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL,
	0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
	0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
	0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
	0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL,
	0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
	0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL,
	0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
	0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
	0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
	0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL,
	0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
	0x28db77f523047d84ULL, 0x32caab7b40c72493ULL,
	0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
	0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
	0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};


/* Process LEN bytes of BUFFER, accumulating context into CTX.
   It is assumed that LEN % 128 == 0.  */
static void rb_sha512_process_block(const void *buffer, size_t len, struct sha512_ctx *ctx)
{
	const uint64_t *words = buffer;
	size_t nwords = len / sizeof(uint64_t);
	uint64_t a = ctx->H[0];
	uint64_t b = ctx->H[1];
	uint64_t c = ctx->H[2];
	uint64_t d = ctx->H[3];
	uint64_t e = ctx->H[4];
	uint64_t f = ctx->H[5];
	uint64_t g = ctx->H[6];
	uint64_t h = ctx->H[7];

	/* First increment the byte count.  FIPS 180-2 specifies the possible
	   length of the file up to 2^128 bits.  Here we only compute the
	   number of bytes.  Do a double word increment.  */
	ctx->total[0] += len;
	if (ctx->total[0] < len)
		++ctx->total[1];

	/* Process all bytes in the buffer with 128 bytes in each round of
	   the loop.  */
	while (nwords > 0)
	{
		uint64_t W[80];
		uint64_t a_save = a;
		uint64_t b_save = b;
		uint64_t c_save = c;
		uint64_t d_save = d;
		uint64_t e_save = e;
		uint64_t f_save = f;
		uint64_t g_save = g;
		uint64_t h_save = h;
		unsigned int t;

		/* Operators defined in FIPS 180-2:4.1.2.  */
		#define SHA512_Ch(x, y, z) ((x & y) ^ (~x & z))
		#define SHA512_Maj(x, y, z) ((x & y) ^ (x & z) ^ (y & z))
		#define SHA512_S0(x) (SHA512_CYCLIC (x, 28) ^ SHA512_CYCLIC (x, 34) ^ SHA512_CYCLIC (x, 39))
		#define SHA512_S1(x) (SHA512_CYCLIC (x, 14) ^ SHA512_CYCLIC (x, 18) ^ SHA512_CYCLIC (x, 41))
		#define SHA512_R0(x) (SHA512_CYCLIC (x, 1) ^ SHA512_CYCLIC (x, 8) ^ (x >> 7))
		#define SHA512_R1(x) (SHA512_CYCLIC (x, 19) ^ SHA512_CYCLIC (x, 61) ^ (x >> 6))

		/* It is unfortunate that C does not provide an operator for
		   cyclic rotation.  Hope the C compiler is smart enough.  */
		#define SHA512_CYCLIC(w, s) ((w >> s) | (w << (64 - s)))

		/* Compute the message schedule according to FIPS 180-2:6.3.2 step 2.  */
		for (t = 0; t < 16; ++t)
		{
			W[t] = SHA512_SWAP(*words);
			++words;
		}
		for (t = 16; t < 80; ++t)
			W[t] = SHA512_R1(W[t - 2]) + W[t - 7] + SHA512_R0(W[t - 15]) + W[t - 16];

		/* The actual computation according to FIPS 180-2:6.3.2 step 3.  */
		for (t = 0; t < 80; ++t)
		{
			uint64_t T1 = h + SHA512_S1(e) + SHA512_Ch(e, f, g) + SHA512_K[t] + W[t];
			uint64_t T2 = SHA512_S0(a) + SHA512_Maj(a, b, c);
			h = g;
			g = f;
			f = e;
			e = d + T1;
			d = c;
			c = b;
			b = a;
			a = T1 + T2;
		}

		/* Add the starting values of the context according to FIPS 180-2:6.3.2
		   step 4.  */
		a += a_save;
		b += b_save;
		c += c_save;
		d += d_save;
		e += e_save;
		f += f_save;
		g += g_save;
		h += h_save;

		/* Prepare for the next round.  */
		nwords -= 16;
	}

	/* Put checksum in context given as argument.  */
	ctx->H[0] = a;
	ctx->H[1] = b;
	ctx->H[2] = c;
	ctx->H[3] = d;
	ctx->H[4] = e;
	ctx->H[5] = f;
	ctx->H[6] = g;
	ctx->H[7] = h;
}


/* Initialize structure containing state of computation.
   (FIPS 180-2:5.3.3)  */
static void rb_sha512_init_ctx(struct sha512_ctx *ctx)
{
	ctx->H[0] = 0x6a09e667f3bcc908ULL;
	ctx->H[1] = 0xbb67ae8584caa73bULL;
	ctx->H[2] = 0x3c6ef372fe94f82bULL;
	ctx->H[3] = 0xa54ff53a5f1d36f1ULL;
	ctx->H[4] = 0x510e527fade682d1ULL;
	ctx->H[5] = 0x9b05688c2b3e6c1fULL;
	ctx->H[6] = 0x1f83d9abfb41bd6bULL;
	ctx->H[7] = 0x5be0cd19137e2179ULL;

	ctx->total[0] = ctx->total[1] = 0;
	ctx->buflen = 0;
}


/* Process the remaining bytes in the internal buffer and the usual
   prolog according to the standard and write the result to RESBUF.

   IMPORTANT: On some systems it is required that RESBUF is correctly
   aligned for a 32 bits value.  */
static void *rb_sha512_finish_ctx(struct sha512_ctx *ctx, void *resbuf)
{
	/* Take yet unprocessed bytes into account.  */
	uint64_t bytes = ctx->buflen;
	size_t pad;
	unsigned int i;

	/* Now count remaining bytes.  */
	ctx->total[0] += bytes;
	if (ctx->total[0] < bytes)
		++ctx->total[1];

	pad = bytes >= 112 ? 128 + 112 - bytes : 112 - bytes;
	memcpy(&ctx->buffer[bytes], SHA512_fillbuf, pad);

	/* Put the 128-bit file length in *bits* at the end of the buffer.  */
	*(uint64_t *) & ctx->buffer[bytes + pad + 8] = SHA512_SWAP(ctx->total[0] << 3);
	*(uint64_t *) & ctx->buffer[bytes + pad] = SHA512_SWAP((ctx->total[1] << 3) |
							(ctx->total[0] >> 61));

	/* Process last bytes.  */
	rb_sha512_process_block(ctx->buffer, bytes + pad + 16, ctx);

	/* Put result from CTX in first 64 bytes following RESBUF.  */
	for (i = 0; i < 8; ++i)
		((uint64_t *) resbuf)[i] = SHA512_SWAP(ctx->H[i]);

	return resbuf;
}


static void rb_sha512_process_bytes(const void *buffer, size_t len, struct sha512_ctx *ctx)
{
	/* When we already have some bits in our internal buffer concatenate
	   both inputs first.  */
	if (ctx->buflen != 0)
	{
		size_t left_over = ctx->buflen;
		size_t add = 256 - left_over > len ? len : 256 - left_over;

		memcpy(&ctx->buffer[left_over], buffer, add);
		ctx->buflen += add;

		if (ctx->buflen > 128)
		{
			rb_sha512_process_block(ctx->buffer, ctx->buflen & ~127, ctx);

			ctx->buflen &= 127;
			/* The regions in the following copy operation cannot overlap.  */
			memcpy(ctx->buffer, &ctx->buffer[(left_over + add) & ~127], ctx->buflen);
		}

		buffer = (const char *)buffer + add;
		len -= add;
	}

	/* Process available complete blocks.  */
	if (len >= 128)
	{
	#if !_STRING_ARCH_unaligned
	/* To check alignment gcc has an appropriate operator.  Other
	   compilers don't.  */
	#	if __GNUC__ >= 2
	#		define SHA512_UNALIGNED_P(p) (((uintptr_t) p) % __alignof__ (uint64_t) != 0)
	#	else
	#		define SHA512_UNALIGNED_P(p) (((uintptr_t) p) % sizeof (uint64_t) != 0)
	#	endif
		if (SHA512_UNALIGNED_P(buffer))
			while (len > 128)
			{
				rb_sha512_process_block(memcpy(ctx->buffer, buffer, 128), 128, ctx);
				buffer = (const char *)buffer + 128;
				len -= 128;
			}
		else
	#endif
		{
			rb_sha512_process_block(buffer, len & ~127, ctx);
			buffer = (const char *)buffer + (len & ~127);
			len &= 127;
		}
	}

	/* Move remaining bytes into internal buffer.  */
	if (len > 0)
	{
		size_t left_over = ctx->buflen;

		memcpy(&ctx->buffer[left_over], buffer, len);
		left_over += len;
		if (left_over >= 128)
		{
			rb_sha512_process_block(ctx->buffer, 128, ctx);
			left_over -= 128;
			memcpy(ctx->buffer, &ctx->buffer[128], left_over);
		}
		ctx->buflen = left_over;
	}
}


/* Define our magic string to mark salt for SHA512 "encryption"
   replacement.  */
static const char sha512_salt_prefix[] = "$6$";

/* Prefix for optional rounds specification.  */
static const char sha512_rounds_prefix[] = "rounds=";

/* Maximum salt string length.  */
#define SHA512_SALT_LEN_MAX 16
/* Default number of rounds if not explicitly specified.  */
#define SHA512_ROUNDS_DEFAULT 5000
/* Minimum number of rounds.  */
#define SHA512_ROUNDS_MIN 1000
/* Maximum number of rounds.  */
#define SHA512_ROUNDS_MAX 999999999

static char *rb_sha512_crypt_r(const char *key, const char *salt, char *buffer, int buflen)
{
	unsigned char alt_result[64] __attribute__ ((__aligned__(__alignof__(uint64_t))));
	unsigned char temp_result[64] __attribute__ ((__aligned__(__alignof__(uint64_t))));
	struct sha512_ctx ctx;
	struct sha512_ctx alt_ctx;
	size_t salt_len;
	size_t key_len;
	size_t cnt;
	char *cp;
	char *copied_key = NULL;
	char *copied_salt = NULL;
	char *p_bytes;
	char *s_bytes;
	/* Default number of rounds.  */
	size_t rounds = SHA512_ROUNDS_DEFAULT;
	int rounds_custom = 0;

	/* Find beginning of salt string.  The prefix should normally always
	   be present.  Just in case it is not.  */
	if (strncmp(sha512_salt_prefix, salt, sizeof(sha512_salt_prefix) - 1) == 0)
		/* Skip salt prefix.  */
		salt += sizeof(sha512_salt_prefix) - 1;

	if (strncmp(salt, sha512_rounds_prefix, sizeof(sha512_rounds_prefix) - 1) == 0)
	{
		const char *num = salt + sizeof(sha512_rounds_prefix) - 1;
		char *endp;
		unsigned long int srounds = strtoul(num, &endp, 10);
		if (*endp == '$')
		{
			salt = endp + 1;
			rounds = MAX(SHA512_ROUNDS_MIN, MIN(srounds, SHA512_ROUNDS_MAX));
			rounds_custom = 1;
		}
	}

	salt_len = MIN(strcspn(salt, "$"), SHA512_SALT_LEN_MAX);
	key_len = strlen(key);

	if ((key - (char *)0) % __alignof__(uint64_t) != 0)
	{
		char *tmp = (char *)alloca(key_len + __alignof__(uint64_t));
		key = copied_key =
			memcpy(tmp + __alignof__(uint64_t)
			       - (tmp - (char *)0) % __alignof__(uint64_t), key, key_len);
	}

	if ((salt - (char *)0) % __alignof__(uint64_t) != 0)
	{
		char *tmp = (char *)alloca(salt_len + __alignof__(uint64_t));
		salt = copied_salt =
			memcpy(tmp + __alignof__(uint64_t)
			       - (tmp - (char *)0) % __alignof__(uint64_t), salt, salt_len);
	}

	/* Prepare for the real work.  */
	rb_sha512_init_ctx(&ctx);

	/* Add the key string.  */
	rb_sha512_process_bytes(key, key_len, &ctx);

	/* The last part is the salt string.  This must be at most 16
	   characters and it ends at the first `$' character (for
	   compatibility with existing implementations).  */
	rb_sha512_process_bytes(salt, salt_len, &ctx);


	/* Compute alternate SHA512 sum with input KEY, SALT, and KEY.  The
	   final result will be added to the first context.  */
	rb_sha512_init_ctx(&alt_ctx);

	/* Add key.  */
	rb_sha512_process_bytes(key, key_len, &alt_ctx);

	/* Add salt.  */
	rb_sha512_process_bytes(salt, salt_len, &alt_ctx);

	/* Add key again.  */
	rb_sha512_process_bytes(key, key_len, &alt_ctx);

	/* Now get result of this (64 bytes) and add it to the other
	   context.  */
	rb_sha512_finish_ctx(&alt_ctx, alt_result);

	/* Add for any character in the key one byte of the alternate sum.  */
	for (cnt = key_len; cnt > 64; cnt -= 64)
		rb_sha512_process_bytes(alt_result, 64, &ctx);
	rb_sha512_process_bytes(alt_result, cnt, &ctx);

	/* Take the binary representation of the length of the key and for every
	   1 add the alternate sum, for every 0 the key.  */
	for (cnt = key_len; cnt > 0; cnt >>= 1)
		if ((cnt & 1) != 0)
			rb_sha512_process_bytes(alt_result, 64, &ctx);
		else
			rb_sha512_process_bytes(key, key_len, &ctx);

	/* Create intermediate result.  */
	rb_sha512_finish_ctx(&ctx, alt_result);

	/* Start computation of P byte sequence.  */
	rb_sha512_init_ctx(&alt_ctx);

	/* For every character in the password add the entire password.  */
	for (cnt = 0; cnt < key_len; ++cnt)
		rb_sha512_process_bytes(key, key_len, &alt_ctx);

	/* Finish the digest.  */
	rb_sha512_finish_ctx(&alt_ctx, temp_result);

	/* Create byte sequence P.  */
	cp = p_bytes = alloca(key_len);
	for (cnt = key_len; cnt >= 64; cnt -= 64)
	{
		memcpy(cp, temp_result, 64);
		cp += 64;
	}
	memcpy(cp, temp_result, cnt);

	/* Start computation of S byte sequence.  */
	rb_sha512_init_ctx(&alt_ctx);

	/* For every character in the password add the entire password.  */
	for (cnt = 0; cnt < (size_t)(16 + alt_result[0]); ++cnt)
		rb_sha512_process_bytes(salt, salt_len, &alt_ctx);

	/* Finish the digest.  */
	rb_sha512_finish_ctx(&alt_ctx, temp_result);

	/* Create byte sequence S.  */
	cp = s_bytes = alloca(salt_len);
	for (cnt = salt_len; cnt >= 64; cnt -= 64)
	{
		memcpy(cp, temp_result, 64);
		cp += 64;
	}
	memcpy(cp, temp_result, cnt);

	/* Repeatedly run the collected hash value through SHA512 to burn
	   CPU cycles.  */
	for (cnt = 0; cnt < rounds; ++cnt)
	{
		/* New context.  */
		rb_sha512_init_ctx(&ctx);

		/* Add key or last result.  */
		if ((cnt & 1) != 0)
			rb_sha512_process_bytes(p_bytes, key_len, &ctx);
		else
			rb_sha512_process_bytes(alt_result, 64, &ctx);

		/* Add salt for numbers not divisible by 3.  */
		if (cnt % 3 != 0)
			rb_sha512_process_bytes(s_bytes, salt_len, &ctx);

		/* Add key for numbers not divisible by 7.  */
		if (cnt % 7 != 0)
			rb_sha512_process_bytes(p_bytes, key_len, &ctx);

		/* Add key or last result.  */
		if ((cnt & 1) != 0)
			rb_sha512_process_bytes(alt_result, 64, &ctx);
		else
			rb_sha512_process_bytes(p_bytes, key_len, &ctx);

		/* Create intermediate result.  */
		rb_sha512_finish_ctx(&ctx, alt_result);
	}

	/* Now we can construct the result string.  It consists of three
	   parts.  */
	memset(buffer, '\0', MAX(0, buflen));
	strncpy(buffer, sha512_salt_prefix, MAX(0, buflen));
	if((cp = strchr(buffer, '\0')) == NULL)
		cp = buffer + MAX(0, buflen);
	buflen -= sizeof(sha512_salt_prefix) - 1;

	if (rounds_custom)
	{
		int n = snprintf(cp, MAX(0, buflen), "%s%zu$",
				 sha512_rounds_prefix, rounds);
		cp += n;
		buflen -= n;
	}

	memset(cp, '\0', MIN((size_t) MAX(0, buflen), salt_len));
	strncpy(cp, salt, MIN((size_t) MAX(0, buflen), salt_len));
	if((cp = strchr(buffer, '\0')) == NULL)
		cp = buffer + salt_len;
	buflen -= MIN((size_t) MAX(0, buflen), salt_len);

	if (buflen > 0)
	{
		*cp++ = '$';
		--buflen;
	}

	b64_from_24bit(alt_result[0], alt_result[21], alt_result[42], 4);
	b64_from_24bit(alt_result[22], alt_result[43], alt_result[1], 4);
	b64_from_24bit(alt_result[44], alt_result[2], alt_result[23], 4);
	b64_from_24bit(alt_result[3], alt_result[24], alt_result[45], 4);
	b64_from_24bit(alt_result[25], alt_result[46], alt_result[4], 4);
	b64_from_24bit(alt_result[47], alt_result[5], alt_result[26], 4);
	b64_from_24bit(alt_result[6], alt_result[27], alt_result[48], 4);
	b64_from_24bit(alt_result[28], alt_result[49], alt_result[7], 4);
	b64_from_24bit(alt_result[50], alt_result[8], alt_result[29], 4);
	b64_from_24bit(alt_result[9], alt_result[30], alt_result[51], 4);
	b64_from_24bit(alt_result[31], alt_result[52], alt_result[10], 4);
	b64_from_24bit(alt_result[53], alt_result[11], alt_result[32], 4);
	b64_from_24bit(alt_result[12], alt_result[33], alt_result[54], 4);
	b64_from_24bit(alt_result[34], alt_result[55], alt_result[13], 4);
	b64_from_24bit(alt_result[56], alt_result[14], alt_result[35], 4);
	b64_from_24bit(alt_result[15], alt_result[36], alt_result[57], 4);
	b64_from_24bit(alt_result[37], alt_result[58], alt_result[16], 4);
	b64_from_24bit(alt_result[59], alt_result[17], alt_result[38], 4);
	b64_from_24bit(alt_result[18], alt_result[39], alt_result[60], 4);
	b64_from_24bit(alt_result[40], alt_result[61], alt_result[19], 4);
	b64_from_24bit(alt_result[62], alt_result[20], alt_result[41], 4);
	b64_from_24bit(0, 0, alt_result[63], 2);

	if (buflen <= 0)
	{
		errno = ERANGE;
		buffer = NULL;
	}
	else
		*cp = '\0';	/* Terminate the string.  */

	/* Clear the buffer for the intermediate result so that people
	   attaching to processes or reading core dumps cannot get any
	   information.  We do it in this way to clear correct_words[]
	   inside the SHA512 implementation as well.  */
	rb_sha512_init_ctx(&ctx);
	rb_sha512_finish_ctx(&ctx, alt_result);
	memset(temp_result, '\0', sizeof(temp_result));
	memset(p_bytes, '\0', key_len);
	memset(s_bytes, '\0', salt_len);
	memset(&ctx, '\0', sizeof(ctx));
	memset(&alt_ctx, '\0', sizeof(alt_ctx));
	if (copied_key != NULL)
		memset(copied_key, '\0', key_len);
	if (copied_salt != NULL)
		memset(copied_salt, '\0', salt_len);

	return buffer;
}


/* This entry point is equivalent to the `crypt' function in Unix
   libcs.  */
static char *rb_sha512_crypt(const char *key, const char *salt)
{
	/* We don't want to have an arbitrary limit in the size of the
	   password.  We can compute an upper bound for the size of the
	   result in advance and so we can prepare the buffer we pass to
	   `rb_sha512_crypt_r'.  */
	static char *buffer;
	static int buflen;
	int needed = (sizeof(sha512_salt_prefix) - 1
		      + sizeof(sha512_rounds_prefix) + 9 + 1 + strlen(salt) + 1 + 86 + 1);

	if (buflen < needed)
	{
		char *new_buffer = (char *)realloc(buffer, needed);
		if (new_buffer == NULL)
			return NULL;

		buffer = new_buffer;
		buflen = needed;
	}

	return rb_sha512_crypt_r(key, salt, buffer, buflen);
}


/* And now blowfish */
/*
 * Copyright 1997 Niels Provos <provos@physnet.uni-hamburg.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Schneier states the maximum key length to be 56 bytes.
 * The way how the subkeys are initalized by the key up
 * to (N+2)*4 i.e. 72 bytes are utilized.
 * Warning: For normal blowfish encryption only 56 bytes
 * of the key affect all cipherbits.
 */

#define BLF_N   16                      /* Number of Subkeys */

/* Blowfish context */
typedef struct BlowfishContext {
        uint32_t S[4][256];    /* S-Boxes */
        uint32_t P[BLF_N + 2]; /* Subkeys */
} blf_ctx;

/* Raw access to customized Blowfish
 *      blf_key is just:
 *      Blowfish_initstate( state )
 *      Blowfish_expand0state( state, key, keylen )
 */

void Blowfish_initstate(blf_ctx *);
void Blowfish_expand0state(blf_ctx *, const uint8_t *, uint16_t);
void Blowfish_expandstate
   (blf_ctx *, const uint8_t *, uint16_t, const uint8_t *, uint16_t);
uint32_t Blowfish_stream2word(const uint8_t *, uint16_t, uint16_t *);

void blf_enc(blf_ctx *, uint32_t *, uint16_t);

/*
 * This code is derived from section 14.3 and the given source
 * in section V of Applied Cryptography, second edition.
 * Blowfish is an unpatented fast block cipher designed by
 * Bruce Schneier.
 */

/*
 * FreeBSD implementation by Paul Herman <pherman@frenchfries.net>
 */

/* Function for Feistel Networks */

#define _F(s, x) ((((s)[        (((x)>>24)&0xFF)]  \
		 + (s)[0x100 + (((x)>>16)&0xFF)]) \
		 ^ (s)[0x200 + (((x)>> 8)&0xFF)]) \
		 + (s)[0x300 + ( (x)     &0xFF)])

#define BLFRND(s, p, i, j, n) (i ^= _F(s, j) ^ (p)[n])

static void
Blowfish_encipher(blf_ctx *c, uint32_t *xl, uint32_t *xr)
{
	uint32_t Xl;
	uint32_t Xr;
	uint32_t *s = c->S[0];
	uint32_t *p = c->P;

	Xl = *xl;
	Xr = *xr;

	Xl ^= p[0];
	BLFRND(s, p, Xr, Xl, 1); BLFRND(s, p, Xl, Xr, 2);
	BLFRND(s, p, Xr, Xl, 3); BLFRND(s, p, Xl, Xr, 4);
	BLFRND(s, p, Xr, Xl, 5); BLFRND(s, p, Xl, Xr, 6);
	BLFRND(s, p, Xr, Xl, 7); BLFRND(s, p, Xl, Xr, 8);
	BLFRND(s, p, Xr, Xl, 9); BLFRND(s, p, Xl, Xr, 10);
	BLFRND(s, p, Xr, Xl, 11); BLFRND(s, p, Xl, Xr, 12);
	BLFRND(s, p, Xr, Xl, 13); BLFRND(s, p, Xl, Xr, 14);
	BLFRND(s, p, Xr, Xl, 15); BLFRND(s, p, Xl, Xr, 16);

	*xl = Xr ^ p[17];
	*xr = Xl;
}

void
Blowfish_initstate(blf_ctx *c)
{

/* P-box and S-box tables initialized with digits of Pi */

	const blf_ctx bf_initstate =

	{ {
		{
			0xd1310ba6, 0x98dfb5ac, 0x2ffd72db, 0xd01adfb7,
			0xb8e1afed, 0x6a267e96, 0xba7c9045, 0xf12c7f99,
			0x24a19947, 0xb3916cf7, 0x0801f2e2, 0x858efc16,
			0x636920d8, 0x71574e69, 0xa458fea3, 0xf4933d7e,
			0x0d95748f, 0x728eb658, 0x718bcd58, 0x82154aee,
			0x7b54a41d, 0xc25a59b5, 0x9c30d539, 0x2af26013,
			0xc5d1b023, 0x286085f0, 0xca417918, 0xb8db38ef,
			0x8e79dcb0, 0x603a180e, 0x6c9e0e8b, 0xb01e8a3e,
			0xd71577c1, 0xbd314b27, 0x78af2fda, 0x55605c60,
			0xe65525f3, 0xaa55ab94, 0x57489862, 0x63e81440,
			0x55ca396a, 0x2aab10b6, 0xb4cc5c34, 0x1141e8ce,
			0xa15486af, 0x7c72e993, 0xb3ee1411, 0x636fbc2a,
			0x2ba9c55d, 0x741831f6, 0xce5c3e16, 0x9b87931e,
			0xafd6ba33, 0x6c24cf5c, 0x7a325381, 0x28958677,
			0x3b8f4898, 0x6b4bb9af, 0xc4bfe81b, 0x66282193,
			0x61d809cc, 0xfb21a991, 0x487cac60, 0x5dec8032,
			0xef845d5d, 0xe98575b1, 0xdc262302, 0xeb651b88,
			0x23893e81, 0xd396acc5, 0x0f6d6ff3, 0x83f44239,
			0x2e0b4482, 0xa4842004, 0x69c8f04a, 0x9e1f9b5e,
			0x21c66842, 0xf6e96c9a, 0x670c9c61, 0xabd388f0,
			0x6a51a0d2, 0xd8542f68, 0x960fa728, 0xab5133a3,
			0x6eef0b6c, 0x137a3be4, 0xba3bf050, 0x7efb2a98,
			0xa1f1651d, 0x39af0176, 0x66ca593e, 0x82430e88,
			0x8cee8619, 0x456f9fb4, 0x7d84a5c3, 0x3b8b5ebe,
			0xe06f75d8, 0x85c12073, 0x401a449f, 0x56c16aa6,
			0x4ed3aa62, 0x363f7706, 0x1bfedf72, 0x429b023d,
			0x37d0d724, 0xd00a1248, 0xdb0fead3, 0x49f1c09b,
			0x075372c9, 0x80991b7b, 0x25d479d8, 0xf6e8def7,
			0xe3fe501a, 0xb6794c3b, 0x976ce0bd, 0x04c006ba,
			0xc1a94fb6, 0x409f60c4, 0x5e5c9ec2, 0x196a2463,
			0x68fb6faf, 0x3e6c53b5, 0x1339b2eb, 0x3b52ec6f,
			0x6dfc511f, 0x9b30952c, 0xcc814544, 0xaf5ebd09,
			0xbee3d004, 0xde334afd, 0x660f2807, 0x192e4bb3,
			0xc0cba857, 0x45c8740f, 0xd20b5f39, 0xb9d3fbdb,
			0x5579c0bd, 0x1a60320a, 0xd6a100c6, 0x402c7279,
			0x679f25fe, 0xfb1fa3cc, 0x8ea5e9f8, 0xdb3222f8,
			0x3c7516df, 0xfd616b15, 0x2f501ec8, 0xad0552ab,
			0x323db5fa, 0xfd238760, 0x53317b48, 0x3e00df82,
			0x9e5c57bb, 0xca6f8ca0, 0x1a87562e, 0xdf1769db,
			0xd542a8f6, 0x287effc3, 0xac6732c6, 0x8c4f5573,
			0x695b27b0, 0xbbca58c8, 0xe1ffa35d, 0xb8f011a0,
			0x10fa3d98, 0xfd2183b8, 0x4afcb56c, 0x2dd1d35b,
			0x9a53e479, 0xb6f84565, 0xd28e49bc, 0x4bfb9790,
			0xe1ddf2da, 0xa4cb7e33, 0x62fb1341, 0xcee4c6e8,
			0xef20cada, 0x36774c01, 0xd07e9efe, 0x2bf11fb4,
			0x95dbda4d, 0xae909198, 0xeaad8e71, 0x6b93d5a0,
			0xd08ed1d0, 0xafc725e0, 0x8e3c5b2f, 0x8e7594b7,
			0x8ff6e2fb, 0xf2122b64, 0x8888b812, 0x900df01c,
			0x4fad5ea0, 0x688fc31c, 0xd1cff191, 0xb3a8c1ad,
			0x2f2f2218, 0xbe0e1777, 0xea752dfe, 0x8b021fa1,
			0xe5a0cc0f, 0xb56f74e8, 0x18acf3d6, 0xce89e299,
			0xb4a84fe0, 0xfd13e0b7, 0x7cc43b81, 0xd2ada8d9,
			0x165fa266, 0x80957705, 0x93cc7314, 0x211a1477,
			0xe6ad2065, 0x77b5fa86, 0xc75442f5, 0xfb9d35cf,
			0xebcdaf0c, 0x7b3e89a0, 0xd6411bd3, 0xae1e7e49,
			0x00250e2d, 0x2071b35e, 0x226800bb, 0x57b8e0af,
			0x2464369b, 0xf009b91e, 0x5563911d, 0x59dfa6aa,
			0x78c14389, 0xd95a537f, 0x207d5ba2, 0x02e5b9c5,
			0x83260376, 0x6295cfa9, 0x11c81968, 0x4e734a41,
			0xb3472dca, 0x7b14a94a, 0x1b510052, 0x9a532915,
			0xd60f573f, 0xbc9bc6e4, 0x2b60a476, 0x81e67400,
			0x08ba6fb5, 0x571be91f, 0xf296ec6b, 0x2a0dd915,
			0xb6636521, 0xe7b9f9b6, 0xff34052e, 0xc5855664,
			0x53b02d5d, 0xa99f8fa1, 0x08ba4799, 0x6e85076a},
		{
			0x4b7a70e9, 0xb5b32944, 0xdb75092e, 0xc4192623,
			0xad6ea6b0, 0x49a7df7d, 0x9cee60b8, 0x8fedb266,
			0xecaa8c71, 0x699a17ff, 0x5664526c, 0xc2b19ee1,
			0x193602a5, 0x75094c29, 0xa0591340, 0xe4183a3e,
			0x3f54989a, 0x5b429d65, 0x6b8fe4d6, 0x99f73fd6,
			0xa1d29c07, 0xefe830f5, 0x4d2d38e6, 0xf0255dc1,
			0x4cdd2086, 0x8470eb26, 0x6382e9c6, 0x021ecc5e,
			0x09686b3f, 0x3ebaefc9, 0x3c971814, 0x6b6a70a1,
			0x687f3584, 0x52a0e286, 0xb79c5305, 0xaa500737,
			0x3e07841c, 0x7fdeae5c, 0x8e7d44ec, 0x5716f2b8,
			0xb03ada37, 0xf0500c0d, 0xf01c1f04, 0x0200b3ff,
			0xae0cf51a, 0x3cb574b2, 0x25837a58, 0xdc0921bd,
			0xd19113f9, 0x7ca92ff6, 0x94324773, 0x22f54701,
			0x3ae5e581, 0x37c2dadc, 0xc8b57634, 0x9af3dda7,
			0xa9446146, 0x0fd0030e, 0xecc8c73e, 0xa4751e41,
			0xe238cd99, 0x3bea0e2f, 0x3280bba1, 0x183eb331,
			0x4e548b38, 0x4f6db908, 0x6f420d03, 0xf60a04bf,
			0x2cb81290, 0x24977c79, 0x5679b072, 0xbcaf89af,
			0xde9a771f, 0xd9930810, 0xb38bae12, 0xdccf3f2e,
			0x5512721f, 0x2e6b7124, 0x501adde6, 0x9f84cd87,
			0x7a584718, 0x7408da17, 0xbc9f9abc, 0xe94b7d8c,
			0xec7aec3a, 0xdb851dfa, 0x63094366, 0xc464c3d2,
			0xef1c1847, 0x3215d908, 0xdd433b37, 0x24c2ba16,
			0x12a14d43, 0x2a65c451, 0x50940002, 0x133ae4dd,
			0x71dff89e, 0x10314e55, 0x81ac77d6, 0x5f11199b,
			0x043556f1, 0xd7a3c76b, 0x3c11183b, 0x5924a509,
			0xf28fe6ed, 0x97f1fbfa, 0x9ebabf2c, 0x1e153c6e,
			0x86e34570, 0xeae96fb1, 0x860e5e0a, 0x5a3e2ab3,
			0x771fe71c, 0x4e3d06fa, 0x2965dcb9, 0x99e71d0f,
			0x803e89d6, 0x5266c825, 0x2e4cc978, 0x9c10b36a,
			0xc6150eba, 0x94e2ea78, 0xa5fc3c53, 0x1e0a2df4,
			0xf2f74ea7, 0x361d2b3d, 0x1939260f, 0x19c27960,
			0x5223a708, 0xf71312b6, 0xebadfe6e, 0xeac31f66,
			0xe3bc4595, 0xa67bc883, 0xb17f37d1, 0x018cff28,
			0xc332ddef, 0xbe6c5aa5, 0x65582185, 0x68ab9802,
			0xeecea50f, 0xdb2f953b, 0x2aef7dad, 0x5b6e2f84,
			0x1521b628, 0x29076170, 0xecdd4775, 0x619f1510,
			0x13cca830, 0xeb61bd96, 0x0334fe1e, 0xaa0363cf,
			0xb5735c90, 0x4c70a239, 0xd59e9e0b, 0xcbaade14,
			0xeecc86bc, 0x60622ca7, 0x9cab5cab, 0xb2f3846e,
			0x648b1eaf, 0x19bdf0ca, 0xa02369b9, 0x655abb50,
			0x40685a32, 0x3c2ab4b3, 0x319ee9d5, 0xc021b8f7,
			0x9b540b19, 0x875fa099, 0x95f7997e, 0x623d7da8,
			0xf837889a, 0x97e32d77, 0x11ed935f, 0x16681281,
			0x0e358829, 0xc7e61fd6, 0x96dedfa1, 0x7858ba99,
			0x57f584a5, 0x1b227263, 0x9b83c3ff, 0x1ac24696,
			0xcdb30aeb, 0x532e3054, 0x8fd948e4, 0x6dbc3128,
			0x58ebf2ef, 0x34c6ffea, 0xfe28ed61, 0xee7c3c73,
			0x5d4a14d9, 0xe864b7e3, 0x42105d14, 0x203e13e0,
			0x45eee2b6, 0xa3aaabea, 0xdb6c4f15, 0xfacb4fd0,
			0xc742f442, 0xef6abbb5, 0x654f3b1d, 0x41cd2105,
			0xd81e799e, 0x86854dc7, 0xe44b476a, 0x3d816250,
			0xcf62a1f2, 0x5b8d2646, 0xfc8883a0, 0xc1c7b6a3,
			0x7f1524c3, 0x69cb7492, 0x47848a0b, 0x5692b285,
			0x095bbf00, 0xad19489d, 0x1462b174, 0x23820e00,
			0x58428d2a, 0x0c55f5ea, 0x1dadf43e, 0x233f7061,
			0x3372f092, 0x8d937e41, 0xd65fecf1, 0x6c223bdb,
			0x7cde3759, 0xcbee7460, 0x4085f2a7, 0xce77326e,
			0xa6078084, 0x19f8509e, 0xe8efd855, 0x61d99735,
			0xa969a7aa, 0xc50c06c2, 0x5a04abfc, 0x800bcadc,
			0x9e447a2e, 0xc3453484, 0xfdd56705, 0x0e1e9ec9,
			0xdb73dbd3, 0x105588cd, 0x675fda79, 0xe3674340,
			0xc5c43465, 0x713e38d8, 0x3d28f89e, 0xf16dff20,
			0x153e21e7, 0x8fb03d4a, 0xe6e39f2b, 0xdb83adf7},
		{
			0xe93d5a68, 0x948140f7, 0xf64c261c, 0x94692934,
			0x411520f7, 0x7602d4f7, 0xbcf46b2e, 0xd4a20068,
			0xd4082471, 0x3320f46a, 0x43b7d4b7, 0x500061af,
			0x1e39f62e, 0x97244546, 0x14214f74, 0xbf8b8840,
			0x4d95fc1d, 0x96b591af, 0x70f4ddd3, 0x66a02f45,
			0xbfbc09ec, 0x03bd9785, 0x7fac6dd0, 0x31cb8504,
			0x96eb27b3, 0x55fd3941, 0xda2547e6, 0xabca0a9a,
			0x28507825, 0x530429f4, 0x0a2c86da, 0xe9b66dfb,
			0x68dc1462, 0xd7486900, 0x680ec0a4, 0x27a18dee,
			0x4f3ffea2, 0xe887ad8c, 0xb58ce006, 0x7af4d6b6,
			0xaace1e7c, 0xd3375fec, 0xce78a399, 0x406b2a42,
			0x20fe9e35, 0xd9f385b9, 0xee39d7ab, 0x3b124e8b,
			0x1dc9faf7, 0x4b6d1856, 0x26a36631, 0xeae397b2,
			0x3a6efa74, 0xdd5b4332, 0x6841e7f7, 0xca7820fb,
			0xfb0af54e, 0xd8feb397, 0x454056ac, 0xba489527,
			0x55533a3a, 0x20838d87, 0xfe6ba9b7, 0xd096954b,
			0x55a867bc, 0xa1159a58, 0xcca92963, 0x99e1db33,
			0xa62a4a56, 0x3f3125f9, 0x5ef47e1c, 0x9029317c,
			0xfdf8e802, 0x04272f70, 0x80bb155c, 0x05282ce3,
			0x95c11548, 0xe4c66d22, 0x48c1133f, 0xc70f86dc,
			0x07f9c9ee, 0x41041f0f, 0x404779a4, 0x5d886e17,
			0x325f51eb, 0xd59bc0d1, 0xf2bcc18f, 0x41113564,
			0x257b7834, 0x602a9c60, 0xdff8e8a3, 0x1f636c1b,
			0x0e12b4c2, 0x02e1329e, 0xaf664fd1, 0xcad18115,
			0x6b2395e0, 0x333e92e1, 0x3b240b62, 0xeebeb922,
			0x85b2a20e, 0xe6ba0d99, 0xde720c8c, 0x2da2f728,
			0xd0127845, 0x95b794fd, 0x647d0862, 0xe7ccf5f0,
			0x5449a36f, 0x877d48fa, 0xc39dfd27, 0xf33e8d1e,
			0x0a476341, 0x992eff74, 0x3a6f6eab, 0xf4f8fd37,
			0xa812dc60, 0xa1ebddf8, 0x991be14c, 0xdb6e6b0d,
			0xc67b5510, 0x6d672c37, 0x2765d43b, 0xdcd0e804,
			0xf1290dc7, 0xcc00ffa3, 0xb5390f92, 0x690fed0b,
			0x667b9ffb, 0xcedb7d9c, 0xa091cf0b, 0xd9155ea3,
			0xbb132f88, 0x515bad24, 0x7b9479bf, 0x763bd6eb,
			0x37392eb3, 0xcc115979, 0x8026e297, 0xf42e312d,
			0x6842ada7, 0xc66a2b3b, 0x12754ccc, 0x782ef11c,
			0x6a124237, 0xb79251e7, 0x06a1bbe6, 0x4bfb6350,
			0x1a6b1018, 0x11caedfa, 0x3d25bdd8, 0xe2e1c3c9,
			0x44421659, 0x0a121386, 0xd90cec6e, 0xd5abea2a,
			0x64af674e, 0xda86a85f, 0xbebfe988, 0x64e4c3fe,
			0x9dbc8057, 0xf0f7c086, 0x60787bf8, 0x6003604d,
			0xd1fd8346, 0xf6381fb0, 0x7745ae04, 0xd736fccc,
			0x83426b33, 0xf01eab71, 0xb0804187, 0x3c005e5f,
			0x77a057be, 0xbde8ae24, 0x55464299, 0xbf582e61,
			0x4e58f48f, 0xf2ddfda2, 0xf474ef38, 0x8789bdc2,
			0x5366f9c3, 0xc8b38e74, 0xb475f255, 0x46fcd9b9,
			0x7aeb2661, 0x8b1ddf84, 0x846a0e79, 0x915f95e2,
			0x466e598e, 0x20b45770, 0x8cd55591, 0xc902de4c,
			0xb90bace1, 0xbb8205d0, 0x11a86248, 0x7574a99e,
			0xb77f19b6, 0xe0a9dc09, 0x662d09a1, 0xc4324633,
			0xe85a1f02, 0x09f0be8c, 0x4a99a025, 0x1d6efe10,
			0x1ab93d1d, 0x0ba5a4df, 0xa186f20f, 0x2868f169,
			0xdcb7da83, 0x573906fe, 0xa1e2ce9b, 0x4fcd7f52,
			0x50115e01, 0xa70683fa, 0xa002b5c4, 0x0de6d027,
			0x9af88c27, 0x773f8641, 0xc3604c06, 0x61a806b5,
			0xf0177a28, 0xc0f586e0, 0x006058aa, 0x30dc7d62,
			0x11e69ed7, 0x2338ea63, 0x53c2dd94, 0xc2c21634,
			0xbbcbee56, 0x90bcb6de, 0xebfc7da1, 0xce591d76,
			0x6f05e409, 0x4b7c0188, 0x39720a3d, 0x7c927c24,
			0x86e3725f, 0x724d9db9, 0x1ac15bb4, 0xd39eb8fc,
			0xed545578, 0x08fca5b5, 0xd83d7cd3, 0x4dad0fc4,
			0x1e50ef5e, 0xb161e6f8, 0xa28514d9, 0x6c51133c,
			0x6fd5c7e7, 0x56e14ec4, 0x362abfce, 0xddc6c837,
			0xd79a3234, 0x92638212, 0x670efa8e, 0x406000e0},
		{
			0x3a39ce37, 0xd3faf5cf, 0xabc27737, 0x5ac52d1b,
			0x5cb0679e, 0x4fa33742, 0xd3822740, 0x99bc9bbe,
			0xd5118e9d, 0xbf0f7315, 0xd62d1c7e, 0xc700c47b,
			0xb78c1b6b, 0x21a19045, 0xb26eb1be, 0x6a366eb4,
			0x5748ab2f, 0xbc946e79, 0xc6a376d2, 0x6549c2c8,
			0x530ff8ee, 0x468dde7d, 0xd5730a1d, 0x4cd04dc6,
			0x2939bbdb, 0xa9ba4650, 0xac9526e8, 0xbe5ee304,
			0xa1fad5f0, 0x6a2d519a, 0x63ef8ce2, 0x9a86ee22,
			0xc089c2b8, 0x43242ef6, 0xa51e03aa, 0x9cf2d0a4,
			0x83c061ba, 0x9be96a4d, 0x8fe51550, 0xba645bd6,
			0x2826a2f9, 0xa73a3ae1, 0x4ba99586, 0xef5562e9,
			0xc72fefd3, 0xf752f7da, 0x3f046f69, 0x77fa0a59,
			0x80e4a915, 0x87b08601, 0x9b09e6ad, 0x3b3ee593,
			0xe990fd5a, 0x9e34d797, 0x2cf0b7d9, 0x022b8b51,
			0x96d5ac3a, 0x017da67d, 0xd1cf3ed6, 0x7c7d2d28,
			0x1f9f25cf, 0xadf2b89b, 0x5ad6b472, 0x5a88f54c,
			0xe029ac71, 0xe019a5e6, 0x47b0acfd, 0xed93fa9b,
			0xe8d3c48d, 0x283b57cc, 0xf8d56629, 0x79132e28,
			0x785f0191, 0xed756055, 0xf7960e44, 0xe3d35e8c,
			0x15056dd4, 0x88f46dba, 0x03a16125, 0x0564f0bd,
			0xc3eb9e15, 0x3c9057a2, 0x97271aec, 0xa93a072a,
			0x1b3f6d9b, 0x1e6321f5, 0xf59c66fb, 0x26dcf319,
			0x7533d928, 0xb155fdf5, 0x03563482, 0x8aba3cbb,
			0x28517711, 0xc20ad9f8, 0xabcc5167, 0xccad925f,
			0x4de81751, 0x3830dc8e, 0x379d5862, 0x9320f991,
			0xea7a90c2, 0xfb3e7bce, 0x5121ce64, 0x774fbe32,
			0xa8b6e37e, 0xc3293d46, 0x48de5369, 0x6413e680,
			0xa2ae0810, 0xdd6db224, 0x69852dfd, 0x09072166,
			0xb39a460a, 0x6445c0dd, 0x586cdecf, 0x1c20c8ae,
			0x5bbef7dd, 0x1b588d40, 0xccd2017f, 0x6bb4e3bb,
			0xdda26a7e, 0x3a59ff45, 0x3e350a44, 0xbcb4cdd5,
			0x72eacea8, 0xfa6484bb, 0x8d6612ae, 0xbf3c6f47,
			0xd29be463, 0x542f5d9e, 0xaec2771b, 0xf64e6370,
			0x740e0d8d, 0xe75b1357, 0xf8721671, 0xaf537d5d,
			0x4040cb08, 0x4eb4e2cc, 0x34d2466a, 0x0115af84,
			0xe1b00428, 0x95983a1d, 0x06b89fb4, 0xce6ea048,
			0x6f3f3b82, 0x3520ab82, 0x011a1d4b, 0x277227f8,
			0x611560b1, 0xe7933fdc, 0xbb3a792b, 0x344525bd,
			0xa08839e1, 0x51ce794b, 0x2f32c9b7, 0xa01fbac9,
			0xe01cc87e, 0xbcc7d1f6, 0xcf0111c3, 0xa1e8aac7,
			0x1a908749, 0xd44fbd9a, 0xd0dadecb, 0xd50ada38,
			0x0339c32a, 0xc6913667, 0x8df9317c, 0xe0b12b4f,
			0xf79e59b7, 0x43f5bb3a, 0xf2d519ff, 0x27d9459c,
			0xbf97222c, 0x15e6fc2a, 0x0f91fc71, 0x9b941525,
			0xfae59361, 0xceb69ceb, 0xc2a86459, 0x12baa8d1,
			0xb6c1075e, 0xe3056a0c, 0x10d25065, 0xcb03a442,
			0xe0ec6e0e, 0x1698db3b, 0x4c98a0be, 0x3278e964,
			0x9f1f9532, 0xe0d392df, 0xd3a0342b, 0x8971f21e,
			0x1b0a7441, 0x4ba3348c, 0xc5be7120, 0xc37632d8,
			0xdf359f8d, 0x9b992f2e, 0xe60b6f47, 0x0fe3f11d,
			0xe54cda54, 0x1edad891, 0xce6279cf, 0xcd3e7e6f,
			0x1618b166, 0xfd2c1d05, 0x848fd2c5, 0xf6fb2299,
			0xf523f357, 0xa6327623, 0x93a83531, 0x56cccd02,
			0xacf08162, 0x5a75ebb5, 0x6e163697, 0x88d273cc,
			0xde966292, 0x81b949d0, 0x4c50901b, 0x71c65614,
			0xe6c6c7bd, 0x327a140a, 0x45e1d006, 0xc3f27b9a,
			0xc9aa53fd, 0x62a80f00, 0xbb25bfe2, 0x35bdd2f6,
			0x71126905, 0xb2040222, 0xb6cbcf7c, 0xcd769c2b,
			0x53113ec0, 0x1640e3d3, 0x38abbd60, 0x2547adf0,
			0xba38209c, 0xf746ce76, 0x77afa1c5, 0x20756060,
			0x85cbfe4e, 0x8ae88dd8, 0x7aaaf9b0, 0x4cf9aa7e,
			0x1948c25c, 0x02fb8a8c, 0x01c36ae4, 0xd6ebe1f9,
			0x90d4f869, 0xa65cdea0, 0x3f09252d, 0xc208e69f,
			0xb74e6132, 0xce77e25b, 0x578fdfe3, 0x3ac372e6}
	},
	{
		0x243f6a88, 0x85a308d3, 0x13198a2e, 0x03707344,
		0xa4093822, 0x299f31d0, 0x082efa98, 0xec4e6c89,
		0x452821e6, 0x38d01377, 0xbe5466cf, 0x34e90c6c,
		0xc0ac29b7, 0xc97c50dd, 0x3f84d5b5, 0xb5470917,
		0x9216d5d9, 0x8979fb1b
	} };

	*c = bf_initstate;

}

uint32_t
Blowfish_stream2word(const uint8_t *data, uint16_t databytes,
    uint16_t *current)
{
	uint8_t i;
	uint16_t j;
	uint32_t temp;

	temp = 0x00000000;
	j = *current;

	for (i = 0; i < 4; i++, j++) {
		if (j >= databytes)
			j = 0;
		temp = (temp << 8) | data[j];
	}

	*current = j;
	return temp;
}

void
Blowfish_expand0state(blf_ctx *c, const uint8_t *key, uint16_t keybytes)
{
	uint16_t i;
	uint16_t j;
	uint16_t k;
	uint32_t temp;
	uint32_t datal;
	uint32_t datar;

	j = 0;
	for (i = 0; i < BLF_N + 2; i++) {
		/* Extract 4 int8 to 1 int32 from keystream */
		temp = Blowfish_stream2word(key, keybytes, &j);
		c->P[i] = c->P[i] ^ temp;
	}

	j = 0;
	datal = 0x00000000;
	datar = 0x00000000;
	for (i = 0; i < BLF_N + 2; i += 2) {
		Blowfish_encipher(c, &datal, &datar);

		c->P[i] = datal;
		c->P[i + 1] = datar;
	}

	for (i = 0; i < 4; i++) {
		for (k = 0; k < 256; k += 2) {
			Blowfish_encipher(c, &datal, &datar);

			c->S[i][k] = datal;
			c->S[i][k + 1] = datar;
		}
	}
}

void
Blowfish_expandstate(blf_ctx *c, const uint8_t *data, uint16_t databytes,
    const uint8_t *key, uint16_t keybytes)
{
	uint16_t i;
	uint16_t j;
	uint16_t k;
	uint32_t temp;
	uint32_t datal;
	uint32_t datar;

	j = 0;
	for (i = 0; i < BLF_N + 2; i++) {
		/* Extract 4 int8 to 1 int32 from keystream */
		temp = Blowfish_stream2word(key, keybytes, &j);
		c->P[i] = c->P[i] ^ temp;
	}

	j = 0;
	datal = 0x00000000;
	datar = 0x00000000;
	for (i = 0; i < BLF_N + 2; i += 2) {
		datal ^= Blowfish_stream2word(data, databytes, &j);
		datar ^= Blowfish_stream2word(data, databytes, &j);
		Blowfish_encipher(c, &datal, &datar);

		c->P[i] = datal;
		c->P[i + 1] = datar;
	}

	for (i = 0; i < 4; i++) {
		for (k = 0; k < 256; k += 2) {
			datal ^= Blowfish_stream2word(data, databytes, &j);
			datar ^= Blowfish_stream2word(data, databytes, &j);
			Blowfish_encipher(c, &datal, &datar);

			c->S[i][k] = datal;
			c->S[i][k + 1] = datar;
		}
	}

}

void
blf_enc(blf_ctx *c, uint32_t *data, uint16_t blocks)
{
	uint32_t *d;
	uint16_t i;

	d = data;
	for (i = 0; i < blocks; i++) {
		Blowfish_encipher(c, d, d + 1);
		d += 2;
	}
}

/* This password hashing algorithm was designed by David Mazieres
 * <dm@lcs.mit.edu> and works as follows:
 *
 * 1. state := InitState ()
 * 2. state := ExpandKey (state, salt, password) 3.
 * REPEAT rounds:
 *	state := ExpandKey (state, 0, salt)
 *      state := ExpandKey(state, 0, password)
 * 4. ctext := "OrpheanBeholderScryDoubt"
 * 5. REPEAT 64:
 * 	ctext := Encrypt_ECB (state, ctext);
 * 6. RETURN Concatenate (salt, ctext);
 *
 */

/*
 * FreeBSD implementation by Paul Herman
 */
/* This implementation is adaptable to current computing power.
 * You can have up to 2^31 rounds which should be enough for some
 * time to come.
 */

#define BCRYPT_VERSION '2'
#define BCRYPT_MAXSALT 16	/* Precomputation is just so nice */
#define BCRYPT_BLOCKS 6		/* Ciphertext blocks */
#define BCRYPT_MINROUNDS 16	/* we have log2(rounds) in salt */

static void encode_base64(uint8_t *, uint8_t *, uint16_t);
static void decode_base64(uint8_t *, uint16_t, const uint8_t *);

static char    encrypted[512]; /* Shouldn't grow more than this */
static char    error[] = ":";

static const uint8_t Base64Code[] =
"./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

static const uint8_t index_64[128] =
{
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 0, 1, 54, 55,
	56, 57, 58, 59, 60, 61, 62, 63, 255, 255,
	255, 255, 255, 255, 255, 2, 3, 4, 5, 6,
	7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
	255, 255, 255, 255, 255, 255, 28, 29, 30,
	31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
	51, 52, 53, 255, 255, 255, 255, 255
};
#define CHAR64(c)  ( (c) > 127 ? 255 : index_64[(c)])

static void
decode_base64(uint8_t *buffer, uint16_t len, const uint8_t *data)
{
	uint8_t *bp = buffer;
	const uint8_t *p = data;
	uint8_t c1, c2, c3, c4;
	while (bp < buffer + len) {
		c1 = CHAR64(*p);
		c2 = CHAR64(*(p + 1));

		/* Invalid data */
		if (c1 == 255 || c2 == 255)
			break;

		*bp++ = (uint8_t)((c1 << 2) | ((c2 & 0x30) >> 4));
		if (bp >= buffer + len)
			break;

		c3 = CHAR64(*(p + 2));
		if (c3 == 255)
			break;

		*bp++ = ((c2 & 0x0f) << 4) | ((c3 & 0x3c) >> 2);
		if (bp >= buffer + len)
			break;

		c4 = CHAR64(*(p + 3));
		if (c4 == 255)
			break;
		*bp++ = ((c3 & 0x03) << 6) | c4;

		p += 4;
	}
}

/* We handle $Vers$log2(NumRounds)$salt+passwd$
   i.e. $2$04$iwouldntknowwhattosayetKdJ6iFtacBqJdKe6aW7ou */

char *
rb_blowfish_crypt(const char *key, const char *salt)
{
	blf_ctx state;
	uint32_t rounds, i, k;
	uint16_t j;
	uint8_t key_len, salt_len, logr, minr;
	uint8_t ciphertext[4 * BCRYPT_BLOCKS] = "OrpheanBeholderScryDoubt";
	uint8_t csalt[BCRYPT_MAXSALT];
	uint32_t cdata[BCRYPT_BLOCKS];
	static const char *magic = "$2a$04$";

		/* Defaults */
	minr = 'a';
	logr = 4;
	rounds = 1 << logr;

        /* If it starts with the magic string, then skip that */
	if(!strncmp(salt, magic, strlen(magic))) {
		salt += strlen(magic);
	}
	else if (*salt == '$') {

		/* Discard "$" identifier */
		salt++;

		if (*salt > BCRYPT_VERSION) {
			/* How do I handle errors ? Return ':' */
			return error;
		}

		/* Check for minor versions */
		if (salt[1] != '$') {
			 switch (salt[1]) {
			 case 'a':
				 /* 'ab' should not yield the same as 'abab' */
				 minr = (uint8_t)salt[1];
				 salt++;
				 break;
			 default:
				 return error;
			 }
		} else
			 minr = 0;

		/* Discard version + "$" identifier */
		salt += 2;

		if (salt[2] != '$')
			/* Out of sync with passwd entry */
			return error;

		/* Computer power doesnt increase linear, 2^x should be fine */
		logr = (uint8_t)atoi(salt);
		rounds = 1 << logr;
		if (rounds < BCRYPT_MINROUNDS)
			return error;

		/* Discard num rounds + "$" identifier */
		salt += 3;
	}


	/* We dont want the base64 salt but the raw data */
	decode_base64(csalt, BCRYPT_MAXSALT, (const uint8_t *)salt);
	salt_len = BCRYPT_MAXSALT;
	key_len = (uint8_t)(strlen(key) + (minr >= 'a' ? 1 : 0));

	/* Setting up S-Boxes and Subkeys */
	Blowfish_initstate(&state);
	Blowfish_expandstate(&state, csalt, salt_len,
	    (const uint8_t *) key, key_len);
	for (k = 0; k < rounds; k++) {
		Blowfish_expand0state(&state, (const uint8_t *) key, key_len);
		Blowfish_expand0state(&state, csalt, salt_len);
	}

	/* This can be precomputed later */
	j = 0;
	for (i = 0; i < BCRYPT_BLOCKS; i++)
		cdata[i] = Blowfish_stream2word(ciphertext, 4 * BCRYPT_BLOCKS, &j);

	/* Now do the encryption */
	for (k = 0; k < 64; k++)
		blf_enc(&state, cdata, BCRYPT_BLOCKS / 2);

	for (i = 0; i < BCRYPT_BLOCKS; i++) {
		ciphertext[4 * i + 3] = cdata[i] & 0xff;
		cdata[i] = cdata[i] >> 8;
		ciphertext[4 * i + 2] = cdata[i] & 0xff;
		cdata[i] = cdata[i] >> 8;
		ciphertext[4 * i + 1] = cdata[i] & 0xff;
		cdata[i] = cdata[i] >> 8;
		ciphertext[4 * i + 0] = cdata[i] & 0xff;
	}


	i = 0;
	encrypted[i++] = '$';
	encrypted[i++] = BCRYPT_VERSION;
	if (minr)
		encrypted[i++] = (int8_t)minr;
	encrypted[i++] = '$';

	snprintf(encrypted + i, 4, "%2.2u$", logr);

	encode_base64((uint8_t *) encrypted + i + 3, csalt, BCRYPT_MAXSALT);
	encode_base64((uint8_t *) encrypted + strlen(encrypted), ciphertext,
	    4 * BCRYPT_BLOCKS - 1);
	return encrypted;
}

static void
encode_base64(uint8_t *buffer, uint8_t *data, uint16_t len)
{
	uint8_t *bp = buffer;
	uint8_t *p = data;
	uint8_t c1, c2;
	while (p < data + len) {
		c1 = *p++;
		*bp++ = Base64Code[(c1 >> 2)];
		c1 = (c1 & 0x03) << 4;
		if (p >= data + len) {
			*bp++ = Base64Code[c1];
			break;
		}
		c2 = *p++;
		c1 |= (c2 >> 4) & 0x0f;
		*bp++ = Base64Code[c1];
		c1 = (c2 & 0x0f) << 2;
		if (p >= data + len) {
			*bp++ = Base64Code[c1];
			break;
		}
		c2 = *p++;
		c1 |= (c2 >> 6) & 0x03;
		*bp++ = Base64Code[c1];
		*bp++ = Base64Code[c2 & 0x3f];
	}
	*bp = '\0';
}
