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
#define HAVE_IRCD_STRINGOPS_H

//
// Misc string utilities
//
namespace ircd
{
	// Simple case insensitive comparison convenience utils
	struct iless;
	struct igreater;
	struct iequals;

	// Vintage
	size_t strlcpy(const mutable_buffer &dst, const string_view &src);
	size_t strlcat(const mutable_buffer &dst, const string_view &src);
	size_t strlcpy(char *const &dest, const char *const &src, const size_t &bufmax);
	size_t strlcat(char *const &dest, const char *const &src, const size_t &bufmax);
	size_t strlcpy(char *const &dest, const string_view &src, const size_t &bufmax);
	size_t strlcat(char *const &dest, const string_view &src, const size_t &bufmax);

	// wrapper to find(T) != npos
	template<class T> bool has(const string_view &, const T &);

	// return view without any trailing characters contained in c
	string_view rstripa(const string_view &str, const string_view &c);

	// return view without any leading characters contained in c
	string_view lstripa(const string_view &str, const string_view &c);

	// return view without leading occurrences of c
	string_view lstrip(const string_view &str, const char &c = ' ');
	string_view lstrip(string_view str, const string_view &c);

	// return view without trailing occurrences of c
	string_view rstrip(const string_view &str, const char &c = ' ');
	string_view rstrip(string_view str, const string_view &c);

	// return view without leading and trailing occurrences of c
	string_view strip(const string_view &str, const char &c = ' ');
	string_view strip(const string_view &str, const string_view &c);

	// split view on first match of delim; delim not included; if no delim then .second empty
	std::pair<string_view, string_view> split(const string_view &str, const char &delim = ' ');
	std::pair<string_view, string_view> split(const string_view &str, const string_view &delim);

	// split view on last match of delim; delim not included; if no delim then .second empty
	std::pair<string_view, string_view> rsplit(const string_view &str, const char &delim = ' ');
	std::pair<string_view, string_view> rsplit(const string_view &str, const string_view &delim);

	// view between first match of delim a and first match of delim b after it
	string_view between(const string_view &str, const string_view &a, const string_view &b);
	string_view between(const string_view &str, const char &a = '(', const char &b = ')');

	// test string endswith delim; or any of the delims in iterable
	bool endswith(const string_view &str, const string_view &val);
	bool endswith(const string_view &str, const char &val);
	template<class It> bool endswith_any(const string_view &str, const It &begin, const It &end);

	// count occurrences of val at end of string
	size_t endswith_count(const string_view &str, const char &val);

	// test string startswith delim; or any of the delims in iterable
	bool startswith(const string_view &str, const string_view &val);
	bool startswith(const string_view &str, const char &val);
	template<class It> bool startswith_any(const string_view &str, const It &begin, const It &end);

	// count occurrences of val at start of string
	size_t startswith_count(const string_view &str, const char &val);

	// test string is surrounded by val (ex. surrounded by quote characters)
	bool surrounds(const string_view &str, const string_view &val);
	bool surrounds(const string_view &str, const char &val);

	// pop trailing char from view
	char chop(string_view &str);

	// remove trailing from view and return num chars removed
	size_t chomp(string_view &str, const char &c = '\n');
	size_t chomp(string_view &str, const string_view &c);
	template<class T, class delim> size_t chomp(iterators<T>, const delim &d);

	// Convenience to strip quotes
	string_view unquote(const string_view &str);
	std::string unquote(std::string &&);

	std::string replace(std::string, const char &before, const char &after);
	std::string replace(std::string, const string_view &before, const string_view &after);
	std::string replace(const string_view &, const char &before, const string_view &after);

	// Truncate view at maximum length
	string_view trunc(const string_view &, const size_t &max);
}

inline ircd::string_view
ircd::trunc(const string_view &s,
            const size_t &max)
{
	return { s.data(), std::min(s.size(), max) };
}

inline std::string
ircd::replace(std::string s,
              const string_view &before,
              const string_view &after)
{
	size_t p(s.find(data(before), 0, size(before)));
	for(; p != s.npos; p = s.find(data(before), p + size(after), size(before)))
		s.replace(p, size(before), data(after), size(after));

	return s;
}

inline std::string
ircd::replace(std::string s,
              const char &before,
              const char &after)
{
	std::replace(begin(s), end(s), before, after);
	return s;
}

/// Remove quotes on an std::string. Only operates on an rvalue reference so
/// that a copy of the string is not created when no quotes are found, and
/// movements can take place if they are. This overload is not needed often;
/// use string_view.
inline std::string
ircd::unquote(std::string &&str)
{
	if(endswith(string_view{str}, '"'))
		str.pop_back();

	if(startswith(string_view{str}, '"'))
		str = str.substr(1);

	return std::move(str);
}

