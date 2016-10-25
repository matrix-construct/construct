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

	id(const JSProtoKey &);
	id(const uint32_t &);
	id(const jsid &);
	id(const JS::HandleString &);
	id(const JS::HandleValue &);
	id(const JS::MutableHandleId &);
	id(const JS::HandleId &);
	id();
	id(id &&) noexcept;
	id(const id &) = delete;
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
id::id(const JS::HandleId &h)
:JS::Rooted<jsid>{*cx, h}
{
}

inline
id::id(const JS::MutableHandleId &h)
:JS::Rooted<jsid>{*cx, h}
{
}

inline
id::id(const JS::HandleValue &h)
:JS::Rooted<jsid>{*cx}
{
	if(!JS_ValueToId(*cx, h, &(*this)))
		throw type_error("Failed to construct id from Value");
}

inline
id::id(const JS::HandleString &h)
:JS::Rooted<jsid>{*cx}
{
	if(!JS_StringToId(*cx, h, &(*this)))
		throw type_error("Failed to construct id from String");
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

} // namespace js
} // namespace ircd
