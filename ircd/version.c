/*
 *   librb: a library used by charybdis and other things
 *   src/version.c
 *
 *   Copyright (C) 1990 Chelsea Ashley Dyerman
 *   Copyright (C) 2008 ircd-ratbox development team
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <rb/rb.h>

const char *ircd_version = RB_VERSION;
const char *ircd_datestr = RB_DATESTR;
const time_t ircd_datecode = RB_DATECODE;

const char *creation = RB_DATESTR;
const char *serno = RB_DATESTR;
const time_t datecode = RB_DATECODE;

const char *infotext[] =
{
"IRCd",
"Based on the original code written by Jarkko Oikarinen",
"Copyright (c) 1988-1991 University of Oulu, Computing Center",
"Copyright (c) 1996-2001 Hybrid Development Team",
"Copyright (c) 2002-2009 ircd-ratbox Development Team",
"Copyright (c) 2005-2016 charybdis development team",
" ",
"This program is free software; you can redistribute it and/or",
"modify it under the terms of the GNU General Public License as",
"published by the Free Software Foundation; either version 2, or",
"(at your option) any later version.",
};
