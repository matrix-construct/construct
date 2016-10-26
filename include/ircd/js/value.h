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

struct value
:JS::Rooted<JS::Value>
{
	using handle = JS::HandleValue;
	using handle_mutable = JS::MutableHandleValue;

	explicit operator std::string() const;
	explicit operator double() const;
	explicit operator uint64_t() const;
	explicit operator int64_t() const;
	explicit operator uint32_t() const;
	explicit operator int32_t() const;
	explicit operator uint16_t() const;
	explicit operator bool() const;

	explicit value(const std::string &);
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

	template<class T> value(const JS::Handle<T> &h);
	template<class T> value(const JS::MutableHandle<T> &h);
	template<class T> value(const JS::Rooted<T> &r);
	template<class T> value(const JS::PersistentRooted<T> &p);

	value();
	value(value &&) noexcept;
	value(const value &) = delete;
	value &operator=(value &&) noexcept;

	friend JSType type(const value &);
	friend bool undefined(const value &);
};

inline
value::value()
:JS::Rooted<JS::Value>{*cx, JS::UndefinedValue()}
{
}

inline
value::value(value &&other)
noexcept
:JS::Rooted<JS::Value>{*cx, other.get()}
{
	other.set(JS::UndefinedValue());
}

inline value &
value::operator=(value &&other)
noexcept
{
	set(other.get());
	other.set(JS::UndefinedValue());
	return *this;
}

template<class T>
value::value(const JS::PersistentRooted<T> &r)
:value(JS::Handle<T>(r))
{
}

template<class T>
value::value(const JS::Rooted<T> &r)
:value(JS::Handle<T>(r))
{
}

template<class T>
value::value(const JS::MutableHandle<T> &h)
:value(h.get())
{
}

template<class T>
value::value(const JS::Handle<T> &h)
:value(h.get())
{
}

inline
value::value(const JS::Value &val)
:JS::Rooted<JS::Value>{*cx, val}
{
}

inline
value::value(JS::Symbol *const &val)
:JS::Rooted<JS::Value>{*cx, JS::SymbolValue(val)}
{
}

inline
value::value(JSObject *const &val)
:JS::Rooted<JS::Value>
{
	*cx,
	val? JS::ObjectValue(*val) : throw internal_error("NULL JSObject")
}
{
}

inline
value::value(JSObject &val)
:JS::Rooted<JS::Value>{*cx, JS::ObjectValue(val)}
{
}

inline
value::value(JSString *const &val)
:JS::Rooted<JS::Value>{*cx, JS::StringValue(val)}
{
}

inline
value::value(JSFunction *const &val)
:JS::Rooted<JS::Value>{*cx}
{
	auto *const obj(JS_GetFunctionObject(val));
	if(unlikely(!obj))
		throw type_error("Function cannot convert to Object");

	set(JS::ObjectValue(*obj));
}

inline
value::value(const jsid &val)
:JS::Rooted<JS::Value>{*cx}
{
	if(!JS_IdToValue(*cx, val, &(*this)))
		throw type_error("Failed to construct value from Id");
}

inline
value::value(const bool &b)
:JS::Rooted<JS::Value>{*cx, JS::BooleanValue(b)}
{
}

inline
value::value(const int32_t &val)
:JS::Rooted<JS::Value>{*cx, JS::Int32Value(val)}
{
}

inline
value::value(const float &val)
:JS::Rooted<JS::Value>{*cx, JS::Float32Value(val)}
{
}

inline
value::value(const double &val)
:JS::Rooted<JS::Value>{*cx, JS::DoubleValue(val)}
{
}

inline
value::value(const nullptr_t &)
:JS::Rooted<JS::Value>{*cx, JS::NullValue()}
{
}

inline
value::value(const std::string &s)
:JS::Rooted<JS::Value>
{
	*cx, [&s]
	{
		auto buf(native_external_copy(s));
		const auto ret(JS_NewExternalString(*cx, buf.release(), s.size(), &native_external_delete));
		return JS::StringValue(ret);
	}()
}
{
}

inline
value::value(const char *const &s)
:JS::Rooted<JS::Value>
{
	*cx, !s? JS::NullValue() : [&s]
	{
		const auto len(strlen(s));
		auto buf(native_external_copy(s, len));
		const auto ret(JS_NewExternalString(*cx, buf.release(), len, &native_external_delete));
		return JS::StringValue(ret);
	}()
}
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

inline JSType
type(const value &val)
{
	return JS_TypeOfValue(*cx, val);
}

inline bool
undefined(const value &val)
{
	return type(val) == JSTYPE_VOID;
}

} // namespace js
} // namespace ircd
