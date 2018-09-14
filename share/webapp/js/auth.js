/*
// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.
*/

'use strict';

/**************************************
 *
 * Authentication flow
 *
 */
mc.auth = async function(flows = {}, opts = {})
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
			flow.result = new mc.error("authentication flow type unsupported");
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
		console.warn("The user's password failed; deleting.");
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
		error.element = $("#charybdis_login_form");
		switch(error.status)
		{
			case 403:
				mc.auth["m.register.guest"].disabled = true;

			default:
				throw new mc.error(error);
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
		error.element = $("#charybdis_login_form");
		delete mc.local.wasspord;
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
