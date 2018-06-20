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
#define HAVE_IRCD_STRING_VIEW_H

namespace ircd
{
	struct string_view;

	constexpr const char *data(const string_view &);
	constexpr size_t size(const string_view &);
	bool empty(const string_view &);
	bool operator!(const string_view &);
	bool defined(const string_view &);
	bool null(const string_view &);

	constexpr string_view operator ""_sv(const char *const literal, const size_t size);
}

namespace std
{
	template<> struct std::hash<ircd::string_view>;
	template<> struct std::less<ircd::string_view>;
	template<> struct std::equal_to<ircd::string_view>;
}

/// Customized std::string_view (experimental TS / C++17)
///
/// This class adds iterator-based (char*, char*) construction to std::string_view which otherwise
/// takes traditional (char*, size_t) arguments. This allows boost::spirit grammars to create
/// string_view's using the raw[] directive achieving zero-copy/zero-allocation parsing.
///
struct ircd::string_view
:std::string_view
{
	// (non-standard)
	explicit operator bool() const
	{
		return !empty();
	}

	/// (non-standard) When data() != nullptr we consider the string defined
	/// downstream in this project wrt JS/JSON. This is the bit of information
	/// we're deciding on for defined|undefined. If this string_view is
	/// constructed from a literal "" we must assert that inputs a valid pointer
	/// in the std::string_view with length 0; stdlib can't optimize that with
	/// a nullptr replacement.
	bool undefined() const
	{
		return data() == nullptr;
	}

	bool defined() const
	{
		return !undefined();
	}

	/// (non-standard) string_view's have no guarantee to be null terminated
	/// and most likely aren't. The std::string_view does not offer the
	/// c_str() function because using it is overwhelmingly likely to be wrong.
	/// Nevertheless if our developer is certain their view is of a null
	/// terminated string where the terminator is one past the end they can
	/// invoke this function rather than data() to assert their intent. Note
	/// that this assertion is still not foolproof because reading beyond
	/// size() might still be incorrect whether or not a null is found there
	/// and there is nothing else we can do. The developer must be sure.
	auto c_str() const
	{
		assert(!data() || data()[size()] == '\0');
		return data();
	}

	/// (non-standard) After using data() == nullptr for undefined, we're fresh
	/// out of legitimate bits here to represent the null type string. In this
	/// case we expect a hack pointer of 0x1 which will mean JS null
	bool null() const
	{
		return data() == reinterpret_cast<const char *>(0x1);
	}

	// (non-standard) our faux insert stub
	// Tricks boost::spirit into thinking this is mutable string (hint: it's not).
	// Instead, the raw[] directive in Qi grammar will use the iterator constructor only.
	// __attribute__((error("string_view is not insertable (hint: use raw[] directive)")))
	void insert(const iterator &, const char &)
	{
		assert(0);
	}

	// (non-standard) our iterator-based assign
	string_view &assign(const char *const &begin, const char *const &end)
	{
		this->~string_view();
		new (this) string_view{begin, size_t(std::distance(begin, end))};
		return *this;
	}

	// (non-standard) intuitive wrapper for remove_suffix.
	// Unlike std::string, we can cheaply involve a reference to the removed character
	// which still exists.
	const char &pop_back()
	{
		const char &ret(back());
		remove_suffix(1);
		return ret;
	}

	// (non-standard) intuitive wrapper for remove_prefix.
	// Unlike std::string, we can cheaply involve a reference to the removed character
	// which still exists.
	const char &pop_front()
	{
		const char &ret(front());
		remove_prefix(1);
		return ret;
	}

	/// (non-standard) resize viewer
	void resize(const size_t &count)
	{
		*this = string_view{data(), data() + count};
	}

	// (non-standard) our iterator-based constructor
	string_view(const char *const &begin, const char *const &end)
	:std::string_view{begin, size_t(std::distance(begin, end))}
	{}

	// (non-standard) our iterator-based constructor
	string_view(const std::string::const_iterator &begin, const std::string::const_iterator &end)
	:string_view{&*begin, &*end}
	{}

	// (non-standard) our array based constructor
	template<size_t SIZE>
	constexpr string_view(const std::array<char, SIZE> &array)
	:string_view
	{
		array.data(), std::find(array.begin(), array.end(), '\0')
	}{}

	// (non-standard) our buffer based constructor
	template<size_t SIZE>
	constexpr string_view(const char (&buf)[SIZE])
	:string_view
	{
		buf, std::find(buf, buf + SIZE, '\0')
	}{}

	// Required due to current instability in stdlib
//	string_view(const std::experimental::string_view &esv)
//	:std::string_view{esv}
//	{}

	// Required due to instability in stdlib
//	constexpr string_view(const std::experimental::fundamentals_v1::basic_string_view<char> &bsv)
//	:std::string_view{bsv}
//	{}

//	constexpr string_view(const char *const &start, const size_t &size)
//	:std::string_view{start, size}
//	{}

	explicit string_view(const std::string &string)
	:std::string_view{string.data(), string.size()}
	{}

	constexpr string_view(const std::string_view &sv)
	:std::string_view{sv}
	{}

	/// Our default constructor sets the elements to 0 for best behavior by
	/// defined() and null() et al.
	constexpr string_view()
	:std::string_view{}
	{}

	using std::string_view::string_view;
};

/// Specialization for std::hash<> participation
template<>
struct std::hash<ircd::string_view>
:std::hash<std::string_view>
{
	using std::hash<std::string_view>::operator();
	using std::hash<std::string_view>::hash;
};

/// Specialization for std::less<> participation
template<>
struct std::less<ircd::string_view>
:std::less<std::string_view>
{
	using std::less<std::string_view>::operator();
	using std::less<std::string_view>::less;
};

/// Specialization for std::equal_to<> participation
template<>
struct std::equal_to<ircd::string_view>
:std::equal_to<std::string_view>
{
	using std::equal_to<std::string_view>::operator();
	using std::equal_to<std::string_view>::equal_to;
};

/// Compile-time conversion from a string literal into a string_view.
constexpr ircd::string_view
ircd::operator ""_sv(const char *const literal, const size_t size)
{
	return string_view{literal, size};
}

inline bool
ircd::operator!(const string_view &str)
{
	return empty(str);
}

inline bool
ircd::empty(const string_view &str)
{
	return str.empty();
}

inline bool
ircd::null(const string_view &str)
{
	return str.null();
}

inline bool
ircd::defined(const string_view &str)
{
	return str.defined();
}

constexpr size_t
ircd::size(const string_view &str)
{
	return str.size();
}

constexpr const char *
ircd::data(const string_view &str)
{
	return str.data();
}
