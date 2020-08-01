// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::cbor
{
	static uint8_t _major(const uint8_t &);       // Major type
	static uint8_t _minor(const uint8_t &);       // Minor type
	static size_t _length(const uint8_t &);       // (1 + size(following()))
}

size_t
ircd::cbor::_length(const uint8_t &a)
{
	switch(_major(a))
	{
		case POSITIVE:
		case NEGATIVE:
		case BINARY:
		case STRING:
		case ARRAY:
		case OBJECT:
		case TAG:
		{
			if(_minor(a) > 23) switch(_minor(a))
			{
				case minor::U8:    return 2;
				case minor::U16:   return 3;
				case minor::U32:   return 5;
				case minor::U64:   return 9;
				default:           throw type_error
				{
					"Unknown minor type (%u); length of header unknown",
					_minor(a)
				};
			}
			else return 1;
		}

		case PRIMITIVE:
		{
			if(_minor(a) > 23) switch(_minor(a))
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
					"Unknown primitive minor type (%u); length of header unknown",
					_minor(a)
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
ircd::cbor::_major(const uint8_t &a)
{
	// shift for higher 3 bits only
	return a >> 5;
}

/// Extract minor type from leading head byte (arg)
uint8_t
ircd::cbor::_minor(const uint8_t &a)
{
	// mask of lower 5 bits only
	static const uint8_t mask
	{
		uint8_t(0xFF) >> 3
	};

	return a & mask;
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
