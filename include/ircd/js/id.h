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

JS::RootedValue id(const jsid &);
const jsid &id(const JSProtoKey &);
const jsid &id(const uint32_t &);
const jsid &id(const JS::HandleString &);
const jsid &id(const JS::HandleValue &);


inline const jsid &
id(const JS::HandleValue &h)
{
	JS::RootedId ret(*cx);
	if(!JS_ValueToId(*cx, h, &ret))
		std::terminate(); //TODO: exception

	return ret;
}

inline const jsid &
id(const JS::HandleString &h)
{
	JS::RootedId ret(*cx);
	if(!JS_StringToId(*cx, h, &ret))
		std::terminate(); //TODO: exception

	return ret;
}

inline const jsid &
id(const uint32_t &index)
{
	JS::RootedId ret(*cx);
	if(!JS_IndexToId(*cx, index, &ret))
		std::terminate(); //TODO: exception

	return ret;
}

inline const jsid &
id(const JSProtoKey &key)
{
	JS::RootedId ret(*cx);
	JS::ProtoKeyToId(*cx, key, &ret);
	return ret;
}

inline JS::RootedValue
id(const jsid &h)
{
	JS::RootedValue ret(*cx);
	if(!JS_IdToValue(*cx, h, &ret))
		std::terminate(); //TODO: exception

	return { *cx, ret };
}

} // namespace js
} // namespace ircd
