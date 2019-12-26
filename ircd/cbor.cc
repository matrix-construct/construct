// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

/// Given a buffer of CBOR this function parses the head data and maintains
/// a const_buffer span of the head. The span includes the leading head byte
/// and one or more integer bytes following the leading byte. If the major
/// type found in this head has a data payload which is not a following-integer
/// then that data starts directly after this head buffer ends.
///
/// The argument buffer must be at least one byte and must at least cover the
/// following-integer bytes (and can also be as large as possible).
///
ircd::cbor::head::head(const const_buffer &buf)
:const_buffer{[&buf]
{
	if(unlikely(size(buf) < sizeof(uint8_t)))
		throw buffer_underrun
		{
			"Item buffer is too small to contain a header"
		};

	const uint8_t &leading
	{
		*reinterpret_cast<const uint8_t *>(data(buf))
	};

	return const_buffer
	{
		data(buf), length(leading)
	};
}()}
{
	if(unlikely(size(*this) > size(buf)))
		throw buffer_underrun
		{
			"Item buffer is too small to contain full header"
		};
}

/// Pun a reference to the integer contained by the bytes following the head.
/// If there are no bytes following the head because the integer is contained
/// as bits packed into the leading head byte, this function will throw.
///
template<class T>
const T &
ircd::cbor::head::following()
const
{
	if(unlikely(size(following()) < sizeof(T)))
		throw buffer_underrun
		{
			"Buffer following header is too small (%zu) for type requiring %zu",
			size(following()),
			sizeof(T)
		};

	return *reinterpret_cast<const T *>(data(following()));
}

/// Return buffer spanning the integer bytes following this head. This may be
/// an empty buffer if the integer byte is packed into bits of the leading
/// byte (denoted by minor()).
ircd::const_buffer
ircd::cbor::head::following()
const
{
	return { data(*this) + 1, length() - 1 };
}

/// Extract the length of the head from the buffer (requires 1 byte of buffer)
size_t
ircd::cbor::head::length()
const
{
	return length(leading());
}

/// Extract the minor type from the reference to the leading byte in the head.
enum ircd::cbor::minor
ircd::cbor::head::minor()
const
{
	return static_cast<enum minor>(minor(leading()));
}

/// Extract the major type from the reference to the leading byte in the head.
enum ircd::cbor::major
ircd::cbor::head::major()
const
{
	return static_cast<enum major>(major(leading()));
}

/// Reference the leading byte of the head.
const uint8_t &
ircd::cbor::head::leading()
const
{
	assert(size(*this) >= sizeof(uint8_t));
	return *reinterpret_cast<const uint8_t *>(data(*this));
}

/// Extract length of head from leading head byte (arg); this is the length of
/// the integer following the leading head byte plus the one leading head
/// byte. This length covers all bytes which come before item payload bytes
/// (when such a payload exists). If this length is 1 then no integer bytes
/// are following the leading head byte. The length/returned value is never 0.
size_t
ircd::cbor::head::length(const uint8_t &a)
{
	switch(major(a))
	{
		case POSITIVE:
		case NEGATIVE:
		case BINARY:
		case STRING:
		case ARRAY:
		case OBJECT:
		case TAG:
		{
			if(minor(a) > 23) switch(minor(a))
			{
				case minor::U8:    return 2;
				case minor::U16:   return 3;
				case minor::U32:   return 5;
				case minor::U64:   return 9;
				default:           throw type_error
				{
					"Unknown minor type (%u); length of header unknown", minor(a)
				};
			}
			else return 1;
		}

		case PRIMITIVE:
		{
			if(minor(a) > 23) switch(minor(a))
			{
				case FALSE:
				case TRUE:
				case NUL:
				case UD:           return 1;
				case minor::F16:   return 3;
				case minor::F32:   return 5;
				case minor::F64:   return 9;
				default:           throw type_error
				{
					"Unknown primitive minor type (%u); length of header unknown", minor(a)
				};
			}
			else return 1;
		}

		default: throw type_error
		{
			"Unknown major type; length of header unknown"
		};
	}
}

/// Extract major type from leading head byte (arg)
uint8_t
ircd::cbor::head::major(const uint8_t &a)
{
	// shift for higher 3 bits only
	static const int &shift(5);
	return a >> shift;
}

/// Extract minor type from leading head byte (arg)
uint8_t
ircd::cbor::head::minor(const uint8_t &a)
{
	// mask of lower 5 bits only
	static const uint8_t &mask
	{
		uint8_t(0xFF) >> 3
	};

	return a & mask;
}

//
// cbor.h
//

enum ircd::cbor::major
ircd::cbor::major(const const_buffer &buf)
{
	const head head(buf);
	return head.major();
}

ircd::string_view
ircd::cbor::reflect(const enum major &major)
{
	switch(major)
	{
		case major::POSITIVE:    return "POSITIVE";
		case major::NEGATIVE:    return "NEGATIVE";
		case major::BINARY:      return "BINARY";
		case major::STRING:      return "STRING";
		case major::ARRAY:       return "ARRAY";
		case major::OBJECT:      return "OBJECT";
		case major::TAG:         return "TAG";
		case major::PRIMITIVE:   return "PRIMITIVE";
	}

	return "??????";
}
