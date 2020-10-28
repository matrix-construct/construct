// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_SYS_PRCTL_H
#include <boost/process.hpp>
#include <boost/process/extend.hpp>

struct ircd::exec::handler
:boost::process::extend::async_handler
{
	exec *e {nullptr};

	template<class executor> void on_fork_error(executor &, const std::error_code &) const noexcept;
	template<class executor> void on_exec_error(executor &, const std::error_code &) const noexcept;
	template<class executor> void on_error(executor&, const std::error_code &) noexcept;

	static void handle_exit(exec &, int, const std::error_code &) noexcept;

	template<class executor> void on_success(executor &) const noexcept;
	template<class executor> void on_exec_setup(executor &) const noexcept;
	template<class executor> void on_setup(executor &) noexcept;

	template<class executor>
	std::function<void (int, const std::error_code &)>
	on_exit_handler(executor &) const noexcept;

	handler(exec *const &);
	~handler() noexcept;
};

decltype(ircd::exec::log)
ircd::exec::log
{
	"exec"
};

decltype(ircd::exec::exec::id_ctr)
ircd::exec::exec::id_ctr;

template<>
decltype(ircd::util::instance_list<ircd::exec>::allocator)
ircd::util::instance_list<ircd::exec>::allocator{};

template<>
decltype(ircd::util::instance_list<ircd::exec>::list)
ircd::util::instance_list<ircd::exec>::list
{
	allocator
};

ircd::exec::exec(const args &args,
                 const opts &opt)
:opt
{
	std::make_unique<opts>(opt)
}
,path
(
	args.at(0)
)
,argv
(
	std::next(begin(args), 1), end(args)
)
,pipe
{
	std::make_unique<pair<boost::process::async_pipe>>
	(
		static_cast<asio::io_context &>(ios::main.context()),
		static_cast<asio::io_context &>(ios::main.context())
	)
}
{
}

ircd::exec::~exec()
noexcept try
{
	join(SIGKILL);
	dock.wait([this]
	{
		return this->pid <= 0;
	});
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "unhandled :%s",
		e.what(),
	};
}

long
ircd::exec::run()
try
{
	assert(pid <= 0);
	assert(!child);

	eptr = {};
	child = std::make_unique<boost::process::child>
	(
		fs::_path(path),
		argv,
		(boost::process::std_in) = pipe->first,
		(boost::process::std_out & boost::process::std_err) = pipe->second,
		handler{this},
		static_cast<asio::io_context &>(ios::main.context())
	);

	return pid;
}
catch(const std::system_error &e)
{
	eptr = std::current_exception();
	log::error
	{
		log, "id:%lu pid:%ld `%s' :%s",
		id,
		pid,
		path,
		e.what(),
	};

	throw;
}

long
ircd::exec::join(const int &sig)
try
{
	if(!signal(sig))
		return code;

	log::dwarning
	{
		log, "id:%lu pid:%ld `%s' signal:%d waiting for exit...",
		id,
		pid,
		path,
		sig,
	};

	//child->wait();
	dock.wait([this]
	{
		return this->pid <= 0;
	});

	assert(!child->running());
	return code;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "id:%lu pid:%ld `%s' join :%s",
		id,
		pid,
		path,
		e.what(),
	};

	throw;
}

bool
ircd::exec::signal(const int &sig)
{
	if(pid <= 0)
		return false;

	if(!child)
		return false;

	if(!child->valid())
		return false;

	if(!child->running())
		return false;

	const bool kill
	{
		#ifdef SIGKILL
			sig == SIGKILL
		#else
			sig == 9
		#endif
	};

	if(kill)
	{
		child->terminate();
		return true;
	}

	#if defined(HAVE_SIGNAL_H)
	if(sig)
		syscall(::kill, pid, sig);
	#endif

	return true;
}

size_t
ircd::exec::write(const const_buffers &bufs)
{
	assert(pipe);
	auto &pipe
	{
		this->pipe->first
	};

	const auto interruption
	{
		[&pipe](ctx::ctx *const &) noexcept
		{
			if(pipe.is_open())
				pipe.cancel();
		}
	};

	size_t ret{0}; ctx::continuation
	{
		continuation::asio_predicate, interruption, [&pipe, &bufs, &ret]
		(auto &yield)
        {
			ret = pipe.async_write_some(bufs, yield);
        }
    };

	return ret;
}

size_t
ircd::exec::read(const mutable_buffers &bufs)
{
	assert(pipe);
	auto &pipe
	{
		this->pipe->second
	};

	const auto interruption
	{
		[&pipe](ctx::ctx *const &) noexcept
		{
			if(pipe.is_open())
				pipe.cancel();
		}
	};

	boost::system::error_code ec;
	size_t ret{0}; ctx::continuation
	{
		continuation::asio_predicate, interruption, [&pipe, &bufs, &ret, &ec]
		(auto &yield)
        {
			ret = pipe.async_read_some(bufs, yield[ec]);
        }
    };

	if(ec)
	{
		assert(!ret);
		if(ec == boost::asio::error::eof)
			return 0;

		throw_system_error(ec);
		__builtin_unreachable();
	}

	assert(ret);
	return ret;
}

