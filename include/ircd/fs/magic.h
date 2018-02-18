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
#define HAVE_IRCD_FS_MAGIC_H

namespace ircd::fs::magic
{
	IRCD_EXCEPTION(fs::error, error)

	int version();

	string_view mime(const mutable_buffer &out, const const_buffer &);
	string_view mime_type(const mutable_buffer &out, const const_buffer &);
	string_view mime_encoding(const mutable_buffer &out, const const_buffer &);
	string_view extensions(const mutable_buffer &out, const const_buffer &);
	string_view description(const mutable_buffer &out, const const_buffer &);
}
