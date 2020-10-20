// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <boost/process.hpp>

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

ircd::exec::exec(const args &args)
:id
{
	++id_ctr
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
,child
{
	std::make_unique<boost::process::child>
	(
		fs::_path(path),
		argv,
		(boost::process::std_in) = pipe->first,
		(boost::process::std_out & boost::process::std_err) = pipe->second
	)
}
,pid
{
	child->id()
}
{
	log::notice
	{
		log, "id:%lu pid:%ld `%s' exec argc:%zu",
		id,
		pid,
		path,
		argv.size(),
	};
}

ircd::exec::~exec()
noexcept try
{
	join(SIGKILL);
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
ircd::exec::join(const int &sig)
try
{
	if(!child)
		return code;

	if(!child->valid())
		return code;

	if(!child->running())
		return code;

	// when milliseconds=0 this branch appears to be taken too much
	if(!child->wait_for(milliseconds(10)))
	{
		const bool signaled
		{
			signal(sig)
		};

		log::dwarning
		{
			log, "id:%lu pid:%ld `%s' signal:%d waiting for exit...",
			id,
			pid,
			path,
			sig,
		};

		child->wait();
	}

	assert(!child->running());
	code = child->exit_code();

	const auto &level
	{
		code == 0?
			log::level::INFO:
			log::level::ERROR
	};

	log::logf
	{
		log, level,
		"id:%lu pid:%ld `%s' exit (%ld)",
		id,
		pid,
		path,
		code,
	};

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
		child->terminate();

	return true;
}

size_t
ircd::exec::write(const const_buffers &bufs)
{
	assert(pipe);
	assert(child);
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
	assert(child);
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
