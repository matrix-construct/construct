/*
 * Based on the SHA-1 C implementation by Steve Reid <steve@edmweb.com>
 * 100% Public Domain
 *
 * Test Vectors (from FIPS PUB 180-1)
 * "abc"
 *   A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D
 * "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
 *   84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1
 * A million repetitions of "a"
 *   34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F
 */

#include "stdinc.h"

#ifdef _WIN32
	#include <winsock2.h> // for htonl()
#else
	#include <netinet/in.h> // for htonl()
#endif

#include "sha1.h"

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

// blk0() and blk() perform the initial expand. blk0() deals with host endianess
#define blk0(i) (block[i] = htonl(block[i]))
#define blk(i) (block[i&15] = rol(block[(i+13)&15]^block[(i+8)&15]^block[(i+2)&15]^block[i&15],1))

// (R0+R1), R2, R3, R4 are the different operations (rounds) used in SHA1
#define R0(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk0(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R1(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R2(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5);w=rol(w,30);
#define R3(v,w,x,y,z,i) z+=(((w|x)&y)|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5);w=rol(w,30);
#define R4(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5);w=rol(w,30);

// hash a single 512-bit block. this is the core of the algorithm
static uint32_t sha1_transform(SHA1 *sha1, const uint8_t buffer[SHA1_BLOCK_LENGTH]) {
	uint32_t a, b, c, d, e;
	uint32_t block[SHA1_BLOCK_LENGTH / 4];

	memcpy(&block, buffer, SHA1_BLOCK_LENGTH);

	// copy sha1->state[] to working variables
	a = sha1->state[0];
	b = sha1->state[1];
	c = sha1->state[2];
	d = sha1->state[3];
	e = sha1->state[4];

	// 4 rounds of 20 operations each (loop unrolled)
	R0(a,b,c,d,e, 0); R0(e,a,b,c,d, 1); R0(d,e,a,b,c, 2); R0(c,d,e,a,b, 3);
	R0(b,c,d,e,a, 4); R0(a,b,c,d,e, 5); R0(e,a,b,c,d, 6); R0(d,e,a,b,c, 7);
	R0(c,d,e,a,b, 8); R0(b,c,d,e,a, 9); R0(a,b,c,d,e,10); R0(e,a,b,c,d,11);
	R0(d,e,a,b,c,12); R0(c,d,e,a,b,13); R0(b,c,d,e,a,14); R0(a,b,c,d,e,15);
	R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);

	R2(a,b,c,d,e,20); R2(e,a,b,c,d,21); R2(d,e,a,b,c,22); R2(c,d,e,a,b,23);
	R2(b,c,d,e,a,24); R2(a,b,c,d,e,25); R2(e,a,b,c,d,26); R2(d,e,a,b,c,27);
	R2(c,d,e,a,b,28); R2(b,c,d,e,a,29); R2(a,b,c,d,e,30); R2(e,a,b,c,d,31);
	R2(d,e,a,b,c,32); R2(c,d,e,a,b,33); R2(b,c,d,e,a,34); R2(a,b,c,d,e,35);
	R2(e,a,b,c,d,36); R2(d,e,a,b,c,37); R2(c,d,e,a,b,38); R2(b,c,d,e,a,39);

	R3(a,b,c,d,e,40); R3(e,a,b,c,d,41); R3(d,e,a,b,c,42); R3(c,d,e,a,b,43);
	R3(b,c,d,e,a,44); R3(a,b,c,d,e,45); R3(e,a,b,c,d,46); R3(d,e,a,b,c,47);
	R3(c,d,e,a,b,48); R3(b,c,d,e,a,49); R3(a,b,c,d,e,50); R3(e,a,b,c,d,51);
	R3(d,e,a,b,c,52); R3(c,d,e,a,b,53); R3(b,c,d,e,a,54); R3(a,b,c,d,e,55);
	R3(e,a,b,c,d,56); R3(d,e,a,b,c,57); R3(c,d,e,a,b,58); R3(b,c,d,e,a,59);

	R4(a,b,c,d,e,60); R4(e,a,b,c,d,61); R4(d,e,a,b,c,62); R4(c,d,e,a,b,63);
	R4(b,c,d,e,a,64); R4(a,b,c,d,e,65); R4(e,a,b,c,d,66); R4(d,e,a,b,c,67);
	R4(c,d,e,a,b,68); R4(b,c,d,e,a,69); R4(a,b,c,d,e,70); R4(e,a,b,c,d,71);
	R4(d,e,a,b,c,72); R4(c,d,e,a,b,73); R4(b,c,d,e,a,74); R4(a,b,c,d,e,75);
	R4(e,a,b,c,d,76); R4(d,e,a,b,c,77); R4(c,d,e,a,b,78); R4(b,c,d,e,a,79);

	// add the working variables back into sha1->state[]
	sha1->state[0] += a;
	sha1->state[1] += b;
	sha1->state[2] += c;
	sha1->state[3] += d;
	sha1->state[4] += e;

	// wipe variables
	a = b = c = d = e = 0;

	return a; // return a to avoid dead-store warning from clang static analyzer
}

void sha1_init(SHA1 *sha1) {
	sha1->state[0] = 0x67452301;
	sha1->state[1] = 0xEFCDAB89;
	sha1->state[2] = 0x98BADCFE;
	sha1->state[3] = 0x10325476;
	sha1->state[4] = 0xC3D2E1F0;
	sha1->count = 0;
}

void sha1_update(SHA1 *sha1, const uint8_t *data, size_t length) {
	size_t i, j;

	j = (size_t)((sha1->count >> 3) & 63);
	sha1->count += (length << 3);

	if ((j + length) > 63) {
		i = 64 - j;

		memcpy(&sha1->buffer[j], data, i);
		sha1_transform(sha1, sha1->buffer);

		for (; i + 63 < length; i += 64) {
			sha1_transform(sha1, &data[i]);
		}

		j = 0;
	} else {
		i = 0;
	}

	memcpy(&sha1->buffer[j], &data[i], length - i);
}

void sha1_final(SHA1 *sha1, uint8_t digest[SHA1_DIGEST_LENGTH]) {
	uint32_t i;
	uint8_t count[8];

	for (i = 0; i < 8; i++) {
		// this is endian independent
		count[i] = (uint8_t)((sha1->count >> ((7 - (i & 7)) * 8)) & 255);
	}

	sha1_update(sha1, (uint8_t *)"\200", 1);

	while ((sha1->count & 504) != 448) {
		sha1_update(sha1, (uint8_t *)"\0", 1);
	}

	sha1_update(sha1, count, 8);

	for (i = 0; i < SHA1_DIGEST_LENGTH; i++) {
		digest[i] = (uint8_t)((sha1->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
	}

	memset(sha1, 0, sizeof(*sha1));
}
