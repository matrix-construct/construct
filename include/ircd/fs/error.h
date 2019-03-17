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
#define HAVE_IRCD_FS_ERROR_H

// Forward declarations for boost
namespace boost::filesystem
{
	struct filesystem_error;
}

namespace ircd::fs
{
	struct error; // does not participate in ircd::exception hierarchy
}

namespace ircd
{
	std::error_code make_error_code(const boost::filesystem::filesystem_error &);
	std::system_error make_system_error(const boost::filesystem::filesystem_error &);

	string_view string(const mutable_buffer &, const boost::filesystem::filesystem_error &);
	std::string string(const boost::filesystem::filesystem_error &);
}

struct ircd::fs::error
:std::system_error
{
	static thread_local char buf[1024];

  public:
	template<class... args>
	error(const boost::filesystem::filesystem_error &e,
	      const string_view &fmt,
	      args&&...);

	template<class... args>
	error(const std::error_code &e,
	      const string_view &fmt,
	      args&&...);

	error(const boost::filesystem::filesystem_error &e);
};

template<class... args>
ircd::fs::error::error(const boost::filesystem::filesystem_error &e,
                       const string_view &fmt,
                       args&&... a)
:error
{
	make_error_code(e), fmt, std::forward<args>(a)...
}
{}

template<class... args>
ircd::fs::error::error(const std::error_code &e,
                       const string_view &fmt,
                       args&&... a)
:std::system_error{[&]
() -> std::system_error
{
	fmt::sprintf{buf, fmt, std::forward<args>(a)...};
	return {e, buf};
}()}
{}
