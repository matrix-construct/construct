/************************************************************************
 * charybdis: an advanced ircd. extensions/spamfilter_expr.c
 * Copyright (C) 2016 Jason Volk
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
 */

#include "stdinc.h"
#include "s_conf.h"
#include "numeric.h"
#include "modules.h"
#include "hook.h"
#include "send.h"
#include "s_serv.h"
#include "hash.h"
#include "newconf.h"
#include "spamfilter.h"

#define PCRE2_STATIC 1
#define PCRE2_CODE_UNIT_WIDTH 8
#define CODE_UNIT uint8_t
#define CODE_SIZE(bytes) (((bytes) * 8) / PCRE2_CODE_UNIT_WIDTH)
#include <pcre2.h>


/* From PCRE2STACK(3) for reference:
 *	As a very rough rule of thumb, you should reckon on about 500 bytes per recursion.
 *	Thus, if you want to limit your stack usage to 8Mb, you should set the limit at 16000
 *	recursions. A 64Mb stack, on the other hand, can support around 128000 recursions.
 *	...  The actual amount of stack used per recursion can vary quite a lot ...
 */
#define JIT_STACK_FRAME_LOWERBOUND  512
#define JIT_STACK_FRAME_UPPERBOUND  768
#define DEFAULT_MATCH_LIMIT         1024
#define DEFAULT_RECURSION_LIMIT     512
#define DEFAULT_PARENS_NEST_LIMIT   32
#define PATTERN_HASH_BITS           18


#define EXPR_ERROR_TOOMANY          -256
#define EXPR_ERROR_EXISTS           -255
#define EXPR_ERROR_DICTFAIL         -254


struct Expr
{
	unsigned int id;
	unsigned int comp_opts;
	unsigned int match_opts;
	unsigned int jit_opts;
	char *pattern;
	pcre2_compile_context *cctx;
	pcre2_code *expr;
	pcre2_match_context *mctx;
	pcre2_match_data *match;
	time_t added;
	time_t last;
	unsigned int hits;
	rb_dlink_node node;
};


/*
 * General conf items & defaults
 */

size_t conf_limit               = 1024;
size_t conf_match_limit         = DEFAULT_MATCH_LIMIT;
size_t conf_recursion_limit     = DEFAULT_RECURSION_LIMIT;
size_t conf_parens_nest_limit   = DEFAULT_PARENS_NEST_LIMIT;
size_t conf_jit_stack_size      = DEFAULT_RECURSION_LIMIT * JIT_STACK_FRAME_LOWERBOUND;
size_t conf_jit_stack_max_size  = DEFAULT_RECURSION_LIMIT * JIT_STACK_FRAME_UPPERBOUND;
unsigned int conf_compile_opts          = 0;
unsigned int conf_match_opts            = PCRE2_NOTBOL | PCRE2_NOTEOL | PCRE2_NOTEMPTY;
unsigned int conf_jit_opts              = PCRE2_JIT_COMPLETE;


/*
 * Module state
 */

rb_dictionary *exprs;       /* Expressions indexed by ID number (a hash of the pattern string) */
pcre2_general_context *gctx;
pcre2_jit_stack *jstack;



static inline
unsigned int hash_pattern(const char *const pattern)
{
	return fnv_hash((const unsigned char *)pattern, PATTERN_HASH_BITS);
}

static
void *expr_malloc_cb(PCRE2_SIZE size, void *const ptr)
{
	return rb_malloc(size);
}

static
void expr_free_cb(void *const ptr, void *const priv)
{
	rb_free(ptr);
}


static
void free_expr(struct Expr *const expr)
{
	if(!expr)
		return;

	pcre2_match_data_free(expr->match);
	pcre2_code_free(expr->expr);
	pcre2_match_context_free(expr->mctx);
	pcre2_compile_context_free(expr->cctx);
	rb_free(expr->pattern);
	rb_free(expr);
}


static
struct Expr *new_expr(const char *const pattern,
                      const unsigned int comp_opts,
                      const unsigned int match_opts,
                      const unsigned int jit_opts,
                      const unsigned char *const tables,
                      int *const errcode,
                      PCRE2_SIZE *const erroff)
{
	struct Expr *const ret = rb_malloc(sizeof(struct Expr));
	ret->id = hash_pattern(pattern);
	ret->comp_opts = comp_opts;
	ret->match_opts = match_opts;
	ret->jit_opts = jit_opts;
	ret->pattern = rb_strdup(pattern);

	if((ret->cctx = pcre2_compile_context_create(gctx)))
	{
		pcre2_set_character_tables(ret->cctx, tables);
		pcre2_set_parens_nest_limit(ret->cctx, conf_parens_nest_limit);
	}

	const CODE_UNIT *const sptr = (CODE_UNIT *) pattern;
	if(!(ret->expr = pcre2_compile(sptr, PCRE2_ZERO_TERMINATED, comp_opts, errcode, erroff, ret->cctx)))
	{
		free_expr(ret);
		return NULL;
	}

	if((*errcode = pcre2_jit_compile(ret->expr, jit_opts)))
	{
		*erroff = 0;
		free_expr(ret);
		return NULL;
	}

	if((ret->mctx = pcre2_match_context_create(gctx)))
	{
		if(conf_match_limit)
			pcre2_set_match_limit(ret->mctx, conf_match_limit);

		if(conf_recursion_limit)
			pcre2_set_recursion_limit(ret->mctx, conf_recursion_limit);
	}

	ret->match = pcre2_match_data_create_from_pattern(ret->expr, gctx);
	ret->added = 0;
	ret->last = 0;
	ret->hits = 0;
	return ret;
}


