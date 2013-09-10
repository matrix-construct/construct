/*
 *  charybdis: An advanced IRCd.
 *  s_assert.h: Definition of the soft assert (s_assert) macro.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
 *  Copyright (C) 2005-2013 Charybdis development team
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
 */

#ifndef INCLUDED_s_assert_h
#define INCLUDED_s_assert_h

#include "config.h"

#ifdef SOFT_ASSERT

#include "logger.h"
#include "send.h"
#include "snomask.h"

#ifdef __GNUC__
#define s_assert(expr)	do								\
			if(!(expr)) {							\
				ilog(L_MAIN,						\
				"file: %s line: %d (%s): Assertion failed: (%s)",	\
				__FILE__, __LINE__, __PRETTY_FUNCTION__, #expr);	\
				sendto_realops_snomask(SNO_GENERAL, L_ALL,		\
				"file: %s line: %d (%s): Assertion failed: (%s)",	\
				__FILE__, __LINE__, __PRETTY_FUNCTION__, #expr);	\
			}								\
			while(0)
#else
#define s_assert(expr)	do								\
			if(!(expr)) {							\
				ilog(L_MAIN,						\
				"file: %s line: %d: Assertion failed: (%s)",		\
				__FILE__, __LINE__, #expr);				\
				sendto_realops_snomask(SNO_GENERAL, L_ALL,		\
				"file: %s line: %d: Assertion failed: (%s)"		\
				__FILE__, __LINE__, #expr);				\
			}								\
			while(0)
#endif
#else
#define s_assert(expr)	assert(expr)
#endif

#endif /* INCLUDED_s_assert_h */