/// Common convenience to remove quotes around the view of the string
inline ircd::string_view
ircd::unquote(const string_view &str)
{
	return strip(str, '"');
}

/// Chomps delim from all of the string views in the iterable (iterators<T> are
/// the T::iterator pair {begin(t), end(t)} of an iterable T) and returns the
/// total number of characters removed from all operations.
template<class T,
         class delim>
size_t
ircd::chomp(iterators<T> its,
            const delim &d)
{
	return std::accumulate(begin(its), end(its), size_t(0), [&d]
	(auto ret, const auto &s)
	{
		return ret += chomp(s, d);
	});
}

/// Removes all characters from the end of the view starting with the last
/// instance of c. Different from rstrip() in that this will remove more than
/// just the delim from the end; it removes both the delim and everything after
/// it from wherever the last delim may be. Removes nothing if no delim is.
inline size_t
ircd::chomp(string_view &str,
            const char &c)
{
	const auto pos(str.find_last_of(c));
	if(pos == string_view::npos)
		return 0;

	assert(str.size() - pos == 1);
	str = str.substr(0, pos);
	return 1;
}

/// Removes all characters from the end of the view starting with the last
/// instance of c. This matches the entire delim string c to chomp it and
/// everything after it.
inline size_t
ircd::chomp(string_view &str,
            const string_view &c)
{
	const auto pos(str.find_last_of(c));
	if(pos == string_view::npos)
		return 0;

	assert(str.size() - pos == c.size());
	str = str.substr(0, pos);
	return c.size();
}

/// Removes any last character from the view, modifying the view, and returning
/// that character.
inline char
ircd::chop(string_view &str)
{
	return !str.empty()? str.pop_back() : '\0';
}

/// Test if a string starts and ends with character
inline bool
ircd::surrounds(const string_view &str,
                const char &val)
{
	return str.size() >= 2 && str.front() == val && str.back() == val;
}

/// Test if a string starts and ends with a string
inline bool
ircd::surrounds(const string_view &str,
                const string_view &val)
{
	return startswith(str, val) && endswith(str, val);
}

/// Count occurrences of val at end of string
inline size_t
ircd::startswith_count(const string_view &str,
                       const char &v)
{
	const auto pos(str.find_first_not_of(v));
	return pos == string_view::npos? str.size() : str.size() - pos - 1;
}

/// Test if a string starts with any of the values in the iterable
template<class It>
bool
ircd::startswith_any(const string_view &str,
                     const It &begin,
                     const It &end)
{
	return std::any_of(begin, end, [&str](const auto &val)
	{
		return startswith(str, val);
	});
}

/// Test if a string starts with a character
inline bool
ircd::startswith(const string_view &str,
                 const char &val)
{
	return !str.empty() && str[0] == val;
}

/// Test if a string starts with a string
inline bool
ircd::startswith(const string_view &str,
                 const string_view &val)
{
	return !str.empty() && str.substr(0, val.size()) == val;
}

/// Count occurrences of val at end of string
inline size_t
ircd::endswith_count(const string_view &str,
                     const char &v)
{
	const auto pos(str.find_last_not_of(v));
	return pos == string_view::npos? str.size() : str.size() - pos - 1;
}

/// Test if a string ends with any of the values in iterable
template<class It>
bool
ircd::endswith_any(const string_view &str,
                   const It &begin,
                   const It &end)
{
	return std::any_of(begin, end, [&str](const auto &val)
	{
		return endswith(str, val);
	});
}

/// Test if a string ends with character
inline bool
ircd::endswith(const string_view &str,
               const char &val)
{
	return !str.empty() && str[str.size()-1] == val;
}

/// Test if a string ends with a string
inline bool
ircd::endswith(const string_view &str,
               const string_view &val)
{
	const ssize_t off(str.size() - val.size());
	return !str.empty() && off >= 0 && str.substr(off) == val;
}

/// View a string between the first match of a and the first match of b
/// after a.
inline ircd::string_view
ircd::between(const string_view &str,
              const string_view &a,
              const string_view &b)
{
	return split(split(str, a).second, b).first;
}

/// View a string between the first match of a and the first match of b
/// after a.
inline ircd::string_view
ircd::between(const string_view &str,
              const char &a,
              const char &b)
{
	return split(split(str, a).second, b).first;
}

/// Split a string on the last match of delim. Delim not included; no match
/// will return original str in pair.first, pair.second empty.
inline std::pair<ircd::string_view, ircd::string_view>
ircd::rsplit(const string_view &str,
             const string_view &delim)
{
	const auto pos(str.rfind(delim));
	if(pos == string_view::npos) return
	{
		str, string_view{}
	};
	else return
	{
		str.substr(0, pos), str.substr(pos + delim.size())
	};
}

