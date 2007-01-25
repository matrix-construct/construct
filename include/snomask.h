/*
 * charybdis: An advanced ircd.
 * snomask.h: Management for user server-notice masks.
 *
 * Copyright (c) 2006 William Pitcock <nenolod@nenolod.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 */

#ifndef SNOMASK_H
#define SNOMASK_H

#include "client.h"

#define SNO_ADD			1
#define SNO_DEL			2

#define SNO_CCONNEXT		0x00000001
#define SNO_BOTS		0x00000002
#define SNO_CCONN		0x00000004
#define SNO_DEBUG		0x00000008
#define SNO_FULL		0x00000010
#define SNO_SKILL		0x00000020
#define SNO_NCHANGE		0x00000040
#define SNO_REJ			0x00000080
#define SNO_GENERAL		0x00000100
#define SNO_UNAUTH		0x00000200
#define SNO_EXTERNAL		0x00000400
#define SNO_SPY			0x00000800
#define SNO_OPERSPY		0x00001000

char *construct_snobuf(unsigned int val);
unsigned int parse_snobuf_to_mask(unsigned int val, const char *sno);
unsigned int find_snomask_slot(void);

extern int snomask_modes[];

#endif
