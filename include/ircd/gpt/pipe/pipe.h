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
#define HAVE_IRCD_GPT_PIPE_H

namespace ircd::gpt::pipe
{
	struct code;
	struct model;
	struct desc;
	struct range;
	struct cycle;
	struct prof;

	extern conf::item<size_t> queue_cycles;

	void init(), fini() noexcept;
};

#include "code.h"
#include "model.h"
#include "desc.h"
#include "range.h"
#include "cycle.h"
#include "prof.h"
