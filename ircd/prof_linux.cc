// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

static_assert
(
	__linux__,
	"This unit is only compiled for linux targets."
);

#include <RB_INC_SYS_SYSCALL_H
#include <RB_INC_SYS_IOCTL_H
#include <RB_INC_SYS_MMAN_H
#include <RB_INC_SYS_RESOURCE_H
#include <linux/perf_event.h>

#ifndef __clang__
	#define IRCD_PROF_ALWAYS_OPTIMIZE __attribute__((optimize("s"), flatten))
#else
	#define IRCD_PROF_ALWAYS_OPTIMIZE
#endif

namespace ircd::prof
{
	std::ostream &debug(std::ostream &, const ::perf_event_mmap_page &);

	template<class... args> event *
	create(group &,
	       const uint32_t &,
	       const uint64_t &,
	       args&&...);

	static event &leader(group &);
	static event *leader(group *const &);
}

struct ircd::prof::event
:instance_list<event>
{
	perf_event_attr attr;
	fs::fd fd;
	uint64_t id {0};
	size_t map_size {0};
	char *map {nullptr};
	perf_event_mmap_page *head {nullptr};
	const_buffer body;

	uint64_t rdpmc() const;
	long ioctl(const ulong &req, const long &arg = 0);
	void reset(const long & = 0);
	void enable(const long & = 0);
	void disable(const long & = 0);

	event(const int &group,
	      const uint32_t &type,
	      const uint64_t &config,
	      const bool &user,
	      const bool &kernel,
	      const bool &use_map = true);

	~event() noexcept;
};

template<>
decltype(ircd::util::instance_list<ircd::prof::event>::allocator)
ircd::util::instance_list<ircd::prof::event>::allocator
{};

template<>
decltype(ircd::util::instance_list<ircd::prof::event>::list)
ircd::util::instance_list<ircd::prof::event>::list
{
	allocator
};

//
// prof
//

void
ircd::prof::reset(group &group)
{
	leader(group).reset(PERF_IOC_FLAG_GROUP);
}

void
IRCD_PROF_ALWAYS_OPTIMIZE
ircd::prof::start(group &group)
{
	leader(group).enable(PERF_IOC_FLAG_GROUP);
}

void
IRCD_PROF_ALWAYS_OPTIMIZE
ircd::prof::stop(group &group)
{
	auto &leader(*group.front());
	leader.disable(PERF_IOC_FLAG_GROUP);
	assert(!group.empty());
}

ircd::prof::event &
IRCD_PROF_ALWAYS_OPTIMIZE
ircd::prof::leader(group &group)
{
	assert(!group.empty() && group.front());
	return *group.front();
}

ircd::prof::event *
ircd::prof::leader(group *const &group)
{
	return group && !group->empty()?
		group->front().get():
		nullptr;
}

template<class... args>
ircd::prof::event *
ircd::prof::create(group &group,
                   const uint32_t &type,
                   const uint64_t &config,
                   args&&... a)
