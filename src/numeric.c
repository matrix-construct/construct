/*
 *  ircd-ratbox: A slightly useful ircd.
 *  numeric.c: Numeric handling functions.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 *  $Id: numeric.c 6 2005-09-10 01:02:21Z nenolod $
 */

#include "stdinc.h"
#include "setup.h"
#include "config.h"
#include "s_conf.h"
#include "numeric.h"
#include "match.h"
#include "common.h"		/* NULL cripes */

#include "messages.tab"

/*
 * form_str
 *
 * inputs	- numeric
 * output	- corresponding string
 * side effects	- NONE
 */
const char *
form_str(int numeric)
{
	const char *num_ptr;

	s_assert(-1 < numeric);
	s_assert(numeric < ERR_LAST_ERR_MSG);
	s_assert(0 != replies[numeric]);

	if(numeric > ERR_LAST_ERR_MSG)
		numeric = ERR_LAST_ERR_MSG;
	if(numeric < 0)
		numeric = ERR_LAST_ERR_MSG;

	num_ptr = replies[numeric];
	if(num_ptr == NULL)
		num_ptr = replies[ERR_LAST_ERR_MSG];

	return (num_ptr);
}
