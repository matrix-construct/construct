// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::cbor
{

}

//
// string
//

ircd::cbor::string::string(const item &item)
:string_view{[&item]
{
	const item::header header(item);
	return string_view
	{
		ircd::buffer::data(item) + header.header_size(), header.value_count()
	};
}()}
{
}

//
// item
//

enum ircd::cbor::item::major
ircd::cbor::major(const item &item)
{
	const item::header &header(item);
	return static_cast<item::major>(item::header::major(header.leading()));
}

ircd::string_view
ircd::cbor::reflect(const item::major &major)
{
	switch(major)
	{
		case item::major::POSITIVE:    return "POSITIVE";
		case item::major::NEGATIVE:    return "NEGATIVE";
		case item::major::BINARY:      return "BINARY";
		case item::major::STRING:      return "STRING";
		case item::major::ARRAY:       return "ARRAY";
		case item::major::OBJECT:      return "OBJECT";
		case item::major::TAG:         return "TAG";
		case item::major::PRIMITIVE:   return "PRIMITIVE";
	}

	return "??????";
}

//
// item::item
//

ircd::cbor::item::item(const const_buffer &buf)
:const_buffer{[&buf]
{
	return const_buffer{buf};
}()}
{
}

//
// item::header
//

ircd::cbor::item::header::header(const const_buffer &buf)
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

size_t
ircd::cbor::item::header::value_count()
const
{
	switch(major(leading()))
	{
		case BINARY:
		case STRING:
		case ARRAY:
		case OBJECT:
		{
			if(minor(leading()) <= 23)
				return minor(leading());

			const auto *const positive
			{
				reinterpret_cast<const cbor::positive *>(data(following()))
			};

			switch(minor(leading()))
			{
				case item::minor::U8:    return ntoh(positive->u8);
				case item::minor::U16:   return ntoh(positive->u16);
				case item::minor::U32:   return ntoh(positive->u32);
				case item::minor::U64:   return ntoh(positive->u64);
				default:                 throw parse_error
				{
					"Unknown value length from minor data"
				};
			}
		}

		case TAG:        return 1; // Indicates 1 item is content
		case POSITIVE:   return 0; // No content
		case NEGATIVE:   return 0; // No content
		case PRIMITIVE:  return 0; // No content
		default:         throw type_error
		{
			"Unknown major type; length of value unknown"
		};
	}
}

size_t
ircd::cbor::item::header::header_size()
const
{
	return length(leading());
}

ircd::const_buffer
ircd::cbor::item::header::following()
const
{
	return { data(*this) + 1, header_size() - 1 };
}

const uint8_t &
ircd::cbor::item::header::leading()
const
{
	assert(size(*this) >= sizeof(uint8_t));
	return *reinterpret_cast<const uint8_t *>(data(*this));
}

size_t
ircd::cbor::item::header::length(const uint8_t &a)
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
				case item::minor::U8:    return 2;
				case item::minor::U16:   return 3;
				case item::minor::U32:   return 5;
				case item::minor::U64:   return 9;
				default:                 throw type_error
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
				case UD:                 return 1;
				case item::minor::F16:   return 3;
				case item::minor::F32:   return 5;
				case item::minor::F64:   return 9;
				default:                 throw type_error
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

uint8_t
ircd::cbor::item::header::major(const uint8_t &a)
{
	static const int &shift(5);
	return a >> shift;
}

uint8_t
ircd::cbor::item::header::minor(const uint8_t &a)
{
	static const uint8_t &mask(0x1F);
	return a & mask;
}
