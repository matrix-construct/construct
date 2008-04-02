/************************************************************************
 *   IRC - Internet Relay Chat, servlink/servlink.c
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
 *   $Id: control.c 1285 2006-05-05 15:03:53Z nenolod $
 */

#include "setup.h"

#include <sys/types.h>

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#ifdef HAVE_LIBZ
#include <zlib.h>
#endif

#include "servlink.h"
#include "io.h"
#include "control.h"

static cmd_handler cmd_set_zip_out_level;
static cmd_handler cmd_start_zip_out;
static cmd_handler cmd_start_zip_in;
static cmd_handler cmd_init;

struct command_def command_table[] = {
	{CMD_SET_ZIP_OUT_LEVEL, cmd_set_zip_out_level, COMMAND_FLAG_DATA},
	{CMD_START_ZIP_OUT, cmd_start_zip_out, 0},
	{CMD_START_ZIP_IN, cmd_start_zip_in, 0},
	{CMD_INJECT_RECVQ, process_recvq, COMMAND_FLAG_DATA},
	{CMD_INJECT_SENDQ, process_sendq, COMMAND_FLAG_DATA},
	{CMD_INIT, cmd_init, 0},
	{CMD_ZIPSTATS, send_zipstats, 0},
	{0, 0, 0}
};

void
cmd_set_zip_out_level(struct ctrl_command *cmd)
{
#ifdef HAVE_LIBZ
	out_state.zip_state.level = *cmd->data;
	if((out_state.zip_state.level < -1) || (out_state.zip_state.level > 9))
		send_error("invalid compression level %d", out_state.zip_state.level);
#else
	send_error("can't set compression level - no libz support!");
#endif
}

void
cmd_start_zip_out(struct ctrl_command *cmd)
{
#ifdef HAVE_LIBZ
	int ret;

	if(out_state.zip)
		send_error("can't start compression - already started!");

	out_state.zip_state.z_stream.total_in = 0;
	out_state.zip_state.z_stream.total_out = 0;
	out_state.zip_state.z_stream.zalloc = (alloc_func) 0;
	out_state.zip_state.z_stream.zfree = (free_func) 0;
	out_state.zip_state.z_stream.data_type = Z_ASCII;

	if(out_state.zip_state.level <= 0)
		out_state.zip_state.level = Z_DEFAULT_COMPRESSION;

	if((ret = deflateInit(&out_state.zip_state.z_stream, out_state.zip_state.level)) != Z_OK)
		send_error("deflateInit failed: %s", zError(ret));

	out_state.zip = 1;
#else
	send_error("can't start compression - no libz support!");
#endif
}

void
cmd_start_zip_in(struct ctrl_command *cmd)
{
#ifdef HAVE_LIBZ
	int ret;

	if(in_state.zip)
		send_error("can't start decompression - already started!");

	in_state.zip_state.z_stream.total_in = 0;
	in_state.zip_state.z_stream.total_out = 0;
	in_state.zip_state.z_stream.zalloc = (alloc_func) 0;
	in_state.zip_state.z_stream.zfree = (free_func) 0;
	in_state.zip_state.z_stream.data_type = Z_ASCII;
	if((ret = inflateInit(&in_state.zip_state.z_stream)) != Z_OK)
		send_error("inflateInit failed: %s", zError(ret));
	in_state.zip = 1;
#else
	send_error("can't start decompression - no libz support!");
#endif
}


void
cmd_init(struct ctrl_command *cmd)
{
	if(in_state.active || out_state.active)
		send_error("CMD_INIT sent twice!");

	in_state.active = 1;
	out_state.active = 1;
	CONTROL.read_cb = read_ctrl;
	CONTROL.write_cb = NULL;
	LOCAL.read_cb = read_data;
	LOCAL.write_cb = NULL;
	REMOTE.read_cb = read_net;
	REMOTE.write_cb = NULL;
}
