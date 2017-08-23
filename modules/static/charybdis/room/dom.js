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
 * DOM queries
 *
 * Operates on the room's DOM element. Be aware that if a room is not
 * the current_room it has no DOM element. This means you can't ever
 * save the elements through room changes because they'll go stale.
 *
 * TODO: needs cleanup
 */

room.dom = {};

room.dom.root = function()
{
	let id = this.id;
	let label = $("#charybdis_room").children("div.room").children("label[for='" + id + "']");
	return $(label).parent();
};

room.dom.exists = function(root = $(this.dom.root()))
{
	return $(root).length > 0;
};

room.dom.items = function(root = $(this.dom.root()))
{
	return $(root).find("div.main").children("div.content").children("div.events");
};

room.dom.item = function(event_id, items = $(this.dom.items()))
{
	return $(items).find("div.event > label[for='" + event_id + "']");
}

room.dom.zoom = function($event, event)
{
	let event_id = event.event_id;
	if(this.control.content.zoom == event_id)
	{
		delete this.control.content.zoom;
		return;
	}

	this.control.content.zoom = event_id;
	this.scroll.to.event(event_id);
};

room.dom.event_is_viewport = function(event_id)
{
	return isElementInViewport($(this.dom.item(event_id)));
}

room.dom.timeline_is_viewport = function()
{
	return this.timeline.map((event) => this.dom.event_is_viewport(event.event_id));
}

room.dom.timeline_in_viewport = function()
{
	return this.timeline.filter((event) => this.dom.event_is_viewport(event.event_id));
}

room.dom.timeline_clicked = function(event, $index)
{
	let explore = $(this.dom.root()).find(".main .explore");
	if(!$(explore).is(":visible"))
		this.dom.toggle_explore(event);

	let open = (key) =>
	{
		if($(key).children("i").hasClass("fa-plus-square"))
			$(key).click();
	};

	let seek = (timeline, index) =>
	{
		$(timeline).find("div.value > div.object > div.member > div.key .name").each((i, e) =>
		{
			if(parseInt($(e).text().trim()) != index)
				return;

			let key = $(e).parent();
			open(key);
		});
	};

	let content = $(explore).children("div.content");
	$(content).find("div.object > div.member > div.key .name").each((i, e) =>
	{
		if($(e).text().trim() != "events")
			return;

		let key = $(e).parent();
		open(key);

		let events = $(key).parent();
		$(events).find("div.object > div.member > div.key .name").each((i, e) =>
		{
			if($(e).text().trim() != "timeline")
				return;

			let key = $(e).parent();
			open(key);

			let timeline = $(key).parent();
			seek(timeline, $index);
		});
	});
};

room.dom.refresh_explore = function(event)
{
	let explore = $(this.dom.root()).find(".main div.explore");
	let content = $(explore).children("div.content");
	content.html("");

	new explorer(this, content);
};

room.dom.toggle_explore = function(event)
{
	this.control.show_explore = !this.control.show_explore;

	let explore = $(this.dom.root()).find(".main .explore");
	let content = $(explore).children(".content");
	if($(content).html().trim().length == 0)
		this.dom.refresh_explore();
};
