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

mc.ng.app.controller('room', class extends mc.ng.controller
{
	constructor($scope)
	{
		super("room", $scope);
		$scope.open_bracket = this.open_bracket;
		$scope.close_bracket = this.close_bracket;
		$scope.delta = this.delta;
		$scope.should_show_target = this.should_show_target;
		$scope.should_show_avatar = this.should_show_avatar;
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
});
