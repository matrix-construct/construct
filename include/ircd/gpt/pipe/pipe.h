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
	struct model;
	struct code;
	struct desc;
	struct exec;

	extern model *default_model;
	extern code *default_code;
	extern desc *default_desc;

	void generate(task &);

	void init(), fini() noexcept;
};

#include "model.h"
#include "code.h"
#include "desc.h"
#include "exec.h"