static
struct Expr *find_expr(const unsigned int id)
{
	return rb_dictionary_retrieve(exprs, &id);
}


static
struct Expr *find_expr_by_str(const char *const pattern)
{
	return find_expr(hash_pattern(pattern));
}


static
int activate_expr(struct Expr *const expr,
                  int *const errcode,
                  char *const errbuf,
                  const size_t errmax)
{
	if(rb_dictionary_size(exprs) >= conf_limit)
	{
		if(errcode && errbuf)
		{
			*errcode = EXPR_ERROR_TOOMANY;
			rb_strlcpy(errbuf, "Maximum active expressions has been reached", errmax);
		}

		return 0;
	}

	if(find_expr_by_str(expr->pattern))
	{
		if(errcode && errbuf)
		{
			*errcode = EXPR_ERROR_EXISTS;
			rb_strlcpy(errbuf, "The pattern is already active", errmax);
		}

		return 0;
	}

	if(!rb_dictionary_add(exprs, &expr->id, expr))
	{
		if(errcode && errbuf)
		{
			*errcode = EXPR_ERROR_DICTFAIL;
			rb_strlcpy(errbuf, "Failed to activate this expression", errmax);
		}

		return 0;
	}

	expr->added = rb_current_time();
	return 1;
}


static
struct Expr *deactivate_expr(const unsigned int id)
{
	return rb_dictionary_delete(exprs, &id);
}


static
struct Expr *activate_new_expr(const char *const pattern,
                               const unsigned int comp_opts,
                               const unsigned int match_opts,
                               const unsigned int jit_opts,
                               const unsigned char *const tables,
                               int *const errcode,
                               size_t *const erroff,
                               char *const errbuf,
                               const size_t errmax)
{
	struct Expr *const expr = new_expr(pattern, comp_opts, match_opts, jit_opts, tables, errcode, erroff);
	if(!expr)
	{
		if(errbuf)
			pcre2_get_error_message(*errcode, (unsigned char *const) errbuf, errmax);

		return NULL;
	}

	if(!activate_expr(expr, errcode, errbuf, errmax))
	{
		*erroff = 0;
		free_expr(expr);
		return NULL;
	}

	return expr;
}


static
int deactivate_and_free_expr(const unsigned int id)
{
	struct Expr *const expr = rb_dictionary_delete(exprs, &id);
	free_expr(expr);
	return expr != NULL;
}


static
const char *str_pcre_info(const long val)
{
	switch(val)
	{
		case PCRE2_INFO_ALLOPTIONS:         return "ALLOPTIONS";
		case PCRE2_INFO_ARGOPTIONS:         return "ARGOPTIONS";
		case PCRE2_INFO_BACKREFMAX:         return "BACKREFMAX";
		case PCRE2_INFO_BSR:                return "BSR";
		case PCRE2_INFO_CAPTURECOUNT:       return "CAPTURECOUNT";
		case PCRE2_INFO_FIRSTCODEUNIT:      return "FIRSTCODEUNIT";
		case PCRE2_INFO_FIRSTCODETYPE:      return "FIRSTCODETYPE";
		case PCRE2_INFO_FIRSTBITMAP:        return "FIRSTBITMAP";
		case PCRE2_INFO_HASCRORLF:          return "HASCRORLF";
		case PCRE2_INFO_JCHANGED:           return "JCHANGED";
		case PCRE2_INFO_JITSIZE:            return "JITSIZE";
		case PCRE2_INFO_LASTCODEUNIT:       return "LASTCODEUNIT";
		case PCRE2_INFO_LASTCODETYPE:       return "LASTCODETYPE";
		case PCRE2_INFO_MATCHEMPTY:         return "MATCHEMPTY";
		case PCRE2_INFO_MATCHLIMIT:         return "MATCHLIMIT";
		case PCRE2_INFO_MAXLOOKBEHIND:      return "MAXLOOKBEHIND";
		case PCRE2_INFO_MINLENGTH:          return "MINLENGTH";
		case PCRE2_INFO_NAMECOUNT:          return "NAMECOUNT";
		case PCRE2_INFO_NAMEENTRYSIZE:      return "NAMEENTRYSIZE";
		case PCRE2_INFO_NAMETABLE:          return "NAMETABLE";
		case PCRE2_INFO_NEWLINE:            return "NEWLINE";
		case PCRE2_INFO_RECURSIONLIMIT:     return "RECURSIONLIMIT";
		case PCRE2_INFO_SIZE:               return "SIZE";
		default:                            return "";
	}
}


