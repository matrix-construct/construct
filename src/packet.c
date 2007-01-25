/*
 *  ircd-ratbox: A slightly useful ircd.
 *  packet.c: Packet handlers.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 *  $Id: packet.c 262 2005-09-22 00:38:45Z jilles $
 */
#include "stdinc.h"
#include "tools.h"
#include "commio.h"
#include "s_conf.h"
#include "s_serv.h"
#include "client.h"
#include "common.h"
#include "ircd.h"
#include "parse.h"
#include "packet.h"
#include "irc_string.h"
#include "memory.h"
#include "hook.h"
#include "send.h"

static char readBuf[READBUF_SIZE];
static void client_dopacket(struct Client *client_p, char *buffer, size_t length);


/*
 * parse_client_queued - parse client queued messages
 */
static void
parse_client_queued(struct Client *client_p)
{
	int dolen = 0;
	int checkflood = 1;

	if(IsAnyDead(client_p))
		return;

	if(IsUnknown(client_p))
	{
		int i = 0;

		for (;;)
		{
			/* rate unknown clients at MAX_FLOOD per loop */
			if(i >= MAX_FLOOD)
				break;

			dolen = linebuf_get(&client_p->localClient->
					    buf_recvq, readBuf, READBUF_SIZE,
					    LINEBUF_COMPLETE, LINEBUF_PARSED);

			if(dolen <= 0 || IsDead(client_p))
				break;

			client_dopacket(client_p, readBuf, dolen);
			i++;

			/* He's dead cap'n */
			if(IsAnyDead(client_p))
				return;
			/* if theyve dropped out of the unknown state, break and move
			 * to the parsing for their appropriate status.  --fl
			 */
			if(!IsUnknown(client_p))
				break;

		}
	}

	if(IsAnyServer(client_p) || IsExemptFlood(client_p))
	{
		while (!IsAnyDead(client_p) && (dolen = linebuf_get(&client_p->localClient->buf_recvq,
					   readBuf, READBUF_SIZE, LINEBUF_COMPLETE,
					   LINEBUF_PARSED)) > 0)
		{
			client_dopacket(client_p, readBuf, dolen);
		}
	}
	else if(IsClient(client_p))
	{

		if(IsOper(client_p) && ConfigFileEntry.no_oper_flood)
			checkflood = 0;
		/*
		 * Handle flood protection here - if we exceed our flood limit on
		 * messages in this loop, we simply drop out of the loop prematurely.
		 *   -- adrian
		 */
		for (;;)
		{
			/* This flood protection works as follows:
			 *
			 * A client is given allow_read lines to send to the server.  Every
			 * time a line is parsed, sent_parsed is increased.  sent_parsed
			 * is decreased by 1 every time flood_recalc is called.
			 *
			 * Thus a client can 'burst' allow_read lines to the server, any
			 * excess lines will be parsed one per flood_recalc() call.
			 *
			 * Therefore a client will be penalised more if they keep flooding,
			 * as sent_parsed will always hover around the allow_read limit
			 * and no 'bursts' will be permitted.
			 */
			if(checkflood)
			{
				if(client_p->localClient->sent_parsed >= client_p->localClient->allow_read)
					break;
			}

			/* allow opers 4 times the amount of messages as users. why 4?
			 * why not. :) --fl_
			 */
			else if(client_p->localClient->sent_parsed >= (4 * client_p->localClient->allow_read))
				break;

			dolen = linebuf_get(&client_p->localClient->
					    buf_recvq, readBuf, READBUF_SIZE,
					    LINEBUF_COMPLETE, LINEBUF_PARSED);

			if(!dolen)
				break;

			client_dopacket(client_p, readBuf, dolen);
			if(IsAnyDead(client_p))
				return;
			client_p->localClient->sent_parsed++;
		}
	}
}

/* flood_endgrace()
 *
 * marks the end of the clients grace period
 */
void
flood_endgrace(struct Client *client_p)
{
	SetFloodDone(client_p);

	/* Drop their flood limit back down */
	client_p->localClient->allow_read = MAX_FLOOD;

	/* sent_parsed could be way over MAX_FLOOD but under MAX_FLOOD_BURST,
	 * so reset it.
	 */
	client_p->localClient->sent_parsed = 0;
}

