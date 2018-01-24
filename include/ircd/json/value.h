/*
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
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
#define HAVE_IRCD_JSON_VALUE_H

namespace ircd::json
{
	struct value;

	using values = std::initializer_list<value>;

	extern const string_view literal_null;
	extern const string_view literal_true;
	extern const string_view literal_false;
	extern const string_view empty_string;
	extern const string_view empty_object;
	extern const string_view empty_array;

	size_t serialized(const bool &);
	size_t serialized(const value *const &begin, const value *const &end);
	size_t serialized(const values &);

	string_view stringify(mutable_buffer &, const value *const &begin, const value *const &end);
}

/// A primitive of the ircd::json system representing a value at runtime.
///
/// This holds state for values apropos a JSON object or array. Value's
/// data can either be in the form of a JSON string or it can be some native
/// machine state. The serial flag indicates the former.
///
/// Value can can hold any of the JSON types in either of these states.
/// This is accomplished with runtime switching and branching but this is
/// still lightweight and without a vtable pointer. The structure is just
/// the size of two pointers like a string_view; we commandeer bits of the
/// second word to hold type, flags, and length information. Thus we can hold
/// large vectors of values at 16 byte alignment and not 24 byte.
///
/// Value is capable of allocation and ownership of its internal data and copy
/// semantics. This is primarily to support recursion and various developer
/// conveniences like nested initializer_list's etc. It is better to
/// std::move() a value than copy it, but the full copy semantic is supported;
/// however, if serial=false then a copy will stringify the data into JSON and
/// the destination will be serial=true,alloc=true; thus copying of complex
/// native values never occurs.
///
/// Take careful note of a quirk with `operator string_view()`: when the
/// value is a STRING type string_view()'ing the value will never show
/// the string with surrounding quotes in view. This is because the value
/// accepts both quoted and unquoted strings as input from the developer, then
/// always serializes correctly; unquoted strings are more natural to work
/// with. This does not apply to other types like OBJECT and array as
/// string_view()'ing those when in a serial state will show surrounding '{'
/// etc.
///
struct ircd::json::value
{
	union // xxx std::variant
	{
		int64_t integer;
		double floating;
		const char *string;
		const struct value *array;
		const struct member *object;
	};

	uint64_t len     : 57;      ///< length indicator
	enum type type   : 3;       ///< json::type indicator
	uint64_t serial  : 1;       ///< only *string is used. type indicates JSON
	uint64_t alloc   : 1;       ///< indicates the pointer for type is owned
	uint64_t floats  : 1;       ///< for NUMBER type, integer or floating

	using create_string_closure = std::function<void (const mutable_buffer &)>;
	void create_string(const size_t &len, const create_string_closure &);

  public:
	bool null() const;          ///< literal null or assets are really null
	bool empty() const;         ///< null() or assets are empty
	bool undefined() const;
	bool operator!() const;     ///< null() or undefined() or empty() or asset Falsy

	operator string_view() const;                ///< NOTE unquote()'s the string value
	explicit operator double() const;
	explicit operator int64_t() const;
	explicit operator std::string() const;       ///< NOTE full stringify() of value

	template<class T> explicit value(const T &specialized);
	value(const string_view &sv, const enum type &);
	template<size_t N> value(const char (&)[N]);
	value(const char *const &s);
	value(const string_view &sv);
	value(const struct member *const &, const size_t &len);
	value(std::unique_ptr<const struct member[]>, const size_t &len); // alloc = true
	value(const struct value *const &, const size_t &len);
	value(std::unique_ptr<const struct value[]>, const size_t &len); // alloc = true
	value(const members &); // alloc = true
	value(const nullptr_t &);
	value();
	value(value &&) noexcept;
	value(const value &);
	value &operator=(value &&) noexcept;
	value &operator=(const value &);
	~value() noexcept;

	friend bool operator==(const value &a, const value &b);
	friend bool operator!=(const value &a, const value &b);
	friend bool operator<=(const value &a, const value &b);
	friend bool operator>=(const value &a, const value &b);
	friend bool operator<(const value &a, const value &b);
	friend bool operator>(const value &a, const value &b);

	friend enum type type(const value &a);
	friend bool defined(const value &);
	friend size_t serialized(const value &);
	friend string_view stringify(mutable_buffer &, const value &);
	friend std::ostream &operator<<(std::ostream &, const value &);
};

namespace ircd::json
{
	template<> value::value(const double &floating);
	template<> value::value(const uint64_t &integer);
	template<> value::value(const int64_t &integer);
	template<> value::value(const float &floating);
	template<> value::value(const uint32_t &integer);
	template<> value::value(const int32_t &integer);
	template<> value::value(const uint16_t &integer);
	template<> value::value(const int16_t &integer);
	template<> value::value(const bool &boolean);
	template<> value::value(const std::string &str);
}

static_assert(sizeof(ircd::json::value) == 16, "");

inline
ircd::json::value::value()
:string{nullptr}
,len{0}
,type{STRING}
,serial{false}
,alloc{false}
,floats{false}
{}

inline
ircd::json::value::value(const string_view &sv,
                         const enum type &type)
:string{sv.data()}
,len{sv.size()}
,type{type}
,serial{type == STRING? surrounds(sv, '"') : true}
,alloc{false}
,floats{false}
{}

inline
ircd::json::value::value(const char *const &s)
:string{s}
,len{strlen(s)}
,type{json::type(s, std::nothrow)}
,serial{type == STRING? surrounds(s, '"') : true}
,alloc{false}
,floats{false}
{}

template<size_t N>
ircd::json::value::value(const char (&str)[N])
:string{str}
,len{strnlen(str, N)}
,type{json::type(str, std::nothrow)}
,serial{type == STRING? surrounds(str, '"') : true}
,alloc{false}
,floats{false}
{}

template<> inline
ircd::json::value::value(const std::string &str)
:value{string_view{str}}
{}

inline
ircd::json::value::value(const string_view &sv)
:value{sv, json::type(sv, std::nothrow)}
{}

template<class T>
ircd::json::value::value(const T &t)
:value{static_cast<string_view>(t)}
{
	static_assert(std::is_base_of<ircd::string_view, T>() ||
	              std::is_convertible<ircd::string_view, T>(), "");
}

inline
ircd::json::value::value(const nullptr_t &)
:value
{
	literal_null, type::LITERAL
}{}

template<> inline
ircd::json::value::value(const bool &boolean)
:value
{
	boolean? literal_true : literal_false, type::LITERAL
}{}

inline
ircd::json::value::value(const struct value *const &array,
                         const size_t &len)
:array{array}
,len{len}
,type{ARRAY}
,serial{false}
,alloc{false}
,floats{false}
{
}

inline
ircd::json::value::value(std::unique_ptr<const struct value[]> array,
                         const size_t &len)
:array{array.get()}
,len{len}
,type{ARRAY}
,serial{false}
,alloc{true}
,floats{false}
{
	array.release();
}

inline
ircd::json::value::value(const struct member *const &object,
                         const size_t &len)
:object{object}
,len{len}
,type{OBJECT}
,serial{false}
,alloc{false}
,floats{false}
{}

inline
ircd::json::value::value(std::unique_ptr<const struct member[]> object,
                         const size_t &len)
:object{object.get()}
,len{len}
,type{OBJECT}
,serial{false}
,alloc{true}
,floats{false}
{
	object.release();
}

template<> inline
ircd::json::value::value(const int64_t &integer)
:integer{integer}
,len{0}
,type{NUMBER}
,serial{false}
,alloc{false}
,floats{false}
{}

template<> inline
ircd::json::value::value(const double &floating)
:floating{floating}
,len{0}
,type{NUMBER}
,serial{false}
,alloc{false}
,floats{true}
{}

template<>
__attribute__((warning("uint64_t narrows to int64_t when used in json::value")))
inline
ircd::json::value::value(const uint64_t &integer)
:value{int64_t(integer)}
{}

template<> inline
ircd::json::value::value(const float &floating)
:value{double(floating)}
{}

template<> inline
ircd::json::value::value(const int32_t &integer)
:value{int64_t(integer)}
{}

template<> inline
ircd::json::value::value(const uint32_t &integer)
:value{int64_t(integer)}
{}

template<> inline
ircd::json::value::value(const int16_t &integer)
:value{int64_t(integer)}
{}

template<> inline
ircd::json::value::value(const uint16_t &integer)
:value{int64_t(integer)}
{}

inline
ircd::json::value::value(value &&other)
noexcept
:integer{other.integer}
,len{other.len}
,type{other.type}
,serial{other.serial}
,alloc{other.alloc}
,floats{other.floats}
{
	other.alloc = false;
}

inline ircd::json::value &
ircd::json::value::operator=(value &&other)
noexcept
{
	this->~value();
	integer = other.integer;
	len = other.len;
	type = other.type;
	serial = other.serial;
	alloc = other.alloc;
	other.alloc = false;
	floats = other.floats;
	return *this;
}

inline bool
ircd::json::defined(const value &a)
{
	return !a.undefined();
}

inline enum ircd::json::type
ircd::json::type(const value &a)
{
	return a.type;
}

inline size_t
ircd::json::serialized(const bool &b)
{
	constexpr const size_t t
	{
		strlen("true")
	};

	constexpr const size_t f
	{
		strlen("false")
	};

	return b? t : f;
}