//
// exec::handler
//

ircd::exec::handler::handler(exec *const &e)
:e{e}
{
}

ircd::exec::handler::~handler()
noexcept
{
}

template<class executor>
std::function<void (int, const std::error_code &)>
ircd::exec::handler::on_exit_handler(executor &ex)
const noexcept
{
	return std::bind(&handler::handle_exit, std::ref(*this->e), ph::_1, ph::_2);
}

template<class executor>
void
ircd::exec::handler::on_setup(executor &ex)
noexcept
{
	assert(e);

	size_t argc(0), envc(0);
	for(auto p(ex.env); p && *p; ++p, ++envc);
	for(auto p(ex.cmd_line); p && *p; ++p, ++argc);

	log::logf
	{
		log, log::level::DEBUG,
		"id:%lu pid:%ld `%s' start; argc:%zu envc:%zu",
		e->id,
		ex.pid,
		ex.exe,
		argc,
		envc,
	};
}

template<class executor>
void
ircd::exec::handler::on_exec_setup(executor &ex)
const noexcept
{
	#if 0 // outputs from child; don't want
	assert(e);

	log::logf
	{
		log, log::level::DEBUG,
		"id:%lu pid:%ld `%s' executing...",
		e->id,
		e->pid,
		e->path,
	};
	#endif

	// Set the parent death signal in case of a crash so we won't go zombie.
	#if defined(HAVE_SYS_PRCTL_H)
	sys::call(prctl, PR_SET_PDEATHSIG, SIGTERM);
	#endif

	// Ignore SIGINT/SIGQUIT that way the administrator striking ctrl-c or
	// ctrl-\ at the console doesn't terminate the child.
	#if defined(HAVE_SIGNAL_H)
	sigset_t ours;
	sys::call(sigemptyset, &ours);
	sys::call(sigaddset, &ours, SIGINT);
	sys::call(sigaddset, &ours, SIGQUIT);
	sys::call(sigprocmask, SIG_BLOCK, &ours, nullptr);
	#endif
}

template<class executor>
void
ircd::exec::handler::on_success(executor &ex)
const noexcept
{
	assert(e);

	e->pid = ex.pid;
	e->dock.notify_all();
	if(ex.pid < 0)
		return;

	const auto &level
	{
		e->opt->exec_log_level
	};

	log::logf
	{
		log, level,
		"id:%lu pid:%ld `%s' spawned...",
		e->id,
		e->pid,
		e->path,
	};
}

void
ircd::exec::handler::handle_exit(exec &e,
                                 int code,
                                 const std::error_code &ec)
noexcept
{
	const unwind _{[&]
	{
		e.pid = 0;
		e.code = code;
		e.dock.notify_all();
	}};

	if(unlikely(ec))
	{
		char ecbuf[64];
		log::error
		{
			log, "id:%lu pid:%ld `%s' exit #%ld :%s",
			e.id,
			e.pid,
			e.path,
			ec.value(),
			string(ecbuf, ec),
		};

		return;
	}

	const auto &level
	{
		code == 0?
			e.opt->exit_log_level:
			log::level::ERROR
	};

	log::logf
	{
		log, level,
		"id:%lu pid:%ld `%s' exit (%ld)",
		e.id,
		e.pid,
		e.path,
		code,
	};
}

template<class executor>
void
ircd::exec::handler::on_error(executor &ex,
                              const std::error_code &ec)
noexcept
{
	assert(e);

	char ecbuf[64];
	log::critical
	{
		log, "id:%lu pid:%ld `%s' fail #%ld :%s",
		e->id,
		e->pid,
		e->path,
		e->code,
		ec.value(),
		string(ecbuf, ec),
	};
}

template<class executor>
void
ircd::exec::handler::on_exec_error(executor &ex,
                                   const std::error_code &ec)
const noexcept
{
	#if 0 // outputs from child; don't want
	assert(e);

	char ecbuf[64];
	log::error
	{
		log, "id:%lu pid:%ld `%s' exec() #%ld :%s",
		e->id,
		e->pid,
		e->path,
		ec.value(),
		string(ecbuf, ec),
	};
	#endif
}

template<class executor>
void
ircd::exec::handler::on_fork_error(executor &ex,
                                   const std::error_code &ec)
const noexcept
{
	assert(e);

	char ecbuf[64];
	log::critical
	{
		log, "id:%lu pid:%ld `%s' fork() #%ld :%s",
		e->id,
		e->pid,
		e->path,
		ec.value(),
		string(ecbuf, ec),
	};
}
