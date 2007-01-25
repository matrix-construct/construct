/************************************************************************
 *   IRC - Internet Relay Chat, servlink/io.h
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
 *   $Id: io.h 6 2005-09-10 01:02:21Z nenolod $
 */

#ifndef INCLUDED_servlink_io_h
#define INCLUDED_servlink_io_h

#include "control.h"

#define IO_READ                 0
#define IO_WRITE                1
#define IO_SELECT               2

#define IO_TYPE(io)     ((io==IO_SELECT)?"select": \
                         ((io==IO_WRITE)?"write":"read"))

#define FD_NAME(fd)     (fd_name(fd))

extern void io_loop(int nfds);
extern void write_data(void);
extern void read_data(void);
extern void write_ctrl(void);
extern void read_ctrl(void);
extern void write_net(void);
extern void read_net(void);
extern void send_error(const char *, ...);
extern void send_data_blocking(int fd, unsigned char *data, int datalen);
extern cmd_handler process_recvq;
extern cmd_handler process_sendq;
extern cmd_handler send_zipstats;

#endif /* INCLUDED_servlink_io_h */
