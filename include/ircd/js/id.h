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

JS::RootedValue id(context &, const jsid &);
const jsid &id(context &, const JSProtoKey &);
const jsid &id(context &, const uint32_t &);
const jsid &id(context &, const JS::HandleString &);
const jsid &id(context &, const JS::HandleValue &);


inline const jsid &
id(context &c,
   const JS::HandleValue &h)
{
	JS::RootedId ret(c);
	if(!JS_ValueToId(c, h, &ret))
		std::terminate(); //TODO: exception

	return ret;
}

inline const jsid &
id(context &c,
   const JS::HandleString &h)
{
	JS::RootedId ret(c);
	if(!JS_StringToId(c, h, &ret))
		std::terminate(); //TODO: exception

	return ret;
}

inline const jsid &
id(context &c,
   const uint32_t &index)
{
	JS::RootedId ret(c);
	if(!JS_IndexToId(c, index, &ret))
		std::terminate(); //TODO: exception

	return ret;
}

inline const jsid &
id(context &c,
   const JSProtoKey &key)
{
	JS::RootedId ret(c);
	JS::ProtoKeyToId(c, key, &ret);
	return ret;
}

inline JS::RootedValue
id(context &c,
   const jsid &h)
{
	JS::RootedValue ret(c);
	if(!JS_IdToValue(c, h, &ret))
		std::terminate(); //TODO: exception

	return { c, ret };
}

} // namespace js
} // namespace ircd
