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
#define HAVE_IRCD_JS_FOR_EACH_H

namespace ircd {
namespace js   {

using closure_id = std::function<void (const JS::HandleId &)>;
using closure_key_val = std::function<void (JS::HandleValue, JS::HandleValue)>;
using closure_mutable_key_val = std::function<void (JS::HandleValue, JS::MutableHandleValue)>;

void for_each(context &, const JS::HandleObject &, const closure_id &);
void for_each(context &, const JS::HandleObject &, const closure_key_val &);
void for_each(context &, const JS::MutableHandleObject &, const closure_mutable_key_val &);


inline void
for_each(context &c,
         const JS::MutableHandleObject &obj,
         const closure_mutable_key_val &closure)
{
	for_each(c, obj, [&c, &obj, &closure]
	(const JS::HandleId &id)
	{
		JS::RootedValue key(c), val(c);
		if(!JS_IdToValue(c, id, &key))
			return;

		JS_GetPropertyById(c, obj, id, &val);
		closure(key, &val);
	});
}

inline void
for_each(context &c,
         const JS::HandleObject &obj,
         const closure_key_val &closure)
{
	for_each(c, obj, [&c, &obj, &closure]
	(const JS::HandleId &id)
	{
		JS::RootedValue key(c), val(c);
		if(!JS_IdToValue(c, id, &key))
			return;

		JS_GetPropertyById(c, obj, id, &val);
		closure(key, val);
	});
}

inline void
for_each(context &c,
         const JS::HandleObject &obj,
         const closure_id &closure)
{
	JS::Rooted<JS::IdVector> props(c, JS::IdVector(c.ptr()));
	if(JS_Enumerate(c, obj, &props))
		for(size_t i(0); i < props.length(); ++i)
			closure(props[i]);
}

} // namespace js
} // namespace ircd
