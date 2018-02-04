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
#define HAVE_IRCD_JS_ID_H

namespace ircd  {
namespace js    {

struct id
:root<jsid>
{
	operator value() const;

	using root<jsid>::root;
	explicit id(const char *const &);  // creates new id
	explicit id(const std::string &);  // creates new id
	id(const string::handle &);
	id(const value::handle &);
	id(const value &);
	id(const string &);
	id(const JSProtoKey &);
	id(const uint32_t &);
	id();
};

bool operator==(const handle<id> &, const char *const &);
bool operator==(const handle<id> &, const std::string &);
bool operator==(const char *const &, const handle<id> &);
bool operator==(const std::string &, const handle<id> &);

inline
id::id()
:id::root::type{}
{
}

inline
id::id(const uint32_t &index)
:id::root::type{}
{
	if(!JS_IndexToId(*cx, index, &(*this)))
		throw type_error("Failed to construct id from uint32_t index");
}

inline
id::id(const JSProtoKey &key)
:id::root::type{}
{
	JS::ProtoKeyToId(*cx, key, &(*this));
}

inline
id::id(const std::string &str)
:id(str.c_str())
{
}

inline
id::id(const char *const &str)
:id::root::type{jsid()}
{
	if(!JS::PropertySpecNameToPermanentId(*cx, str, const_cast<jsid *>(this->address())))
		throw type_error("Failed to create id from native string");
}

inline
id::id(const string &h)
:id::id(string::handle(h))
{
}

inline
id::id(const value &h)
:id::id(value::handle(h))
{
}

inline
id::id(const value::handle &h)
:id::root::type{}
{
	if(!JS_ValueToId(*cx, h, &(*this)))
		throw type_error("Failed to construct id from Value");
}

inline
id::id(const string::handle &h)
:id::root::type{}
{
	if(!JS_StringToId(*cx, h, &(*this)))
		throw type_error("Failed to construct id from String");
}

inline
id::operator value()
const
{
	value ret;
	if(!JS_IdToValue(*cx, *this, &ret))
		throw type_error("Failed to construct id from String");

	return ret;
}

inline bool
operator==(const std::string &a, const handle<id> &b)
{
	return operator==(a.c_str(), b);
}

inline bool
operator==(const char *const &a, const handle<id> &b)
{
	return JS::PropertySpecNameEqualsId(a, b);
}

inline bool
operator==(const handle<id> &a, const std::string &b)
{
	return operator==(a, b.c_str());
}

inline bool
operator==(const handle<id> &a, const char *const &b)
{
	return JS::PropertySpecNameEqualsId(b, a);
}

} // namespace js
} // namespace ircd
