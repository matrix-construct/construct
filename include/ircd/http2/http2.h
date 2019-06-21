// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_HTTP2_H

/// HyperText TransPort / 2.x
namespace ircd::http2
{
	extern const string_view connection_preface;
}

#include "error.h"
#include "frame.h"
#include "settings.h"
#include "stream.h"
