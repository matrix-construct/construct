// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

//
// object
//

ircd::cbor::object::object(const const_buffer &buf)
:item{buf}
{
	const head &head{*this};
	if(unlikely(head.major() != major::OBJECT))
		throw type_error
		{
			"Supplied item is a '%s' and not an OBJECT", reflect(head.major())
		};
}

ircd::cbor::object::member
ircd::cbor::object::operator[](const string_view &name)
const
{
	const_iterator it{*this};
	for(; it; ++it)
	{
		const string item{it->first};
		if(item.value() == name)
			return *it;
	}

	throw std::out_of_range
	{
		"object member not found"
	};
}

ircd::const_buffer
ircd::cbor::object::value()
const
{
	return {};
}

size_t
ircd::cbor::object::count()
const
{
	const head &head{*this};
	const positive &positive{head};
	return uint64_t(positive);
}

//
// object::const_iterator
//

ircd::cbor::object::const_iterator::const_iterator(const cbor::object &object)
:object{object}
,state{[this]() -> member
{
	if(!this->object.count())
		return {};

	const const_buffer &obuf{this->object};
	const head &head{obuf};
	const_buffer key
	{
		ircd::data(obuf) + head.length(), ircd::end(obuf)
	};

	const cbor::head &keyhead{key};
	switch(keyhead.major())
	{
		case major::POSITIVE:
		case major::NEGATIVE:
		case major::TAG:
		case major::PRIMITIVE:
		{
			std::get<1>(key) = std::get<0>(key) + keyhead.length();
			break;
		}

		case major::BINARY:
		case major::STRING:
		{
			const cbor::positive keypositive(keyhead);
			std::get<1>(key) = std::get<0>(key) + keyhead.length() + uint64_t(keypositive);
			break;
		}

		case major::ARRAY:
		case major::OBJECT:

		default: throw type_error
		{
			"Unknown major type; length of value unknown"
		};
	}

	const_buffer val
	{
		ircd::end(key), ircd::end(obuf)
	};

	const cbor::head &valhead{val};
	switch(valhead.major())
	{
		case major::POSITIVE:
		case major::NEGATIVE:
		case major::TAG:
		case major::PRIMITIVE:
		{
			std::get<1>(val) = std::get<0>(val) + valhead.length();
			break;
		}

		case major::BINARY:
		case major::STRING:
		{
			const cbor::positive valpositive(valhead);
			std::get<1>(val) = std::get<0>(val) + valhead.length() + uint64_t(valpositive);
			break;
		}

		case major::ARRAY:
		case major::OBJECT:

		default: throw type_error
		{
			"Unknown major type; length of value unknown"
		};
	}

	return { key, val };
}()}
{
	const const_buffer &obuf{this->object};
	if(unlikely(data(state.first) > data(obuf) + size(obuf)))
		throw buffer_underrun
		{
			"Object iteration leads beyond the supplied object buffer"
		};

}

ircd::cbor::object::const_iterator &
ircd::cbor::object::const_iterator::operator++()
{

	return *this;
}

//
// array
//

ircd::cbor::array::array(const const_buffer &buf)
:item{buf}
{
	const head &head{*this};
	if(unlikely(head.major() != major::ARRAY))
		throw type_error
		{
			"Supplied item is a '%s' and not an ARRAY", reflect(head.major())
		};
}

ircd::cbor::item
ircd::cbor::array::operator[](size_t i)
const
{
	const_iterator it{*this};
	for(; it && i > 0; --i, ++it);
	return *it;
}

ircd::cbor::array::const_iterator
ircd::cbor::array::begin()
const
{
	const_iterator ret(*this);
	if(!this->count())
		return ret;

	const const_buffer &abuf{*this};
	const head &head{abuf};
	ret.state = { ircd::data(abuf) + head.length(), ircd::end(abuf) };
	const cbor::head &elemhead{ret.state};
	switch(elemhead.major())
	{
		case major::POSITIVE:
		case major::NEGATIVE:
		case major::TAG:
		case major::PRIMITIVE:
		{
			std::get<1>(ret.state) = std::get<0>(ret.state) + elemhead.length();
			break;
		}

		case major::BINARY:
		case major::STRING:
		{
			const cbor::positive elempositive(elemhead);
			std::get<1>(ret.state) = std::get<0>(ret.state) + elemhead.length() + uint64_t(elempositive);
			break;
		}

		case major::ARRAY:
		case major::OBJECT:

		default: throw type_error
		{
			"Unknown major type; length of value unknown"
		};
	}

	if(unlikely(data(ret.state) > data(abuf) + size(abuf)))
		throw buffer_underrun
		{
			"Array iteration leads beyond the supplied array buffer"
		};

	return ret;
}

