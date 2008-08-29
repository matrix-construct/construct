/*
 *  charybdis: A slightly useful ircd.
 *  supported.h: isupport (005) numeric
 *
 *  Entirely rewritten, August 2006 by Jilles Tjoelker
 *  Copyright (C) 2006 Jilles Tjoelker
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 * 
 *  1.Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  2.Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  3.The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 *  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 *  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *  $Id: supported.h 1887 2006-08-29 13:42:56Z jilles $
 */

#ifndef INCLUDED_supported_h
#define INCLUDED_supported_h

extern void add_isupport(const char *, const char *(*)(const void *), const void *);
extern const void *change_isupport(const char *, const char *(*)(const void *), const void *);
extern void delete_isupport(const char *);
extern void show_isupport(struct Client *);
extern void init_isupport(void);

extern const char *isupport_intptr(const void *);
extern const char *isupport_boolean(const void *);
extern const char *isupport_string(const void *);
extern const char *isupport_stringptr(const void *);

#endif /* INCLUDED_supported_h */
