// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_EXEC_H

namespace ircd
{
	struct exec;

	mutable_buffer read(exec &, const mutable_buffer &);
	const_buffer write(exec &, const const_buffer &);
}

// Forward declarations for boost because it is not included here.
namespace boost::process
{
	struct child;

	namespace detail::posix   { struct async_pipe; }
	namespace detail::windows { struct async_pipe; }
	using detail::posix::async_pipe; //TODO: XXX
}

/// Subprocess interface.
///
struct ircd::exec
:instance_list<ircd::exec>
{
	using args = vector_view<const string_view>;
	using const_buffers = vector_view<const const_buffer>;
	using mutable_buffers = vector_view<const mutable_buffer>;

	static log::log log;
	static uint64_t id_ctr;

	uint64_t id {0};
	std::string path;
	std::vector<std::string> argv;
	std::unique_ptr<pair<boost::process::async_pipe>> pipe;
	std::unique_ptr<boost::process::child> child;
	long pid {0};  // set on spawn
	long code {0}; // set on exit

  public:
	size_t read(const mutable_buffers &);
	size_t write(const const_buffers &);
	bool signal(const int &sig);
	long join(const int &sig = 0);

	exec(const args &);
	exec(exec &&) = delete;
	exec(const exec &) = delete;
	exec &operator=(exec &&) = delete;
	exec &operator=(const exec &) = delete;
	~exec() noexcept;
};

inline ircd::const_buffer
ircd::write(exec &p,
            const const_buffer &buf)
{
	return const_buffer
	{
		data(buf), p.write(vector_view<const const_buffer>{&buf, 1})
	};
}

inline ircd::mutable_buffer
ircd::read(exec &p,
           const mutable_buffer &buf)
{
	return mutable_buffer
	{
		data(buf), p.read(vector_view<const mutable_buffer>{&buf, 1})
	};
}
