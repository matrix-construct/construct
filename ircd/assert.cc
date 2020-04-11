// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_SIGNAL_H

#if defined(RB_ASSERT) && !defined(RB_ASSERT_INTRINSIC)
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunreachable-code"
#endif __clang__
void
__assert_fail(const char *__assertion,
              const char *__file,
              unsigned int __line,
              const char *__function)
{
	ircd::print_assertion(__assertion, __file, __line, __function);

	if(ircd::soft_assert)
		return;

	if(strcmp(RB_ASSERT, "quit") == 0)
		ircd::quit();

	else if(strcmp(RB_ASSERT, "trap") == 0)
		ircd::debugtrap();

	#if defined(HAVE_EXCEPTION)
	else if(strcmp(RB_ASSERT, "term") == 0)
	{
		std::terminate();
		__builtin_unreachable();
	}
	#endif

	#if defined(HAVE_CSTDLIB)
	else if(strcmp(RB_ASSERT, "abort") == 0)
	{
		abort();
		__builtin_unreachable();
	}
	#endif

	#if defined(HAVE_SIGNAL_H)
	else if(strcmp(RB_ASSERT, "SIGTRAP") == 0)
		raise(SIGTRAP);
	#endif

	#if defined(HAVE_SIGNAL_H)
	else if(strcmp(RB_ASSERT, "SIGSTOP") == 0)
		raise(SIGSTOP);
	#endif

	#if defined(HAVE_SIGNAL_H)
	else if(strcmp(RB_ASSERT, "SIGQUIT") == 0)
		raise(SIGQUIT);
	#endif

	else __builtin_trap();
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif __clang__
#endif

void
ircd::print_assertion(const char *const &__assertion,
                      const char *const &__file,
                      const unsigned &__line,
                      const char *const &__function)
noexcept
{
	if(strcmp(__assertion, "critical") == 0)
		return;

	fprintf(stderr, "\nassertion failed [%s +%u] %s :%s\n",
	        __file,
	        __line,
	        __function,
	        __assertion);
}
