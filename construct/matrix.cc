// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/matrix.h>
#include "construct.h"
#include "matrix.h"

decltype(construct::matrix)
construct::matrix;

void
construct::matrix::init()
{
	assert(construct::matrix == nullptr);
	construct::matrix = new matrix{};
}

void
construct::matrix::quit()
{
	if(!construct::matrix)
		return;

	construct::matrix->context.terminate();
}

construct::matrix::matrix()
:context
{
	"matrix",
	1048576,
	std::bind(&matrix::main, this),
	ircd::context::DISPATCH,
}
{
}

construct::matrix::~matrix()
noexcept
{
}

void
construct::matrix::main()
noexcept try
{
	ircd::run::changed::dock.wait([]
	{
		return ircd::run::level != ircd::run::level::READY;
	});

	if(ircd::run::level != ircd::run::level::START || ircd::run::level != ircd::run::level::RUN)
		return;

	ircd::matrix instance
	{

	};

	const ircd::scope_restore this_instance
	{
		this->instance, std::addressof(instance)
	};

	ircd::mods::import<ircd::log::log> log
	{
		instance.module, "ircd::m::log"
	};

	ircd::log::notice
	{
		log, "Matrix Constructed"
	};

	dock.notify_all();
	ircd::run::changed::dock.wait([]
	{
		return false
		|| ircd::run::level == ircd::run::level::QUIT
		|| ircd::run::level == ircd::run::level::HALT
		;
	});

	ircd::log::notice
	{
		log, "Matrix Shutdown..."
	};
}
catch(const ircd::ctx::interrupted &e)
{
	ircd::log::debug
	{
		"construct::matrix :%s", e.what()
	};
}
catch(const ircd::ctx::terminated &e)
{
	ircd::log::debug
	{
		"construct::matrix: terminated."
	};
}
catch(const std::exception &e)
{
	ircd::log::error
	{
		"construct::matrix :%s", e.what()
	};
}
