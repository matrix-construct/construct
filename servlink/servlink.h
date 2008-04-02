/************************************************************************
 *   IRC - Internet Relay Chat, servlink/servlink.h
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   $Id: servlink.h 1285 2006-05-05 15:03:53Z nenolod $
 */

#ifndef INCLUDED_servlink_servlink_h
#define INCLUDED_servlink_servlink_h

#include "setup.h"

#ifdef HAVE_LIBZ
#include <zlib.h>
#endif

/* do not use stdin/out/err, as it seems to break on solaris */
#define CONTROL               fds[0]
#define LOCAL                 fds[1]
#define REMOTE                 fds[2]

#undef  SERVLINK_DEBUG

#define READLEN                  16384

#ifdef HAVE_LIBZ
#define BUFLEN                   READLEN * 6	/* allow for decompression */
#else
#define BUFLEN                   READLEN
#endif


#ifdef HAVE_LIBZ
struct zip_state
{
	z_stream z_stream;
	int level;		/* compression level */
};
#endif

struct slink_state
{
	unsigned int crypt:1;
	unsigned int zip:1;
	unsigned int active:1;

	unsigned char buf[BUFLEN * 2];
	unsigned int ofs;
	unsigned int len;

#ifdef HAVE_LIBZ
	struct zip_state zip_state;
#endif
};


typedef void (io_callback) (void);

struct fd_table
{
	int fd;
	io_callback *read_cb;
	io_callback *write_cb;
};

extern struct slink_state in_state;
extern struct slink_state out_state;
extern struct fd_table fds[3];

#endif /* INCLUDED_servlink_servlink_h */
