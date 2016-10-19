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

using closure_id = std::function<void (const id &)>;
using closure_key_val = std::function<void (const value &, const value &)>;
using closure_mutable_key_val = std::function<void (const value &, value &)>;

void for_each(const object &, const closure_id &);
void for_each(const object &, const closure_key_val &);
void for_each(object &, const closure_mutable_key_val &);


inline void
for_each(object &obj,
         const closure_mutable_key_val &closure)
{
	for_each(obj, [&obj, &closure]
	(const id &hid)
	{
		value val(get(obj, hid));
		const value key(hid);
		closure(key, val);
	});
}

inline void
for_each(const object &obj,
         const closure_key_val &closure)
{
	for_each(obj, [&obj, &closure]
	(const id &hid)
	{
		const value val(get(obj, hid));
		const value key(hid);
		closure(key, val);
	});
}

inline void
for_each(const object &obj,
         const closure_id &closure)
{
	JS::Rooted<JS::IdVector> props(*cx, JS::IdVector(cx->ptr()));
	if(JS_Enumerate(*cx, obj, &props))
		for(size_t i(0); i < props.length(); ++i)
			closure(props[i]);
}

} // namespace js
} // namespace ircd
