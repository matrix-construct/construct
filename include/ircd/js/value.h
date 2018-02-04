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
#define HAVE_IRCD_JS_VALUE_H

namespace ircd {
namespace js   {

// Use Value to carry a non-gc host pointer value
template<class T> T *pointer_value(const JS::Value &);
JS::Value pointer_value(const void *const &);
JS::Value pointer_value(void *const &);

struct value
:root<JS::Value>
{
	IRCD_OVERLOAD(pointer)

	explicit operator std::string() const;
	explicit operator double() const;
	explicit operator uint64_t() const;
	explicit operator int64_t() const;
	explicit operator uint32_t() const;
	explicit operator int32_t() const;
	explicit operator uint16_t() const;
	explicit operator bool() const;

	template<class T> value(const JS::MutableHandle<T> &h);
	template<class T> value(const JS::Handle<T> &h);
	value(pointer_t, void *const &);
	value(const std::string &);
	value(const char *const &);
	value(const nullptr_t &);
	value(const double &);
	value(const float &);
	value(const uint64_t &);
	value(const int32_t &);
	value(const bool &);
	value(const jsid &);
	value(JSObject &);
	value(JSObject *const &);
	value(JSString *const &);
	value(JSFunction *const &);
	value(JS::Symbol *const &);
	value(const JS::Value &);
	value();
};

JSType type(const handle<value> &val);
JSType type(const value &val);
bool undefined(const handle<value> &val);
bool undefined(const value &val);
bool is_array(const handle<value> &val);
bool is_array(const value &val);

inline
value::value()
:value::root::type{JS::UndefinedValue()}
{
}

inline
value::value(const JS::Value &val)
:value::root::type{val}
{
}

inline
value::value(JS::Symbol *const &val)
:value::root::type{JS::SymbolValue(val)}
{
}

inline
value::value(JSObject *const &val)
:value::root::type
{
	val? JS::ObjectValue(*val) : throw internal_error("NULL JSObject")
}
{
}

inline
value::value(JSObject &val)
:value::root::type{JS::ObjectValue(val)}
{
}

inline
value::value(JSString *const &val)
:value::root::type{JS::StringValue(val)}
{
}

inline
value::value(JSFunction *const &val)
:value::root::type{}
{
	auto *const obj(JS_GetFunctionObject(val));
	if(unlikely(!obj))
		throw type_error("Function cannot convert to Object");

	(*this) = JS::ObjectValue(*obj);
}

inline
value::value(const jsid &val)
:value::root::type{}
{
	if(!JS_IdToValue(*cx, val, &(*this)))
		throw type_error("Failed to construct value from Id");
}

inline
value::value(const bool &b)
:value::root::type{JS::BooleanValue(b)}
{
}

inline
value::value(const int32_t &val)
:value::root::type{JS::Int32Value(val)}
{
}

inline
value::value(const uint64_t &val)
:value::root::type{JS::DoubleValue(val)}
{
}

inline
value::value(const float &val)
:value::root::type{JS::Float32Value(val)}
{
}

inline
value::value(const double &val)
:value::root::type{JS::DoubleValue(val)}
{
}

inline
value::value(const nullptr_t &)
:value::root::type{JS::NullValue()}
{
}

inline
value::value(const std::string &s)
:value::root::type{[&s]
{
	if(s.empty())
		return JS::StringValue(JS_GetEmptyString(*cx));

	auto buf(native_external_copy(s));
	const auto ret(JS_NewExternalString(*cx, buf.get(), s.size(), &native_external_delete));
	buf.release();
	return JS::StringValue(ret);
}()}
{
}

inline
value::value(const char *const &s)
:value::root::type{[&s]
{
	if(!s || !*s)
		return JS::StringValue(JS_GetEmptyString(*cx));

	const auto len(strlen(s));
	auto buf(native_external_copy(s, len));
	const auto ret(JS_NewExternalString(*cx, buf.get(), len, &native_external_delete));
	buf.release();
	return JS::StringValue(ret);
}()}
{
}

inline
value::value(pointer_t,
             void *const &ptr)
:value::root::type{pointer_value(ptr)}
{
}

template<class T>
value::value(const JS::Handle<T> &h)
:value(h.get())
{
}

template<class T>
value::value(const JS::MutableHandle<T> &h)
:value(h.get())
{
}

inline
value::operator bool()
const
{
	return JS::ToBoolean(*this);
}

inline
value::operator uint16_t()
const
{
	uint16_t ret;
	if(!JS::ToUint16(*cx, *this, &ret))
		throw type_error("Failed cast to uint16_t");

	return ret;
}

inline
value::operator int32_t()
const
{
	int32_t ret;
	if(!JS::ToInt32(*cx, *this, &ret))
		throw type_error("Failed cast to int32_t");

	return ret;
}

inline
value::operator uint32_t()
const
{
	uint32_t ret;
	if(!JS::ToUint32(*cx, *this, &ret))
		throw type_error("Failed cast to uint32_t");

	return ret;
}

inline
value::operator int64_t()
const
{
	int64_t ret;
	if(!JS::ToInt64(*cx, *this, &ret))
		throw type_error("Failed cast to int64_t");

	return ret;
}

inline
value::operator uint64_t()
const
{
	uint64_t ret;
	if(!JS::ToUint64(*cx, *this, &ret))
		throw type_error("Failed cast to uint64_t");

	return ret;
}

inline
value::operator double()
const
{
	double ret;
	if(!JS::ToNumber(*cx, *this, &ret))
		throw type_error("Failed cast to double");

	return ret;
}

inline
value::operator std::string()
const
{
	const auto s(JS::ToString(*cx, *this));
	return s? native(s) : throw type_error("Failed to cast to string");
}

inline bool
is_array(const value &val)
{
	return is_array(handle<value>(val));
}

inline bool
is_array(const handle<value> &val)
{
	bool ret;
	if(!JS_IsArrayObject(*cx, val, &ret))
		throw internal_error("Failed to query if value is array");

	return ret;
}

inline bool
undefined(const value &val)
{
	return type(val) == JSTYPE_VOID;
}

inline bool
undefined(const handle<value> &val)
{
	return type(val) == JSTYPE_VOID;
}

inline JSType
type(const value &val)
{
	return JS_TypeOfValue(*cx, val);
}

inline JSType
type(const handle<value> &val)
{
	return JS_TypeOfValue(*cx, val);
}

inline JS::Value
pointer_value(const void *const &ptr)
{
	return pointer_value(const_cast<void *>(ptr));
}

inline JS::Value
pointer_value(void *const &ptr)
{
	JS::Value ret;
	ret.setPrivate(ptr);
	return ret;
}

template<class T>
T *
pointer_value(const JS::Value &val)
{
	const auto ret(val.toPrivate());
	return reinterpret_cast<T *>(ret);
}

} // namespace js
} // namespace ircd
