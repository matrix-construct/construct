// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

//
// Human readable time suite
//

ircd::string_view
ircd::util::pretty_nanoseconds(const mutable_buffer &out,
                               const long double &ns)
{
	static const std::array<string_view, 7> unit
	{
		"nanoseconds",
		"microseconds",
		"milliseconds",
		"seconds",
		"minutes",
		"hours",
		"days",
	};

	auto pos(0);
	long double val(ns);

	// nanoseconds -> microseconds
	if(val > 1000.0)
	{
		val /= 1000;
		++pos;
	}
	else goto done;

	// microseconds -> milliseconds
	if(val > 1000.0)
	{
		val /= 1000;
		++pos;
	}
	else goto done;

	// milliseconds -> seconds
	if(val > 1000.0)
	{
		val /= 1000;
		++pos;
	}
	else goto done;

	// seconds -> minutes
	if(val > 60.0)
	{
		val /= 60;
		++pos;
	}
	else goto done;

	// minutes -> hours
	if(val > 60.0)
	{
		val /= 60;
		++pos;
	}
	else goto done;

	// hours -> days
	if(val > 12.0)
	{
		val /= 12;
		++pos;
	}
	else goto done;

	done:
	return fmt::sprintf
	{
		out, "%.2lf %s",
		val,
		unit.at(pos)
	};
}

//
// Human readable space suite
//

std::string
ircd::util::pretty_only(const human_readable_size &value)
{
	return util::string(32, [&value]
	(const mutable_buffer &out)
	{
		return pretty_only(out, value);
	});
}

ircd::string_view
ircd::util::pretty_only(const mutable_buffer &out,
                        const human_readable_size &value)
try
{
	return fmt::sprintf
	{
		out, "%.2lf %s",
		std::get<long double>(value),
		std::get<const string_view &>(value)
	};
}
catch(const std::out_of_range &e)
{
	return fmt::sprintf
	{
		out, "%lu B",
		std::get<uint64_t>(value)
	};
}

std::string
ircd::util::pretty(const human_readable_size &value)
{
	return util::string(64, [&value]
	(const mutable_buffer &out)
	{
		return pretty(out, value);
	});
}

ircd::string_view
ircd::util::pretty(const mutable_buffer &out,
                   const human_readable_size &value)
try
{
	return fmt::sprintf
	{
		out, "%.2lf %s (%lu)",
		std::get<long double>(value),
		std::get<const string_view &>(value),
		std::get<uint64_t>(value)
	};
}
catch(const std::out_of_range &e)
{
	return fmt::sprintf
	{
		out, "%lu B",
		std::get<uint64_t>(value)
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

ircd::human_readable_size
ircd::util::si(const uint64_t &value)
{
	static const std::array<string_view, 7> unit
	{
		"B", "KB", "MB", "GB", "TB", "PB", "EB"
	};

	auto pos(0);
	long double v(value);
	for(; v > 1000.0; v /= 1000.0, ++pos);
	return
	{
		value, v, unit.at(pos)
	};
}

//
// binary <-> hex suite
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
		::snprintf(tmp, sizeof(tmp), "%02x", in[i]);
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