ircd::cbor::array::const_iterator
ircd::cbor::array::end()
const
{
	return {*this};
}

ircd::const_buffer
ircd::cbor::array::value()
const
{
	return {};
}

size_t
ircd::cbor::array::count()
const
{
	const head &head{*this};
	const positive &positive{head};
	return uint64_t(positive);
}

//
// array::const_iterator
//

ircd::cbor::array::const_iterator::const_iterator(const cbor::array &array)
:array{array}
{
}

ircd::cbor::array::const_iterator &
ircd::cbor::array::const_iterator::operator++()
{
	const const_buffer &buf{this->array};
	state =
	{
		data(state) + size(state), data(buf) + size(buf)
	};

	if(data(state) == data(buf) + size(buf))
		return *this;

	assert(!!state);
	const head &head{state};
	switch(head.major())
	{
		case major::POSITIVE:
		case major::NEGATIVE:
		case major::TAG:
		case major::PRIMITIVE:
		{
			std::get<1>(state) = std::get<0>(state) + head.length();
			break;
		}

		case major::BINARY:
		case major::STRING:
		{
			const positive positive(head);
			std::get<1>(state) = std::get<0>(state) + head.length() + uint64_t(positive);
			break;
		}

		case major::ARRAY:
		case major::OBJECT:

		default: throw type_error
		{
			"Unknown major type; length of value unknown"
		};
	}

	if(unlikely(data(state) > data(buf) + size(buf)))
		throw buffer_underrun
		{
			"Array iteration leads beyond the supplied array buffer"
		};

	return *this;
}

//
// string
//

ircd::cbor::string::string(const const_buffer &buf)
:binary{buf}
{
	const head &head{*this};
	if(unlikely(head.major() != major::STRING))
		throw type_error
		{
			"Supplied item is a '%s' and not a STRING",
			reflect(head.major())
		};
}

ircd::cbor::string::operator
string_view()
const
{
	return value();
}

ircd::string_view
ircd::cbor::string::value()
const
{
	const binary &bin{*this};
	return string_view{bin.value()};
}

//
// binary
//

ircd::cbor::binary::binary(const const_buffer &buf)
:positive
{
	buf
}
{
	item &item{*this};
	const head &head{item};
	if(unlikely(head.major() != major::BINARY && head.major() != major::STRING))
		throw type_error
		{
			"Supplied item is a '%s' and not a BINARY or STRING",
			reflect(head.major())
		};

	const_buffer &itembuf{item};
	itembuf = value();

	if(unlikely(size(itembuf) > size(buf)))
		throw buffer_underrun
		{
			"Length of binary data item (%zu) exceeds supplied buffer (%zu)",
			size(itembuf),
			size(buf)
		};
}

ircd::const_buffer
ircd::cbor::binary::value()
const
{
	const item &item{*this};
	const const_buffer &buf{item};
	const head head{item};

	const positive &positive{*this};
	const uint64_t length{positive};
	return const_buffer
	{
		ircd::data(buf) + head.length(), length
	};
}

//
// negative
//

ircd::cbor::negative::negative(const head &head)
:positive{head}
{
}

ircd::cbor::negative::operator
int64_t()
const
{
	return value();
}

int64_t
ircd::cbor::negative::value()
const
{
	const positive positive(*this);
	const uint64_t &val(positive);
	return 1 - val;
}

//
// positive
//

ircd::cbor::positive::positive(const head &head)
:item{head}
{
}

ircd::cbor::positive::operator
uint64_t()
const
{
	return value();
}

uint64_t
ircd::cbor::positive::value()
const
{
	const head &head{*this};
	if(head.minor() > 23) switch(head.minor())
	{
		case minor::U8:    return head.following<uint8_t>();
		case minor::U16:   return ntoh(head.following<uint16_t>());
		case minor::U32:   return ntoh(head.following<uint32_t>());
		case minor::U64:   return ntoh(head.following<uint64_t>());
		default:           throw parse_error
		{
			"Unknown minor type; length of value unknown"
		};
	}
	else return head.minor();
}

//
// item
//

ircd::cbor::item::item(const const_buffer &buf)
:const_buffer{buf}
{}

ircd::const_buffer
ircd::cbor::item::value()
const
{
	return {};
}

ircd::cbor::item::operator
ircd::cbor::head()
const
{
	const const_buffer &buf(*this);
	return cbor::head{buf};
}

//
// head
//

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