/// Split a string on the last match of delim. Delim not included; no match
/// will return original str in pair.first, pair.second empty.
inline std::pair<ircd::string_view, ircd::string_view>
ircd::rsplit(const string_view &str,
             const char &delim)
{
	const auto pos(str.find_last_of(delim));
	if(pos == string_view::npos) return
	{
		str, string_view{}
	};
	else return
	{
		str.substr(0, pos), str.substr(pos + 1)
	};
}

/// Split a string on the first match of delim. Delim not included; no match
/// will return original str in pair.first, pair.second empty.
inline std::pair<ircd::string_view, ircd::string_view>
ircd::split(const string_view &str,
            const string_view &delim)
{
	const auto pos(str.find(delim));
	if(pos == string_view::npos) return
	{
		str, string_view{}
	};
	else return
	{
		str.substr(0, pos), str.substr(pos + delim.size())
	};
}

/// Split a string on the first match of delim. Delim not included; no match
/// will return original str in pair.first, pair.second empty.
inline std::pair<ircd::string_view, ircd::string_view>
ircd::split(const string_view &str,
            const char &delim)
{
	const auto pos(str.find(delim));
	if(pos == string_view::npos) return
	{
		str, string_view{}
	};
	else return
	{
		str.substr(0, pos), str.substr(pos + 1)
	};
}

/// Remove leading and trailing instances of c from the returned view
inline ircd::string_view
ircd::strip(const string_view &str,
            const string_view &c)
{
	return lstrip(rstrip(str, c), c);
}

/// Remove leading and trailing instances of c from the returned view
inline ircd::string_view
ircd::strip(const string_view &str,
            const char &c)
{
	return lstrip(rstrip(str, c), c);
}

/// Remove trailing instances of c from the returned view
inline ircd::string_view
ircd::rstrip(string_view str,
             const string_view &c)
{
	while(endswith(str, c))
		str = str.substr(0, size(str) - size(c));

	return str;
}

/// Remove trailing instances of c from the returned view
inline ircd::string_view
ircd::rstrip(const string_view &str,
             const char &c)
{
	const auto pos(str.find_last_not_of(c));
	return pos != string_view::npos? string_view{str.substr(0, pos + 1)} : str;
}

/// Remove leading instances of c from the returned view
inline ircd::string_view
ircd::lstrip(string_view str,
             const string_view &c)
{
	while(startswith(str, c))
		str = str.substr(size(c));

	return str;
}

/// Remove leading instances of c from the returned view.
inline ircd::string_view
ircd::lstrip(const string_view &str,
             const char &c)
{
	const auto pos(str.find_first_not_of(c));
	return pos != string_view::npos? string_view{str.substr(pos)}:
	                                 string_view{str.data(), size_t{0}};
}

/// Remove leading instances of any character in c from the returned view
inline ircd::string_view
ircd::lstripa(const string_view &str,
              const string_view &c)
{
	const auto pos(str.find_first_not_of(c));
	return pos != string_view::npos? string_view{str.substr(pos)} : str;
}

/// Remove trailing instances of any character in c from the returned view
inline ircd::string_view
ircd::rstripa(const string_view &str,
              const string_view &c)
{
	const auto pos(str.find_last_not_of(c));
	return pos != string_view::npos? string_view{str.substr(0, pos + 1)} : str;
}

template<class T>
bool
ircd::has(const string_view &s,
          const T &t)
{
	return s.find(t) != s.npos;
}

inline size_t
ircd::strlcpy(const mutable_buffer &dst,
              const string_view &src)
{
	return strlcpy(data(dst), src, size(dst));
}

/// Copy a string to dst will guaranteed null terminated output
inline size_t
ircd::strlcpy(char *const &dst,
              const string_view &src,
              const size_t &max)
{
	if(!max)
		return 0;

	const size_t len
	{
		std::min(src.size(), max - 1)
	};

	memcpy(dst, src.data(), len);
	dst[len] = '\0';
	return len;
}

inline size_t
#ifndef HAVE_STRLCPY
ircd::strlcpy(char *const &dst,
              const char *const &src,
              const size_t &max)
{
	const auto len{strnlen(src, max)};
	return strlcpy(dst, {src, len}, max);
}
#else
ircd::strlcpy(char *const &dst,
              const char *const &src,
              const size_t &max)
{
	return ::strlcpy(dst, src, max);
}
#endif

inline size_t
ircd::strlcat(const mutable_buffer &dst,
              const string_view &src)
{
	return strlcat(data(dst), src, size(dst));
}

