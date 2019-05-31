// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_MAGIC_H

namespace ircd::magic
{
	magic_t cookie;

	// magic_getflags() may not be available; this will supplement for our
	// tracking of their flags state.
	int flags{MAGIC_NONE};

	static void throw_on_error(const magic_t &);
	static void set_flags(const magic_t &, const int &flags);
	static string_view call(const magic_t &, const int &flags, const mutable_buffer &, const std::function<const char *()> &);
	static string_view call(const magic_t &, const int &flags, const mutable_buffer &, const const_buffer &);

	static void version_check();
}

decltype(ircd::magic::version_api)
ircd::magic::version_api
{
	"magic", info::versions::API, MAGIC_VERSION
};

decltype(ircd::magic::version_abi)
ircd::magic::version_abi
{
	"magic", info::versions::ABI, ::magic_version()
};

ircd::magic::init::init()
{
	version_check();

	if(unlikely(!(cookie = ::magic_open(flags))))
		throw error{"magic_open() failed"};

	//TODO: conf; TODO: windows
	static const char *const paths[]
	{
		"/usr/local/share/misc/magic.mgc",
		"/usr/share/misc/magic.mgc",
		nullptr
	};

	for(const char *const *path{paths}; *path; ++path)
		if(fs::exists(*path))
			if(magic_check(cookie, *path) == 0)
				if(magic_load(cookie, *path) == 0)
					return;

	throw error
	{
		"Failed to open any magic database"
	};
}

ircd::magic::init::~init()
noexcept
{
	::magic_close(cookie);
}

ircd::string_view
ircd::magic::description(const mutable_buffer &out,
                         const const_buffer &buffer)
{
	return call(cookie, MAGIC_NONE, out, buffer);
}

ircd::string_view
ircd::magic::extensions(const mutable_buffer &out,
                        const const_buffer &buffer)
{
	return call(cookie, MAGIC_EXTENSION, out, buffer);
}

ircd::string_view
ircd::magic::mime_encoding(const mutable_buffer &out,
                           const const_buffer &buffer)
{
	return call(cookie, MAGIC_MIME_ENCODING, out, buffer);
}

ircd::string_view
ircd::magic::mime_type(const mutable_buffer &out,
                       const const_buffer &buffer)
{
	return call(cookie, MAGIC_MIME_TYPE, out, buffer);
}

ircd::string_view
ircd::magic::mime(const mutable_buffer &out,
                  const const_buffer &buffer)
{
	return call(cookie, MAGIC_MIME, out, buffer);
}

ircd::string_view
ircd::magic::call(const magic_t &cookie,
                  const int &flags,
                  const mutable_buffer &out,
                  const const_buffer &buffer)
{
	return call(cookie, flags, out, [&cookie, &buffer]
	{
		return magic_buffer(cookie, data(buffer), size(buffer));
	});
}

ircd::string_view
ircd::magic::call(const magic_t &cookie,
                  const int &ours,
                  const mutable_buffer &out,
                  const std::function<const char *()> &closure)
{
	const auto &theirs{magic::flags};
	const unwind reset{[&theirs, &cookie]
	{
		set_flags(cookie, theirs);
	}};

	set_flags(cookie, ours);
	string_view str
	{
		closure()
	};

	if(!str)
	{
		throw_on_error(cookie);
		str = "application/octet-stream"_sv;
	}

	return { data(out), copy(out, str) };
}

void
ircd::magic::set_flags(const magic_t &cookie,
                       const int &flags)
{
	if(magic_setflags(cookie, flags) == -1)
		throw_on_error(cookie);
}

void
ircd::magic::throw_on_error(const magic_t &cookie)
{
	const char *const errstr
	{
		::magic_error(cookie)
	};

	if(errstr)
		throw error
		{
			"%s", errstr
		};

	assert(magic_errno(cookie) == 0);
}

void
ircd::magic::version_check()
{
	if(likely(MAGIC_VERSION == ::magic_version()))
		return;

	log::warning
	{
		"Linked libmagic version %d is not the compiled magic.h version %d.\n",
		::magic_version(),
		MAGIC_VERSION
	};
}