static
const char *str_pcre_comp(const long val)
{
	switch(val)
	{
		case PCRE2_ALLOW_EMPTY_CLASS:       return "ALLOW_EMPTY_CLASS";       /* C       */
		case PCRE2_ALT_BSUX:                return "ALT_BSUX";                /* C       */
		case PCRE2_AUTO_CALLOUT:            return "AUTO_CALLOUT";            /* C       */
		case PCRE2_CASELESS:                return "CASELESS";                /* C       */
		case PCRE2_DOLLAR_ENDONLY:          return "DOLLAR_ENDONLY";          /*   J M D */
		case PCRE2_DOTALL:                  return "DOTALL";                  /* C       */
		case PCRE2_DUPNAMES:                return "DUPNAMES";                /* C       */
		case PCRE2_EXTENDED:                return "EXTENDED";                /* C       */
		case PCRE2_FIRSTLINE:               return "FIRSTLINE";               /*   J M D */
		case PCRE2_MATCH_UNSET_BACKREF:     return "MATCH_UNSET_BACKREF";     /* C J M   */
		case PCRE2_MULTILINE:               return "MULTILINE";               /* C       */
		case PCRE2_NEVER_UCP:               return "NEVER_UCP";               /* C       */
		case PCRE2_NEVER_UTF:               return "NEVER_UTF";               /* C       */
		case PCRE2_NO_AUTO_CAPTURE:         return "NO_AUTO_CAPTURE";         /* C       */
		case PCRE2_NO_AUTO_POSSESS:         return "NO_AUTO_POSSESS";         /* C       */
		case PCRE2_NO_DOTSTAR_ANCHOR:       return "NO_DOTSTAR_ANCHOR";       /* C       */
		case PCRE2_NO_START_OPTIMIZE:       return "NO_START_OPTIMIZE";       /*   J M D */
		case PCRE2_UCP:                     return "UCP";                     /* C J M D */
		case PCRE2_UNGREEDY:                return "UNGREEDY";                /* C       */
		case PCRE2_UTF:                     return "UTF";                     /* C J M D */
		case PCRE2_NEVER_BACKSLASH_C:       return "NEVER_BACKSLASH_C";       /* C       */
		case PCRE2_ALT_CIRCUMFLEX:          return "ALT_CIRCUMFLEX";          /*   J M D */
		case PCRE2_ANCHORED:                return "ANCHORED";                /* C   M D */
		case PCRE2_NO_UTF_CHECK:            return "NO_UTF_CHECK";            /* C   M D */
		default:                            return "";
	}
}


static
const char *str_pcre_jit(const long val)
{
	switch(val)
	{
		case PCRE2_JIT_COMPLETE:            return "COMPLETE";
		case PCRE2_JIT_PARTIAL_SOFT:        return "PARTIAL_SOFT";
		case PCRE2_JIT_PARTIAL_HARD:        return "PARTIAL_HARD";
		default:                            return "";
	}
}


static
const char *str_pcre_match(const long val)
{
	switch(val)
	{
		case PCRE2_NOTBOL:                  return "NOTBOL";
		case PCRE2_NOTEOL:                  return "NOTEOL";
		case PCRE2_NOTEMPTY:                return "NOTEMPTY";
		case PCRE2_NOTEMPTY_ATSTART:        return "NOTEMPTY_ATSTART";
		case PCRE2_PARTIAL_SOFT:            return "PARTIAL_SOFT";
		case PCRE2_PARTIAL_HARD:            return "PARTIAL_HARD";
		case PCRE2_DFA_RESTART:             return "DFA_RESTART";
		case PCRE2_DFA_SHORTEST:            return "DFA_SHORTEST";
		case PCRE2_SUBSTITUTE_GLOBAL:       return "SUBSTITUTE_GLOBAL";
		default:                            return "";
	}
}


static
long reflect_pcre_info(const char *const str)
{
	for(int i = 0; i < 48; i++)
		if(rb_strcasecmp(str_pcre_info(i), str) == 0)
			return i;

	return -1;
}


static
long reflect_pcre_comp(const char *const str)
{
	unsigned long i = 1;
	for(; i; i <<= 1)
		if(rb_strcasecmp(str_pcre_comp(i), str) == 0)
			return i;

	return i;
}


static
long reflect_pcre_jit(const char *const str)
{
	unsigned long i = 1;
	for(; i; i <<= 1)
		if(rb_strcasecmp(str_pcre_jit(i), str) == 0)
			return i;

	return i;
}


static
long reflect_pcre_match(const char *const str)
{
	unsigned long i = 1;
	for(; i; i <<= 1)
		if(rb_strcasecmp(str_pcre_match(i), str) == 0)
			return i;

	return i;
}


static
long parse_pcre_opts(const char *const str,
                     long (*const reflector)(const char *))
{
	static char s[BUFSIZE];
	static const char *delim = "|";

	unsigned long ret = 0;
	rb_strlcpy(s, str, sizeof(s));
	char *ctx, *tok = strtok_r(s, delim, &ctx); do
	{
		ret |= reflector(tok);
	}
	while((tok = strtok_r(NULL, delim, &ctx)) != NULL);

	return ret;
}


static
int strlcat_pcre_opts(const long opts,
                      char *const buf,
                      const size_t max,
                      const char *(*const strfun)(const long))
{
	int ret = 0;
	for(unsigned long o = 1; o; o <<= 1)
	{
		if(opts & o)
		{
			const char *const str = strfun(o);
			if(strlen(str))
			{
				ret = rb_strlcat(buf, str, max);
				ret = rb_strlcat(buf, "|", max);
			}
		}
	}

	if(ret && buf[ret-1] == '|')
		buf[ret-1] = '\0';

	if(!ret)
		ret = rb_strlcat(buf, "0", max);

	return ret;
}


