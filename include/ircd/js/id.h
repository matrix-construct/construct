/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#define HAVE_IRCD_JS_ID_H

namespace ircd {
namespace js   {

struct id
:JS::Rooted<jsid>
{
	using handle = JS::HandleId;
	using handle_mutable = JS::MutableHandleId;

	explicit id(const char *const &);         // creates new id (permanent)
	explicit id(const std::string &);         // creates new id (permanent)
	id(const string::handle &);
	id(const value::handle &);
	id(const JSProtoKey &);
	id(const uint32_t &);
	id(const handle_mutable &);
	id(const handle &);
	id(const jsid &);
	id();
	id(id &&) noexcept;
	id(const id &) = delete;

	friend bool operator==(const id &, const char *const &);
	friend bool operator==(const id &, const std::string &);
	friend bool operator==(const char *const &, const id &);
	friend bool operator==(const std::string &, const id &);
};

inline
id::id()
:JS::Rooted<jsid>{*cx}
{
}

inline
id::id(id &&other)
noexcept
:JS::Rooted<jsid>{*cx, other}
{
}

inline
id::id(const jsid &i)
:JS::Rooted<jsid>{*cx, i}
{
}

inline
id::id(const handle &h)
:JS::Rooted<jsid>{*cx, h}
{
}

inline
id::id(const handle_mutable &h)
:JS::Rooted<jsid>{*cx, h}
{
}

inline
id::id(const uint32_t &index)
:JS::Rooted<jsid>{*cx}
{
	if(!JS_IndexToId(*cx, index, &(*this)))
		throw type_error("Failed to construct id from uint32_t index");
}

inline
id::id(const JSProtoKey &key)
:JS::Rooted<jsid>{*cx}
{
	JS::ProtoKeyToId(*cx, key, &(*this));
}

inline
id::id(const value::handle &h)
:JS::Rooted<jsid>{*cx}
{
	if(!JS_ValueToId(*cx, h, &(*this)))
		throw type_error("Failed to construct id from Value");
}

inline
id::id(const string::handle &h)
:JS::Rooted<jsid>{*cx}
{
	if(!JS_StringToId(*cx, h, &(*this)))
		throw type_error("Failed to construct id from String");
}

inline
id::id(const std::string &str)
:id(str.c_str())
{
}

inline
id::id(const char *const &str)
:JS::Rooted<jsid>{*cx, jsid()}
{
	if(!JS::PropertySpecNameToPermanentId(*cx, str, address()))
		throw type_error("Failed to create id from native string");
}

inline bool
operator==(const std::string &a, const id &b)
{
	return operator==(a.c_str(), b);
}

inline bool
operator==(const char *const &a, const id &b)
{
	return JS::PropertySpecNameEqualsId(a, b);
}

inline bool
operator==(const id &a, const std::string &b)
{
	return operator==(a, b.c_str());
}

inline bool
operator==(const id &a, const char *const &b)
{
	return JS::PropertySpecNameEqualsId(b, a);
}

} // namespace js
} // namespace ircd
