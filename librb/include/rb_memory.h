/*
 *  ircd-ratbox: A slightly useful ircd.
 *  memory.h: A header for the memory functions.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 */

#ifndef RB_LIB_H
#error "Do not use rb_memory.h directly"
#endif

#ifndef _RB_MEMORY_H
#define _RB_MEMORY_H

#include <stdlib.h>

#define RB_UNIQUE_PTR(deleter) __attribute__((cleanup(deleter)))
#define RB_AUTO_PTR RB_UNIQUE_PTR(rb_raii_free)

void rb_outofmemory(void) __attribute__((noreturn));

#ifdef __clang__
__attribute__((malloc, returns_nonnull))
#else
__attribute__((malloc, alloc_size(1), returns_nonnull))
#endif
static inline void *
rb_malloc(size_t size)
{
	void *ret = calloc(1, size);
	if(rb_unlikely(ret == NULL))
		rb_outofmemory();
	return (ret);
}

#ifdef __clang__
__attribute__((returns_nonnull))
#else
__attribute__((alloc_size(2), returns_nonnull))
#endif
static inline void *
rb_realloc(void *x, size_t y)
{
	void *ret = realloc(x, y);

	if(rb_unlikely(ret == NULL))
		rb_outofmemory();
	return (ret);
}

__attribute__((returns_nonnull))
static inline char *
rb_strndup(const char *x, size_t y)
{
	char *ret = (char *)malloc(y);
	if(rb_unlikely(ret == NULL))
		rb_outofmemory();
	rb_strlcpy(ret, x, y);
	return (ret);
}

__attribute__((returns_nonnull))
static inline char *
rb_strdup(const char *x)
{
	char *ret = (char *)malloc(strlen(x) + 1);
	if(rb_unlikely(ret == NULL))
		rb_outofmemory();
	strcpy(ret, x);
	return (ret);
}

// overrule libc's prototype and ignore the cast warning, allowing proper const
// propagation without casting everywhere in the real code.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
static inline void
rb_free(const void *const ptr)
{
	if(rb_likely(ptr != NULL))
		free((void *)ptr);
}
#pragma GCC diagnostic pop

static inline void
rb_raii_free(const void *const ptr)
{
	if(!ptr)
		return;

	const void *const _ptr = *(const void *const *)ptr;
	if(!_ptr)
		return;

	rb_free(_ptr);
}

#endif /* _I_MEMORY_H */
