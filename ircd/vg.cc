// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_VALGRIND_VALGRIND_H
#include <RB_INC_VALGRIND_MEMCHECK_H
#include <RB_INC_VALGRIND_CALLGRIND_H

///////////////////////////////////////////////////////////////////////////////
//
// ircd/allocator.h
//

bool
ircd::vg::active()
{
	#ifdef HAVE_VALGRIND_VALGRIND_H
	return RUNNING_ON_VALGRIND;
	#else
	return false;
	#endif
}

size_t
ircd::vg::errors()
{
	#ifdef HAVE_VALGRIND_VALGRIND_H
	return VALGRIND_COUNT_ERRORS;
	#else
	return 0;
	#endif
}

//
// valgrind memcheck
//

void
ircd::allocator::vg::set_noaccess(const const_buffer &buf)
{
	#ifdef HAVE_VALGRIND_MEMCHECK_H
	VALGRIND_MAKE_MEM_NOACCESS(data(buf), size(buf));
	#endif
}

void
ircd::allocator::vg::set_undefined(const const_buffer &buf)
{
	#ifdef HAVE_VALGRIND_MEMCHECK_H
	VALGRIND_MAKE_MEM_UNDEFINED(data(buf), size(buf));
	#endif
}

void
ircd::allocator::vg::set_defined(const const_buffer &buf)
{
	#ifdef HAVE_VALGRIND_MEMCHECK_H
	VALGRIND_MAKE_MEM_DEFINED(data(buf), size(buf));
	#endif
}

bool
ircd::allocator::vg::defined(const const_buffer &buf)
{
	#ifdef HAVE_VALGRIND_MEMCHECK_H
	return VALGRIND_CHECK_MEM_IS_DEFINED(data(buf), size(buf)) == 0;
	#else
	return true;
	#endif
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/prof.h
//

namespace ircd::prof::vg
{
	static bool _enabled;
}

void
ircd::prof::vg::stop()
noexcept
{
	#ifdef HAVE_VALGRIND_CALLGRIND_H
	CALLGRIND_STOP_INSTRUMENTATION;
	assert(_enabled);
	_enabled = false;
	#endif
}

void
ircd::prof::vg::start()
noexcept
{
	#ifdef HAVE_VALGRIND_CALLGRIND_H
	assert(!_enabled);
	_enabled = true;
	CALLGRIND_START_INSTRUMENTATION;
	#endif
}

void
ircd::prof::vg::reset()
{
	#ifdef HAVE_VALGRIND_CALLGRIND_H
	CALLGRIND_ZERO_STATS;
	#endif
}

void
ircd::prof::vg::toggle()
{
	#ifdef HAVE_VALGRIND_CALLGRIND_H
	CALLGRIND_TOGGLE_COLLECT;
	#endif
}

void
ircd::prof::vg::dump(const char *const reason)
{
	#ifdef HAVE_VALGRIND_CALLGRIND_H
	CALLGRIND_DUMP_STATS_AT(reason);
	#endif
}

bool
ircd::prof::vg::enabled()
{
	return _enabled;
}
