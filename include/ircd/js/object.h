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
#define HAVE_IRCD_JS_OBJECT_H

namespace ircd {
namespace js   {

struct object
:JS::Rooted<JSObject *>
{
	using handle = JS::HandleObject;
	using handle_mutable = JS::MutableHandleObject;

	operator JS::Value() const;

	// new object
	object(const JSClass *const &, const handle &ctor, const JS::HandleValueArray &args);
	object(const JSClass *const &, const JS::CallArgs &args);
	object(const JSClass *const &, const object &proto);
	object(const JSClass *const &);

	object(const object::handle_mutable &h): object{h.get()} {}
	object(const object::handle &h): object{h.get()} {}
	explicit object(const value &);
	object(JSObject *const &);
	object(JSObject &);
	object();
	object(object &&) noexcept;
	object(const object &) = delete;
	object &operator=(object &&) noexcept;
};

inline
object::object()
:JS::Rooted<JSObject *>
{
	*cx,
	JS_NewPlainObject(*cx)
}
{
}

inline
object::object(object &&other)
noexcept
:JS::Rooted<JSObject *>{*cx, other}
{
	other.set(nullptr);
}

inline object &
object::operator=(object &&other)
noexcept
{
	set(other.get());
	other.set(nullptr);
	return *this;
}

inline
object::object(JSObject &obj)
:JS::Rooted<JSObject *>{*cx, &obj}
{
}

inline
object::object(JSObject *const &obj)
:JS::Rooted<JSObject *>{*cx, obj}
{
	if(unlikely(!get()))
		throw internal_error("NULL object");
}

inline
object::object(const value &val)
:JS::Rooted<JSObject *>{*cx}
{
	if(!JS_ValueToObject(*cx, val, &(*this)))
		throw type_error("Value is not an Object");
}

inline
object::object(const JSClass *const &clasp)
:JS::Rooted<JSObject *>
{
	*cx,
	JS_NewObject(*cx, clasp)
}
{
}

inline
object::object(const JSClass *const &clasp,
               const object &proto)
:JS::Rooted<JSObject *>
{
	*cx,
	JS_NewObjectWithGivenProto(*cx, clasp, proto)
}
{
}

inline
object::object(const JSClass *const &clasp,
               const JS::CallArgs &args)
:JS::Rooted<JSObject *>
{
	*cx,
	JS_NewObjectForConstructor(*cx, clasp, args)
}
{
}

inline
object::object(const JSClass *const &clasp,
               const object::handle &ctor,
               const JS::HandleValueArray &args)
:JS::Rooted<JSObject *>
{
	*cx,
	JS_New(*cx, ctor, args)
}
{
}

inline
object::operator JS::Value()
const
{
	return get()? JS::ObjectValue(*get()) : JS::NullValue();
}

} // namespace js
} // namespace ircd