/// Append a string to dst will guaranteed null terminated output; Expects
/// dst to have null termination before calling this function.
inline size_t
ircd::strlcat(char *const &dst,
              const string_view &src,
              const size_t &max)
{
	const auto pos{strnlen(dst, max)};
	const auto remain{max - pos};
	strlcpy(dst + pos, src, remain);
	return pos + src.size();
}

inline size_t
#ifndef HAVE_STRLCAT
ircd::strlcat(char *const &dst,
              const char *const &src,
              const size_t &max)
{
	const auto len{strnlen(src, max)};
	return strlcat(dst, {src, len}, max);
}
#else
ircd::strlcat(char *const &dst,
              const char *const &src,
              const size_t &max)
{
	return ::strlcat(dst, src, max);
}
#endif

/// Case insensitive string comparison deciding which string compares 'less'
/// than the other.
struct ircd::iless
{
	using is_transparent = std::true_type;

	bool s;

	operator const bool &() const                { return s;                                       }

	bool operator()(const string_view &a, const string_view &b) const;
	bool operator()(const string_view &a, const std::string &b) const;
	bool operator()(const std::string &a, const string_view &b) const;
	bool operator()(const std::string &a, const std::string &b) const;

	template<class A,
	         class B>
	iless(A&& a, B&& b)
	:s{operator()(std::forward<A>(a), std::forward<B>(b))}
	{}

	iless() = default;
};

inline bool
ircd::iless::operator()(const std::string &a,
                        const std::string &b)
const
{
	return operator()(string_view{a}, string_view{b});
}

inline bool
ircd::iless::operator()(const string_view &a,
                        const std::string &b)
const
{
	return operator()(a, string_view{b});
}

inline bool
ircd::iless::operator()(const std::string &a,
                        const string_view &b)
const
{
	return operator()(string_view{a}, b);
}

inline bool
ircd::iless::operator()(const string_view &a,
                        const string_view &b)
const
{
	return std::lexicographical_compare(begin(a), end(a), begin(b), end(b), []
	(const char &a, const char &b)
	{
		return tolower(a) < tolower(b);
	});
}

/// Case insensitive string comparison deciding if two strings are equal
struct ircd::iequals
{
	using is_transparent = std::true_type;

	bool s;

	operator const bool &() const                { return s;                                       }

	bool operator()(const string_view &a, const string_view &b) const;
	bool operator()(const string_view &a, const std::string &b) const;
	bool operator()(const std::string &a, const string_view &b) const;
	bool operator()(const std::string &a, const std::string &b) const;

	template<class A,
	         class B>
	iequals(A&& a, B&& b)
	:s{operator()(std::forward<A>(a), std::forward<B>(b))}
	{}

	iequals() = default;
};

inline bool
ircd::iequals::operator()(const std::string &a,
                          const std::string &b)
const
{
	return operator()(string_view{a}, string_view{b});
}

inline bool
ircd::iequals::operator()(const string_view &a,
                          const std::string &b)
const
{
	return operator()(a, string_view{b});
}

inline bool
ircd::iequals::operator()(const std::string &a,
                          const string_view &b)
const
{
	return operator()(string_view{a}, b);
}

inline bool
ircd::iequals::operator()(const string_view &a,
                          const string_view &b)
const
{
	return std::equal(begin(a), end(a), begin(b), end(b), []
	(const char &a, const char &b)
	{
		return tolower(a) == tolower(b);
	});
}

/// Case insensitive string comparison deciding which string compares 'greater'
/// than the other.
struct ircd::igreater
{
	using is_transparent = std::true_type;

	bool s;

	operator const bool &() const                { return s;                                       }

	bool operator()(const string_view &a, const string_view &b) const;
	bool operator()(const string_view &a, const std::string &b) const;
	bool operator()(const std::string &a, const string_view &b) const;
	bool operator()(const std::string &a, const std::string &b) const;

	template<class A,
	         class B>
	igreater(A&& a, B&& b)
	:s{operator()(std::forward<A>(a), std::forward<B>(b))}
	{}

	igreater() = default;
};

inline bool
ircd::igreater::operator()(const std::string &a,
                           const std::string &b)
const
{
	return operator()(string_view{a}, string_view{b});
}

inline bool
ircd::igreater::operator()(const string_view &a,
                           const std::string &b)
const
{
	return operator()(a, string_view{b});
}

inline bool
ircd::igreater::operator()(const std::string &a,
                           const string_view &b)
const
{
	return operator()(string_view{a}, b);
}

inline bool
ircd::igreater::operator()(const string_view &a,
                           const string_view &b)
const
{
	return std::lexicographical_compare(begin(a), end(a), begin(b), end(b), []
	(const char &a, const char &b)
	{
		return tolower(a) > tolower(b);
	});
}
