// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::sys::log)
ircd::sys::log
{
	"sys"
};

ircd::string_view
ircd::sys::get(const mutable_buffer &out,
               const string_view &relpath)
try
{
	thread_local char buf[1024];
	string_view path(relpath);
	path = lstrip(relpath, "/sys/"_sv); // tolerate any errant /sys/ prefix
	path = strlcpy(buf, "/sys/"_sv);
	path = strlcat(buf, relpath);

	fs::fd::opts fdopts;
	fdopts.errlog = false;
	const fs::fd fd
	{
		path, fdopts
	};

	string_view ret
	{
		data(out), sys::call(::read, fd, data(out), size(out))
	};

	ret = rstrip(ret, '\n');
	ret = rstrip(ret, ' ');
	return ret;
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	#if 0
	log::derror
	{
		log, "sysfs query `%s' :%s",
		relpath,
		e.what(),
	};
	#endif

	return {};
}
