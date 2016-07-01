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

const char *rb_version = RB_VERSION;
const char *rb_datestr = RB_DATESTR;
const time_t rb_datecode = RB_DATECODE;
const char *rb_creation = RB_DATESTR;
const char *rb_serno = RB_DATESTR;
const char *rb_infotext[] =
{
  "librb --",
  "Based on the original code written by Jarkko Oikarinen",
  "Copyright 1988, 1989, 1990, 1991 University of Oulu, Computing Center",
  "Copyright (c) 1996-2001 Hybrid Development Team",
  "Copyright (c) 2002-2008 ircd-ratbox Development Team",
  "",
  "This program is free software; you can redistribute it and/or",
  "modify it under the terms of the GNU General Public License as",
  "published by the Free Software Foundation; either version 2, or",
  "(at your option) any later version.",
  "",
  "librb was based on ircd-ratbox's libratbox. This contains various utility",
  "functions. See the CREDITS in Charybdis for more information.",
  "",
  0,
};
