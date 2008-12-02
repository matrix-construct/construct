/*
 *  ircd-ratbox: A slightly useful ircd.
 *  linebuf.h: A header for the linebuf code.
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
 *  $Id: rb_linebuf.h 26092 2008-09-19 15:13:52Z androsyn $
 */

#ifndef RB_LIB_H
# error "Do not use linebuf.h directly"
#endif

#ifndef __LINEBUF_H__
#define __LINEBUF_H__

#define LINEBUF_COMPLETE        0
#define LINEBUF_PARTIAL         1
#define LINEBUF_PARSED          0
#define LINEBUF_RAW             1

struct _buf_line;
struct _buf_head;

/* How big we want a buffer - 510 data bytes, plus space for a '\0' */
#define BUF_DATA_SIZE		511

typedef struct _buf_line
{
	char buf[BUF_DATA_SIZE + 2];
	uint8_t terminated;	/* Whether we've terminated the buffer */
	uint8_t raw;		/* Whether this linebuf may hold 8-bit data */
	int len;		/* How much data we've got */
	int refcount;		/* how many linked lists are we in? */
} buf_line_t;

typedef struct _buf_head
{
	rb_dlink_list list;	/* the actual dlink list */
	int len;		/* length of all the data */
	int alloclen;		/* Actual allocated data length */
	int writeofs;		/* offset in the first line for the write */
	int numlines;		/* number of lines */
} buf_head_t;



/* they should be functions, but .. */
#define rb_linebuf_len(x)		((x)->len)
#define rb_linebuf_alloclen(x)	((x)->alloclen)
#define rb_linebuf_numlines(x)	((x)->numlines)

void rb_linebuf_init(size_t heap_size);
void rb_linebuf_newbuf(buf_head_t *);
void rb_linebuf_donebuf(buf_head_t *);
int rb_linebuf_parse(buf_head_t *, char *, int, int);
int rb_linebuf_get(buf_head_t *, char *, int, int, int);
void rb_linebuf_putmsg(buf_head_t *, const char *, va_list *, const char *, ...);
void rb_linebuf_put(buf_head_t *, const char *, ...);
void rb_linebuf_putbuf(buf_head_t * bufhead, const char *buffer);
void rb_linebuf_attach(buf_head_t *, buf_head_t *);
void rb_count_rb_linebuf_memory(size_t *, size_t *);
int rb_linebuf_flush(rb_fde_t *F, buf_head_t *);


#endif
