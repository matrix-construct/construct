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

/**
 ******************************************************************************
 * Public Rooms
 *
 */

mc.rooms.public = {};

mc.rooms.menu["PUBLIC"] =
{
	icon: "fa-rss",
	selected: () => mc.rooms.mode == "PUBLIC",
	choice: () => mc.rooms.search.feedback == "BY SERVER",
	order: 5,
};

mc.rooms.menu["GLOBAL"] =
{
	order: 6,
	icon: "fa-globe",
	selected: () => mc.rooms.mode == "GLOBAL",
	choice: () => mc.rooms.search.feedback == "BY SERVER",
};

mc.rooms.menu["GLOBAL"].click = async function($event)
{
	let opts =
	{
		query:
		{
			server: mc.rooms.search.value,
		},

		content:
		{
			include_all_networks: true,
			third_party_instance_id: undefined,
		},
	};

	mc.rooms.mode = "GLOBAL";
	return mc.rooms.public.click($event, opts);
};

mc.rooms.menu["PUBLIC"].click = async function($event)
{
	let opts =
	{
		query:
		{
			server: mc.rooms.search.value,
		},
	};

	mc.rooms.mode = "PUBLIC";
	return mc.rooms.public.click($event, opts);
};

mc.rooms.public.click = async function($event, opts = {})
{
	mc.rooms.loading = true;
	let rooms = await mc.rooms.public.fetch(opts);
	mc.rooms.list = rooms.map(mc.rooms.get);
	mc.rooms.list.opts = opts;
	mc.rooms.loading = false;
};

/** IO to fetch public rooms.
 * The results come in chunks of an array. One chunk is returned from this
 * function per call. To receieve additional chunks of the same result, call
 * this function again with the same ctx.
 */
mc.rooms.public.fetch = async function(ctx = {})
{
	Object.defaults(ctx,
	{
		estimate: 0,
		count: 0,
		query:
		{
			limit: 64,
		},
	});

	let request = mc.m.publicRooms.get(ctx);
	let data = await request.response;
	let rooms = data.chunk;

	ctx.estimate = data.total_room_count_estimate;
	ctx.query.since = data.next_batch;
	ctx.count += rooms.length;

	return rooms;
};
