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

namespace ircd::fs
{
	struct error; // does not participate in ircd::exception hierarchy

	extern const std::error_code eof;
}

namespace ircd
{
	string_view string(const mutable_buffer &, const std::filesystem::filesystem_error &);
	std::string string(const std::filesystem::filesystem_error &);
}

struct ircd::fs::error
:std::filesystem::filesystem_error
{
	static constexpr size_t max_len {4096};

	template<class... args>
	[[clang::internal_linkage]]
	error(const std::filesystem::filesystem_error &e,
	      const string_view &fmt,
	      args&&...);

	template<class... args>
	[[clang::internal_linkage]]
	error(const std::error_code &e,
	      const string_view &fmt,
	      args&&...);

	error(const std::error_code &e, const string_view &fmt);
	error(const std::filesystem::filesystem_error &e, const string_view &fmt);
	error(const std::filesystem::filesystem_error &e);
	~error() noexcept;
};

template<class... args>
[[gnu::noinline, gnu::visibility("internal")]]
ircd::fs::error::error(const std::filesystem::filesystem_error &e,
                       const string_view &fmt,
                       args&&... a)
:std::filesystem::filesystem_error
{
	fmt::snstringf{max_len, fmt, std::forward<args>(a)...},
	e.path1(),
	e.path2(),
	e.code(),
}
{}

template<class... args>
[[gnu::noinline, gnu::visibility("internal")]]
ircd::fs::error::error(const std::error_code &e,
                       const string_view &fmt,
                       args&&... a)
:std::filesystem::filesystem_error
{
	fmt::snstringf{max_len, fmt, std::forward<args>(a)...},
	e,
}
{}
