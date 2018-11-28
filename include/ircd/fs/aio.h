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
#define HAVE_IRCD_FS_AIO_H

// Public and unconditional interface for Linux AIO. This file is part of
// the standard include stack and available whether or not this platform
// is Linux with AIO, and whether or not it's enabled, etc. If it is not
// most of this stuff does nothing and will have null values.
//
namespace ircd::fs::aio
{
	struct init;
	struct kernel;
	struct request;

	extern kernel *context;
}

struct ircd::fs::aio::init
{
	init();
	~init() noexcept;
};