try
{
	const int gfd
	{
		leader(&group)? leader(group).fd : -1
	};

	group.emplace_back(std::make_unique<event>
	(
		gfd, type, config, std::forward<args>(a)...
	));

	return group.back().get();
}
catch(const std::exception &e)
{
	log::dwarning
	{
		"Failed to create event type:%u config:%lu :%s",
		type,
		config,
		e.what()
	};

	return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
//
// prof/psi.h
//

decltype(ircd::prof::psi::supported)
ircd::prof::psi::supported
{
	info::kernel_version[0] > 4 ||
	(info::kernel_version[0] >= 4 && info::kernel_version[1] >= 20)
};

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

///////////////////////////////////////////////////////////////////////////////
//
// prof/instructions.h
//

ircd::prof::instructions::instructions()
{
	if(!create(this->group, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, true, false))
		throw error
		{
			"Cannot sample instruction counter."
		};

	reset(this->group);
	start(this->group);
}

ircd::prof::instructions::~instructions()
noexcept
{
}

const uint64_t &
IRCD_PROF_ALWAYS_OPTIMIZE
ircd::prof::instructions::sample()
{
	retired = prof::leader(group).rdpmc();
	return retired;
}

const uint64_t &
ircd::prof::instructions::at()
const
{
	return retired;
}

//
// time_*() suite
//

uint64_t
ircd::prof::time_thrd()
{
	struct ::timespec tv;
	syscall(::clock_gettime, CLOCK_THREAD_CPUTIME_ID, &tv);
	return ulong(tv.tv_sec) * 1000000000UL + tv.tv_nsec;
}

uint64_t
ircd::prof::time_proc()
{
	struct ::timespec tv;
	syscall(::clock_gettime, CLOCK_PROCESS_CPUTIME_ID, &tv);
	return ulong(tv.tv_sec) * 1000000000UL + tv.tv_nsec;
}

///////////////////////////////////////////////////////////////////////////////
//
// prof::system
//

decltype(ircd::prof::system::group)
ircd::prof::system::group;

ircd::prof::system
ircd::prof::operator-(const system &a,
                      const system &b)
{
	system ret(a);
	ret -= b;
	return ret;
}

ircd::prof::system
ircd::prof::operator+(const system &a,
                      const system &b)
{
	system ret(a);
	ret += b;
	return ret;
}

ircd::prof::system &
ircd::prof::operator-=(system &a,
                       const system &b)
{
	for(size_t i(0); i < a.size(); ++i)
		for(size_t j(0); j < a[i].size(); ++j)
			a[i][j] -= b[i][j];

	return a;
}

ircd::prof::system &
ircd::prof::operator+=(system &a,
                       const system &b)
{
	for(size_t i(0); i < a.size(); ++i)
		for(size_t j(0); j < a[i].size(); ++j)
			a[i][j] += b[i][j];

	return a;
}

ircd::prof::system &
ircd::prof::hotsample(system &s)
noexcept
{
	thread_local char buf[1024];

	auto &leader
	{
		prof::leader(system::group)
	};

	const const_buffer read
	{
		buf,  size_t(syscall(::read, int(leader.fd), buf, sizeof(buf)))
	};

	for_each(read, [&s]
	(const type &type, const uint64_t &val)
	{
		auto &r0
		{
			s.at(size_t(type.counter))
		};

		auto &r1
		{
			r0.at(size_t(type.dpl))
		};

		r1 = val;
	});

	return s;
}

void
ircd::prof::for_each(const const_buffer &buf,
                     const read_closure &closure)
{
	struct head
	{
		uint64_t nr, te, tr;
	}
	const *const &head
	{
		reinterpret_cast<const struct head *>(data(buf))
	};

	struct body
	{
		uint64_t val, id;
	}
	const *const &body
	{
		reinterpret_cast<const struct body *>(data(buf) + sizeof(struct head))
	};

	// Start with the pseudo-results; these should always be the same for
	// non-hw profiling, so the DPL is meaningless.
	closure(type{dpl::KERNEL, uint8_t(-1)}, head->te);
	closure(type{dpl::USER, uint8_t(-1)}, head->tr);

	// Iterate the result list
	for(size_t i(0); i < head->nr; ++i)
		for(auto it(begin(event::list)); it != end(event::list); ++it)
			if((*it)->id == body[i].id)
				return closure(type(**it), body[i].val);
}

ircd::prof::system::system(sample_t)
noexcept
{
	stop(group);
	hotsample(*this);
	start(group);
}

ircd::prof::system::~system()
noexcept
{
}

/*
	create(system::group, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_CLOCK,          true,  false);
	create(system::group, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_CLOCK,          false,  true);
	create(system::group, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_TASK_CLOCK,         true,  false);
	create(system::group, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_TASK_CLOCK,         false,  true);
	create(system::group, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MIN,    true,  false);
	create(system::group, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MIN,    false,  true);
	create(system::group, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MAJ,    true,  false);
	create(system::group, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MAJ,    false,  true);
	create(system::group, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES,   true,  false);
	create(system::group, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES,   false,  true);
	create(system::group, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS,     true,  false);
	create(system::group, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS,     false,  true);

	system::group.clear();
*/

///////////////////////////////////////////////////////////////////////////////
//
// prof::event
//

ircd::prof::event::event(const int &group,
                         const uint32_t &type,
                         const uint64_t &config,
                         const bool &user,
                         const bool &kernel,
                         const bool &use_map)
:attr{[&]
{
	struct ::perf_event_attr ret {0};
	ret.size = sizeof(ret);

	ret.type = type;
	ret.config = config;
	ret.exclude_user = !user;
	ret.exclude_kernel = !kernel;

	ret.read_format |= PERF_FORMAT_GROUP;
	ret.read_format |= PERF_FORMAT_ID;
	ret.read_format |= PERF_FORMAT_TOTAL_TIME_ENABLED;
	ret.read_format |= PERF_FORMAT_TOTAL_TIME_RUNNING;

	ret.exclude_idle = true;
	ret.exclude_host = false;
	ret.exclude_hv = true;
	ret.exclude_guest = true;
	ret.exclude_callchain_user = true;
	ret.exclude_callchain_kernel = true;

	ret.disabled = true;
	return ret;
}()}
,fd{[this, &group]
{
	ulong flags(0);
	flags |= PERF_FLAG_FD_CLOEXEC;

	const int cpu(-1);
	const pid_t pid(0);
	return int(syscall<SYS_perf_event_open>(&attr, pid, cpu, group, flags));
}()}
,id{[this]
{
	uint64_t ret;
	syscall(::ioctl, int(fd), PERF_EVENT_IOC_ID, &ret);
	return ret;
}()}
,map_size
{
	use_map && type == PERF_TYPE_HARDWARE?
		size_t(1UL + 0UL) * info::page_size:
		0UL
}
,map{[this]
{
	int prot(0);
	prot |= PROT_READ;
	prot |= PROT_WRITE;

	int flags(0);
	flags |= MAP_SHARED;

	void *const ret
	{
		map_size?
			::mmap(nullptr, map_size, prot, flags, int(this->fd), 0):
			nullptr
	};

	if(ret == (void *)-1)
		throw std::system_error
		{
			errno, std::system_category()
		};

	if(map_size && ret == nullptr)
		throw error
		{
			"mmap(2) failed on event (fd:%d)", int(fd)
		};

	return reinterpret_cast<char *>(ret);
}()}
,head
{
	map?
		reinterpret_cast<::perf_event_mmap_page *>(map):
		nullptr
}
,body
{
	head?
		map + head->data_offset:
		nullptr,
	head?
		head->data_size:
		0UL
}
{
	assert(size(body) % info::page_size == 0);
	assert(map_size % info::page_size == 0);
}

ircd::prof::event::~event()
noexcept
{
	assert(!map || map_size);
	assert(!map_size || map);

	if(map)
		syscall(::munmap, map, map_size);
}

inline void
IRCD_PROF_ALWAYS_OPTIMIZE
ircd::prof::event::disable(const long &arg)
{
	::ioctl(int(fd), PERF_EVENT_IOC_DISABLE, arg);
}

inline void
IRCD_PROF_ALWAYS_OPTIMIZE
ircd::prof::event::enable(const long &arg)
{
	const int &fd(this->fd);

	#if defined(__x86_64__)
		__builtin_ia32_mfence();
		__builtin_ia32_lfence();
	#endif

	::ioctl(fd, PERF_EVENT_IOC_ENABLE, arg);
}

void
ircd::prof::event::reset(const long &arg)
{
	ioctl(PERF_EVENT_IOC_RESET, arg);
}

long
ircd::prof::event::ioctl(const ulong &req,
                         const long &arg)
{
	return syscall(::ioctl, int(fd), req, arg);
}

inline uint64_t
IRCD_PROF_ALWAYS_OPTIMIZE
ircd::prof::event::rdpmc()
const
{
	assert(head->cap_user_time);
	assert(head->cap_user_rdpmc);

	uint64_t ret;
	uint32_t seq; do
	{
		seq = head->lock;
		__sync_synchronize();
		//assert(head->time_enabled == head->time_running);
		ret = head->offset;
		ret += head->index? x86::rdpmc(head->index - 1) : 0UL;
		__sync_synchronize();
	}
	while(head->lock != seq);
	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//
// prof::type
//

ircd::prof::type::type(const enum dpl &dpl,
                       const uint8_t &type_id,
                       const uint8_t &counter,
                       const uint8_t &cacheop,
                       const uint8_t &cacheres)
:dpl{dpl}
,type_id{type_id}
,counter{counter}
,cacheop{cacheop}
,cacheres{cacheres}
{
}

ircd::prof::type::type(const event &event)
:dpl
{
	event.attr.exclude_kernel? dpl::USER : dpl::KERNEL
}
,type_id
{
	uint8_t(event.attr.type)
}
,counter
{
	uint8_t(event.attr.config)
}
,cacheop
{
	uint8_t(event.attr.config >> 8)
}
,cacheres
{
	uint8_t(event.attr.config >> 16)
}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// internal
//

std::ostream &
ircd::prof::debug(std::ostream &s,
                  const ::perf_event_mmap_page &head)
{
	s << "version:               " << head.version              << std::endl;
	s << "compat:                " << head.compat_version       << std::endl;
	s << "lock:                  " << head.lock                 << std::endl;
	s << "index:                 " << head.index                << std::endl;
	s << "offset:                " << head.offset               << std::endl;
	s << "time_enabled:          " << head.time_enabled         << std::endl;
	s << "time_running:          " << head.time_running         << std::endl;
	s << "cap_user_rdpmc:        " << head.cap_user_rdpmc       << std::endl;
	s << "cap_user_time:         " << head.cap_user_time        << std::endl;
	s << "cap_user_time_zero:    " << head.cap_user_time_zero   << std::endl;
	s << "pmc_width:             " << head.pmc_width            << std::endl;
	s << "time_shift:            " << head.time_shift           << std::endl;
	s << "time_mult:             " << head.time_mult            << std::endl;
	s << "time_offset:           " << head.time_offset          << std::endl;
	s << "data_head:             " << head.data_head            << std::endl;
	s << "data_tail:             " << head.data_tail            << std::endl;
	s << "data_offset:           " << head.data_offset          << std::endl;
	s << "data_size:             " << head.data_size            << std::endl;
	s << "aux_head:              " << head.aux_head             << std::endl;
	s << "aux_tail:              " << head.aux_tail             << std::endl;
	s << "aux_offset:            " << head.aux_offset           << std::endl;
	s << "aux_size:              " << head.aux_size             << std::endl;

	return s;
}
