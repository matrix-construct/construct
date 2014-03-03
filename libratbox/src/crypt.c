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
	for (cnt = 0; cnt < key_len; ++cnt)
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
