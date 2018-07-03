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
#define HAVE_IRCD_JS_STRING_H

namespace ircd  {
namespace js    {

// Fundamental utils
bool latin1(const JSString *const &);
bool external(const JSString *const &);
size_t size(const JSString *const &);
char16_t at(const JSString *const &, const size_t &);

// Direct access to the string data via a pointer within a protective closure.
using string16_closure = std::function<void (const char16_t *const &, const size_t &)>;
using string8_closure = std::function<void (const char *const &, const size_t &)>;
void observe16(const JSString *const &, const string16_closure &);
void observe8(const JSString *const &, const string8_closure &);
void observe(const JSString *const &, const std::pair<string8_closure, string16_closure> &);

// Convert to native and copy into circular buffer.
const size_t CSTR_BUFS = 8;
const size_t CSTR_BUFSIZE = 1024;
char *c_str(const JSString *const &);

struct string
:root<JSString *>
{
	IRCD_OVERLOAD(pinned)
	IRCD_OVERLOAD(literal)

	char *c_str() const;                         // Copy into rotating buf
	size_t native_size() const;
	size_t size() const;
	bool empty() const;
	char16_t operator[](const size_t &at) const;

	operator std::string() const;
	operator std::u16string() const;
	operator JS::Value() const;
	operator value() const;

	using root<JSString *>::root;
	string(pinned_t, const char16_t *const &);
	string(pinned_t, const char *const &);
	string(pinned_t, const string &);
	string(literal_t, const char16_t *const &);
	string(const char16_t *const &, const size_t &len);
	string(const char16_t *const &);
	string(const std::u16string &);
	string(const char *const &, const size_t &len);
	string(const std::string &);
	string(const char *const &);
	string(const value &);
	string(JSString *const &);
	string(JSString &);
	string();

	struct less
	{
		using is_transparent = std::true_type;
		template<class A, class B> bool operator()(const A &, const B &) const;
	};

	friend std::ostream & operator<<(std::ostream &os, const string &s);
};

template<class T> constexpr bool is_string();
template<class A, class B> constexpr bool string_argument();

auto hash(const string &s);

int cmp(const string &a, const string &b);
int cmp(const char *const &a, const string &b);
int cmp(const string &a, const char *const &b);
int cmp(const string &a, const std::string &b);
int cmp(const std::string &a, const string &b);
bool operator==(const string &a, const char *const &b);
bool operator==(const char *const &a, const string &b);

template<class A,
         class B>
using string_comparison = typename std::enable_if<string_argument<A, B>(), bool>::type;
template<class A, class B> string_comparison<A, B> operator==(const A &a, const B &b);
template<class A, class B> string_comparison<A, B> operator!=(const A &a, const B &b);
template<class A, class B> string_comparison<A, B> operator>(const A &a, const B &b);
template<class A, class B> string_comparison<A, B> operator<(const A &a, const B &b);
template<class A, class B> string_comparison<A, B> operator>=(const A &a, const B &b);
template<class A, class B> string_comparison<A, B> operator<=(const A &a, const B &b);
template<class A, class B> string_comparison<A, B> operator==(const A &a, const B &b);
template<class A, class B> string_comparison<A, B> operator!=(const A &a, const B &b);

using string_pair = std::pair<string, string>;
string_pair splita(const string &s, const char16_t &c);
string_pair splita(const string &s, const char &c);        // split() but skips multiple contiguous c
string_pair split(const string &s, const char16_t &c);     // split on first position of c
string_pair split(const string &s, const char &c);
string substr(const string &s, const size_t &pos, const size_t &len);
string operator+(const string &left, const string &right);
string &operator+=(string &left, const string &right);

using string_closure = std::function<void (const string &)>;
void tokens(const string &, const char &sep, const string_closure &);

inline
string::string()
:string::root::type
{
	JS_GetEmptyString(*cx)
}
{
}

inline
string::string(JSString &val)
:string::root::type{&val}
{
}

inline
string::string(JSString *const &val)
:string::root::type
{
	likely(val)? val : throw internal_error("NULL string")
}
{
}

inline
string::string(const value &val)
:string::root::type
{
	JS::ToString(*cx, val)?: throw type_error("Failed to convert value to string")
}
{
}

inline
string::string(const std::string &s)
:string(s.data(), s.size())
{
}

inline
string::string(const char *const &s)
:string(s, strlen(s))
{
}

inline
string::string(const char *const &s,
               const size_t &len)
:string::root::type{[&s, &len]
{
	if(!s || !*s)
		return JS_GetEmptyString(*cx);

	auto buf(native_external_copy(s, len));
	return JS_NewExternalString(*cx, buf.release(), len, &native_external_delete);
}()}
{
	if(unlikely(!this->get()))
		throw type_error("Failed to construct string from character array");
}

inline
string::string(const std::u16string &s)
:string(s.data(), s.size())
{
}

inline
string::string(const char16_t *const &s)
:string(s, std::char_traits<char16_t>::length(s))
{
}

inline
string::string(const char16_t *const &s,
               const size_t &len)
:string::root::type{[&s, &len]
{
	if(!s || !*s)
		return JS_GetEmptyString(*cx);

	// JS_NewExternalString does not require a null terminated buffer, but we are going
	// to terminate anyway in case the deleter ever wants to iterate a canonical vector.
	auto buf(std::make_unique<char16_t[]>(len+1));
	memcpy(buf.get(), s, len * 2);
	buf.get()[len] = char16_t(0);
	return JS_NewExternalString(*cx, buf.release(), len, &native_external_delete);
}()}
{
	if(unlikely(!this->get()))
		throw type_error("Failed to construct string from character array");
}

inline
string::string(literal_t,
               const char16_t *const &s)
:string::root::type
{
	s && *s?
	JS_NewExternalString(*cx, s, std::char_traits<char16_t>::length(s), &native_external_static):
	JS_GetEmptyString(*cx)
}
{
	if(unlikely(!this->get()))
		throw type_error("Failed to construct string from wide character literal");
}

inline
string::string(pinned_t,
               const string &s)
:string::root::type
{
	JS_AtomizeAndPinJSString(*cx, s)
}
{
	if(unlikely(!this->get()))
		throw type_error("Failed to intern JSString");
}

inline
string::string(pinned_t,
               const char *const &s)
:string::root::type
{
	JS_AtomizeAndPinStringN(*cx, s, strlen(s))
}
{
	if(unlikely(!this->get()))
		throw type_error("Failed to construct pinned string from character array");
}

inline
string::string(pinned_t,
               const char16_t *const &s)
:string::root::type
{
	JS_AtomizeAndPinUCStringN(*cx, s, std::char_traits<char16_t>::length(s))
}
{
	if(unlikely(!this->get()))
		throw type_error("Failed to construct pinned string from wide character array");
}

inline char16_t
string::operator[](const size_t &pos)
const
{
	return at(this->get(), pos);
}

inline
string::operator value()
const
{
	return static_cast<JS::Value>(*this);
}

inline
string::operator JS::Value()
const
{
	return JS::StringValue(this->get());
}

inline
string::operator std::string()
const
{
	return native(this->get());
}

inline
string::operator std::u16string()
const
{
	return locale::char16::conv(native(this->get()));
}

inline
char *
string::c_str()
const
{
	return js::c_str(this->get());
}

inline bool
string::empty()
const
{
	return size() == 0;
}

inline size_t
string::size()
const
{
	return js::size(this->get());
}

inline size_t
string::native_size()
const
{
	return js::native_size(this->get());
}

template<class A,
         class B>
bool
string::less::operator()(const A &a, const B &b)
const
{
	return cmp(a, b) < 0;
}

inline std::ostream &
operator<<(std::ostream &os, const string &s)
{
	os << std::string(s);
	return os;
}

inline void
tokens(const string &str,
       const char &sep,
       const string_closure &closure)
{
	for(auto pair(splita(str, sep));; pair = splita(pair.second, sep))
	{
		closure(pair.first);
		if(pair.second.empty())
			break;
	}
}

inline std::pair<string, string>
split(const string &s,
      const char &c)
{
	return split(s, char16_t(c));
}

inline std::pair<string, string>
splita(const string &s,
       const char &c)
{
	return splita(s, char16_t(c));
}

inline std::pair<string, string>
split(const string &s,
      const char16_t &c)
{
	size_t a(0);
	for(; a < size(s) && at(s, a) != c; ++a);

	return
	{
		substr(s, 0, a),
		a + 1 < size(s)? substr(s, a + 1, size(s) - a) : string()
	};
}

inline std::pair<string, string>
splita(const string &s,
       const char16_t &c)
{
	size_t a(0);
	for(; a < size(s) && at(s, a) != c; ++a);

	size_t b(a);
	for(; b < size(s) && at(s, b) == c; ++b);

	return
	{
		substr(s, 0, a),
		b < size(s)? substr(s, b, size(s) - b) : string()
	};
}

inline string
substr(const string &s,
       const size_t &pos,
       const size_t &len)
{
	const auto _len(len == size_t(-1)? size(s) - pos : len);
	const auto ret(JS_NewDependentString(*cx, s, pos, _len));
	if(!ret)
		throw std::out_of_range("substr(): invalid arguments");

	return ret;
}

inline string &
operator+=(string &left,
           const string &right)
{
	left = operator+(left, right);
	return left;
}

inline string
operator+(const string &left,
          const string &right)
{
	return JS_ConcatStrings(*cx, left, right);
}

template<class A,
         class B>
string_comparison<A, B>
operator>(const A &a, const B &b)
{
	return cmp(a, b) > 0;
}

template<class A,
         class B>
string_comparison<A, B>
operator<(const A &a, const B &b)
{
	return cmp(a, b) < 0;
}

template<class A,
         class B>
string_comparison<A, B>
operator>=(const A &a, const B &b)
{
	return cmp(a, b) >= 0;
}

template<class A,
         class B>
string_comparison<A, B>
operator<=(const A &a, const B &b)
{
	return cmp(a, b) <= 0;
}

template<class A,
         class B>
string_comparison<A, B>
operator==(const A &a, const B &b)
{
	return cmp(a, b) == 0;
}

template<class A,
         class B>
string_comparison<A, B>
operator!=(const A &a, const B &b)
{
	return !(operator==(a, b));
}

inline bool
operator==(const string &a, const char *const &b)
{
	bool ret;
	if(unlikely(!JS_StringEqualsAscii(*cx, a, b, &ret)))
		throw internal_error("Failed to compare string to native");

	return ret;
}

inline bool
operator==(const char *const &a, const string &b)
{
	bool ret;
	if(unlikely(!JS_StringEqualsAscii(*cx, b, a, &ret)))
		throw internal_error("Failed to compare string to native");

	return ret;
}

inline int
cmp(const string &a,
    const std::string &b)
{
	return cmp(a, b.c_str());
}

inline int
cmp(const std::string &a,
    const string &b)
{
	return cmp(a.c_str(), b);
}

inline int
cmp(const string &a,
    const char *const &b)
{
	return cmp(a, string(b));
}

inline int
cmp(const char *const &a,
    const string &b)
{
	return cmp(string(a), b);
}

inline int
cmp(const string &a,
    const string &b)
{
	int32_t ret;
	if(unlikely(!JS_CompareStrings(*cx, a, b, &ret)))
		throw internal_error("Failed to compare strings");

	return ret;
}

inline auto
hash(const string &s)
{
	//TODO: optimize
	return ircd::hash(std::u16string(s));
}

template<class A,
         class B>
constexpr bool
string_argument()
{
	return is_string<A>() || is_string<B>();
}

template<class T>
constexpr bool
is_string()
{
	return std::is_base_of<string, T>();
}

inline void
observe(const JSString *const &str,
        const std::pair<string8_closure, string16_closure> &closure)
{
	if(latin1(str))
		observe8(str, closure.first);
	else
		observe16(str, closure.second);
}

inline void
observe8(const JSString *const &str,
         const string8_closure &closure)
{
	JS::AutoCheckCannotGC ngc;

	size_t length;
	const auto ptr(JS_GetLatin1StringCharsAndLength(*cx, ngc, const_cast<JSString *>(str), &length));
	closure(reinterpret_cast<const char *>(ptr), length);
}

inline void
observe16(const JSString *const &str,
          const string16_closure &closure)
{
	JS::AutoCheckCannotGC ngc;

	size_t length;
	const auto ptr(JS_GetTwoByteStringCharsAndLength(*cx, ngc, const_cast<JSString *>(str), &length));
	closure(ptr, length);
}

inline char16_t
at(const JSString *const &s,
   const size_t &pos)
{
	char16_t ret;
	if(unlikely(!JS_GetStringCharAt(*cx, const_cast<JSString *>(s), pos, &ret)))
		throw range_error("index %zu is out of range", pos);

	return ret;
}

inline size_t
size(const JSString *const &s)
{
	return JS_GetStringLength(const_cast<JSString *>(s));
}

inline bool
external(const JSString *const &s)
{
	return JS_IsExternalString(const_cast<JSString *>(s));
}

inline bool
latin1(const JSString *const &s)
{
	return JS_StringHasLatin1Chars(const_cast<JSString *>(s));
}

} // namespace js
} // namespace ircd
