// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
//
// util/util.h
//

size_t
ircd::util::size(std::ostream &s)
{
	const auto cur(s.tellp());
	s.seekp(0, std::ios::end);
	const auto ret(s.tellp());
	s.seekp(cur, std::ios::beg);
	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//
// util/env.h
//

ircd::string_view
ircd::util::getenv(const string_view &key)
{
	thread_local char keystr[128];
	if(unlikely(size(key) >= sizeof(keystr)))
		throw error
		{
			"getenv(): variable key is too long."
		};

	// Ensure the key is null terminated for the std:: call.
	const size_t len
	{
		strlcpy(keystr, key)
	};

	const string_view var
	{
		std::getenv(keystr)
	};

	return var;
}

///////////////////////////////////////////////////////////////////////////////
//
// util/pretty.h
//

//
// Human readable time suite
//

namespace ircd { inline namespace util
{
	using pretty_time_formats = std::array<string_view, 2>;
	using pretty_time_element = std::tuple<pretty_time_formats, long double>;

	template<size_t i,
	         class T>
	static string_view pretty(const mutable_buffer &, const T &, const uint &fmt);

	[[gnu::visibility("internal")]]
	extern const std::array<pretty_time_element, 9> pretty_time_unit;
}}

decltype(ircd::util::pretty_time_unit)
ircd::util::pretty_time_unit
{{
	//  fmt=0              fmt=1
	{ { "nanoseconds",     "ns" },     1000.0L  },
	{ { "microseconds",    "us" },     1000.0L  },
	{ { "milliseconds",    "ms" },     1000.0L  },
	{ { "seconds",         "s"  },       60.0L  },
	{ { "minutes",         "m"  },       60.0L  },
	{ { "hours",           "h"  },       24.0L  },
	{ { "days",            "d"  },        7.0L  },
	{ { "weeks",           "w"  },        4.0L  },
	{ { "months",          "M"  },       12.0L  },
}};

template<size_t i,
         class T>
inline ircd::string_view
ircd::util::pretty(const mutable_buffer &out,
                   const T &dur,
                   const uint &fmt)
{
	static const auto &unit
	{
		pretty_time_unit
	};

	const string_view &fmtstr
	{
		fmt == 1?
			"%.2lf%s"_sv:
			"%.2lf %s"_sv
	};

	size_t pos(i);
	long double val(dur.count());
	for(; val > std::get<1>(unit.at(pos)) && pos < unit.size() - 1; ++pos)
		val /= std::get<1>(unit.at(pos));

	return fmt::sprintf
	{
		out, fmtstr,
		val,
		std::get<0>(unit.at(pos)).at(fmt)
	};
}

ircd::string_view
ircd::util::pretty(const mutable_buffer &out,
                   const nanoseconds &val,
                   const uint &fmt)
{
	return pretty<0>(out, val, fmt);
}

ircd::string_view
ircd::util::pretty(const mutable_buffer &out,
                   const microseconds &val,
                   const uint &fmt)
{
	return pretty<1>(out, val, fmt);
}

ircd::string_view
ircd::util::pretty(const mutable_buffer &out,
                   const milliseconds &val,
                   const uint &fmt)
{
	return pretty<2>(out, val, fmt);
}

ircd::string_view
ircd::util::pretty(const mutable_buffer &out,
                   const seconds &val,
                   const uint &fmt)
{
	return pretty<3>(out, val, fmt);
}

//
// Human readable space suite
//

decltype(ircd::util::pretty_fmt)
ircd::util::pretty_fmt
{
	"%.2lf %s (%lu)"_sv,
	"%.2lf %s"_sv,
	"%.2lf%s"_sv,
	string_view{},
};

std::string
ircd::util::pretty(const human_readable_size &value,
                   const uint &fmt)
{
	return pretty(value, pretty_fmt[fmt]);
}

ircd::string_view
ircd::util::pretty(const mutable_buffer &out,
                   const human_readable_size &value,
                   const uint &fmt)
{
	return pretty(out, pretty_fmt[fmt], value);
}

std::string
ircd::util::pretty(const human_readable_size &value,
                   const string_view &fmt)
{
	return util::string(64, [&value, &fmt]
	(const mutable_buffer &out)
	{
		return pretty(out, fmt, value);
	});
}

ircd::string_view
ircd::util::pretty(const mutable_buffer &out,
                   const string_view &fmt,
                   const human_readable_size &value)
try
{
	return fmt::sprintf
	{
		out, fmt,
		std::get<long double>(value),
		std::get<const string_view &>(value),
		std::get<uint64_t>(value)
	};
}
catch(const std::out_of_range &e)
{
	return fmt::sprintf
	{
		out, "%lu",
		std::get<uint64_t>(value)
	};
}

ircd::human_readable_size
ircd::util::si(const uint64_t &value)
{
	static const std::array<string_view, 7> unit
	{
		" ", "K", "M", "G", "T", "P", "E"
	};

	auto pos(0);
	long double v(value);
	for(; v > 1000.0; v /= 1000.0, ++pos);
	return
	{
		value, v, unit.at(pos)
	};
}

ircd::human_readable_size
ircd::util::iec(const uint64_t &value)
{
	static const std::array<string_view, 7> unit
	{
		"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"
	};

	auto pos(0);
	long double v(value);
	for(; v > 1024.0; v /= 1024.0, ++pos);
	return
	{
		value, v, unit.at(pos)
	};
}

///////////////////////////////////////////////////////////////////////////////
//
// util/string.h
//

/// Close over the common pattern to write directly into a post-C++11 standard
/// string through the data() member requiring a const_cast. Closure returns
/// a view of the data actually written to the buffer.
std::string
ircd::util::string(const size_t &size,
                   const string_closure_view &closure)
{
	return string(size, [&closure]
	(const mutable_buffer &buffer)
	{
		return ircd::size(closure(buffer));
	});
}

/// Close over the common pattern to write directly into a post-C++11 standard
/// string through the data() member requiring a const_cast. Closure returns
/// the final size of the data written into the buffer.
std::string
ircd::util::string(const size_t &size,
                   const string_closure_size &closure)
{
	const size_t alloc_size
	{
		size & ~SHRINK_TO_FIT
	};

	std::string ret(alloc_size, char{});
	const mutable_buffer buf
	{
		mutable_cast(ret.data()), ret.size()
	};

	const size_t consumed
	{
		closure(buf)
	};

	assert(consumed <= buffer::size(buf));
	data(buf)[consumed] = '\0';
	ret.resize(consumed);

	if(size & SHRINK_TO_FIT)
		ret.shrink_to_fit();

	return ret;
}

std::string
ircd::util::string(const const_buffer &buf)
{
	return string(data(buf), size(buf));
}

std::string
ircd::util::string(const char *const &buf,
                   const size_t &size)
{
	return std::string{buf, size};
}

std::string
ircd::util::string(const uint8_t *const &buf,
                   const size_t &size)
{
	return string(reinterpret_cast<const char *>(buf), size);
}

///////////////////////////////////////////////////////////////////////////////
//
// util/timer.h
//

ircd::util::timer::timer(const std::function<void ()> &func)
:timer{}
{
	func();
	stop();
}

void
ircd::util::timer::stop()
{
	const auto now
	{
		!stopped()?
			clock::now():
			start
	};

	const auto elapsed
	{
		duration_cast<decltype(accumulator)>(now - start)
	};

	accumulator += elapsed;
	start = clock::time_point::min();
}

void
ircd::util::timer::cont()
{
	const auto now
	{
		clock::now()
	};

	const auto elapsed
	{
		!stopped()?
			duration_cast<decltype(accumulator)>(now - start):
			decltype(accumulator)(0)
	};

	accumulator += elapsed;
	start = now;
}

std::string
ircd::util::timer::pretty(const int &fmt)
const
{
	return util::pretty(at<nanoseconds>(), fmt);
}

ircd::string_view
ircd::util::timer::pretty(const mutable_buffer &out,
                          const int &fmt)
const
{
	return util::pretty(out, at<nanoseconds>(), fmt);
}

///////////////////////////////////////////////////////////////////////////////
//
// util/u2a.h
//

std::string
ircd::util::u2a(const const_buffer &in)
{
	return string(size(in) * 2, [&in]
	(const mutable_buffer &out)
	{
		return u2a(out, in);
	});
}

ircd::string_view
ircd::util::u2a(const mutable_buffer &out,
                const const_buffer &in)
{
	char *p(data(out));
	for(size_t i(0); i < size(in) && p + 2 <= end(out); ++i)
	{
		char tmp[3];
		::snprintf(tmp, sizeof(tmp), "%02x", uint8_t(in[i]));
		*p++ = tmp[0];
		*p++ = tmp[1];
	}

	return { data(out), p };
}

ircd::const_buffer
ircd::util::a2u(const mutable_buffer &out,
                const const_buffer &in)
{
	const size_t len{size(in) / 2};
	for(size_t i(0); i < len; ++i)
	{
		const char gl[3]
		{
			in[i * 2],
			in[i * 2 + 1],
			'\0'
		};

		out[i] = strtol(gl, nullptr, 16);
	}

	return { data(out), len };
}

///////////////////////////////////////////////////////////////////////////////
//
// util/unwind.h
//

ircd::util::unwind_defer::~unwind_defer()
noexcept
{
	static ios::descriptor descriptor
	{
		"ircd.unwind"
	};

	ircd::dispatch
	{
		descriptor, ios::defer, std::move(func)
	};
}

///////////////////////////////////////////////////////////////////////////////
//
// util/what.h
//

/// Get what() from exception_ptr
///
ircd::string_view
ircd::util::what(const std::exception_ptr eptr)
noexcept try
{
	if(likely(eptr))
		std::rethrow_exception(eptr);

	return {};
}
catch(const std::exception &e)
{
	return e.what();
}
catch(...)
{
	return {};
}
