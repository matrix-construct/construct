// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <boost/tokenizer.hpp>

ircd::string_view
ircd::tokens_before(const string_view &str,
                    const char &sep,
                    const size_t &i)
{
	const char ssep[2] { sep, '\0' };
	return tokens_before(str, ssep, i);
}

ircd::string_view
ircd::tokens_before(const string_view &str,
                    const char *const &sep,
                    const size_t &i)
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = boost::char_separator<char>;

	const delim d{sep};
	const boost::tokenizer<delim, iter, type> view
	{
		str, d
	};

	string_view ret;
	auto it(begin(view));
	for(size_t j(0); it != end(view) && j < i; ++it, j++)
		ret = { begin(view)->data(), it->data() + it->size() };

	return ret;
}

ircd::string_view
ircd::tokens_after(const string_view &str,
                   const char &sep,
                   const size_t &i)
{
	const char ssep[2] { sep, '\0' };
	return tokens_after(str, ssep, i);
}

ircd::string_view
ircd::tokens_after(const string_view &str,
                   const char *const &sep,
                   const size_t &i)
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = boost::char_separator<char>;

	const delim d{sep};
	const boost::tokenizer<delim, iter, type> view
	{
		str, d
	};

	auto it(begin(view));
	for(size_t j(0); it != end(view); ++it, j++)
		if(j > i)
			return string_view{it->data(), str.data() + str.size()};

	return {};
}

ircd::string_view
ircd::token_first(const string_view &str,
                  const char &sep)
{
	const char ssep[2] { sep, '\0' };
	return token(str, ssep, 0);
}

ircd::string_view
ircd::token_first(const string_view &str,
                  const char *const &sep)
{
	return token(str, sep, 0);
}

ircd::string_view
ircd::token_last(const string_view &str,
                 const char &sep)
{
	const char ssep[2] { sep, '\0' };
	return token_last(str, ssep);
}

ircd::string_view
ircd::token_last(const string_view &str,
                 const char *const &sep)
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = boost::char_separator<char>;

	const delim d{sep};
	const boost::tokenizer<delim, iter, type> view
	{
		str, d
	};

	auto it(begin(view));
	if(it == end(view))
		return str.empty()? str : throw std::out_of_range("token out of range");

	string_view ret(*it);
	for(++it; it != end(view); ++it)
		ret = *it;

	return ret;
}

ircd::string_view
ircd::token(const string_view &str,
            const char &sep,
            const size_t &i,
            const string_view &def)
{
	const char ssep[2] { sep, '\0' };
	return token(str, ssep, i, def);
}

ircd::string_view
ircd::token(const string_view &str,
            const char *const &sep,
            const size_t &i,
            const string_view &def)
try
{
	return token(str, sep, i);
}
catch(const std::out_of_range &)
{
	return def;
}

ircd::string_view
ircd::token(const string_view &str,
            const char &sep,
            const size_t &i)
{
	const char ssep[2] { sep, '\0' };
	return token(str, ssep, i);
}

ircd::string_view
ircd::token(const string_view &str,
            const char *const &sep,
            const size_t &i)
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = boost::char_separator<char>;

	const delim d{sep};
	const boost::tokenizer<delim, iter, type> view
	{
		str, d
	};

	return *at(begin(view), end(view), i);
}

size_t
ircd::token_count(const string_view &str,
                  const char &sep)
{
	const char ssep[2] { sep, '\0' };
	return token_count(str, ssep);
}

size_t
ircd::token_count(const string_view &str,
                  const char *const &sep)
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = boost::char_separator<char>;

	const delim d{sep};
	const boost::tokenizer<delim, iter, type> view
	{
		str, d
	};

	return std::distance(begin(view), end(view));
}

size_t
ircd::tokens(const string_view &str,
             const char &sep,
             const mutable_buffer &buf,
             const token_view &closure)
{
	const char ssep[2] { sep, '\0' };
	return tokens(str, ssep, buf, closure);
}

size_t
ircd::tokens(const string_view &str,
             const char *const &sep,
             const mutable_buffer &buf,
             const token_view &closure)
{
	char *ptr(data(buf));
	char *const stop(data(buf) + size(buf));
	tokens(str, sep, [&closure, &ptr, &stop]
	(const string_view &token)
	{
		const size_t terminated_size(token.size() + 1);
		const size_t remaining(std::distance(ptr, stop));
		if(remaining < terminated_size)
			return;

		char *const dest(ptr);
		ptr += strlcpy(dest, token.data(), terminated_size);
		closure(string_view(dest, token.size()));
	});

	return std::distance(data(buf), ptr);
}

size_t
ircd::tokens(const string_view &str,
             const char &sep,
             const size_t &limit,
             const token_view &closure)
{
	const char ssep[2] { sep, '\0' };
	return tokens(str, ssep, limit, closure);
}

size_t
ircd::tokens(const string_view &str,
             const char *const &sep,
             const size_t &limit,
             const token_view &closure)
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = boost::char_separator<char>;

	const delim d{sep};
	const boost::tokenizer<delim, iter, type> view
	{
		str, d
	};

	size_t i(0);
	for(auto it(begin(view)); i < limit && it != end(view); ++it, i++)
		closure(*it);

	return i;
}

void
ircd::tokens(const string_view &str,
             const char &sep,
             const token_view &closure)
{
	const char ssep[2] { sep, '\0' };
	tokens(str, ssep, closure);
}

void
ircd::tokens(const string_view &str,
             const char *const &sep,
             const token_view &closure)
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = boost::char_separator<char>;

	const delim d{sep};
	const boost::tokenizer<delim, iter, type> view
	{
		str, d
	};

	std::for_each(begin(view), end(view), closure);
}
