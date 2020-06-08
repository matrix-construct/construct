// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_SYS_SYSMACROS_H

bool
ircd::fs::dev::for_each(const blk_closure &closure)
{
	return for_each(string_view{}, closure);
}

bool
ircd::fs::dev::for_each(const string_view &type,
                        const blk_closure &closure)
{
	for(const auto &dir : fs::ls("/sys/dev/block")) try
	{
		const auto &[major, minor]
		{
			split(filename(path_scratch, dir), ':')
		};

		const ulong id
		{
			dev::id({lex_cast<ulong>(major), lex_cast<ulong>(minor)})
		};

		char dtbuf[32];
		if(type && blk::devtype(dtbuf, id) != type)
			continue;

		if(!closure(id, blk(id)))
			return false;
	}
	catch(const ctx::interrupted &)
	{
		throw;
	}
	catch(const std::exception &e)
	{
		log::error
		{
			log, "%s :%s",
			dir,
			e.what(),
		};
	}

	return true;
}

ircd::string_view
ircd::fs::dev::sysfs(const mutable_buffer &out,
                     const ulong &id,
                     const string_view &relpath)
try
{
	const string_view path{fmt::sprintf
	{
		path_scratch, "/sys/dev/block/%s/%s",
		sysfs_id(name_scratch, id),
		relpath
	}};

	fs::fd::opts fdopts;
	fdopts.errlog = false;
	const fs::fd fd
	{
		path, fdopts
	};

	fs::read_opts ropts;
	ropts.aio = false;
	string_view ret
	{
		fs::read(fd, out, ropts)
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
		log, "sysfs query dev_id:%lu `%s' :%s",
		id,
		relpath,
		e.what(),
	};
	#endif

	return {};
}

ircd::string_view
ircd::fs::dev::sysfs_id(const mutable_buffer &out,
                        const ulong &id)
{
	return sysfs_id(out, dev::id(id));
}

ircd::string_view
ircd::fs::dev::sysfs_id(const mutable_buffer &out,
                        const major_minor &id)
{
	return fmt::sprintf
	{
		out, "%lu:%lu", id.first, id.second
	};
}

ulong
ircd::fs::dev::id(const major_minor &id)
{
	return makedev(id.first, id.second);
}

ircd::fs::dev::major_minor
ircd::fs::dev::id(const ulong &id)
{
	return
	{
		major(id), minor(id)
	};
}

//
// dev::blk
//

ircd::fs::dev::blk::blk(const ulong &id)
:type
{
	ircd::string(15, [&id]
	(const mutable_buffer &buf)
	{
		return devtype(buf, id);
	})
}
,vendor
{
	ircd::string(15, [&id]
	(const mutable_buffer &buf)
	{
		return sysfs(buf, id, "device/vendor");
	})
}
,model
{
	ircd::string(64, [&id]
	(const mutable_buffer &buf)
	{
		return sysfs(buf, id, "device/model");
	})
}
,rev
{
	ircd::string(15, [&id]
	(const mutable_buffer &buf)
	{
		return sysfs(buf, id, "device/rev");
	})
}
,size
{
	sysfs(id, "size")
}
,queue_depth
{
	sysfs(id, "device/queue_depth")
}
,nr_requests
{
	sysfs(id, "queue/nr_requests")
}
,rotational
{
	sysfs<bool>(id, "queue/rotational", false)
}
{
}

ircd::string_view
ircd::fs::dev::blk::devtype(const mutable_buffer &buf,
                            const ulong &id)
{
	char tmp[128];
	string_view ret;
	tokens(sysfs(tmp, id, "uevent"), '\n', [&buf, &ret]
	(const string_view &kv)
	{
		const auto &[key, value]
		{
			split(kv, '=')
		};

		if(key == "DEVTYPE")
			ret = strlcpy(buf, value);
	});

	return ret;
}
