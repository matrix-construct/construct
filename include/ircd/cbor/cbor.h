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

	struct positive;
	struct negative;
	struct binary;
	struct string;
	struct array;
	struct object;
	struct tag;
	struct primitive;

	enum major :uint8_t;
	enum minor :uint8_t;
	struct head;
	struct item;

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

/// Abstract item
///
struct ircd::cbor::item
:const_buffer
{
	operator head() const;
	const_buffer value() const;

	item(const const_buffer &buf);
	item() = default;
};

/// Positive integer item.
///
/// Represents a positive integer item. This is a buffer spanning the item,
/// which is same span as the cbor::head buffer because positive integer items
/// only have a payload of an integer item within or following the leading head
/// byte.
///
/// The integer value is returned through the cast operator.
///
struct ircd::cbor::positive
:item
{
	uint64_t value() const;
	operator uint64_t() const;

	positive(const head &);
	positive() = default;
};

/// Negative integer item.
///
/// This is similar to cbor::positive except that the integer returned is signed
/// and computed from 1 - cbor::positive(buf).
///
struct ircd::cbor::negative
:positive
{
	int64_t value() const;
	operator int64_t() const;

	negative(const head &);
	negative() = default;
};

/// Binary buffer item
///
///
struct ircd::cbor::binary
:positive
{
	const_buffer value() const;

	binary(const const_buffer &);
};

struct ircd::cbor::string
:binary
{
	string_view value() const;
	explicit operator string_view() const;

	string(const const_buffer &);
};

struct ircd::cbor::array
:item
{
	struct const_iterator;

	size_t count() const;
	const_buffer value() const;

	const_iterator begin() const;
	const_iterator end() const;

	item operator[](size_t i) const;

	array(const const_buffer &);
};

struct ircd::cbor::array::const_iterator
{
	cbor::array array;
	const_buffer state;

  public:
	bool operator!() const
	{
		return ircd::empty(state);
	}

	explicit operator bool() const
	{
		return !operator!();
	}

	const const_buffer &operator*() const
	{
		return state;
	}

	const const_buffer *operator->() const
	{
		return &state;
	}

	const_iterator &operator++();

	const_iterator(const cbor::array &);
};

struct ircd::cbor::object
:item
{
	struct member;
	struct const_iterator;

	size_t count() const;
	const_buffer value() const;
	member operator[](const string_view &name) const;

	object(const const_buffer &);
};

struct ircd::cbor::object::member
:std::pair<item, item>
{
	using std::pair<item, item>::pair;
};

struct ircd::cbor::object::const_iterator
{
	cbor::object object;
	member state;

  public:
	bool operator!() const
	{
		return ircd::empty(state.first);
	}

	explicit operator bool() const
	{
		return !operator!();
	}

	const member &operator*() const
	{
		return state;
	}

	const member *operator->() const
	{
		return &state;
	}

	const_iterator &operator++();

	const_iterator(const cbor::object &);
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

struct ircd::cbor::item
:const_buffer
{
	item(const const_buffer &);
	item() = default;

	friend enum major major(const item &);
};

struct ircd::cbor::binary
:const_buffer
{
	binary(const item &);
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
*/

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