/*
 * flood_recalc
 *
 * recalculate the number of allowed flood lines. this should be called
 * once a second on any given client. We then attempt to flush some data.
 */
void
flood_recalc(int fd, void *data)
{
	struct Client *client_p = data;
	struct LocalUser *lclient_p = client_p->localClient;

	/* This can happen in the event that the client detached. */
	if(!lclient_p)
		return;

	/* allow a bursting client their allocation per second, allow
	 * a client whos flooding an extra 2 per second
	 */
	if(IsFloodDone(client_p))
		lclient_p->sent_parsed -= 2;
	else
		lclient_p->sent_parsed = 0;

	if(lclient_p->sent_parsed < 0)
		lclient_p->sent_parsed = 0;

	if(--lclient_p->actually_read < 0)
		lclient_p->actually_read = 0;

	parse_client_queued(client_p);

	if(IsAnyDead(client_p))
		return;

	/* and finally, reset the flood check */
	comm_setflush(fd, 1000, flood_recalc, client_p);
}

/*
 * read_ctrl_packet - Read a 'packet' of data from a servlink control
 *                    link and process it.
 */
void
read_ctrl_packet(int fd, void *data)
{
	struct Client *server = data;
	struct LocalUser *lserver = server->localClient;
	struct SlinkRpl *reply;
	int length = 0;
	unsigned char tmp[2];
	unsigned char *len = tmp;
	struct SlinkRplDef *replydef;
#ifdef USE_IODEBUG_HOOKS
	hook_data_int hdata;
#endif

	s_assert(lserver != NULL);
	if(IsAnyDead(server))
		return;

	reply = &lserver->slinkrpl;


	if(!reply->command)
	{
		reply->gotdatalen = 0;
		reply->readdata = 0;
		reply->data = NULL;

		length = read(fd, tmp, 1);

		if(length <= 0)
		{
			if((length == -1) && ignoreErrno(errno))
				goto nodata;
			error_exit_client(server, length);
			return;
		}

		reply->command = tmp[0];
	}

	for (replydef = slinkrpltab; replydef->handler; replydef++)
	{
		if((int)replydef->replyid == reply->command)
			break;
	}

	/* we should be able to trust a local slink process...
	 * and if it sends an invalid command, that's a bug.. */
	s_assert(replydef->handler);

	if((replydef->flags & SLINKRPL_FLAG_DATA) && (reply->gotdatalen < 2))
	{
		/* we need a datalen u16 which we don't have yet... */
		length = read(fd, len, (2 - reply->gotdatalen));
		if(length <= 0)
		{
			if((length == -1) && ignoreErrno(errno))
				goto nodata;
			error_exit_client(server, length);
			return;
		}

		if(reply->gotdatalen == 0)
		{
			reply->datalen = *len << 8;
			reply->gotdatalen++;
			length--;
			len++;
		}
		if(length && (reply->gotdatalen == 1))
		{
			reply->datalen |= *len;
			reply->gotdatalen++;
			if(reply->datalen > 0)
				reply->data = MyMalloc(reply->datalen);
		}

		if(reply->gotdatalen < 2)
			return;	/* wait for more data */
	}

	if(reply->readdata < reply->datalen)	/* try to get any remaining data */
	{
		length = read(fd, (reply->data + reply->readdata),
			      (reply->datalen - reply->readdata));
		if(length <= 0)
		{
			if((length == -1) && ignoreErrno(errno))
				goto nodata;
			error_exit_client(server, length);
			return;
		}

		reply->readdata += length;
		if(reply->readdata < reply->datalen)
			return;	/* wait for more data */
	}

#ifdef USE_IODEBUG_HOOKS
	hdata.client = server;
	hdata.arg1 = NULL;
	hdata.arg2 = reply->command;
	hdata.data = NULL;
	call_hook(h_iorecvctrl_id, &hdata);
#endif

	/* we now have the command and any data, pass it off to the handler */
	(*replydef->handler) (reply->command, reply->datalen, reply->data, server);

	/* reset SlinkRpl */
	if(reply->datalen > 0)
		MyFree(reply->data);
	reply->command = 0;

	if(IsAnyDead(server))
		return;

      nodata:
	/* If we get here, we need to register for another COMM_SELECT_READ */
	comm_setselect(fd, FDLIST_SERVER, COMM_SELECT_READ, read_ctrl_packet, server, 0);
}

