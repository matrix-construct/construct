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

namespace ircd  {
namespace js    {
namespace basic {

template<lifetime L>
struct object
:root<JSObject *, L>
{
	using handle = typename root<JSObject *, L>::handle;
	using handle_mutable = typename root<JSObject *, L>::handle_mutable;

	operator JS::Value() const;

	// new object
	object(const JSClass *const &, const handle &ctor, const JS::HandleValueArray &args);
	object(const JSClass *const &, const JS::CallArgs &args);
	object(const JSClass *const &, const object &proto);
	object(const JSClass *const &);

	template<class T, lifetime LL> object(const root<T, LL> &);
	using root<JSObject *, L>::root;
	object(const value<L> &);
	object(JSObject *const &);
	object(JSObject &);
	object();
};

} // namespace basic

using object = basic::object<lifetime::stack>;
using heap_object = basic::object<lifetime::heap>;

//
// Implementation
//
namespace basic {

template<lifetime L>
object<L>::object()
:object<L>::root::type
{
	JS_NewPlainObject(*cx)
}
{
}

template<lifetime L>
object<L>::object(JSObject &obj)
:object<L>::root::type{&obj}
{
}

template<lifetime L>
object<L>::object(JSObject *const &obj)
:object<L>::root::type{obj}
{
	if(unlikely(!this->get()))
		throw internal_error("NULL object");
}

template<lifetime L>
object<L>::object(const value<L> &val)
:object<L>::root::type{}
{
	if(!JS_ValueToObject(*cx, val, &(*this)))
		throw type_error("Value is not an Object");
}

template<lifetime L>
template<class T,
         lifetime LL>
object<L>::object(const root<T, LL> &o)
:object{o.get()}
{
}

template<lifetime L>
object<L>::object(const JSClass *const &clasp)
:object<L>::root::type
{
	JS_NewObject(*cx, clasp)
}
{
}

template<lifetime L>
object<L>::object(const JSClass *const &clasp,
                  const object &proto)
:object<L>::root::type
{
	JS_NewObjectWithGivenProto(*cx, clasp, proto)
}
{
}

template<lifetime L>
object<L>::object(const JSClass *const &clasp,
                  const JS::CallArgs &args)
:object<L>::root::type
{
	JS_NewObjectForConstructor(*cx, clasp, args)
}
{
}

template<lifetime L>
object<L>::object(const JSClass *const &clasp,
                  const object::handle &ctor,
                  const JS::HandleValueArray &args)
:object<L>::root::type
{
	JS_New(*cx, ctor, args)
}
{
}

template<lifetime L>
object<L>::operator JS::Value()
const
{
	return this->get()? JS::ObjectValue(*this->get()) : JS::NullValue();
}

} // namespace basic
} // namespace js
} // namespace ircd
