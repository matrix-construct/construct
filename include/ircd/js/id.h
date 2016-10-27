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

namespace ircd  {
namespace js    {
namespace basic {

template<lifetime L>
struct id
:root<jsid, L>
{
	using root<jsid, L>::root;
	explicit id(const char *const &);  // creates new id
	explicit id(const std::string &);  // creates new id
	id(const typename string<L>::handle &);
	id(const typename value<L>::handle &);
	id(const JSProtoKey &);
	id(const uint32_t &);
	id(const jsid &);
	id();
};

template<lifetime L> bool operator==(const id<L> &, const char *const &);
template<lifetime L> bool operator==(const id<L> &, const std::string &);
template<lifetime L> bool operator==(const char *const &, const id<L> &);
template<lifetime L> bool operator==(const std::string &, const id<L> &);

} // namespace basic

using id = basic::id<lifetime::stack>;
using heap_id = basic::id<lifetime::heap>;

//
// Implementation
//
namespace basic {

template<lifetime L>
id<L>::id()
:id<L>::root::type{}
{
}

template<lifetime L>
id<L>::id(const jsid &i)
:id<L>::root::type{i}
{
}

template<lifetime L>
id<L>::id(const uint32_t &index)
:id<L>::root::type{}
{
	if(!JS_IndexToId(*cx, index, &(*this)))
		throw type_error("Failed to construct id from uint32_t index");
}

template<lifetime L>
id<L>::id(const JSProtoKey &key)
:id<L>::root::type{}
{
	JS::ProtoKeyToId(*cx, key, &(*this));
}

template<lifetime L>
id<L>::id(const std::string &str)
:id(str.c_str())
{
}

template<lifetime L>
id<L>::id(const char *const &str)
:id<L>::root::type{jsid()}
{
	if(!JS::PropertySpecNameToPermanentId(*cx, str, this->address()))
		throw type_error("Failed to create id from native string");
}

template<lifetime L>
id<L>::id(const typename value<L>::handle &h)
:id<L>::root::type{}
{
	if(!JS_ValueToId(*cx, h, &(*this)))
		throw type_error("Failed to construct id from Value");
}

template<lifetime L>
id<L>::id(const typename string<L>::handle &h)
:id<L>::root::type{}
{
	if(!JS_StringToId(*cx, h, &(*this)))
		throw type_error("Failed to construct id from String");
}

template<lifetime L>
bool
operator==(const std::string &a, const id<L> &b)
{
	return operator==(a.c_str(), b);
}

template<lifetime L>
bool
operator==(const char *const &a, const id<L> &b)
{
	return JS::PropertySpecNameEqualsId(a, b);
}

template<lifetime L>
bool
operator==(const id<L> &a, const std::string &b)
{
	return operator==(a, b.c_str());
}

template<lifetime L>
bool
operator==(const id<L> &a, const char *const &b)
{
	return JS::PropertySpecNameEqualsId(b, a);
}

} // namespace basic
} // namespace js
} // namespace ircd
