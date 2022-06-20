// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_GPT_PIPE_CODE_H

/// Pipe code segment.
struct ircd::gpt::pipe::code
:cl::code
{
	static conf::item<std::string> default_path;
	static conf::item<std::string> default_compile_opts;
	static conf::item<std::string> default_link_opts;
	static conf::item<std::string> cache_path;

	static string_view make_cache_path(const mutable_buffer &);

	static cl::code from_cache();
	static cl::code from_source(const string_view &comp_opts = {}, const string_view &link_opts = {});
	static cl::code from_bitcode(const string_view &link_opts = {});

	void set_cache(const string_view &path);
	bool put_cache();

	code();
	~code() noexcept;
};
