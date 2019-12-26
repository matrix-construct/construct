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
	struct head;

	string_view reflect(const enum major &);
	enum major major(const const_buffer &);
}

/// Item head.
///
/// This object represents the head byte and any following-integer bytes under
/// its const_buffer. If the major type has a payload, it starts immediately
/// following the end of this object's buffer. The first byte of this object's
/// buffer is the leading head byte. This object's buffer will never be empty()
/// unless it's default-initialized (i.e not pointing at anything).
///
/// This is used to query information about the item from the head data.
///
struct ircd::cbor::head
:const_buffer
{
	static uint8_t major(const uint8_t &);       // Major type
	static uint8_t minor(const uint8_t &);       // Minor type
	static size_t length(const uint8_t &);       // (1 + size(following()))

	// Member ops when buffer has >= 1 byte
	const uint8_t &leading() const;              // Reference the leading byte
	enum major major() const;                    // major(leading())
	enum minor minor() const;                    // minor(leading())
	size_t length() const;                       // length(leading())

	// Get bytes following leading byte based on major/minor
	const_buffer following() const;
	template<class T> const T &following() const;

	// Construct from at least first byte of item or more
	head(const const_buffer &);
	head() = default;
	head(head &&) = default;
	head(const head &) = default;
};

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
