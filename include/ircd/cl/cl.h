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
#define HAVE_IRCD_CL_CL_H

/// OpenCL Interface
namespace ircd::cl
{
	struct work;
	struct data;
	struct code;
	struct kern;
	struct exec;

	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, opencl_error)
	IRCD_EXCEPTION(error, unavailable)

	using read_closure = std::function<void (const_buffer)>;
	using write_closure = std::function<void (mutable_buffer)>;

	string_view reflect_error(const int code) noexcept;

	void flush();
	void sync();

	extern log::log log;

	extern const info::versions version_api;
	extern info::versions version_abi;

	extern conf::item<bool> enable;
	extern conf::item<bool> profile_queue;
	extern conf::item<uint64_t> watchdog_tsc;
	extern conf::item<milliseconds> nice_rate;
	extern conf::item<std::string> path;
	extern conf::item<std::string> envs[];
}

#include "work.h"
#include "data.h"
#include "code.h"
#include "kern.h"
#include "exec.h"
#include "init.h"
