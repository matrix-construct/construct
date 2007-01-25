/*
 *  ircd-ratbox: A slightly useful ircd.
 *  linebuf.h: A header for the linebuf code.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 *  $Id: linebuf.h 376 2005-12-07 15:00:41Z nenolod $
 */

#ifndef __LINEBUF_H__
#define __LINEBUF_H__

#include "tools.h"

/* How big we want a buffer - 510 data bytes, plus space for a '\0' */
#define BUF_DATA_SIZE		511

#define LINEBUF_COMPLETE        0
#define LINEBUF_PARTIAL         1
#define LINEBUF_PARSED          0
#define LINEBUF_RAW             1

struct _buf_line;
struct _buf_head;

typedef struct _buf_line buf_line_t;
typedef struct _buf_head buf_head_t;

struct _buf_line
{
	char buf[BUF_DATA_SIZE + 2];
	unsigned int terminated;	/* Whether we've terminated the buffer */
	unsigned int flushing;	/* Whether we're flushing .. */
	unsigned int raw;	/* Whether this linebuf may hold 8-bit data */
	int len;		/* How much data we've got */
	int refcount;		/* how many linked lists are we in? */
	struct _buf_line *next;	/* next in free list */
};

struct _buf_head
{
	dlink_list list;	/* the actual dlink list */
	int len;		/* length of all the data */
	int alloclen;		/* Actual allocated data length */
	int writeofs;		/* offset in the first line for the write */
	int numlines;		/* number of lines */
};

/* they should be functions, but .. */
#define linebuf_len(x)		((x)->len)
#define linebuf_alloclen(x)	((x)->alloclen)
#define linebuf_numlines(x)	((x)->numlines)

extern void linebuf_init(void);
/* declared as static */
/* extern buf_line_t *linebuf_new_line(buf_head_t *); */
/* extern void linebuf_done_line(buf_head_t *, buf_line_t *, dlink_node *); */
/* extern int linebuf_skip_crlf(char *, int); */
/* extern void linebuf_terminate_crlf(buf_head_t *, buf_line_t *); */
extern void linebuf_newbuf(buf_head_t *);
extern void client_flush_input(struct Client *);
extern void linebuf_donebuf(buf_head_t *);
extern int linebuf_parse(buf_head_t *, char *, int, int);
extern int linebuf_get(buf_head_t *, char *, int, int, int);
extern void linebuf_putmsg(buf_head_t *, const char *, va_list *, const char *, ...);
extern int linebuf_flush(int, buf_head_t *);
extern void linebuf_attach(buf_head_t *, buf_head_t *);
extern void count_linebuf_memory(size_t *, size_t *);
#endif
