/*
 * charybdis: An advanced ircd.
 * snomask.c: Management for user server-notice masks.
 *
 * Copyright (c) 2006 William Pitcock <nenolod@nenolod.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * $Id$
 */

#include "stdinc.h"
#include "client.h"
#include "snomask.h"

/* *INDENT-OFF* */
int snomask_modes[256] = {
        /* 0x00 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x0F */
        /* 0x10 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x1F */
        /* 0x20 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x2F */
        /* 0x30 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x3F */
        0,                      /* @ */
        0,                      /* A */
        0,                      /* B */
        SNO_CCONNEXT,           /* C */
        0,		        /* D */
        0,                      /* E */
        0,                      /* F */
        0,                      /* G */
        0,                      /* H */
        0,                      /* I */
        0,                      /* J */
        0,                      /* K */
        0,                      /* L */
        0,                      /* M */
        0,                      /* N */
        0,                      /* O */
        0,                      /* P */
        0,		        /* Q */
        0,                      /* R */
        0,		        /* S */
        0,                      /* T */
        0,                      /* U */
        0,                      /* V */
        0,                      /* W */
        0,                      /* X */
        0,                      /* Y */
        SNO_OPERSPY,	        /* Z */
        /* 0x5B */ 0, 0, 0, 0, 0, 0, /* 0x60 */
        0,		        /* a */
        SNO_BOTS,	        /* b */
        SNO_CCONN,              /* c */
        SNO_DEBUG,              /* d */
        0,                      /* e */
        SNO_FULL,               /* f */
        0,                      /* g */
        0,                      /* h */
        0,                      /* i */
        0,                      /* j */
        SNO_SKILL,              /* k */
        0,                      /* l */
        0,                      /* m */
        SNO_NCHANGE,            /* n */
        0,                      /* o */
        0,                      /* p */
        0,                      /* q */
        SNO_REJ,                /* r */
        SNO_GENERAL,            /* s */
        0,                      /* t */
        SNO_UNAUTH,             /* u */
        0,                      /* v */
        0,                      /* w */
        SNO_EXTERNAL,           /* x */
        SNO_SPY,                /* y */
        0,                      /* z */
        /* 0x7B */ 0, 0, 0, 0, 0, /* 0x7F */
        /* 0x80 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x9F */
        /* 0x90 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x9F */
        /* 0xA0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xAF */
        /* 0xB0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xBF */
        /* 0xC0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xCF */
        /* 0xD0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xDF */
        /* 0xE0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xEF */
        /* 0xF0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  /* 0xFF */
};
/* *INDENT-ON* */

static char snobuf[BUFSIZE];

/*
 * construct_snobuf
 *
 * inputs       - client to generate snomask string for
 * outputs      - snomask string of client
 * side effects - NONE
 */
char *
construct_snobuf(unsigned int val)
{
        int i;
        char *ptr = snobuf;

        *ptr = '\0';
	*ptr++ = '+';

        for (i = 0; i < 128; i++)
                if (snomask_modes[i] && (val & snomask_modes[i]))
                        *ptr++ = (char) i;

        *ptr++ = '\0';

	return snobuf;
}

/*
 * parse_snobuf_to_mask
 *
 * inputs       - value to alter bitmask for, snomask itself
 * outputs      - replacement bitmask to set
 * side effects - NONE
 */
unsigned int
parse_snobuf_to_mask(unsigned int val, const char *sno)
{
	const char *p;
	int what = SNO_ADD;

	if (sno == NULL)
		return val;

	for (p = sno; *p != '\0'; p++)
	{
		switch(*p)
		{
			case '+':
				what = SNO_ADD;
				break;
			case '-':
				what = SNO_DEL;
				break;
			default:
				if (what == SNO_ADD)
					val |= snomask_modes[(unsigned char) *p];
				else if (what == SNO_DEL)
					val &= ~snomask_modes[(unsigned char) *p];

				break;
		}
	}

	return val;
}

/*
 * find_snomask_slot
 *
 * inputs       - NONE
 * outputs      - an available umode bitmask or
 *                0 if no umodes are available
 * side effects - NONE
 */
unsigned int
find_snomask_slot(void)
{
	unsigned int all_umodes = 0, my_umode = 0, i;

	for (i = 0; i < 128; i++)
		all_umodes |= snomask_modes[i];

	for (my_umode = 1; my_umode && (all_umodes & my_umode);
		my_umode <<= 1);

	return my_umode;
}

