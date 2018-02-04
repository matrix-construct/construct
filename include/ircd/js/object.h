// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_JS_OBJECT_H

namespace ircd  {
namespace js    {

using object_handle = JS::Handle<JSObject *>;
using object_handle_mutable = JS::MutableHandle<JSObject *>;

// get()/set() et al overload on reserved{n} to manipulate reserved slot n
IRCD_STRONG_TYPEDEF(uint, reserved)

// Get the JSClass from which the trap can also be derived.
bool has_jsclass(const JSObject *const &);
const JSClass &jsclass(JSObject *const &);

// Get the flags from the object's JSClass or 0.
uint flags(const JSObject *const &);

// Get the `this` global from any object
JSObject *current_global(JSObject *const &);

// Misc utils
bool is_array(const object_handle &);
bool is_extensible(const object_handle &);
uint32_t size(const object_handle &);
bool deep_freeze(const object_handle &);
bool freeze(const object_handle &);
size_t bytecodes(const object_handle &, uint8_t *const &buf, const size_t &size);

struct object
:root<JSObject *>
{
	IRCD_OVERLOAD(json)
	IRCD_OVERLOAD(array)
	IRCD_OVERLOAD(uninitialized)

	using handle = object_handle;
	using handle_mutable = object_handle_mutable;

	explicit operator JSString *() const;
	explicit operator JS::Value() const;
	operator value() const;

	object constructor() const;                  // Get the constructor
	object prototype() const;                    // Get the prototype
	void prototype(object::handle);              // Set the prototype

	bool is_array() const;
	uint32_t size() const;                       // Number elements in array object
	void resize(const uint32_t &);               // Set the number of elements in array object

	object clone() const;                        // Copy the object and prototype

	// new object
	object(const JSClass *const &, const handle &ctor, const JS::HandleValueArray &args);
	object(const JSClass *const &, const JS::CallArgs &args);
	object(const JSClass *const &, const object &proto);
	object(const JSClass *const &);

	object(const uint8_t *const &bytecode, const size_t &size);
	using root<JSObject *>::root;
	template<class... args> object(json_t, args&&...);
	object(array_t, const size_t &length);
	object(const JS::HandleValueArray &);
	object(std::initializer_list<std::pair<const char *, value>>);
	object(const value &);
	object(JSObject *const &);
	object(JSObject &);
	object(nullptr_t);
	object(uninitialized_t);
	object();

	static object global();                      // current_global(cx)
};

inline
object::object()
:object::root::type
{
	JS_NewPlainObject(*cx)
}
{
	if(unlikely(!this->get()))
		throw internal_error("NULL object (plain)");
}

inline
object::object(uninitialized_t)
:object::root::type{}
{
}

inline
object::object(nullptr_t)
:object{value{nullptr}}
{
}

inline
object::object(JSObject &obj)
:object::root::type{&obj}
{
	if(unlikely(!this->get()))
		throw internal_error("NULL object (ref)");
}

inline
object::object(JSObject *const &obj)
:object::root::type{obj}
{
	if(unlikely(!this->get()))
		throw internal_error("NULL object");
}

inline
object::object(const value &val)
:object::root::type{}
{
	if(!JS_ValueToObject(*cx, val, &(*this)))
		throw type_error("Value is not an Object");
}

inline
object::object(std::initializer_list<std::pair<const char *, value>> list)
:object{}
{
	for(const auto &pair : list)
	{
		const auto &key(pair.first);
		const auto &val(pair.second);
		if(!JS_SetProperty(*cx, *this, key, val))
			throw jserror(jserror::pending);
	}
}

inline
object::object(const JS::HandleValueArray &values)
:object::root::type
{
	JS_NewArrayObject(*cx, values)
}
{
	if(unlikely(!this->get()))
		throw internal_error("NULL object (array)");
}

inline
object::object(array_t,
               const size_t &length)
:object::root::type
{
	JS_NewArrayObject(*cx, length)
}
{
	if(unlikely(!this->get()))
		throw internal_error("NULL object (array)");
}

template<class... args>
object::object(json_t,
               args&&... a)
:object(js::json::parse(std::forward<args>(a)...))
{
}

inline
object::object(const JSClass *const &clasp)
:object::root::type
{
	JS_NewObject(*cx, clasp)
}
{
	if(unlikely(!this->get()))
		throw internal_error("NULL object (clasp)");
}

inline
object::object(const JSClass *const &clasp,
               const object &proto)
:object::root::type
{
	JS_NewObjectWithGivenProto(*cx, clasp, proto)
}
{
	if(unlikely(!this->get()))
		throw internal_error("NULL object (with given proto)");
}

inline
object::object(const JSClass *const &clasp,
               const JS::CallArgs &args)
:object::root::type
{
	JS_NewObjectForConstructor(*cx, clasp, args)
}
{
	if(unlikely(!this->get()))
		throw internal_error("NULL object (for constructor)");
}

inline
object::object(const JSClass *const &clasp,
               const object::handle &ctor,
               const JS::HandleValueArray &args)
:object::root::type
{
	JS_New(*cx, ctor, args)
}
{
	if(unlikely(!this->get()))
		throw internal_error("NULL object (new)");
}

inline
object::object(const uint8_t *const &bytecode,
               const size_t &size)
:object::root::type
{
	//JS_DecodeInterpretedFunction(*cx, bytecode, size)
}
{
	if(unlikely(!this->get()))
		throw jserror(jserror::pending);
}

inline object
object::global()
{
	return current_global();
}

inline object
object::clone()
const
{
	return JS_CloneObject(*cx, *this, prototype());
}

inline void
object::resize(const uint32_t &length)
{
	if(!JS_SetArrayLength(*cx, *this, length))
		throw internal_error("Failed to set array object length");
}

inline uint32_t
object::size()
const
{
	return js::size(*this);
}

inline bool
object::is_array()
const
{
	return js::is_array(handle(*this));
}

inline object
object::constructor()
const
{
	return JS_GetConstructor(*cx, *this);
}

inline void
object::prototype(object::handle obj)
{
	if(!JS_SetPrototype(*cx, *this, obj))
		throw internal_error("Failed to set prototype for object");
}

inline object
object::prototype()
const
{
	object ret;
	if(!JS_GetPrototype(*cx, *this, &ret))
		throw internal_error("Failed to get prototype for object");

	return ret;
}

inline
object::operator value()
const
{
	return static_cast<JS::Value>(*this);
}

inline
object::operator JS::Value()
const
{
	assert(this->get());
	return JS::ObjectValue(*this->get());
}

inline
object::operator JSString *()
const
{
	assert(this->get());
	return JS_BasicObjectToString(*cx, *this);
}

inline size_t
bytecodes(const object_handle &obj,
          uint8_t *const &buf,
          const size_t &size)
{
	uint32_t ret;
	const custom_ptr<void> ptr
	{
		//JS_EncodeInterpretedFunction(*cx, obj, &ret), js_free
	};

	const auto cpsz(std::min(size_t(ret), size));
	memcpy(buf, ptr.get(), cpsz);
	return cpsz;
}

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
is_array(const object &obj)
{
	return is_array(object::handle(obj));
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

inline JSObject *
current_global(JSObject *const &obj)
{
	return JS_GetGlobalForObject(*cx, obj);
}

inline uint
flags(const JSObject *const &obj)
{
	return has_jsclass(obj)? jsclass(const_cast<JSObject *>(obj)).flags : 0;
}

inline const JSClass &
jsclass(JSObject *const &obj)
{
	assert(has_jsclass(obj));
	const auto jsc(JS_GetClass(obj));
	return *const_cast<JSClass *>(jsc);
}

inline bool
has_jsclass(const JSObject *const &obj)
{
	assert(obj);
	return JS_GetClass(const_cast<JSObject *>(obj)) != nullptr;
}

} // namespace js
} // namespace ircd