static
size_t expr_info_val(struct Expr *const expr,
                     const int what,
                     char *const buf,
                     const size_t len)
{
	union
	{
		int32_t v_int;
		uint32_t v_uint;
		size_t v_size;
	} v;

	const int ret = pcre2_pattern_info(expr->expr, what, &v);
	if(ret < 0)
		return pcre2_get_error_message(ret, (unsigned char *const) buf, len);

	switch(what)
	{
		case PCRE2_INFO_ALLOPTIONS:
		case PCRE2_INFO_ARGOPTIONS:
		case PCRE2_INFO_BACKREFMAX:
		case PCRE2_INFO_BSR:
		case PCRE2_INFO_CAPTURECOUNT:
		case PCRE2_INFO_FIRSTCODEUNIT:
		case PCRE2_INFO_FIRSTCODETYPE:
		case PCRE2_INFO_FIRSTBITMAP:
		case PCRE2_INFO_HASCRORLF:
		case PCRE2_INFO_JCHANGED:
		case PCRE2_INFO_LASTCODEUNIT:
		case PCRE2_INFO_LASTCODETYPE:
		case PCRE2_INFO_MATCHEMPTY:
		case PCRE2_INFO_MATCHLIMIT:
		case PCRE2_INFO_MAXLOOKBEHIND:
		case PCRE2_INFO_MINLENGTH:
		case PCRE2_INFO_NAMECOUNT:
		case PCRE2_INFO_NAMEENTRYSIZE:
		case PCRE2_INFO_NAMETABLE:
		case PCRE2_INFO_NEWLINE:
		case PCRE2_INFO_RECURSIONLIMIT:
			return snprintf(buf, len, "%u", v.v_uint);

		case PCRE2_INFO_SIZE:
		case PCRE2_INFO_JITSIZE:
			return snprintf(buf, len, "%zu", v.v_size);

		default:
			return rb_strlcpy(buf, "Requested information unsupported.", len);
	}
}


static
size_t expr_info(struct Expr *const expr,
                 const int what[],
                 const size_t num_what,
                 char *const buf,
                 const ssize_t len)
{
	static char valbuf[BUFSIZE];

	ssize_t boff = 0;
	for(size_t i = 0; i < num_what && boff < len; i++)
	{
		expr_info_val(expr, what[i], valbuf, sizeof(valbuf));
		boff += snprintf(buf+boff, len-boff, "%s[%s] ", str_pcre_info(what[i]), valbuf);
	}

	return boff;
}


static
int match_expr(struct Expr *const expr,
               const char *const text,
               const size_t len,
               const size_t off,
               const unsigned int options)
{
	const CODE_UNIT *const sptr = (CODE_UNIT *) text;
	const PCRE2_SIZE slen = CODE_SIZE(len);
	const PCRE2_SIZE soff = CODE_SIZE(off);
	const unsigned int opts = options | expr->match_opts;
	const int ret = jstack? pcre2_jit_match(expr->expr, sptr, slen, soff, opts, expr->match, expr->mctx):
	                        pcre2_match(expr->expr, sptr, slen, soff, opts, expr->match, expr->mctx);
	if(ret > 0)
	{
		expr->hits++;
		expr->last = rb_current_time();
	}
	else if(rb_unlikely(ret < -1))
	{
		static char errbuf[BUFSIZE];
		pcre2_get_error_message(ret, (unsigned char *const) errbuf, sizeof(errbuf));
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
		                       "spamfilter: Expression #%u error (%d): %s",
		                       expr->id,
		                       ret,
		                       errbuf);
	}

	return ret;
}


static
struct Expr *match_any_expr(const char *const text,
                            const size_t len,
                            const size_t off,
                            const unsigned int options)
{
	struct Expr *expr;
	rb_dictionary_iter state;
	RB_DICTIONARY_FOREACH(expr, &state, exprs)
	{
		if(match_expr(expr, text, len, off, options) > 0)
			return expr;
	}

	return NULL;
}



/*
 * Command handlers
 */

