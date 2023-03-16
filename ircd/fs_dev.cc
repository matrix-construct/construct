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

ircd::string_view
ircd::fs::dev::sysfs(const mutable_buffer &out,
                     const ulong &id,
                     const string_view &relpath)
{
	thread_local char path_buf[1024];
	const string_view path{fmt::sprintf
	{
		path_buf, "dev/block/%s/%s",
		sysfs_id(name_scratch, id),
		relpath
	}};

	return sys::get(out, path);
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

decltype(ircd::fs::dev::blk::SECTOR_SIZE)
ircd::fs::dev::blk::SECTOR_SIZE
{
	512
};

decltype(ircd::fs::dev::blk::BASE_PATH)
ircd::fs::dev::blk::BASE_PATH
{
	"/sys/dev/block"
};

bool
ircd::fs::dev::blk::for_each(const closure &closure)
{
	return for_each(string_view{}, closure);
}

bool
ircd::fs::dev::blk::for_each(const string_view &type,
                             const closure &closure)
{
	for(const auto &dir : fs::ls(blk::BASE_PATH)) try
	{
		const auto &[major, minor]
		{
			split(filename(path_scratch, dir), ':')
		};

		if(!major || !minor)
			continue;

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

//
// dev::blk::blk
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
,sector_size
{
	sysfs(id, "queue/hw_sector_size")
}
,physical_block
{
	sysfs(id, "queue/physical_block_size")
}
,logical_block
{
	sysfs(id, "queue/logical_block_size")
}
,minimum_io
{
	sysfs(id, "queue/minimum_io_size")
}
,optimal_io
{
	sysfs(id, "queue/optimal_io_size")
}
,sectors
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
,scheduler
{
	ircd::string(64, [&id]
	(const mutable_buffer &buf)
	{
		return sysfs(buf, id, "queue/scheduler");
	})
}
,rotational
{
	sysfs<bool>(id, "queue/rotational", false)
}
,merges
{
	!sysfs<bool>(id, "queue/nomerges", true)
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

//
// dev::stats
//

ircd::fs::dev::stats
ircd::fs::dev::stats::get(const major_minor &id)
{
	stats ret;
	for_each([&id, &ret]
	(const auto &stats)
	{
		if(stats.id == id)
		{
			ret = stats;
			return false;
		}
		else return true;
	});

	return ret;
}

bool
ircd::fs::dev::stats::for_each(const closure &closure)
{
	thread_local char buf[16_KiB];

	const fs::fd fd
	{
		"/proc/diskstats", fs::fd::opts
		{
			.mode = std::ios::in,
		},
	};

	fs::read_opts opts;
	opts.aio = false;
	const string_view read
	{
		fs::read(fd, buf, opts)
	};

	return tokens(read, '\n', [&closure]
	(const auto &line)
	{
		return closure(stats(line));
	});
}

//
// dev::stats::stats
//

ircd::fs::dev::stats::stats(const string_view &line)
{
	string_view item[20];
	const auto items
	{
		tokens(line, ' ', item)
	};

	id.first = lex_cast<uint64_t>(item[0]);
	id.second = lex_cast<uint64_t>(item[1]);
	strlcpy(name, item[2]);

	read = lex_cast<uint64_t>(item[3]);
	read_merged = lex_cast<uint64_t>(item[4]);
	read_sectors = lex_cast<uint64_t>(item[5]);
	read_time = lex_cast<milliseconds>(item[6]);

	write = lex_cast<uint64_t>(item[7]);
	write_merged = lex_cast<uint64_t>(item[8]);
	write_sectors = lex_cast<uint64_t>(item[9]);
	write_time = lex_cast<milliseconds>(item[10]);

	io_current = lex_cast<uint64_t>(item[11]);
	io_time = lex_cast<milliseconds>(item[12]);
	io_weighted_time = lex_cast<milliseconds>(item[13]);

	if(items <= 14)
		return;

	discard = lex_cast<uint64_t>(item[14]);
	discard_merged = lex_cast<uint64_t>(item[15]);
	discard_sectors = lex_cast<uint64_t>(item[16]);
	discard_time = lex_cast<milliseconds>(item[17]);

	if(items <= 18)
		return;

	flush = lex_cast<uint64_t>(item[18]);
	flush_time = lex_cast<milliseconds>(item[19]);

	if(items <= 20)
		return;
}
