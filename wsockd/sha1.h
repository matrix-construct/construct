/*
 * Based on the SHA-1 C implementation by Steve Reid <steve@edmweb.com>
 * 100% Public Domain
 */

#ifndef SHA1_H
#define SHA1_H

#include <stddef.h>
#include <stdint.h>

#define SHA1_BLOCK_LENGTH 64
#define SHA1_DIGEST_LENGTH 20

typedef struct {
	uint32_t state[5];
	uint64_t count;
	uint8_t buffer[SHA1_BLOCK_LENGTH];
} SHA1;

void sha1_init(SHA1 *sha1);
void sha1_update(SHA1 *sha1, const uint8_t *data, size_t length);
void sha1_final(SHA1 *sha1, uint8_t digest[SHA1_DIGEST_LENGTH]);

#endif // SHA1_H
