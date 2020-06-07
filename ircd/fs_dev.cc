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

decltype(ircd::fs::dev::block)
ircd::fs::dev::block;

//
// init
//

ircd::fs::dev::init::init()
{
	#ifdef __linux__
	for(const auto &dir : fs::ls("/sys/dev/block")) try
	{
		const auto &[major, minor]
		{
			split(filename(path_scratch, dir), ':')
		};

		const major_minor device_major_minor
		{
			lex_cast<ulong>(major), lex_cast<ulong>(minor)
		};

		const auto iit
		{
			block.emplace(device_major_minor, id(device_major_minor))
		};

		assert(iit.second);
		const auto &bd(iit.first->second);
		if(!bd.is_device || bd.type != "disk")
			block.erase(iit.first);
	}
	catch(const std::exception &e)
	{
		log::derror
		{
			log, "%s :%s",
			dir,
			e.what(),
		};
	}
	#endif

	for(const auto &[mm, bd] : block)
	{
		char pbuf[48];
		log::info
		{
			log, "%s %u:%2u %s %s %s %s queue depth:%zu requests:%zu",
			bd.type,
			std::get<0>(mm),
			std::get<1>(mm),
			bd.vendor,
			bd.model,
			bd.rev,
			pretty(pbuf, iec(bd.size)),
			bd.queue_depth,
			bd.nr_requests,
		};
	}

}

ircd::fs::dev::init::~init()
noexcept
{
}

//
// fs::dev
//

#ifdef __linux__
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

	fs::read_opts opts;
	opts.aio = false;
	string_view ret
	{
		fs::read(path, out, opts)
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
	log::derror
	{
		log, "sysfs query dev_id:%lu `%s' :%s",
		id,
		relpath,
		e.what(),
	};

	return {};
}
#else
ircd::string_view
ircd::fs::dev::sysfs(const mutable_buffer &out,
                     const ulong &id,
                     const string_view &relpath)
{
	throw panic
	{
		"sysfs(5) is not available."
	};
}
#endif

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
// dev::device
//

ircd::fs::dev::blkdev::blkdev(const ulong &id)
:is_device
{
	fs::is_dir(fmt::sprintf
	{
		path_scratch, "/sys/dev/block/%s/device",
		sysfs_id(name_scratch, id),
	})
}
,is_queue
{
	fs::is_dir(fmt::sprintf
	{
		path_scratch, "/sys/dev/block/%s/queue",
		sysfs_id(name_scratch, id),
	})
}
,type
{
	!is_device? std::string{}:
	ircd::string(8, [&id]
	(const mutable_buffer &buf)
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
	})
}
,vendor
{
	!is_device? std::string{}:
	ircd::string(12, [&id]
	(const mutable_buffer &buf)
	{
		return sysfs(buf, id, "device/vendor");
	})
}
,model
{
	!is_device? std::string{}:
	ircd::string(64, [&id]
	(const mutable_buffer &buf)
	{
		return sysfs(buf, id, "device/model");
	})
}
,rev
{
	!is_device? std::string{}:
	ircd::string(12, [&id]
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
	sysfs<bool>(id, "queue/rotational", true)
}
{
}
