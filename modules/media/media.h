// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

static m::room::id::buf
file_room_id(const string_view &server,
             const string_view &file)
{
	size_t len;
	thread_local char buf[512];
	len = strlcpy(buf, server);
	len = strlcat(buf, "/"_sv);
	len = strlcat(buf, file);
	const sha256::buf hash
	{
		sha256{string_view{buf, len}}
	};

	return m::room::id::buf
	{
		b58encode(buf, hash), my_host()
	};
}