static
int dump_pcre_config(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	union
	{
		uint32_t v_uint;
		uint64_t v_ulong;
		char *v_buf;
	} v;

	if((v.v_ulong = pcre2_config(PCRE2_CONFIG_VERSION, NULL)) > 0)
	{
		v.v_ulong *= (PCRE2_CODE_UNIT_WIDTH / 8);
		v.v_buf = rb_malloc(v.v_ulong);
		pcre2_config(PCRE2_CONFIG_VERSION, v.v_buf);
		sendto_one_notice(source_p, ":\2%-30s\2: (%s)", "PCRE2 VERSION", v.v_buf);
		rb_free(v.v_buf);
	}

	if(pcre2_config(PCRE2_CONFIG_BSR, &v) == 0)
		sendto_one_notice(source_p, ":\2%-30s\2: %d (%s)", "PCRE2 BSR", v.v_uint,
		                  v.v_uint == PCRE2_BSR_UNICODE? "all Unicode line endings":
		                  v.v_uint == PCRE2_BSR_ANYCRLF? "CR, LF, or CRLF only":
		                                                 "???");

	if(pcre2_config(PCRE2_CONFIG_JIT, &v) == 0)
		sendto_one_notice(source_p, ":\2%-30s\2: %d (%s)", "PCRE2 JIT", v.v_uint,
		                  v.v_uint == 0? "UNAVAILABLE":
		                  v.v_uint == 1? "AVAILABLE":
		                                 "???");

	if((v.v_ulong = pcre2_config(PCRE2_CONFIG_JITTARGET, NULL)) > 0)
	{
		v.v_ulong *= (PCRE2_CODE_UNIT_WIDTH / 8);
		v.v_buf = rb_malloc(v.v_ulong);
		pcre2_config(PCRE2_CONFIG_JITTARGET, v.v_buf);
		sendto_one_notice(source_p, ":\2%-30s\2: (%s)", "PCRE2 JITTARGET", v.v_buf);
		rb_free(v.v_buf);
	}

	if(pcre2_config(PCRE2_CONFIG_LINKSIZE, &v) == 0)
		sendto_one_notice(source_p, ":\2%-30s\2: %d", "PCRE2 LINKSIZE", v.v_uint);

	if(pcre2_config(PCRE2_CONFIG_MATCHLIMIT, &v) == 0)
		sendto_one_notice(source_p, ":\2%-30s\2: %u", "PCRE2 MATCHLIMIT", v.v_uint);

	if(pcre2_config(PCRE2_CONFIG_PARENSLIMIT, &v) == 0)
		sendto_one_notice(source_p, ":\2%-30s\2: %u", "PCRE2 PARENSLIMIT", v.v_uint);

	if(pcre2_config(PCRE2_CONFIG_RECURSIONLIMIT, &v) == 0)
		sendto_one_notice(source_p, ":\2%-30s\2: %u", "PCRE2 RECURSIONLIMIT", v.v_uint);

	if(pcre2_config(PCRE2_CONFIG_NEWLINE, &v) == 0)
		sendto_one_notice(source_p, ":\2%-30s\2: %d (%s)", "PCRE2 NEWLINE", v.v_uint,
		                  v.v_uint == PCRE2_NEWLINE_CR?       "CR":
		                  v.v_uint == PCRE2_NEWLINE_LF?       "LF":
		                  v.v_uint == PCRE2_NEWLINE_CRLF?     "CRLF":
		                  v.v_uint == PCRE2_NEWLINE_ANYCRLF?  "ANYCRLF":
		                  v.v_uint == PCRE2_NEWLINE_ANY?      "ANY":
		                                                      "???");

	if(pcre2_config(PCRE2_CONFIG_STACKRECURSE, &v) == 0)
		sendto_one_notice(source_p, ":\2%-30s\2: %d", "PCRE2 STACKRECURSE", v.v_uint);

	if(pcre2_config(PCRE2_CONFIG_UNICODE, &v) == 0)
		sendto_one_notice(source_p, ":\2%-30s\2: %d (%s)", "PCRE2 UNICODE", v.v_uint,
		                  v.v_uint == 0? "UNAVAILABLE":
		                  v.v_uint == 1? "AVAILABLE":
		                                 "???");

	if((v.v_ulong = pcre2_config(PCRE2_CONFIG_UNICODE_VERSION, NULL)) > 0)
	{
		v.v_ulong *= (PCRE2_CODE_UNIT_WIDTH / 8);
		v.v_buf = rb_malloc(v.v_ulong);
		pcre2_config(PCRE2_CONFIG_UNICODE_VERSION, v.v_buf);
		sendto_one_notice(source_p, ":\2%-30s\2: (%s)", "PCRE2 UNICODE_VERSION", v.v_buf);
		rb_free(v.v_buf);
	}

	return 0;
}


static
int spamexpr_info(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(parc > 0 && !IsOper(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "SPAMEXPR INFO");
		sendto_one_notice(source_p, ":Only operators can give arguments to this command.");
		return 0;
	}

	if(parc < 1 && IsOper(source_p))
	{
		dump_pcre_config(client_p, source_p, parc, parv);
		return 0;
	}

	const unsigned int id = atol(parv[0]);
	struct Expr *const expr = find_expr(id);
	if(!expr)
	{
		sendto_one_notice(source_p, ":Failed to find any expression with ID %u.", id);
		return 0;
	}

	const int num_what = parc - 1;
	int what[num_what];
	for(int i = 0; i < num_what; i++)
		what[i] = reflect_pcre_info(parv[i+1]);

	static char buf[BUFSIZE];
	expr_info(expr, what, num_what, buf, sizeof(buf));

	char comp_opts[128] = {0}, match_opts[128] = {0}, jit_opts[128] = {0};
	strlcat_pcre_opts(expr->comp_opts, comp_opts, sizeof(comp_opts), str_pcre_comp);
	strlcat_pcre_opts(expr->match_opts, match_opts, sizeof(match_opts), str_pcre_match);
	strlcat_pcre_opts(expr->jit_opts, jit_opts, sizeof(jit_opts), str_pcre_jit);
	sendto_one_notice(source_p, ":#%u time[%lu] last[%lu] hits[%d] [%s][%s][%s] %s %s",
	                  expr->id,
	                  expr->added,
	                  expr->last,
	                  expr->hits,
	                  comp_opts,
	                  match_opts,
	                  jit_opts,
	                  buf,
	                  expr->pattern);
	return 0;
}


static
int spamexpr_list(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(parc > 0 && !IsOper(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "SPAMEXPR LIST");
		sendto_one_notice(source_p, ":Only operators can give arguments to this command.");
		return 0;
	}

	const char *nparv[parc + 2];
	char id[16];
	nparv[0] = id;
	nparv[parc+1] = NULL;
	for(size_t i = 0; i < parc; i++)
		nparv[i+1] = parv[i];

	struct Expr *expr;
	rb_dictionary_iter state;
	RB_DICTIONARY_FOREACH(expr, &state, exprs)
	{
		snprintf(id, sizeof(id), "%u", expr->id);
		spamexpr_info(client_p, source_p, parc+1, nparv);
	}

	sendto_one_notice(source_p, ":End of expression list.");
	return 0;
}


/*
 * The option fields are string representations of the options or'ed together.
 * Use 0 for no option.
 * example:  CASELESS|ANCHORED|DOTALL
 */
