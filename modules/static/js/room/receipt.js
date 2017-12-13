/*
 * IRCd Charybdis 5/Matrix
 *
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk (jason@zemos.net)
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
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
 */

'use strict';


/**************************************
 * Receipts
 *
 */

room.receipt = {};

room.receipt.__proto__ = function(room)
{
	if(!maybe(() => room.local.receipt))
		room.local.receipt = {};

	this.local = room.local.receipt;
};

room.receipt.send = async function(event_id, type = "m.read", opts = {})
{
	if(this.control.receipt_in_progress)
		return false;

	this.control.receipt_in_progress = event_id;
	let request = client.m.rooms.receipt.post(this.id, event_id, type, opts); try
	{
		let data = await request.response;
		return;
	}
	finally
	{
		this.control.receipt_in_progress = null;
	}
};

room.receipt.send.current = function()
{
	let current = this.timeline.current_event();
	if(!current)
		return false;

	if(!current.event_id)
		return false;

	if(mc.event.is_local(current))
		return false;

	if(current.type == "m.receipt")
		return false;

	if(maybe(() => current["m.read"][mc.my.mxid]))
		return false;

	if(maybe(() => current.origin_server_ts <= this.local.ts))
		return false;

	let receipts =
	{
		[current.event_id]:
		{
			"m.read":
			{
				[mc.my.mxid]:
				{
					ts: mc.date.now(),
				},
			},
		},
	};

	this.local.ts = mc.now();
	this.receipt.update(receipts);
	return this.receipt.send(current.event_id);
};

room.receipt.update = function(receipts)
{
	Object.each(receipts, (event_id, read) =>
	{
		mayif(() => this.timeline.get(event_id), (event) =>
		{
			this.receipt.update.event(event, read);
		});
	});
};

/**
 * merges updates to an object now attached to the event "m.read"
 * which explicitly specified so other keys in the receipt may
 * not clobber.
 */
room.receipt.update.event = function(event, read)
{
	Object.update(event,
	{
		"m.read": read["m.read"],
	});
};

/**
 * Sorts the receipts by timestamp so they can be viewed in the order of
 * reception.
 *
 * THIS FUNCTION MUST HAVE STABLE RESULTS for Angular and receipts with
 * the same timestamp must have some alternative ordering property.
 *
 * @returns Array of mxids which are keys for receipts ordered by ts
 */
room.receipt.sorted = function(receipts)
{
	return Object.keys(receipts).sort((a, b) =>
	{
		let tsa = receipts[a].ts;
		let tsb = receipts[b].ts;

		if(tsa === undefined && tsb === undefined)
			return 0;
		else if(tsa === undefined)
			return -1;
		else if(tsb === undefined)
			return 1;
		else
			return tsa < tsb?  -1:
			       tsa > tsb?   1:
			       a < b?      -1:
			       a > b?       1:
			                    0;
	});

	return ret;
};
