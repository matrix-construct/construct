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

/** Master list
 *
 * This is really simple. The rooms > main > list will view this container
 * in order. Elements are instances of mc.room. Control of this container
 * has no special coordination.
 */
mc.rooms.list = [];

mc.ng.app.controller('rooms.list', class extends mc.ng.controller
{
	constructor($scope)
	{
		super("rooms.list", $scope);
		this.loading = true;
	}

	constructed()
	{
		this.loading = false;
	}

	destructor()
	{
	}

	scroll(event)
	{
		let e = $(event.delegateTarget);
		if(!this.edge && mc.scroll.at.top(e))
		{
			this.edge = "top";
			return this.scroll_up();
		}

		if(!this.edge && mc.scroll.at.bottom(e))
		{
			this.edge = "bottom";
			return this.scroll_down();
		}

		// By setting this.edge we prevent repeat calls to the async scroll functions
		// which will result in a digest loop even if no IO is done.
		this.edge = null;
	}

	async scroll_up()
	{
	}

	async scroll_down()
	{
		await fetch();
	}

	async fetch()
	{
		if(!maybe(() => mc.rooms.list.opts))
			return;

		let opts = mc.rooms.list.opts;
		delete mc.rooms.list.opts;
		let rooms = await mc.rooms.public.fetch(opts);
		mc.rooms.list = mc.rooms.list.concat(rooms.map(mc.rooms.get));
		mc.rooms.list.opts = opts;
	}
});