static
int spamexpr_add(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(!IsOper(source_p) && !IsServer(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "SPAMEXPR ADD");
		return 0;
	}

	if(parc < 4)
	{
		sendto_one_notice(source_p, ":Usage: ADD <compile opts|0> <match opts|0> <jit opts|0> <expression>");
		return 0;
	}

	const long comp_opts = parse_pcre_opts(parv[0], reflect_pcre_comp)    | conf_compile_opts;
	const long match_opts = parse_pcre_opts(parv[1], reflect_pcre_match)  | conf_match_opts;
	const long jit_opts = parse_pcre_opts(parv[2], reflect_pcre_jit)      | conf_jit_opts;
	const char *const pattern = parv[3];

	size_t erroff;
	int errcode;
	static char errbuf[BUFSIZE];
	struct Expr *const expr = activate_new_expr(pattern, comp_opts, match_opts, jit_opts, NULL, &errcode, &erroff, errbuf, sizeof(errbuf));
	if(!expr)
	{
		if(IsPerson(source_p))
			sendto_one_notice(source_p, ":Invalid expression (%d) @%zu: %s.",
			                  errcode,
			                  erroff,
			                  errbuf);
		return 0;
	}

	if(MyClient(source_p) && IsPerson(source_p))
	{
		sendto_server(client_p, NULL, CAP_ENCAP, NOCAPS,
		              ":%s ENCAP * SPAMEXPR ADD %s %s %s :%s",
		              client_p->id,
		              parv[0],
		              parv[1],
		              parv[2],
		              parv[3]);

		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
		                       "spamfilter: Expression #%u added: \"%s\".",
		                       expr->id,
		                       expr->pattern);

		sendto_one_notice(source_p, ":Added expression #%u.", expr->id);
	}

	return 0;
}


static
int spamexpr_del(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(!IsOper(source_p) && !IsServer(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "SPAMEXPR DEL");
		return 0;
	}

	if(parc < 1)
	{
		sendto_one_notice(source_p, ":Must specify an expression id number.");
		return 0;
	}

	const unsigned int id = atol(parv[0]);
	if(!deactivate_and_free_expr(id))
	{
		sendto_one_notice(source_p, ":Failed to deactivate any expression with ID #%u.", id);
		return 0;
	}

	if(MyClient(source_p) && IsPerson(source_p))
	{
		sendto_server(client_p, NULL, CAP_ENCAP, NOCAPS,
		              ":%s ENCAP * SPAMEXPR DEL %u",
		              client_p->id,
		              id);

		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
		                       "spamfilter: Expression #%u removed.",
		                       id);

		sendto_one_notice(source_p, ":Removed expression #%u.", id);
	}

	return 0;
}


static
int spamexpr_test(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(!IsOper(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "SPAMEXPR TEST");
		return 0;
	}

	if(parc < 2)
	{
		sendto_one_notice(source_p, ":Specify an ID and text argument, or ID -1 for all.");
		return 0;
	}

	if(!rb_dictionary_size(exprs))
	{
		sendto_one_notice(source_p, ":No expressions have been added to test.");
		return 0;
	}

	const unsigned int id = atol(parv[0]);
	const size_t len = strlen(parv[1]);

	if(id > 0)
	{
		struct Expr *expr = find_expr(id);
		if(!expr)
		{
			sendto_one_notice(source_p, ":Failed to find expression with ID #%u", id);
			return 0;
		}

		const int ret = match_expr(expr, parv[1], len, 0, 0);
		sendto_one_notice(source_p, ":#%-2d: (%d) %s", id, ret, ret > 0? "POSITIVE" : "NEGATIVE");
		return 0;
	}

	struct Expr *expr;
	rb_dictionary_iter state;
	RB_DICTIONARY_FOREACH(expr, &state, exprs)
	{
		const int ret = match_expr(expr, parv[1], len, 0, 0);
		sendto_one_notice(source_p, ":#%-2d: (%d) %s", id, ret, ret > 0? "POSITIVE" : "NEGATIVE");
	}

	return 0;
}


static
int spamexpr_sync(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(!IsOper(source_p) && !IsServer(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "SPAMEXPR SYNC");
		return 0;
	}

	struct Expr *expr;
	rb_dictionary_iter state;
	RB_DICTIONARY_FOREACH(expr, &state, exprs)
	{
		char comp_opts[128] = {0}, match_opts[128] = {0}, jit_opts[128] = {0};
		strlcat_pcre_opts(expr->comp_opts, comp_opts, sizeof(comp_opts), str_pcre_comp);
		strlcat_pcre_opts(expr->match_opts, match_opts, sizeof(match_opts), str_pcre_match);
		strlcat_pcre_opts(expr->jit_opts, jit_opts, sizeof(jit_opts), str_pcre_jit);
		sendto_server(&me, NULL, CAP_ENCAP, NOCAPS,
		              ":%s ENCAP * SPAMEXPR ADD %s %s %s :%s",
		              me.id,
		              comp_opts,
		              match_opts,
		              jit_opts,
		              expr->pattern);
	}

	return 0;
}


static void
m_spamexpr(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(parc <= 1)
	{
		sendto_one_notice(source_p, ":Insufficient parameters.");
		return;
	}

	if(rb_strcasecmp(parv[1], "LIST") == 0)
	{
		spamexpr_list(client_p, source_p, parc - 2, parv + 2);
		return;
	}

	if(rb_strcasecmp(parv[1], "INFO") == 0)
	{
		spamexpr_info(client_p, source_p, parc - 2, parv + 2);
		return;
	}

	if(rb_strcasecmp(parv[1], "ADD") == 0)
	{
		spamexpr_add(client_p, source_p, parc - 2, parv + 2);
		return;
	}

	if(rb_strcasecmp(parv[1], "DEL") == 0)
	{
		spamexpr_del(client_p, source_p, parc - 2, parv + 2);
		return;
	}

	if(rb_strcasecmp(parv[1], "TEST") == 0)
	{
		spamexpr_test(client_p, source_p, parc - 2, parv + 2);
		return;
	}

	if(rb_strcasecmp(parv[1], "SYNC") == 0)
	{
		spamexpr_sync(client_p, source_p, parc - 2, parv + 2);
		return;
	}

	sendto_one_notice(source_p, ":Command not found.");
}

