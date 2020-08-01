// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_CBOR_H

/// Concise Binary Object Representation (RFC7049)
///
namespace ircd::cbor
{
	IRCD_EXCEPTION(ircd::error, error);
	IRCD_EXCEPTION(error, type_error);
	IRCD_EXCEPTION(error, parse_error);
	IRCD_EXCEPTION(parse_error, buffer_underrun);

	enum major :uint8_t;
	enum minor :uint8_t;

	string_view reflect(const enum major &);

}

/// RFC7049 Major type codes
enum ircd::cbor::major
:uint8_t
{
	POSITIVE   = 0,  ///< Z*
	NEGATIVE   = 1,  ///< Z-
	BINARY     = 2,  ///< Raw byte sequence
	STRING     = 3,  ///< UTF-8 character sequence
	ARRAY      = 4,  ///< Array of items
	OBJECT     = 5,  ///< Dictionary of items
	TAG        = 6,  ///< CBOR extensions (IANA registered)
	PRIMITIVE  = 7,  ///< Literals / floats
};

/// RFC7049 Minor type codes
enum ircd::cbor::minor
:uint8_t
{
	FALSE      = 20,  ///< False
	TRUE       = 21,  ///< True
	NUL        = 22,  ///< Null
	UD         = 23,  ///< Undefined value
	U8         = 24,  ///< 8 bits follow
	U16        = 25,  ///< 16 bits follow
	F16        = 25,  ///< IEEE754 half-precision (16 bits follow)
	U32        = 26,  ///< 32 bits follow
	F32        = 26,  ///< IEEE754 single-precision (32 bits follow)
	U64        = 27,  ///< 64 bits follow
	F64        = 27,  ///< IEEE754 double-precision (64 bits follow)
	STREAM     = 31,  ///< Variable length (terminated by BREAK)
	BREAK      = 31,  ///< Terminator code
};
