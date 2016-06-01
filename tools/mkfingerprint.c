/*
 *  mkfingerprint.c: Create certificate fingerprints using librb
 *  Copyright 2016 simon Arlott
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "rb_lib.h"
#include "certfp.h"

int main(int argc, char *argv[])
{
	uint8_t certfp[RB_SSL_CERTFP_LEN+1] = { 0 };
	const char *method_str;
	const char *filename;
	int method;
	const char *prefix;
	int ret;
	int i;

	if (argc != 3) {
		printf("mkfingerprint <method> <filename>\n");
		printf("  Valid methods: "
			CERTFP_NAME_CERT_SHA1 ", "
			CERTFP_NAME_CERT_SHA256 ", "
			CERTFP_NAME_CERT_SHA512 ", "
			CERTFP_NAME_SPKI_SHA256 ", "
			CERTFP_NAME_SPKI_SHA512 "\n");
		return 1;
	}

	method_str = argv[1];
	filename = argv[2];

	if (!strcmp(method_str, CERTFP_NAME_CERT_SHA1)) {
		method = RB_SSL_CERTFP_METH_CERT_SHA1;
		prefix = CERTFP_PREFIX_CERT_SHA1;
	} else if (!strcmp(method_str, CERTFP_NAME_CERT_SHA256)) {
		method = RB_SSL_CERTFP_METH_CERT_SHA256;
		prefix = CERTFP_PREFIX_CERT_SHA256;
	} else if (!strcmp(method_str, CERTFP_NAME_CERT_SHA512)) {
		method = RB_SSL_CERTFP_METH_CERT_SHA512;
		prefix = CERTFP_PREFIX_CERT_SHA512;
	} else if (!strcmp(method_str, CERTFP_NAME_SPKI_SHA256)) {
		method = RB_SSL_CERTFP_METH_SPKI_SHA256;
		prefix = CERTFP_PREFIX_SPKI_SHA256;
	} else if (!strcmp(method_str, CERTFP_NAME_SPKI_SHA512)) {
		method = RB_SSL_CERTFP_METH_SPKI_SHA512;
		prefix = CERTFP_PREFIX_SPKI_SHA512;
	} else {
		printf("Unknown method: %s\n", method_str);
		return 1;
	}

	ret = rb_get_ssl_certfp_file(filename, certfp, method);
	if (ret < 0) {
		perror(filename);
		return 1;
	} else if (ret == 0) {
		fprintf(stderr, "Unknown error\n");
		return 1;
	}

	printf("%s", prefix);
	for (i = 0; i < ret; i++) {
		printf("%02x", certfp[i]);
	}
	printf("\n");
	return 0;
}
