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
 * Authentication flow
 *
 */
mc.auth = async function(flows = {})
{
	let request = mc.m.login.get();
	let login = await request.response;
	Object.update(flows, login.flows);
	for(let i in flows)
	{
		let flow = flows[i];
		let type = flow.type;
		let handler = mc.auth[type];
		if(handler === undefined)
		{
			flow.result = new mc.error("unsupported");
		}
		else try
		{
			flow.result = await handler();
		}
		catch(error)
		{
			flow.result = error;
		}
		finally
		{
			// True result indicates authentication; the flow can be cut.
			if(flow.result === true)
				return true;
		}
	}

	return false;
}

/** Implements m.login.password auth type
 */
mc.auth["m.login.password"] = async function(opts = {})
{
	let user_id = mc.local.user_id;
	if(!user_id)
		throw new mc.error("no user_id specified");

	let password = mc.local.wasspord;
	if(!password)
		throw new mc.error("no password specified");

	let request = mc.m.login.password(user_id, password, opts); try
	{
		let data = await request.response;
		mc.session.guest = false;
		mc.instance.authentic = true;
		Object.update(mc.session, data);
		return true;
	}
	catch(error)
	{
		delete mc.local.wasspord;
		throw error;
	}
};

/** Implements m.login.token auth type
 *
 * (non-functional)
 */
mc.auth["m.login.token"] = async function(opts = {})
{
	let user_id = mc.local.user_id;
	let token = mc.local.token;
	let request = mc.m.login.token(user_id, access_token, opts); try
	{
		let data = await request.response;
		mc.instance.authentic = true;
		Object.update(mc.session, data);
		return true;
	}
	catch(error)
	{
		throw error;
	}
};

mc.auth["m.register.guest"] = async function(opts = {})
{
	let request = mc.m.register.post(opts); try
	{
		let data = await request.response;
		mc.session.guest = true;
		mc.instance.authentic = true;
		Object.update(mc.session, data);
		return true;
	}
	catch(error)
	{
		switch(error.status)
		{
			case 403:
				mc.auth["m.register.guest"].disabled = true;
				throw error;

			default:
				throw error;
		}
	}
};

mc.auth["m.register.user"] = async function(opts = {})
{
	Object.update(opts,
	{
		query:
		{
			kind: "user"
		},

		content:
		{
			username: mc.local.user_id,
			password: mc.local.wasspord,
		}
	});

	let request = mc.m.register.post(opts); try
	{
		let data = await request.response;
		mc.session.guest = false;
		mc.instance.authentic = true;
		Object.update(mc.session, data);
		return true;
	}
	catch(error)
	{
		switch(error.status)
		{
			case 403:
				mc.auth["m.register.user"].disabled = true;
				//[[fallthrough]]

			default:
				throw new mc.error(error);
		}
	}
};

mc.auth.logout = async function(opts = {})
{
	if(!mc.instance.authentic)
		return false;

	let request = mc.m.logout.post(opts);
	let data = await request.response;
	delete mc.session.access_token;
	mc.instance.authentic = false;
	return true;
};
