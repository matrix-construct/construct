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

using object_handle = JS::Handle<JSObject *>;
using object_handle_mutable = JS::MutableHandle<JSObject *>;

// Get the JSClass from which the trap can also be derived.
const JSClass &jsclass(const object_handle &);

bool is_extensible(const object_handle &);
bool is_array(const object_handle &);
uint32_t size(const object_handle &);

// Private data slot (trap must have flag JSCLASS_HAS_PRIVATE)
template<class T> T &priv(const object_handle &);
void priv(const object_handle &, void *const &);
void priv(const object_handle &, const void *const &);

bool deep_freeze(const object_handle &);
bool freeze(const object_handle &);

IRCD_STRONG_TYPEDEF(uint, reserved)

namespace basic {

template<lifetime L>
struct object
:root<JSObject *, L>
{
	IRCD_OVERLOAD(array)
	IRCD_OVERLOAD(uninitialized)
	using handle = object_handle;
	using handle_mutable = object_handle_mutable;

	operator JS::Value() const;

	// for array objects
	uint32_t size() const;
	void resize(const uint32_t &);

	// new object
	object(const JSClass *const &, const handle &ctor, const JS::HandleValueArray &args);
	object(const JSClass *const &, const JS::CallArgs &args);
	object(const JSClass *const &, const object &proto);
	object(const JSClass *const &);

	template<class T, lifetime LL> object(const root<T, LL> &);
	using root<JSObject *, L>::root;
	object(array_t, const size_t &length);
	object(const JS::HandleValueArray &);
	object(const value<L> &);
	object(JSObject *const &);
	object(JSObject &);
	object(uninitialized_t);
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
object<L>::object(uninitialized_t)
:object<L>::root::type{}
{
}

template<lifetime L>
object<L>::object()
:object<L>::root::type
{
	JS_NewPlainObject(*cx)
}
{
	if(unlikely(!this->get()))
		throw internal_error("NULL object (plain)");
}

template<lifetime L>
object<L>::object(JSObject &obj)
:object<L>::root::type{&obj}
{
	if(unlikely(!this->get()))
		throw internal_error("NULL object (ref)");
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
object<L>::object(const JS::HandleValueArray &values)
:object<L>::root::type
{
	JS_NewArrayObject(*cx, values)
}
{
	if(unlikely(!this->get()))
		throw internal_error("NULL object (array)");
}

template<lifetime L>
object<L>::object(array_t,
                  const size_t &length)
:object<L>::root::type
{
	JS_NewArrayObject(*cx, length)
}
{
	if(unlikely(!this->get()))
		throw internal_error("NULL object (array)");
}

template<lifetime L>
template<class T,
         lifetime LL>
object<L>::object(const root<T, LL> &o)
:object{o.get()}
{
	if(unlikely(!this->get()))
		throw internal_error("NULL object (cross-lifetime)");
}

template<lifetime L>
object<L>::object(const JSClass *const &clasp)
:object<L>::root::type
{
	JS_NewObject(*cx, clasp)
}
{
	if(unlikely(!this->get()))
		throw internal_error("NULL object (clasp)");
}

template<lifetime L>
object<L>::object(const JSClass *const &clasp,
                  const object &proto)
:object<L>::root::type
{
	JS_NewObjectWithGivenProto(*cx, clasp, proto)
}
{
	if(unlikely(!this->get()))
		throw internal_error("NULL object (with given proto)");
}

template<lifetime L>
object<L>::object(const JSClass *const &clasp,
                  const JS::CallArgs &args)
:object<L>::root::type
{
	JS_NewObjectForConstructor(*cx, clasp, args)
}
{
	if(unlikely(!this->get()))
		throw internal_error("NULL object (for constructor)");
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
	if(unlikely(!this->get()))
		throw internal_error("NULL object (new)");
}

template<lifetime L>
void
object<L>::resize(const uint32_t &length)
{
	if(!JS_SetArrayLength(*cx, *this, length))
		throw internal_error("Failed to set array object length");
}

template<lifetime L>
uint32_t
object<L>::size()
const
{
	return js::size(*this);
}

template<lifetime L>
object<L>::operator JS::Value()
const
{
	return this->get()? JS::ObjectValue(*this->get()) : JS::NullValue();
}

} // namespace basic

inline bool
freeze(const object_handle &obj)
{
	return JS_FreezeObject(*cx, obj);
}

inline bool
deep_freeze(const object_handle &obj)
{
	return JS_DeepFreezeObject(*cx, obj);
}

inline uint32_t
size(const object_handle &obj)
{
	uint32_t ret;
	if(!JS_GetArrayLength(*cx, obj, &ret))
		throw internal_error("Failed to get array object length");

	return ret;
}

inline bool
is_array(const object_handle &obj)
{
	bool ret;
	if(!JS_IsArrayObject(*cx, obj, &ret))
		throw internal_error("Failed to query if object is array");

	return ret;
}

inline bool
is_extensible(const object_handle &obj)
{
	bool ret;
	if(!JS_IsExtensible(*cx, obj, &ret))
		throw internal_error("Failed to query object extensibility");

	return ret;
}

inline void
priv(const object_handle &obj,
     const void *const &ptr)
{
	JS_SetPrivate(obj, const_cast<void *>(ptr));
}

inline void
priv(const object_handle &obj,
     void *const &ptr)
{
	JS_SetPrivate(obj, ptr);
}

template<class T>
T &
priv(const object_handle &obj)
{
	const auto &jsc(jsclass(obj));
	const auto ret(JS_GetInstancePrivate(*cx, obj, &jsc, nullptr));
	if(!ret)
		throw error("Object has no private data");

	return *reinterpret_cast<T *>(ret);
}

inline const JSClass &
jsclass(const object_handle &obj)
{
	const auto jsc(JS_GetClass(obj));
	if(unlikely(!jsc))
		throw error("Object has no JSClass");

	return *const_cast<JSClass *>(jsc);
}

} // namespace js
} // namespace ircd
