// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

/// This unit is compiled for all targets, even though this is a linux-specific
/// interface -- for now. An explicit support condition like this could be
/// removed at some point.
decltype(ircd::prof::psi::supported)
ircd::prof::psi::supported
{
	#if defined(__linux__)
	info::kernel_version[0] > 4 ||
	(info::kernel_version[0] >= 4 && info::kernel_version[1] >= 20)
	#endif
};

/// Position sensitive
decltype(ircd::prof::psi::path)
ircd::prof::psi::path
{
	"/proc/pressure/cpu",
	"/proc/pressure/memory",
	"/proc/pressure/io",
};

decltype(ircd::prof::psi::cpu)
ircd::prof::psi::cpu
{
	"cpu"
};

decltype(ircd::prof::psi::mem)
ircd::prof::psi::mem
{
	"memory"
};

decltype(ircd::prof::psi::io)
ircd::prof::psi::io
{
	"io"
};

//
// prof::psi::metric::refresh
//

ircd::prof::psi::file &
ircd::prof::psi::wait(const vector_view<const trigger> &cmd)
try
{
	static const size_t max{3};
	size_t trig_num {0}, trig_idx[max]
	{
		size_t(-1),
		size_t(-1),
		size_t(-1),
	};

	// Associate all of the trigger inputs (cmd) with one of the files; the
	// cmds can be arranged any way and may not be for all files or any.
	for(size_t i(0); i < cmd.size(); ++i)
	{
		const auto it
		{
			std::find_if(begin(path), end(path), [&cmd, &i]
			(const auto &name)
			{
				return lstrip(name, "/proc/pressure/") == cmd[i].file.name;
			})
		};

		const auto pos
		{
			std::distance(begin(path), it)
		};

		if(unlikely(size_t(pos) >= max))
			throw error
			{
				"%s does not exist",
				cmd[i].file.name,
			};

		trig_idx[pos] = i;
		trig_num++;
	}

	const fs::fd::opts opts
	{
		std::ios::in | std::ios::out
	};

	// Open the fd's; if triggers were given we don't open files that were
	// not included in the cmd vector; otherwise we open all files.
	const fs::fd fd[max]
	{
		!trig_num || trig_idx[0] < max?
			fs::fd{path[0], opts}:
			fs::fd{},

		!trig_num || trig_idx[1] < max?
			fs::fd{path[1], opts}:
			fs::fd{},

		!trig_num || trig_idx[2] < max?
			fs::fd{path[2], opts}:
			fs::fd{},
	};

	// Write all triggers to their respective file
	for(size_t i(0); i < max; ++i)
	{
		if(trig_idx[i] >= max)
			continue;

		const auto &trig(cmd[trig_idx[i]]); try
		{
			// psi_write() in the kernel wants a write length of one greater
			// than the length of the string, but it places a \0 in its own
			// buffer unconditionally. This is noteworthy because our string
			// may not be null terminated and this length requirement smells.
			assert(trig.file.name == lstrip(path[i], "/proc/pressure/"));
			syscall(::write, fd[i], trig.string.c_str(), size(trig.string) + 1);
		}
		catch(const ctx::interrupted &)
		{
			throw;
		}
		catch(const std::exception &e)
		{
			log::error
			{
				"Failed to set pressure stall trigger [%s] on /proc/pressure/%s :%s",
				trig.string,
				trig.file.name,
				e.what(),
			};

			throw;
		}
	}

	// Yield ircd::ctx until fd[n] has a result.
	const size_t n
	{
		fs::select(fd)
	};

	switch(n)
	{
		case 0:  return cpu;
		case 1:  return mem;
		case 2:  return io;
		default:
			always_assert(false);
			__builtin_unreachable();
	}
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		"Failed to poll pressure stall information :%s",
		e.what(),
	};

	throw;
}

bool
ircd::prof::psi::refresh(file &file)
noexcept try
{
	if(!supported)
		return false;

	if(unlikely(!file.name))
		return false;

	const auto &path
	{
		fs::path(fs::path_scratch, vector_view<const string_view>
		{
			"/proc/pressure"_sv, file.name
		})
	};

	// Copy value into userspace
	char buf[256];
	fs::read_opts opts;
	opts.aio = false; // can't read /proc through AIO
	opts.all = false; // don't need posix read-loop; make one read(2) only.
	const auto &result
	{
		fs::read(path, buf, opts)
	};

	tokens(result, '\n', [&file] // Read each line
	(const string_view &line)
	{
		const auto &[type, vals]
		{
			split(line, ' ')
		};

		// The first token tells us what the metric is; we have allocated
		// results for the following
		if(type != "full" && type != "some")
			return;

		auto &metric
		{
			type == "full"?
				file.full:
				file.some
		};

		size_t i(0);
		tokens(vals, ' ', [&file, &metric, &i] // Read each key=value pair
		(const string_view &key_val)
		{
			const auto &[key, val]
			{
				split(key_val, '=')
			};

			if(key == "total")
			{
				const auto total(lex_cast<microseconds>(val));
				metric.stall.relative = total - metric.stall.total;
				metric.stall.window = duration_cast<microseconds>(now<system_point>() - file.sampled);
				metric.stall.pct = metric.stall.window.count()?
					metric.stall.relative.count() / double(metric.stall.window.count()):
					0.0;
				metric.stall.pct *= 100;
				metric.stall.total = total;
				return;
			}
			else if(startswith(key, "avg") && i < metric.avg.size())
			{
				metric.avg.at(i).window = lex_cast<seconds>(lstrip(key, "avg"));
				metric.avg.at(i).pct = lex_cast<float>(val);
				++i;
			}
		});
	});

	file.sampled = ircd::now<system_point>();
	return true;
}
catch(const std::exception &e)
{
	log::error
	{
		"Failed to refresh pressure stall information '%s' :%s",
		file.name,
		e.what(),
	};

	return false;
}
