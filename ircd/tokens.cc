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

namespace ircd
{
	struct string_separator;
	struct char_separator;
};

struct [[gnu::visibility("internal")]]
ircd::string_separator
{
	const string_view &delim;

	template<class iterator,
	         class token>
	bool operator()(iterator &next, iterator end, token &ret) const noexcept;
	void reset() noexcept;

	string_separator(const string_view &delim) noexcept;
};

struct [[gnu::visibility("internal")]]
ircd::char_separator
{
	const char &delim;

	template<class iterator,
	         class token>
	bool operator()(iterator &next, iterator end, token &ret) const noexcept;
	void reset() noexcept;

	char_separator(const char &delim) noexcept;
};

//
// interface
//

ircd::pair<ircd::string_view>
ircd::tokens_split(const string_view &str,
                   const char sep,
                   const size_t at,
                   const size_t skip)
noexcept
{
	const ssize_t before(at);
	const ssize_t after
	{
		(before - 1) + ssize_t(skip)
	};

	return pair<string_view>
	{
		tokens_before(str, sep, before),
		tokens_after(str, sep, after)
	};
}

ircd::pair<ircd::string_view>
ircd::tokens_split(const string_view &str,
                   const string_view &sep,
                   const size_t at,
                   const size_t skip)
noexcept
{
	const ssize_t before(at);
	const ssize_t after
	{
		(before - 1) + ssize_t(skip)
	};

	return pair<string_view>
	{
		tokens_before(str, sep, before),
		tokens_after(str, sep, after)
	};
}

ircd::string_view
ircd::tokens_before(const string_view &str,
                    const char sep,
                    const size_t i)
noexcept
{
	assert(sep != '\0');
	const char _sep[2]
	{
		sep, '\0'
	};

	return tokens_before(str, _sep, i);
}

ircd::string_view
ircd::tokens_before(const string_view &str,
                    const string_view &sep,
                    const size_t i)
noexcept
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = string_separator;

	const delim d{sep};
	const boost::tokenizer<delim, iter, type> view
	{
		str, d
	};

	string_view ret;
	auto it(begin(view));
	for(size_t j(0); it != end(view) && j < i; ++it, j++)
		ret =
		{
			begin(view)->data(), it->data() + it->size()
		};

	return ret;
}

ircd::string_view
ircd::tokens_after(const string_view &str,
                   const char sep,
                   const ssize_t i)
noexcept
{
	assert(sep != '\0');
	const char _sep[2]
	{
		sep, '\0'
	};

	return tokens_after(str, _sep, i);
}

ircd::string_view
ircd::tokens_after(const string_view &str,
                   const string_view &sep,
                   const ssize_t i)
noexcept
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = string_separator;

	const delim d{sep};
	const boost::tokenizer<delim, iter, type> view
	{
		str, d
	};

	auto it(begin(view));
	for(ssize_t j(0); it != end(view); ++it, j++)
		if(j > i)
			return string_view
			{
				it->data(), str.data() + str.size()
			};

	return {};
}

ircd::string_view
ircd::token_first(const string_view &str,
                  const char sep)
{
	assert(sep != '\0');
	const char _sep[2]
	{
		sep, '\0'
	};

	return token(str, _sep, 0);
}

ircd::string_view
ircd::token_first(const string_view &str,
                  const string_view &sep)
{
	return token(str, sep, 0);
}

ircd::string_view
ircd::token_last(const string_view &str,
                 const char sep)
{
	assert(sep != '\0');
	const char _sep[2]
	{
		sep, '\0'
	};

	return token_last(str, _sep);
}

ircd::string_view
ircd::token_last(const string_view &str,
                 const string_view &sep)
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = string_separator;

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
            const char sep,
            const size_t i,
            const string_view &def)
noexcept try
{
	return token(str, sep, i);
}
catch(const std::out_of_range &)
{
	return def;
}

ircd::string_view
ircd::token(const string_view &str,
            const string_view &sep,
            const size_t i,
            const string_view &def)
noexcept try
{
	return token(str, sep, i);
}
catch(const std::out_of_range &)
{
	return def;
}

ircd::string_view
ircd::token(const string_view &str,
            const char sep,
            const size_t i)
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = char_separator;

	const delim d{sep};
	const boost::tokenizer<delim, iter, type> view
	{
		str, d
	};

	return *at(begin(view), end(view), i);
}

ircd::string_view
ircd::token(const string_view &str,
            const string_view &sep,
            const size_t i)
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = string_separator;

	const delim d{sep};
	const boost::tokenizer<delim, iter, type> view
	{
		str, d
	};

	return *at(begin(view), end(view), i);
}

bool
ircd::token_exists(const string_view &str,
                   const char sep,
                   const string_view &tok)
