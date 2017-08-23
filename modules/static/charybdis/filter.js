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

/******************************************************************************
 *
 * Filter subsystem
 *
 */

/**
 * Get a filter ID by name. This is the primary API for functions preparing query
 * strings to make filtered requests with pre-defined/named filters.
 */
mc.filter = function(name)
{
	return maybe(() => mc.filter.filter[name].filter_id);
};

/**
 * This holds our predefined filters. They will obtain ID's when registered with
 * the server. A container in mc.session also holds ID's obtained with a string
 * as a value naming the filter in this object.
 */
mc.filter.filter =
{
	// Primary filter
	sync:
	{
		// ID given by server. We cross-reference in mc.session.filter[id]
		filter_id: undefined,

		// Actual filter
		content:
		{
		},
	},

	initial_sync:
	{
		content:
		{
			room:
			{
				not_rooms:
				[
					//"!vfFxDRtZSSdspfTSEr:matrix.org",
				],

				state:
				{
					not_types:
					[
						"m.room.member",
					],
				},
			},
		},
	},

	image_search:
	{
		content:
		{
			event_fields:
			[
//				"event_id",
				//"content.msgtype",
			],
/*
			room:
			{
				timeline:
				{
					types:
					[
						//"m.room.message",
					],
				},
			},
*/
		},
	},
};

Object.clean(mc.filter.filter);


/* Filter initialization on client load. Checks the filter ID's remembered
 * from storage and posts any new filters the server doesn't know about.
 *
 * TODO: This will be removed and the primary filter API will manage the filters as
 * it's called instead using await to post filters and return with IDs if necessary.
 */
mc.filter.init = async function(opts = {})
{
	for(let filter_id in mc.session.filter)
	{
		let name = mc.session.filter[filter_id];
		if(!(name in mc.filter.filter))
			continue;

		let item = mc.filter.filter[name];
		if(item.filter_id !== undefined)
			continue;

		let res = await mc.filter.init.get(filter_id);
		if(res === false)
			console.log("Purged unused filter id[" + filter_id + "]");
		else
			console.info("Associated filter '" + name + "' id[" + filter_id + "]");
	}

	for(let name in mc.filter.filter)
	{
		let item = mc.filter.filter[name];
		if(item.filter_id !== undefined)
			continue;

		let filter_id = await mc.filter.init.post(name);
		console.log("Posted filter '" + name + "' id[" + filter_id + "]");
	}
};

/** Attempt to get a filter we have an ID for and associate it
 * @returns the name for the filter ID. If no name, the filter
 * ID is forgotten to optimize the next init.
 */
mc.filter.init.get = async function(filter_id, opts = {})
{
	let request = mc.m.filter.get(filter_id, opts); try
	{
		let data = await request.response;
		let filter = JSON.stringify(data);
		for(let name in mc.filter.filter)
		{
			let item = mc.filter.filter[name];
			let content = JSON.stringify(item.content);
			if(filter == content)
			{
				item.filter_id = filter_id;
				mc.session.filter[filter_id] = name;
				return name;
			}
		}

		delete mc.session.filter[filter_id];
		return false;
	}
	catch(error)
	{
		switch(error.status)
		{
			case 404:
				delete mc.session.filter[filter_id];
				return false;

			default:
				throw error;
		}
	}
};

/** Posts a new filter, associates the new ID and saves it to session
 * @returns the filter_id for the name
 */
mc.filter.init.post = async function(name, opts = {})
{
	let item = mc.filter.filter[name];
	let request = mc.m.filter.post(item.content, opts);
	let data = await request.response;
	item.filter_id = data.filter_id;
	mc.session.filter[data.filter_id] = name;
	return item.filter_id;
};
