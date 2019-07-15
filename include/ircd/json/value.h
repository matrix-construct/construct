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
#define HAVE_IRCD_JSON_VALUE_H

namespace ircd::json
{
	struct value;

	using values = std::initializer_list<value>;

	enum type type(const value &a);
	bool defined(const value &);

	size_t serialized(const bool &);
	size_t serialized(const value &);
	size_t serialized(const value *const &begin, const value *const &end);
	size_t serialized(const values &);

	string_view stringify(mutable_buffer &, const value &);
	string_view stringify(mutable_buffer &, const value *const &begin, const value *const &end);
	std::ostream &operator<<(std::ostream &, const value &);

	bool operator==(const value &a, const value &b);
	bool operator!=(const value &a, const value &b);
	bool operator<=(const value &a, const value &b);
	bool operator>=(const value &a, const value &b);
	bool operator<(const value &a, const value &b);
	bool operator>(const value &a, const value &b);
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
	static const size_t max_string_size;

	union
	{
		int64_t integer;
		double floating;
		const char *string;
		const struct value *array;
		const struct member *object;
	};

	uint64_t len     : 57;      ///< length indicator
	uint64_t type    : 3;       ///< json::type indicator
	uint64_t serial  : 1;       ///< only *string is used. type indicates JSON
	uint64_t alloc   : 1;       ///< indicates the pointer for type is owned
	uint64_t floats  : 1;       ///< for NUMBER type, integer or floating

	using create_string_closure = std::function<void (mutable_buffer &)>;
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

	value(const members &); // alloc = true
	value(const struct member *const &, const size_t &len);
	value(std::unique_ptr<const struct member[]> &&, const size_t &len); // alloc = true
	value(const struct value *const &, const size_t &len);
	value(std::unique_ptr<const struct value[]> &&, const size_t &len); // alloc = true
	template<size_t N> value(const char (&)[N], const enum type &);
	template<size_t N> value(const char (&)[N]);
	explicit value(const std::string &, const enum type &);
	explicit value(const std::string &);
	value(const string_view &sv, const enum type &);
	value(const string_view &sv);
	value(const char *const &, const enum type &);
	value(const char *const &s);
	explicit value(const double &);
	explicit value(const int64_t &);
	explicit value(const int32_t &);
	explicit value(const uint32_t &);
	explicit value(const int16_t &);
	explicit value(const uint16_t &);
	explicit value(const int8_t &);
	explicit value(const uint8_t &);
	explicit value(const bool &);
	value(const json::object &);
	value(const json::array &);
	value(const nullptr_t &);
	value();
	value(value &&) noexcept;
	value(const value &);
	value &operator=(value &&) noexcept;
	value &operator=(const value &);
	~value() noexcept;
};

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
ircd::json::value::value(const nullptr_t &)
:value
{
	literal_null, type::LITERAL
}
{
}

inline
ircd::json::value::value(const json::object &sv)
:value{sv, OBJECT}
{
}

inline
ircd::json::value::value(const json::array &sv)
:value{sv, ARRAY}
{
}

inline
ircd::json::value::value(const bool &boolean)
:value
{
	boolean? literal_true : literal_false, type::LITERAL
}
{
}

inline
ircd::json::value::value(const uint8_t &integer)
:value{int64_t{integer}}
{
}

inline
ircd::json::value::value(const int8_t &integer)
:value{int64_t{integer}}
{
}

inline
ircd::json::value::value(const uint16_t &integer)
:value{int64_t{integer}}
{
}

inline
ircd::json::value::value(const int16_t &integer)
:value{int64_t{integer}}
{
}

inline
ircd::json::value::value(const uint32_t &integer)
:value{int64_t{integer}}
{
}

inline
ircd::json::value::value(const int32_t &integer)
:value{int64_t{integer}}
{
}

inline
ircd::json::value::value(const int64_t &integer)
:integer{integer}
,len{0}
,type{NUMBER}
,serial{false}
,alloc{false}
,floats{false}
{
}

inline
ircd::json::value::value(const double &floating)
:floating{floating}
,len{0}
,type{NUMBER}
,serial{false}
,alloc{false}
,floats{true}
{
}

inline
ircd::json::value::value(const char *const &str)
:value{string_view{str}}
{
}

inline
ircd::json::value::value(const char *const &str,
                         const enum type &type)
:value{string_view{str}, type}
{
}

inline
ircd::json::value::value(const string_view &sv)
:value{sv, json::type(sv, strict, std::nothrow)}
{
}

inline
ircd::json::value::value(const string_view &sv,
                         const enum type &type)
:string{sv.data()}
,len{sv.size()}
,type{type}
,serial{type == STRING? surrounds(sv, '"') : true}
,alloc{false}
,floats{false}
{
}

inline
ircd::json::value::value(const std::string &s)
:value{s, json::type(s, strict, std::nothrow)}
{
}

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
ircd::json::value::value(std::unique_ptr<const struct value[]> &&array,
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
ircd::json::value::value(std::unique_ptr<const struct member[]> &&object,
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

template<size_t N>
ircd::json::value::value(const char (&str)[N])
:value{string_view{str, strnlen(str, N)}}
{}

template<size_t N>
ircd::json::value::value(const char (&str)[N],
                         const enum type &type)
:value{string_view{str, strnlen(str, N)}, type}
{}
