// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_NET_BPF_H

namespace ircd::net::bpf
{
	struct map;
	struct prog;

	extern log::log log;
}

struct ircd::net::bpf::map
{
	fs::fd fd;

  public:
	explicit operator bool() const
	{
		return bool(fd);
	}

	map();
	~map() noexcept;
};

struct ircd::net::bpf::prog
{
	const_buffer insns;
	mutable_buffer log_buf;
	fs::fd fd;

  public:
	explicit operator bool() const
	{
		return bool(fd);
	}

	prog(const const_buffer &, const mutable_buffer &log_buf);
	prog(const const_buffer &);
	~prog() noexcept;
};
