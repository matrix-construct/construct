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
#define HAVE_IRCD_JS_VALUE_H

namespace ircd {
namespace js   {

// Use Value to carry a non-gc host pointer value
template<class T> T *pointer_value(const JS::Value &);
JS::Value pointer_value(const void *const &);
JS::Value pointer_value(void *const &);

namespace basic {

template<lifetime L>
struct value
:root<JS::Value, L>
{
	explicit operator std::string() const;
	explicit operator double() const;
	explicit operator uint64_t() const;
	explicit operator int64_t() const;
	explicit operator uint32_t() const;
	explicit operator int32_t() const;
	explicit operator uint16_t() const;
	explicit operator bool() const;

	template<class T, lifetime LL> value(const root<T, LL> &r);
	template<class T> value(const JS::MutableHandle<T> &h);
	template<class T> value(const JS::Handle<T> &h);
	value(const std::string &);
	value(const char *const &);
	value(const nullptr_t &);
	value(const double &);
	value(const float &);
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

template<lifetime L> JSType type(const value<L> &);
template<lifetime L> bool undefined(const value<L> &);

} // namespace basic

using value = basic::value<lifetime::stack>;
using heap_value = basic::value<lifetime::heap>;

//
// Implementation
//
namespace basic {

template<lifetime L>
value<L>::value()
:value<L>::root::type{JS::UndefinedValue()}
{
}

template<lifetime L>
value<L>::value(const JS::Value &val)
:value<L>::root::type{val}
{
}

template<lifetime L>
value<L>::value(JS::Symbol *const &val)
:value<L>::root::type{JS::SymbolValue(val)}
{
}

template<lifetime L>
value<L>::value(JSObject *const &val)
:value<L>::root::type
{
	val? JS::ObjectValue(*val) : throw internal_error("NULL JSObject")
}
{
}

template<lifetime L>
value<L>::value(JSObject &val)
:value<L>::root::type{JS::ObjectValue(val)}
{
}

template<lifetime L>
value<L>::value(JSString *const &val)
:value<L>::root::type{JS::StringValue(val)}
{
}

template<lifetime L>
value<L>::value(JSFunction *const &val)
:value<L>::root::type{}
{
	auto *const obj(JS_GetFunctionObject(val));
	if(unlikely(!obj))
		throw type_error("Function cannot convert to Object");

	this->set(JS::ObjectValue(*obj));
}

template<lifetime L>
value<L>::value(const jsid &val)
:value<L>::root::type{}
{
	if(!JS_IdToValue(*cx, val, &(*this)))
		throw type_error("Failed to construct value from Id");
}

template<lifetime L>
value<L>::value(const bool &b)
:value<L>::root::type{JS::BooleanValue(b)}
{
}

template<lifetime L>
value<L>::value(const int32_t &val)
:value<L>::root::type{JS::Int32Value(val)}
{
}

template<lifetime L>
value<L>::value(const float &val)
:value<L>::root::type{JS::Float32Value(val)}
{
}

template<lifetime L>
value<L>::value(const double &val)
:value<L>::root::type{JS::DoubleValue(val)}
{
}

template<lifetime L>
value<L>::value(const nullptr_t &)
:value<L>::root::type{JS::NullValue()}
{
}

template<lifetime L>
value<L>::value(const std::string &s)
:value<L>::root::type{[&s]
{
	auto buf(native_external_copy(s));
	const auto ret(JS_NewExternalString(*cx, buf.release(), s.size(), &native_external_delete));
	return JS::StringValue(ret);
}()}
{
}

template<lifetime L>
value<L>::value(const char *const &s)
:value<L>::root::type{!s? JS::NullValue() : [&s]
{
	const auto len(strlen(s));
	auto buf(native_external_copy(s, len));
	const auto ret(JS_NewExternalString(*cx, buf.release(), len, &native_external_delete));
	return JS::StringValue(ret);
}()}
{
}

template<lifetime L>
template<class T>
value<L>::value(const JS::Handle<T> &h)
:value(h.get())
{
}

template<lifetime L>
template<class T>
value<L>::value(const JS::MutableHandle<T> &h)
:value(h.get())
{
}

template<lifetime L>
template<class T,
         lifetime LL>
value<L>::value(const root<T, LL> &r):
value(r.get())
{
}

template<lifetime L>
value<L>::operator bool()
const
{
	return JS::ToBoolean(*this);
}

template<lifetime L>
value<L>::operator uint16_t()
const
{
	uint16_t ret;
	if(!JS::ToUint16(*cx, *this, &ret))
		throw type_error("Failed cast to uint16_t");

	return ret;
}

template<lifetime L>
value<L>::operator int32_t()
const
{
	int32_t ret;
	if(!JS::ToInt32(*cx, *this, &ret))
		throw type_error("Failed cast to int32_t");

	return ret;
}

template<lifetime L>
value<L>::operator uint32_t()
const
{
	uint32_t ret;
	if(!JS::ToUint32(*cx, *this, &ret))
		throw type_error("Failed cast to uint32_t");

	return ret;
}

template<lifetime L>
value<L>::operator int64_t()
const
{
	int64_t ret;
	if(!JS::ToInt64(*cx, *this, &ret))
		throw type_error("Failed cast to int64_t");

	return ret;
}

template<lifetime L>
value<L>::operator uint64_t()
const
{
	uint64_t ret;
	if(!JS::ToUint64(*cx, *this, &ret))
		throw type_error("Failed cast to uint64_t");

	return ret;
}

template<lifetime L>
value<L>::operator double()
const
{
	double ret;
	if(!JS::ToNumber(*cx, *this, &ret))
		throw type_error("Failed cast to double");

	return ret;
}

template<lifetime L>
value<L>::operator std::string()
const
{
	const auto s(JS::ToString(*cx, *this));
	return s? native(s) : throw type_error("Failed to cast to string");
}

template<lifetime L>
JSType
type(const value<L> &val)
{
	return JS_TypeOfValue(*cx, val);
}

template<lifetime L>
bool
undefined(const value<L> &val)
{
	return type(val) == JSTYPE_VOID;
}

} // namespace basic

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
