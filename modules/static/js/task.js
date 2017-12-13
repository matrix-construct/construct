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
 *
 */

mc.task = class
{
	constructor(generator, opts = {})
	{
		Object.defaults(opts,
		{
			interval: 0,       // The attempt cadence.
			skips: false,      // skips cycles when there is no idle time
			fills: true,       // invokes generator until idle slice has diminished.
		});

		this.opts = opts;
		this.generator = generator;

		this.rid = null;       // request id
		this.cycle = 0;        // number of task handler invocations
		this.skips = 0;        // number of skipped cycles due to no idle time
		this.generation = 0;   // number of generator next invocations

		// Note the initial execution of the generator
		// does NOT take place on an idle slice.
		this.generated = generator();
		this.generation++;

		// Init task cycle
		this.request();
	}

	request()
	{
		if(this.requested())
			this.cancel();

		let handler = (deadline) =>
		{
			this.rid = null;
			this.handler(deadline);
			this.cycle++;
		};

		let opts =
		{
			timeout: this.opts.interval,
		};

		this.rid = window.requestIdleCallback(handler, opts);
	}

	handler(deadline)
	{
		let remain = () => deadline.timeRemaining();
		let timeout = deadline.didTimeout;

		if(timeout && this.opts.skips)
		{
			this.skips++;
			this.request();
			return;
		}

		if(this.generate(remain))
			this.request();
	}

	generate(remain)
	{
		let result; do
		{
			result = this.generated.next();
			this.generation++;
		}
		while(!result.done && remain() > 0)

		return result.done;
	}

	cancel()
	{
		if(!this.requested())
			return false;

		window.cancelIdleCallback(this.rid);
		this.rid = null;
		return true;
	}

	requested()
	{
		return this.rid !== undefined && this.rid !== null;
	}
};
