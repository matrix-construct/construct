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

mc.ng.app.controller('room', class extends mc.ng.controller
{
	constructor($scope)
	{
		super("room", $scope);
		$scope.open_bracket = this.open_bracket;
		$scope.close_bracket = this.close_bracket;
		$scope.sender_domid = this.sender_domid;
		$scope.sender_sid = this.sender_sid;
		$scope.delta = this.delta;
		$scope.should_show_target = this.should_show_target;
		$scope.should_show_avatar = this.should_show_avatar;
		$scope.dots_to_underscores = this.dots_to_underscores;
		$scope.handler_path = this.handler_path;
		$scope.handler_exists = this.handler_exists;
		$scope.sender_is_server = this.sender_is_server;
	}

	destructor()
	{
	}

	constructed()
	{
	}

	delta(newval)
	{
		//if(!$scope.room.opts.local)
		//	window.console.log("room id: " + $scope.room.id);
		debug.object({W: $scope.$$watchers}, 3);
		//console.log("A " + debug.stringify(oldval, 1));
		console.log("B " + debug.stringify(newval, 1));
		//window.console.log("C " + debug.stringify($scope.room.timeline, 1));
		//$scope.room.timeline = newval;
		return newval;
	}
	//$scope.$watchCollection('room.timeline', delta);
	//$scope.$watchCollection('room.state.history_visibility.content', hist);

	should_show_target(event)
	{
		if(!event.target || Object.keys(event.target) == 0)
			return false;

		if(event.target == event.sender)
			return false;

		return true;
	}

	should_show_avatar(url)
	{
		if(!url)
			return false;

		return url.startsWith("http://") ||
		       url.startsWith("https://");
	}

	sender_domid(sender)
	{
		if(!sender)
			return sender;

		let sid = this.sender_sid(sender);
		let domid = mc.m.domid(sender);
		if(!domid)
			return sender;

		let remain = 24 - sid.length;
		let start = remain < domid.length?  domid.length - remain : 0;
		let ret = domid.substr(start, remain);
		if(ret.length < domid.length)
			ret = "..." + ret;

		return ret;
	}

	sender_sid(sender)
	{
		let displayname = maybe(() => mc.users[sender].displayname);
		let str = displayname? displayname : mc.m.sid(sender);
		let len = 28 - 3;
		return str? str.substr(0, len) : sender;
	}

	open_bracket(event)
	{
		if(event.state_key !== undefined)
			return '';

		switch(event.type)
		{
			case "m.room.message":
			{
				switch(maybe(() => event.content.msgtype))
				{
					case "m.notice":  return '(';
					case "m.emote":   return '';
					default:          return '<';
				}
			}

			case "m.room.member":
				return '';

			default:
				return '<';
		}
	}

	close_bracket(event)
	{
		if(event.state_key !== undefined)
			return '';

		switch(event.type)
		{
			case "m.room.message":
			{
				switch(maybe(() => event.content.msgtype))
				{
					case "m.notice":  return ')';
					case "m.emote":   return '';
					default:          return '>';
				}
			}

			case "m.room.member":
				return '';

			default:
				return '>';
		}
	}

	dots_to_underscores(str)
	{
		return defined(str)? str.replace(/\./g, '_') : str;
	}

	handler_path(type)
	{
		let part = Array.isArray(type)? type : [type];
		let sel = "ircd_room_event";
		for(let i in part)
			sel += "__" + this.dots_to_underscores(part[i]);

		return sel;
	}

	handler_exists(type)
	{
		let path = "#" + this.handler_path(type);
		return $(path).length > 0;
	}

	//TODO: arbitrary
	sender_is_server(event)
	{
		let sender = maybe(() => event.sender);
		if(!sender)
			return false;

		return mc.m.sid(sender) == "ircd";
	}
});
