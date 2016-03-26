/*
 * Extban that combines other extbans.
 *
 * Basic example:
 * $&:~a,m:*!*@gateway/web/cgi-irc*
 * Which means: match unidentified webchat users.
 * ("m" is another new extban type, which just does a normal match).
 *
 * More complicated example:
 * $&:~a,|:(m:*!*@gateway/web/foo,m:*!*@gateway/web/bar)
 * Which means: unidentified and using the foo or bar gateway.
 *
 * Rules:
 *
 * - Optional pair of parens around data.
 *
 * - component bans are separated by commas, but commas between
 *   matching pairs of parens are skipped.
 *
 * - Unbalanced parens are an error.
 *
 * - Parens, commas and backslashes can be escaped by backslashes.
 *
 * - A backslash before any character other than a paren or backslash
 *   is just a backslash (backslash and character are both used).
 *
 * - Non-existant extbans are invalid.
 *   This is primarily for consistency with non-combined bans:
 *   the ircd does not let you set +b $f unless the 'f' extban is loaded,
 *   so setting $&:f should be impossible too.
 *
 * Issues:
 * - Backslashes double inside nested bans.
 *   Hopefully acceptable because they should be rare.
 *
 * - Is performance good enough?
 *   I suspect it is, but have done no load testing.
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "ircd.h"

static const char extb_desc[] = "Combination ($&, $|) extban types";

// #define MOD_DEBUG(s) sendto_realops_snomask(SNO_DEBUG, L_NETWIDE, (s))
#define MOD_DEBUG(s)
#define RETURN_INVALID	{ recursion_depth--; return EXTBAN_INVALID; }

static int _modinit(void);
static void _moddeinit(void);
static int eb_or(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);
static int eb_and(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);
static int eb_combi(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type, bool is_and);
static int recursion_depth = 0;

DECLARE_MODULE_AV2(extb_extended, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, extb_desc);

static int
_modinit(void)
{
	extban_table['&'] = eb_and;
	extban_table['|'] = eb_or;

	return 0;
}

static void
_moddeinit(void)
{
	extban_table['&'] = NULL;
	extban_table['|'] = NULL;
}

static int eb_or(const char *data, struct Client *client_p,
				 struct Channel *chptr, long mode_type)
{
	return eb_combi(data, client_p, chptr, mode_type, false);
}

static int eb_and(const char *data, struct Client *client_p,
				  struct Channel *chptr, long mode_type)
{
	return eb_combi(data, client_p, chptr, mode_type, true);
}

static int eb_combi(const char *data, struct Client *client_p,
					struct Channel *chptr, long mode_type, bool is_and)
{
	const char *p, *banend;
	bool have_result = false;
	int allowed_nodes = 11;
	size_t datalen;

	if (recursion_depth >= 5) {
		MOD_DEBUG("combo invalid: recursion depth too high");
		return EXTBAN_INVALID;
	}

	if (EmptyString(data)) {
		MOD_DEBUG("combo invalid: empty data");
		return EXTBAN_INVALID;
	}

	datalen = strlen(data);
	if (datalen > BANLEN) {
		/* I'd be sad if this ever happened, but if it does we
		 * could overflow the buffer used below, so...
		 */
		MOD_DEBUG("combo invalid: > BANLEN");
		return EXTBAN_INVALID;
	}
	banend = data + datalen;

	if (data[0] == '(') {
		p = data + 1;
		banend--;
		if (*banend != ')') {
			MOD_DEBUG("combo invalid: starting but no closing paren");
			return EXTBAN_INVALID;
		}
	} else {
		p = data;
	}

	/* Empty combibans are invalid. */
	if (banend == p) {
		MOD_DEBUG("combo invalid: no data (after removing parens)");
		return EXTBAN_INVALID;
	}

	/* Implementation note:
	 * I want it to be impossible to set a syntactically invalid combi-ban.
	 * (mismatched parens).
	 * That is: valid_extban should return false for those.
	 * Ideally we do not parse the entire ban when actually matching it:
	 * we can just short-circuit if we already know the ban is valid.
	 * Unfortunately there is no separate hook or mode_type for validation,
	 * so we always keep parsing even after we have determined a result.
	 */

	recursion_depth++;

	while (--allowed_nodes) {
		bool invert = false;
		char *child_data, child_data_buf[BANLEN];
		ExtbanFunc f;

		if (*p == '~') {
			invert = true;
			p++;
			if (p == banend) {
				MOD_DEBUG("combo invalid: no data after ~");
				RETURN_INVALID;
			}
		}

		f = extban_table[(unsigned char) *p++];
		if (!f) {
			MOD_DEBUG("combo invalid: non-existant child extban");
			RETURN_INVALID;
		}

		if (*p == ':') {
			unsigned int parencount = 0;
			bool escaped = false, done = false;
			char *o;

			p++;

			/* Possible optimization: we can skip the actual copy if
			 * we already have_result.
			 */
			o = child_data = child_data_buf;
			while (true) {
				if (p == banend) {
					if (parencount) {
						MOD_DEBUG("combo invalid: EOD while in parens");
						RETURN_INVALID;
					}
					break;
				}

				if (escaped) {
					if (*p != '(' && *p != ')' && *p != '\\' && *p != ',')
						*o++ = '\\';
					*o++ = *p++;
					escaped = false;
				} else {
					switch (*p) {
					case '\\':
						escaped = true;
						break;
					case '(':
						parencount++;
						*o++ = *p;
						break;
					case ')':
						if (!parencount) {
							MOD_DEBUG("combo invalid: negative parencount");
							RETURN_INVALID;
						}
						parencount--;
						*o++ = *p;
						break;
					case ',':
						if (parencount)
							*o++ = *p;
						else
							done = true;
						break;
					default:
						*o++ = *p;
						break;
					}
					if (done)
						break;
					p++;
				}
			}
			*o = '\0';
		} else {
			child_data = NULL;
		}

		if (!have_result) {
			int child_result = f(child_data, client_p, chptr, mode_type);

			if (child_result == EXTBAN_INVALID) {
				MOD_DEBUG("combo invalid: child invalid");
				RETURN_INVALID;
			}

			/* Convert child_result to a plain boolean result */
			if (invert)
				child_result = child_result == EXTBAN_NOMATCH;
			else
				child_result = child_result == EXTBAN_MATCH;

			if (is_and ? !child_result : child_result)
				have_result = true;
		}

		if (p == banend)
			break;

		if (*p++ != ',') {
			MOD_DEBUG("combo invalid: no ',' after ban");
			RETURN_INVALID;
		}

		if (p == banend) {
			MOD_DEBUG("combo invalid: banend after ','");
			RETURN_INVALID;
		}
	}

	/* at this point, *p should == banend */
	if (p != banend) {
		MOD_DEBUG("combo invalid: more child extbans than allowed");
		RETURN_INVALID;
	}

	recursion_depth--;

	if (is_and)
		return have_result ? EXTBAN_NOMATCH : EXTBAN_MATCH;
	else
		return have_result ? EXTBAN_MATCH : EXTBAN_NOMATCH;
}
