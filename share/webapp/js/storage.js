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
 * Local & Session storage
 *
 * Lifetimes in order from most persistent to most transient:
 *
 * + mc.account - synchronized with server account_data (requires auth)
 * + mc.local - synchronized with window localStorage
 * + mc.session - synchronized with window sessionStorage
 * + mc.instance - not synchronized; lifetime is the same as mc object.
 *
 * Note that the storage objects are serialized to JSON when actually being stored.
 * Anything placed inside a live storage object should have a JSON representation.
 *
 * Note that storage is partitioned based on the mc.opts.base_url. This ensures
 * if the target server changes for whatever reason the webstorage objects will be
 * different for different base URL's.
 *
 */

mc.storage = {};

/* Reads from webstorage into the client's webstorage globals.
 */
mc.storage.init = function()
{
	let prefix = mc.opts.base_url.hash();
	let update = (facility, target) =>
	{
		let item = facility.getItem(prefix);
		if(item == null)
			return;

		let data = JSON.parse(item);
		Object.update(target, data);
	};

	update(localStorage, mc.local);
	update(sessionStorage, mc.session);
	//console.log("read local storage objects");

	if(typeof(mc.session.model) != "object")
		mc.session.model = {};

	mc.model = mc.session.model;

	if(typeof(mc.model.show) != "object")
		mc.model.show = {};

	mc.show = mc.model.show;
};

/** Writes back the client's webstorage objects.
 */
mc.storage.sync = function()
{
	let prefix = mc.opts.base_url.hash();
	let update = (facility, source) =>
	{
		let item = JSON.stringify(source);
		facility.setItem(prefix, item);
	};

	update(localStorage, mc.local);
	update(sessionStorage, mc.session);
	//console.log("wrote local storage objects");
};

/**
 * account_data based storage
 */
mc.storage.account = {};

/** Update account-based storage object
 */
mc.storage.account.update = function(data)
{
	Object.update(mc.account, data);
};

/** Write back account-based storage to server
 */
mc.storage.account.sync = function()
{
	if(!mc.instance.authentic)
		return false;

	return mc.storage.account.sync.data();
};

mc.storage.account.sync.data = async function()
{
	let data = mc.account;
	let user_id = mc.my.mxid;
	let type = mc.opts.account_data.key;
	if(empty(data))
		return;

	let request = mc.m.user.account_data.put(user_id, type, data); try
	{
		let response = await request.response;
	}
	catch(error)
	{
		console.error("Error synchronizing account_data: " + error);
	}
};
