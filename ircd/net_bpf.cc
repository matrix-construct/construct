// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <sys/syscall.h>
#include <linux/bpf.h>

namespace ircd::net::bpf
{
	struct [[clang::internal_linkage]] call;

	static constexpr uint
	log_bufs{8},
	log_buf_sz{4_KiB};

	static thread_local uint log_bufn;
	static thread_local char log_buf[log_bufs][log_buf_sz];
}

class [[gnu::visibility("internal")]]
ircd::net::bpf::call
{
	int ret;

  public:
	operator const int &() const;

	call(const int &, union bpf_attr *const &);
	call(const int &, const union bpf_attr &);
};

decltype(ircd::net::bpf::log)
ircd::net::bpf::log
{
	"net.bpf"
};

//
// bpf::prog
//

ircd::net::bpf::prog::prog(const const_buffer &insns)
:prog
{
	insns, mutable_buffer
	{
		bpf::log_buf[log_bufn++ % log_bufs], log_buf_sz
	}
}
{
	assert(log_bufn <= log_bufs); //TODO: XXX
}

ircd::net::bpf::prog::prog(const const_buffer &insns,
                           const mutable_buffer &log_buf)
try
:insns
{
	insns
}
,log_buf
{
	log_buf
}
,fd
{
	#if defined(__clang__) || RB_CXX_EPOCH >= 11
	empty(insns)? -1: call
	{
		BPF_PROG_LOAD, bpf_attr
		{
			.prog_type = BPF_PROG_TYPE_SOCKET_FILTER,
			.insn_cnt = u32(size(insns) / sizeof(bpf_insn)),
			.insns = uintptr_t(data(insns)),
			#ifdef _GNU_SOURCE
			.license = uintptr_t("GPL"),
			#endif
			.log_level = !empty(log_buf)? 1U: 0,
			.log_size = u32(size(log_buf)),
			.log_buf = uintptr_t(data(log_buf)),
		}
	}
	#endif
}
{
	if(!fd)
		return;

	log::debug
	{
		log, "Loaded prog:%p fd:%d bin:%p bytes:%zu",
		this,
		int(fd),
		ircd::data(insns),
		ircd::size(insns),
	};
}
catch(const std::exception &e)
{
	const string_view log_str
	{
		data(log_buf), strnlen(data(log_buf), size(log_buf))
	};

	uint i(0);
	ircd::tokens(log_str, '\n', [this, &i]
	(const string_view &line)
	{
		if(likely(line))
			log::error
			{
				log, "Log prog:%p %2u :%s",
				this,
				i++,
				line,
			};
	});

	log::critical
	{
		log, "Failed to load prog:%p bin:%p bytes:%zu :%s",
		this,
		data(insns),
		size(insns),
		e.what(),
	};

	throw;
}

ircd::net::bpf::prog::~prog()
noexcept
{
	if(!fd)
		return;

	log::debug
	{
		log, "Unloading prog:%p fd:%d ...",
		this,
		int(fd),
	};
}

//
// bpf::map
//

ircd::net::bpf::map::map()
try
:fd
{
	#if defined(__clang__) || RB_CXX_EPOCH >= 11
	call
	{
		BPF_MAP_CREATE, bpf_attr
		{
			.map_type = BPF_MAP_TYPE_UNSPEC,
			.key_size = 8,
			.value_size = 8,
			.max_entries = 8,
		},
	},
	#endif
}
{
}
catch(const std::exception &e)
{
	char pbuf[48];
	log::error
	{
		log, "Mapping failed :%s",
		e.what(),
	};

	throw;
}

ircd::net::bpf::map::~map()
noexcept
{
}

//
// internal
//

ircd::net::bpf::call::call(const int &cmd,
                           const union bpf_attr &attr)
:call
{
	cmd, mutable_cast(&attr)
}
{
}

ircd::net::bpf::call::call(const int &cmd,
                           union bpf_attr *const &attr)
:ret
{
	int(sys::call<SYS_bpf>(cmd, attr, sizeof(union bpf_attr)))
}
{
}

ircd::net::bpf::call::operator
const int &()
const
{
	return ret;
}
