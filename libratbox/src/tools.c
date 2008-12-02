/*
 *  ircd-ratbox: A slightly useful ircd.
 *  tools.c: Various functions needed here and there.
 *
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
 *  $Id: tools.c 26170 2008-10-26 20:59:07Z androsyn $
 *
 *  Here is the original header:
 *
 *  Useful stuff, ripped from places ..
 *  adrian chadd <adrian@creative.net.au>
 *
 *  The TOOLS_C define builds versions of the functions in tools.h
 *  so that they end up in the resulting object files.  If its not
 *  defined, tools.h will build inlined versions of the functions
 *  on supported compilers
 */
#define _GNU_SOURCE 1
#include <libratbox_config.h>
#include <ratbox_lib.h>
#include <rb_tools.h>


/*
 * init_rb_dlink_nodes
 *
 */
static rb_bh *dnode_heap;
void
rb_init_rb_dlink_nodes(size_t dh_size)
{

	dnode_heap = rb_bh_create(sizeof(rb_dlink_node), dh_size, "librb_dnode_heap");
	if(dnode_heap == NULL)
		rb_outofmemory();
}

/*
 * make_rb_dlink_node
 *
 * inputs	- NONE
 * output	- pointer to new rb_dlink_node
 * side effects	- NONE
 */
rb_dlink_node *
rb_make_rb_dlink_node(void)
{
	return (rb_bh_alloc(dnode_heap));
}

/*
 * free_rb_dlink_node
 *
 * inputs	- pointer to rb_dlink_node
 * output	- NONE
 * side effects	- free given rb_dlink_node 
 */
void
rb_free_rb_dlink_node(rb_dlink_node *ptr)
{
	assert(ptr != NULL);
	rb_bh_free(dnode_heap, ptr);
}

/* rb_string_to_array()
 *   Changes a given buffer into an array of parameters.
 *   Taken from ircd-ratbox.
 *
 * inputs	- string to parse, array to put in
 * outputs	- number of parameters
 */
int
rb_string_to_array(char *string, char **parv, int maxpara)
{
	char *p, *xbuf = string;
	int x = 0;

	parv[x] = NULL;

	if(string == NULL || string[0] == '\0')
		return x;

	while(*xbuf == ' ')	/* skip leading spaces */
		xbuf++;
	if(*xbuf == '\0')	/* ignore all-space args */
		return x;

	do
	{
		if(*xbuf == ':')	/* Last parameter */
		{
			xbuf++;
			parv[x++] = xbuf;
			parv[x] = NULL;
			return x;
		}
		else
		{
			parv[x++] = xbuf;
			parv[x] = NULL;
			if((p = strchr(xbuf, ' ')) != NULL)
			{
				*p++ = '\0';
				xbuf = p;
			}
			else
				return x;
		}
		while(*xbuf == ' ')
			xbuf++;
		if(*xbuf == '\0')
			return x;
	}
	while(x < maxpara - 1);

	if(*p == ':')
		p++;

	parv[x++] = p;
	parv[x] = NULL;
	return x;
}

#ifndef HAVE_STRLCAT
size_t
rb_strlcat(char *dest, const char *src, size_t count)
{
	size_t dsize = strlen(dest);
	size_t len = strlen(src);
	size_t res = dsize + len;

	dest += dsize;
	count -= dsize;
	if(len >= count)
		len = count - 1;
	memcpy(dest, src, len);
	dest[len] = 0;
	return res;
}
#else
size_t
rb_strlcat(char *dest, const char *src, size_t count)
{
	return strlcat(dest, src, count);
}
#endif

#ifndef HAVE_STRLCPY
size_t
rb_strlcpy(char *dest, const char *src, size_t size)
{
	size_t ret = strlen(src);

	if(size)
	{
		size_t len = (ret >= size) ? size - 1 : ret;
		memcpy(dest, src, len);
		dest[len] = '\0';
	}
	return ret;
}
#else
size_t
rb_strlcpy(char *dest, const char *src, size_t size)
{
	return strlcpy(dest, src, size);
}
#endif


#ifndef HAVE_STRNLEN
size_t
rb_strnlen(const char *s, size_t count)
{
	const char *sc;
	for(sc = s; count-- && *sc != '\0'; ++sc)
		;;
	return sc - s;
}
#else
size_t
rb_strnlen(const char *s, size_t count)
{
	return strnlen(s, count);
}
#endif

/* rb_basename
 *
 * input        -
 * output       -
 * side effects -  
 */
char *
rb_basename(const char *path)
{
        const char *s;

        if(!(s = strrchr(path, '/')))
                s = path;
        else
                s++;
	return rb_strdup(s);
}

/*
 * rb_dirname
 */

char *
rb_dirname (const char *path)
{
	char *s;

	s = strrchr(path, '/');
	if(s == NULL)
	{
		return rb_strdup(".");
	}	

	/* remove extra slashes */
	while(s > path && *s == '/')
		--s;

	return rb_strndup(path, ((uintptr_t)s - (uintptr_t)path) + 2); 
}
