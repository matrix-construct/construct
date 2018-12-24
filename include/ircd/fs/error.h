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

namespace ircd
{
	struct error;

	std::error_code make_error_code(const boost::filesystem::filesystem_error &);
}

struct ircd::fs::error
:std::system_error
,ircd::error
{
	const char *what() const noexcept override;

	template<class... args>
	error(const string_view &fmt = " ",
	      args&&...);

	template<class... args>
	error(const std::error_code &,
	      const string_view &fmt,
	      args&&...);

	template<class... args>
	error(const std::system_error &,
	      const string_view &fmt,
	      args&&...);

	template<class... args>
	error(const boost::filesystem::filesystem_error &,
	      const string_view &fmt,
	      args&&...);

	error(const std::error_code &e);
	error(const std::system_error &);
	error(const boost::filesystem::filesystem_error &);
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
ircd::fs::error::error(const std::system_error &e,
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
:std::system_error
{
	make_error_code(e)
}
,ircd::error
{
	fmt, std::forward<args>(a)...
}
{}

template<class... args>
ircd::fs::error::error(const string_view &fmt,
                       args&&... a)
:std::system_error
{
	make_error_code(std::errc::invalid_argument)
}
,ircd::error
{
	fmt, std::forward<args>(a)...
}
{}