/*
 * read_packet - Read a 'packet' of data from a connection and process it.
 */
void
read_packet(int fd, void *data)
{
	struct Client *client_p = data;
	struct LocalUser *lclient_p = client_p->localClient;
	int length = 0;
	int lbuf_len;

	int binary = 0;
#ifdef USE_IODEBUG_HOOKS
	hook_data_int hdata;
#endif
	if(IsAnyDead(client_p))
		return;

	/*
	 * Read some data. We *used to* do anti-flood protection here, but
	 * I personally think it makes the code too hairy to make sane.
	 *     -- adrian
	 */
	length = read(client_p->localClient->fd, readBuf, READBUF_SIZE);

	if(length <= 0)
	{
		if((length == -1) && ignoreErrno(errno))
		{
			comm_setselect(client_p->localClient->fd, FDLIST_IDLECLIENT,
				       COMM_SELECT_READ, read_packet, client_p, 0);
			return;
		}
		error_exit_client(client_p, length);
		return;
	}

#ifdef USE_IODEBUG_HOOKS
	hdata.client = client_p;
	hdata.arg1 = readBuf;
	hdata.arg2 = length;
	call_hook(h_iorecv_id, &hdata);
#endif

	if(client_p->localClient->lasttime < CurrentTime)
		client_p->localClient->lasttime = CurrentTime;
	client_p->flags &= ~FLAGS_PINGSENT;

	/*
	 * Before we even think of parsing what we just read, stick
	 * it on the end of the receive queue and do it when its
	 * turn comes around.
	 */
	if(IsHandshake(client_p) || IsUnknown(client_p))
		binary = 1;

	lbuf_len = linebuf_parse(&client_p->localClient->buf_recvq, readBuf, length, binary);

	lclient_p->actually_read += lbuf_len;

	if(IsAnyDead(client_p))
		return;
		
	/* Attempt to parse what we have */
	parse_client_queued(client_p);

	if(IsAnyDead(client_p))
		return;
		
	/* Check to make sure we're not flooding */
	if(!IsAnyServer(client_p) &&
	   (linebuf_alloclen(&client_p->localClient->buf_recvq) > ConfigFileEntry.client_flood))
	{
		if(!(ConfigFileEntry.no_oper_flood && IsOper(client_p)))
		{
			exit_client(client_p, client_p, client_p, "Excess Flood");
			return;
		}
	}

	/* If we get here, we need to register for another COMM_SELECT_READ */
	if(PARSE_AS_SERVER(client_p))
	{
		comm_setselect(client_p->localClient->fd, FDLIST_SERVER, COMM_SELECT_READ,
			      read_packet, client_p, 0);
	}
	else
	{
		comm_setselect(client_p->localClient->fd, FDLIST_IDLECLIENT,
			       COMM_SELECT_READ, read_packet, client_p, 0);
	}
}

/*
 * client_dopacket - copy packet to client buf and parse it
 *      client_p - pointer to client structure for which the buffer data
 *             applies.
 *      buffer - pointr to the buffer containing the newly read data
 *      length - number of valid bytes of data in the buffer
 *
 * Note:
 *      It is implicitly assumed that dopacket is called only
 *      with client_p of "local" variation, which contains all the
 *      necessary fields (buffer etc..)
 */
void
client_dopacket(struct Client *client_p, char *buffer, size_t length)
{
	s_assert(client_p != NULL);
	s_assert(buffer != NULL);

	if(client_p == NULL || buffer == NULL)
		return;
	if(IsAnyDead(client_p))
		return;
	/* 
	 * Update messages received
	 */
	++me.localClient->receiveM;
	++client_p->localClient->receiveM;

	/* 
	 * Update bytes received
	 */
	client_p->localClient->receiveB += length;

	if(client_p->localClient->receiveB > 1023)
	{
		client_p->localClient->receiveK += (client_p->localClient->receiveB >> 10);
		client_p->localClient->receiveB &= 0x03ff;	/* 2^10 = 1024, 3ff = 1023 */
	}

	me.localClient->receiveB += length;

	if(me.localClient->receiveB > 1023)
	{
		me.localClient->receiveK += (me.localClient->receiveB >> 10);
		me.localClient->receiveB &= 0x03ff;
	}

	parse(client_p, buffer, buffer + length);
}
