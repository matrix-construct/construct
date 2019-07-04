// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_MALLOC_H

#if defined(__GNU_LIBRARY__) && defined(HAVE_MALLOC_H) && defined(IRCD_ALLOCATOR_USE_DEFAULT)
	#define IRCD_ALLOCATOR_USE_GNU
#endif

namespace ircd::allocator
{
	void *(*their_malloc_hook)(size_t, const void *);
	static void *malloc_hook(size_t, const void *);
	static void install_malloc_hook();
	static void uninstall_malloc_hook();

	void *(*their_realloc_hook)(void *, size_t, const void *);
	static void *realloc_hook(void *, size_t, const void *);
	static void install_realloc_hook();
	static void uninstall_realloc_hook();

	void (*their_free_hook)(void *, const void *);
	static void free_hook(void *, const void *);
	static void install_free_hook();
	static void uninstall_free_hook();
}

#ifdef IRCD_ALLOCATOR_USE_GNU
ircd::string_view
ircd::allocator::info(const mutable_buffer &buf)
{
	std::stringstream out;
	pubsetbuf(out, buf);

	const auto ma
	{
		::mallinfo()
	};

	char pbuf[96];
	out << "arena:       " << pretty(pbuf, iec(ma.arena)) << std::endl
	    << "ordblks:     " << ma.ordblks << std::endl
	    << "smblks:      " << ma.smblks << std::endl
	    << "hblks:       " << ma.hblks << std::endl
	    << "hblkhd:      " << pretty(pbuf, iec(ma.hblkhd)) << std::endl
	    << "usmblks:     " << pretty(pbuf, iec(ma.usmblks)) << std::endl
	    << "fsmblks:     " << pretty(pbuf, iec(ma.fsmblks)) << std::endl
	    << "uordblks:    " << pretty(pbuf, iec(ma.uordblks)) << std::endl
	    << "fordblks:    " << pretty(pbuf, iec(ma.fordblks)) << std::endl
	    << "keepcost:    " << pretty(pbuf, iec(ma.keepcost)) << std::endl
	    ;

	return view(out, buf);
}
#endif

#ifdef IRCD_ALLOCATOR_USE_GNU
bool
ircd::allocator::trim(const size_t &pad)
noexcept
{
	return malloc_trim(pad);
}
#endif

#ifdef IRCD_ALLOCATOR_USE_GNU
void
ircd::allocator::scope::hook_init()
noexcept
{
	install_malloc_hook();
	install_realloc_hook();
	install_free_hook();
}
#endif

#ifdef IRCD_ALLOCATOR_USE_GNU
void
ircd::allocator::scope::hook_fini()
noexcept
{
	uninstall_malloc_hook();
	uninstall_realloc_hook();
	uninstall_free_hook();
}
#endif

#if defined(__GNU_LIBRARY__) && defined(HAVE_MALLOC_H)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
void
ircd::allocator::install_malloc_hook()
{
	assert(!their_malloc_hook);
	their_malloc_hook = __malloc_hook;
	__malloc_hook = malloc_hook;
}
#pragma GCC diagnostic pop
#endif

#if defined(__GNU_LIBRARY__) && defined(HAVE_MALLOC_H)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
void
ircd::allocator::uninstall_malloc_hook()
{
	__malloc_hook = their_malloc_hook;
}
#pragma GCC diagnostic pop
#endif

#if defined(__GNU_LIBRARY__) && defined(HAVE_MALLOC_H)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
void
ircd::allocator::install_realloc_hook()
{
	assert(!their_realloc_hook);
	their_realloc_hook = __realloc_hook;
	__realloc_hook = realloc_hook;
}
#pragma GCC diagnostic pop
#endif

#if defined(__GNU_LIBRARY__) && defined(HAVE_MALLOC_H)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
void
ircd::allocator::uninstall_realloc_hook()
{
	__realloc_hook = their_realloc_hook;
}
#endif

#if defined(__GNU_LIBRARY__) && defined(HAVE_MALLOC_H)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
void
ircd::allocator::install_free_hook()
{
	assert(!their_free_hook);
	their_free_hook = __free_hook;
	__free_hook = free_hook;
}
#pragma GCC diagnostic pop
#endif

#if defined(__GNU_LIBRARY__) && defined(HAVE_MALLOC_H)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
void
ircd::allocator::uninstall_free_hook()
{
	__free_hook = their_free_hook;
}
#pragma GCC diagnostic pop
#endif
