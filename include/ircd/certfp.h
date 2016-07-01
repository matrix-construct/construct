/*
 *  charybdis: A useful ircd.
 *  certpf.h: Fingerprint method strings
 *
 *  Copyright 2016 Simon Arlott
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 */

#ifndef INCLUDED_certfp_h
#define INCLUDED_certfp_h

#define CERTFP_NAME_CERT_SHA1		"sha1"
#define CERTFP_PREFIX_CERT_SHA1		""
#define CERTFP_NAME_CERT_SHA256		"sha256"
#define CERTFP_PREFIX_CERT_SHA256	""
#define CERTFP_NAME_CERT_SHA512		"sha512"
#define CERTFP_PREFIX_CERT_SHA512	""
/* These prefixes are copied from RFC 7218 */
#define CERTFP_NAME_SPKI_SHA256		"spki_sha256"
#define CERTFP_PREFIX_SPKI_SHA256	"SPKI:SHA2-256:"
#define CERTFP_NAME_SPKI_SHA512		"spki_sha512"
#define CERTFP_PREFIX_SPKI_SHA512	"SPKI:SHA2-512:"

#endif /* INCLUDED_certfp_h */