noexcept
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = char_separator;

	const delim d{sep};
	const boost::tokenizer<delim, iter, type> view
	{
		str, d
	};

	return std::find(begin(view), end(view), tok) != end(view);
}

bool
ircd::token_exists(const string_view &str,
                   const string_view &sep,
                   const string_view &tok)
noexcept
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = string_separator;

	const delim d{sep};
	const boost::tokenizer<delim, iter, type> view
	{
		str, d
	};

	return std::find(begin(view), end(view), tok) != end(view);
}

size_t
ircd::token_count(const string_view &str,
                  const char sep)
noexcept
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = char_separator;

	const delim d{sep};
	const boost::tokenizer<delim, iter, type> view
	{
		str, d
	};

	return std::distance(begin(view), end(view));
}

size_t
ircd::token_count(const string_view &str,
                  const string_view &sep)
noexcept
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = string_separator;

	const delim d{sep};
	const boost::tokenizer<delim, iter, type> view
	{
		str, d
	};

	return std::distance(begin(view), end(view));
}

size_t
ircd::tokens(const string_view &str,
             const char sep_,
             const mutable_buffer &buf,
             const token_view &closure)
{
	assert(sep_ != '\0');
	const char _sep[2]
	{
		sep_, '\0'
	};

	const string_view sep
	{
		_sep
	};

	return tokens(str, sep, buf, closure);
}

size_t
ircd::tokens(const string_view &str,
             const string_view &sep,
             const mutable_buffer &buf,
             const token_view &closure)
{
	char *ptr(data(buf));
	char *const stop(data(buf) + size(buf));
	tokens(str, sep, [&closure, &ptr, &stop]
	(const string_view &token) -> bool
	{
		const size_t terminated_size(token.size() + 1);
		const size_t remaining(std::distance(ptr, stop));
		if(remaining < terminated_size)
			return false;

		char *const dest(ptr);
		ptr += strlcpy(dest, token.data(), terminated_size);
		return closure(string_view(dest, token.size()));
	});

	return std::distance(data(buf), ptr);
}

size_t
ircd::tokens(const string_view &str,
             const char sep,
             const size_t limit,
             const token_view &closure)
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = char_separator;

	const delim d{sep};
	const boost::tokenizer<delim, iter, type> view
	{
		str, d
	};

	size_t i(0);
	for(auto it(begin(view)); i < limit && it != end(view); ++it, i++)
		if(!closure(*it))
			break;

	return i;
}

size_t
ircd::tokens(const string_view &str,
             const string_view &sep,
             const size_t limit,
             const token_view &closure)
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = string_separator;

	const delim d{sep};
	const boost::tokenizer<delim, iter, type> view
	{
		str, d
	};

	size_t i(0);
	for(auto it(begin(view)); i < limit && it != end(view); ++it, i++)
		if(!closure(*it))
			break;

	return i;
}

bool
ircd::tokens(const string_view &str,
             const char sep,
             const token_view &closure)
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = char_separator;

	const delim d{sep};
	const boost::tokenizer<delim, iter, type> view
	{
		str, d
	};

	for(auto it(begin(view)); it != end(view); ++it)
		if(!closure(*it))
			return false;

	return true;
}

bool
ircd::tokens(const string_view &str,
             const string_view &sep,
             const token_view &closure)
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = string_separator;

	const delim d{sep};
	const boost::tokenizer<delim, iter, type> view
	{
		str, d
	};

	for(auto it(begin(view)); it != end(view); ++it)
		if(!closure(*it))
			return false;

	return true;
}

//
// string_separator
//

ircd::string_separator::string_separator(const string_view &delim)
noexcept
:delim
{
	delim
}
{
}

void
ircd::string_separator::reset()
noexcept
{
	//TODO: ???
}

template<class iterator,
         class token>
bool
ircd::string_separator::operator()(iterator &start,
                                   iterator stop,
                                   token &ret)
const noexcept
{
	do
	{
		if(start == stop)
			return false;

		const string_view input
		{
			start, stop
		};

		string_view remain;
		std::tie(ret, remain) = ircd::split(input, delim);
		start = remain?
			begin(remain):
			stop;
	}
	while(!ret);
	return true;
}

//
// char_separator
//

ircd::char_separator::char_separator(const char &delim)
noexcept
:delim
{
	delim
}
{
}

void
ircd::char_separator::reset()
noexcept
{
	//TODO: ???
}

template<class iterator,
         class token>
bool
ircd::char_separator::operator()(iterator &start,
                                 iterator stop,
                                 token &ret)
const noexcept
{
	do
	{
		if(start == stop)
			return false;

		const string_view input
		{
			start, stop
		};

		string_view remain;
		std::tie(ret, remain) = ircd::split(input, delim);
		start = remain?
			begin(remain):
			stop;
	}
	while(!ret);
	return true;
}
