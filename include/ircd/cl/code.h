// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_CL_CODE_H

/// cl_program wrapping
struct ircd::cl::code
{
	static const size_t iov_max;

	void *handle {nullptr};

	explicit operator bool() const;

	long status() const;
	size_t devs() const;
	size_t bins(const vector_view<size_t> &) const;
	size_t bins_size() const;

	vector_view<const mutable_buffer> bin(vector_view<mutable_buffer>) const;
	string_view src(const mutable_buffer &) const;

	void compile(const string_view &opts = {});
	void link(const string_view &opts = {});
	void build(const string_view &opts = {});

  private:
	void create(const vector_view<const const_buffer> &bins);
	void create(const vector_view<const string_view> &srcs);

  public:
	explicit code(const const_buffer &bc);
	explicit code(const vector_view<const const_buffer> &bins);
	code(const vector_view<const string_view> &srcs);
	code(const string_view &src);
	code() = default;
	code(code &&) noexcept;
	code(const code &) = delete;
	code &operator=(code &&) noexcept;
	code &operator=(const code &) = delete;
	~code() noexcept;
};

inline
ircd::cl::code::code(code &&o)
noexcept
:handle{std::move(o.handle)}
{
	o.handle = nullptr;
}

inline ircd::cl::code &
ircd::cl::code::operator=(code &&o)
noexcept
{
	this->~code();
	handle = std::move(o.handle);
	o.handle = nullptr;
	return *this;
}

inline ircd::cl::code::operator
bool()
const
{
	return handle;
}
