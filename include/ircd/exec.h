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
	struct opts;
	struct handler;
	using args = vector_view<const string_view>;
	using const_buffers = vector_view<const const_buffer>;
	using mutable_buffers = vector_view<const mutable_buffer>;

	static log::log log;
	static uint64_t id_ctr;

	uint64_t id {0};
	std::unique_ptr<opts> opt;
	std::string path;
	std::vector<std::string> argv;
	std::unique_ptr<pair<boost::process::async_pipe>> pipe;
	std::unique_ptr<boost::process::child> child;
	std::exception_ptr eptr;
	ctx::dock dock;
	long pid {-1L};            // > 0 when running; <= 0 during exec/halt
	long code {0};             // set on exit

  public:
	size_t read(const mutable_buffers &);
	size_t write(const const_buffers &);
	bool signal(const int &sig);
	long join(const int &sig = 0);  // returns exit code
	long run();                     // returns pid or throws

	exec(const args &, const opts &);
	exec(const args &);
	exec(exec &&) = delete;
	exec(const exec &) = delete;
	exec &operator=(exec &&) = delete;
	exec &operator=(const exec &) = delete;
	~exec() noexcept;
};

/// Exec options
///
struct ircd::exec::opts
{
	/// Child executions will be logged at this level (use DEBUG to quiet)
	ircd::log::level exec_log_level = ircd::log::level::NOTICE;

	/// Child exits will be logged at this level (use DEBUG to quiet); note
	/// non-zero exits are still logged with level::ERROR.
	ircd::log::level exit_log_level = ircd::log::level::INFO;
};

inline
ircd::exec::exec(const args &args)
:exec{args, opts{}}
{}

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
