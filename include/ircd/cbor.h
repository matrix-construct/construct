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
	IRCD_EXCEPTION(error, type_error);
	IRCD_EXCEPTION(error, parse_error);
	IRCD_EXCEPTION(parse_error, buffer_underrun);

	union primitive;
	union positive;
	union negative;
	struct binary;
	struct string;
	struct array;
	struct object;
	struct tag;

	struct item;
}

struct ircd::cbor::item
:const_buffer
{
	enum major :uint8_t;
	enum minor :uint8_t;
	struct header;
	struct value;

	item(const const_buffer &buf);
	item() = default;

	friend string_view reflect(const major &);
	friend enum major major(const item &);
};

struct ircd::cbor::item::header
:const_buffer
{
	static uint8_t major(const uint8_t &);
	static uint8_t minor(const uint8_t &);
	static size_t length(const uint8_t &);

	const uint8_t &leading() const;
	const_buffer following() const;
	size_t header_size() const;
	size_t value_count() const;

	header(const const_buffer &);
	header() = default;
};

struct ircd::cbor::string
:string_view
{
	using string_view::string_view;
	string(const item &);
};

union ircd::cbor::positive
{
	uint8_t u8;
	uint16_t u16;
	uint32_t u32;
	uint64_t u64;
};

/*
union ircd::cbor::primitive
{
	bool ud;
	bool nul;
	bool boolean;
	float fsingle;
	double fdouble;
};

union ircd::cbor::negative
{
	int8_t u8;
	int16_t u16;
	int32_t u32;
	int64_t u64;
};

struct ircd::cbor::binary
:const_buffer
{
	using const_buffer::const_buffer;
	binary(const item &);
};

struct ircd::cbor::array
:const_buffer
{
	using const_buffer::const_buffer;
	array(const item &);
};

struct ircd::cbor::object
:const_buffer
{
	using member = std::pair<item, item>;

	using const_buffer::const_buffer;
	object(const item &);
};

struct ircd::cbor::tag
:const_buffer
{
	using const_buffer::const_buffer;
	tag(const item &);
};

struct ircd::cbor::value
{
	item::header header; union
	{
		cbor::primitive primitive;
		cbor::positive positive;
		cbor::negative negative;
		cbor::binary binary;
		cbor::string string;
		cbor::value *array;
		cbor::object::member *object;
	};
};
*/

enum ircd::cbor::item::major
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

enum ircd::cbor::item::minor
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
