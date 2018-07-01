// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
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

	struct item;

	struct positive;
	struct negative;
	struct binary;
	struct string;
	struct array;
	struct object;
	struct tag;
	struct primitive;
}

struct ircd::cbor::item
{
	enum major :uint8_t;
	enum minor :uint8_t;
	struct header;
};

struct ircd::cbor::item::header
{
	uint8_t major :3;
	uint8_t minor :5;
};

enum ircd::cbor::item::major
:uint8_t
{
	POSITIVE   = 0,
	NEGATIVE   = 1,
	BINARY     = 2,
	STRING     = 3,
	ARRAY      = 4,
	OBJECT     = 5,
	TAG        = 6,
	PRIMITIVE  = 7,
};

enum ircd::cbor::item::minor
:uint8_t
{
	FALSE     = 20,  ///< False
	TRUE      = 21,  ///< True
	NUL       = 22,  ///< Null
	UD        = 23,  ///< Undefined value
	U8        = 24,  ///< 8 bits follow
	U16       = 25,  ///< 16 bits follow
	F16       = 25,  ///< IEEE754 half-precision (16 bits follow)
	U32       = 26,  ///< 32 bits follow
	F32       = 26,  ///< IEEE754 single-precision (32 bits follow)
	U64       = 27,  ///< 64 bits follow
	F64       = 27,  ///< IEEE754 double-precision (64 bits follow)
	STREAM    = 31,  ///< Variable length (terminated by BREAK)
	BREAK     = 31,  ///< Terminator code
};