struct Message msgtab =
{
	"SPAMEXPR", 0, 0, 0, 0,
	{
		mg_ignore,                 /* unregistered clients   */
		{ m_spamexpr, 0   },       /* local clients          */
		{ m_spamexpr, 0   },       /* remote clients         */
		{ m_spamexpr, 0   },       /* servers                */
		{ m_spamexpr, 0   },       /* ENCAP                  */
		{ m_spamexpr, 0   }        /* ircops                 */
	}
};



/*
 * Hook handlers
 */

static
void hook_spamfilter_query(hook_data_privmsg_channel *const hook)
{
	static char reason[64];

	if(hook->approved != 0)
		return;

	struct Expr *const expr = match_any_expr(hook->text, strlen(hook->text), 0,0);
	if(!expr)
		return;

	snprintf(reason, sizeof(reason), "expr: matched #%u", expr->id);
	hook->reason = reason;
	hook->approved = -1;
}


static
void hook_doing_stats(hook_data_int *const data)
{
	const char statchar = (char)data->arg2;
	if(statchar != STATCHAR_SPAMFILTER)
		return;

	struct Expr *expr;
	rb_dictionary_iter state;
	RB_DICTIONARY_FOREACH(expr, &state, exprs)
	{
		char comp_opts[128] = {0}, match_opts[128] = {0};
		strlcat_pcre_opts(expr->comp_opts, comp_opts, sizeof(comp_opts), str_pcre_comp);
		strlcat_pcre_opts(expr->match_opts, match_opts, sizeof(match_opts), str_pcre_match);
		sendto_one_numeric(data->client, RPL_STATSDEBUG,
		                   "%c %u %lu %lu %s %s :%s",
		                   statchar,
		                   expr->hits,
		                   expr->last,
		                   expr->added,
		                   strlen(comp_opts)? comp_opts : "*",
		                   strlen(match_opts)? match_opts : "*",
		                   expr->pattern);
	}
}


static
void hook_server_introduced(hook_data_client *const data)
{
	if(data && data->target)
	{
		spamexpr_sync(data->target, data->target, 0,NULL);
		sendto_server(&me, NULL, CAP_ENCAP, NOCAPS,
		              ":%s ENCAP %s SPAMEXPR SYNC",
		              me.id,
		              data->target->id);
		return;
	}

	sendto_server(&me, NULL, CAP_ENCAP, NOCAPS,
	              ":%s ENCAP * SPAMEXPR SYNC",
	              me.id);
}



/*
 * Expression dictionary functors
 */

static
int exprs_comparator(const unsigned int *const a, const unsigned int *const b)
{
	return *b - *a;
}

static
void exprs_destructor(rb_dictionary_element *const ptr, void *const priv)
{
	free_expr(ptr->data);
}



/*
 * Conf handlers
 */

static
int set_parm_opt(const conf_parm_t *const parm,
                 unsigned int *const dest,
                 long (*const reflector)(const char *))
{
	const unsigned int opt = reflector(parm->v.string);
	if(!opt)
		return 0;

	*dest |= opt;
	return 1;
}


static
void set_parm_opts(const conf_parm_t *const val,
                   unsigned int *const dest,
                   long (*const reflector)(const char *),
                   const char *const optname)
{
	for(const conf_parm_t *parm = val; parm; parm = parm->next)
		if(!set_parm_opt(parm, dest, reflector))
			conf_report_error("Unrecognized PCRE %s option: %s", optname, parm->v.string);
}


/* general */
static void set_conf_limit(void *const val)               { conf_limit = *(int *)val;               }
static void set_conf_match_limit(void *const val)         { conf_match_limit = *(int *)val;         }
static void set_conf_recursion_limit(void *const val)     { conf_recursion_limit = *(int *)val;     }
static void set_conf_parens_nest_limit(void *const val)   { conf_parens_nest_limit = *(int *)val;   }
static void set_conf_jit_stack_size(void *const val)      { conf_jit_stack_size = *(int *)val;      }
static void set_conf_jit_stack_max_size(void *const val)  { conf_jit_stack_max_size = *(int *)val;  }

static
void set_conf_compile_opts(void *const val)
{
	conf_compile_opts = 0;
	set_parm_opts(val, &conf_compile_opts, reflect_pcre_comp, "compile");
}

static
void set_conf_match_opts(void *const val)
{
	conf_match_opts = 0;
	set_parm_opts(val, &conf_match_opts, reflect_pcre_match, "match");
}

static
void set_conf_jit_opts(void *const val)
{
	conf_jit_opts = 0;
	set_parm_opts(val, &conf_jit_opts, reflect_pcre_jit, "jit");
}



/*
 * spamexpr conf block
 */

struct
{
	char pattern[BUFSIZE];
	unsigned int comp_opts;
	unsigned int match_opts;
	unsigned int jit_opts;
}
conf_spamexpr_cur;

static
void conf_spamexpr_comp_opts(void *const val)
{
	set_parm_opts(val, &conf_spamexpr_cur.comp_opts, reflect_pcre_comp, "compile");
}

static
void conf_spamexpr_match_opts(void *const val)
{
	set_parm_opts(val, &conf_spamexpr_cur.match_opts, reflect_pcre_match, "match");
}

static
void conf_spamexpr_jit_opts(void *const val)
{
	set_parm_opts(val, &conf_spamexpr_cur.jit_opts, reflect_pcre_jit, "jit");
}


static
void conf_spamexpr_pattern(void *const val)
{
	const char *const pattern = val;
	rb_strlcpy(conf_spamexpr_cur.pattern, pattern, sizeof(conf_spamexpr_cur.pattern));
}


static
int conf_spamexpr_start(struct TopConf *const tc)
{
	memset(&conf_spamexpr_cur, 0x0, sizeof(conf_spamexpr_cur));
	conf_spamexpr_cur.comp_opts |= conf_compile_opts;
	conf_spamexpr_cur.match_opts |= conf_match_opts;
	conf_spamexpr_cur.jit_opts |= conf_jit_opts;
	return 0;
}


static
int conf_spamexpr_end(struct TopConf *const tc)
{
	if(EmptyString(conf_spamexpr_cur.pattern))
	{
		conf_report_error("spamexpr block needs a pattern");
		return -1;
	}

	int errcode;
	size_t erroff;
	static char errbuf[BUFSIZE];
	struct Expr *const expr = activate_new_expr(conf_spamexpr_cur.pattern,
	                                            conf_spamexpr_cur.comp_opts,
	                                            conf_spamexpr_cur.match_opts,
	                                            conf_spamexpr_cur.jit_opts,
	                                            NULL,
	                                            &errcode,
	                                            &erroff,
	                                            errbuf,
	                                            sizeof(errbuf));
	if(!expr && errcode != EXPR_ERROR_EXISTS)
		conf_report_error("Invalid spamexpr block (%d) @%d: %s.",
			              errcode,
			              erroff,
			              errbuf);
	return 0;
}


struct ConfEntry conf_spamexpr[] =
{
	{ "compile_opts",     CF_STRING | CF_FLIST,   conf_spamexpr_comp_opts,   0, NULL },
	{ "match_opts",       CF_STRING | CF_FLIST,   conf_spamexpr_match_opts,  0, NULL },
	{ "jit_opts",         CF_STRING | CF_FLIST,   conf_spamexpr_jit_opts,    0, NULL },
	{ "pattern",          CF_QSTRING,             conf_spamexpr_pattern,     0, NULL },
	{ "\0",               0,                      NULL,                      0, NULL }
};


static
int conf_spamfilter_expr_end(struct TopConf *const tc)
{
	if(jstack)
	{
		pcre2_jit_stack_free(jstack);
		jstack = NULL;
	}

	if(conf_jit_stack_size && conf_jit_stack_max_size)
		jstack = pcre2_jit_stack_create(conf_jit_stack_size, conf_jit_stack_max_size, gctx);

	return 0;
}


struct ConfEntry conf_spamfilter_expr[] =
{
	{ "limit",              CF_INT,                set_conf_limit,               0,  NULL },
	{ "match_limit",        CF_INT,                set_conf_match_limit,         0,  NULL },
	{ "recursion_limit",    CF_INT,                set_conf_recursion_limit,     0,  NULL },
	{ "parens_nest_limit",  CF_INT,                set_conf_parens_nest_limit,   0,  NULL },
	{ "jit_stack_size",     CF_INT,                set_conf_jit_stack_size,      0,  NULL },
	{ "jit_stack_max_size", CF_INT,                set_conf_jit_stack_max_size,  0,  NULL },
	{ "compile_opts",       CF_STRING | CF_FLIST,  set_conf_compile_opts,        0,  NULL },
	{ "match_opts",         CF_STRING | CF_FLIST,  set_conf_match_opts,          0,  NULL },
	{ "jit_opts",           CF_STRING | CF_FLIST,  set_conf_jit_opts,            0,  NULL },
	{ "\0",                 0,                     NULL,                         0,  NULL }
};


/*
 * Module main
 */

static
int modinit(void)
{
	exprs = rb_dictionary_create("exprs", exprs_comparator);
	gctx = pcre2_general_context_create(expr_malloc_cb, expr_free_cb, NULL);

	/* Block for general configuration */
	add_top_conf("spamfilter_expr", NULL, conf_spamfilter_expr_end, conf_spamfilter_expr);

	/* Block(s) for any expressions */
	add_top_conf("spamexpr", conf_spamexpr_start, conf_spamexpr_end, conf_spamexpr);

	/* If the module was loaded but no rehash occurs, we still need action with the defaults */
	conf_spamfilter_expr_end(NULL);
	hook_server_introduced(NULL);
	return 0;
}


static
void modfini(void)
{
	if(jstack)
		pcre2_jit_stack_free(jstack);

	remove_top_conf("spamexpr");
	remove_top_conf("spamfilter_expr");
	pcre2_general_context_free(gctx);
	rb_dictionary_destroy(exprs, exprs_destructor, NULL);
}


mapi_clist_av1 clist[] =
{
	&msgtab,
	NULL
};

mapi_hfn_list_av1 hfnlist[] =
{
	{ "spamfilter_query", (hookfn)hook_spamfilter_query       },
	{ "server_introduced", (hookfn)hook_server_introduced     },
	{ "doing_stats", (hookfn)hook_doing_stats                 },
	{ NULL, NULL                                              }
};

DECLARE_MODULE_AV1
(
	spamfilter_expr,
	modinit,
	modfini,
	clist,
	NULL,
	hfnlist,
	"$Revision: 0 $"
);
