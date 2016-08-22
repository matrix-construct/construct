/*
 * +W snomask: Displays if a local user has done a WHOIS request on you.
 * derived from spy_whois_notice.c.
 *
 * If #define OPERONLY is removed, then any user can use this snomask
 * (you need to put ~servnotice in oper_only_umodes for this to work).
 */

/* undefine this to allow anyone to receive whois notifications */
#define OPERONLY

using namespace ircd;

static const char sno_desc[] =
	"Adds server notice mask +W that allows "
#ifdef OPERONLY
	"operators"
#else
	"users"
#endif
	" to receive notices for when a WHOIS has been done on them";

void show_whois(hook_data_client *);

mapi_hfn_list_av1 whois_hfnlist[] = {
	{"doing_whois", (hookfn) show_whois},
	{"doing_whois_global", (hookfn) show_whois},
	{NULL, NULL}
};

static int
init(void)
{
	snomask_modes['W'] = find_snomask_slot();

	return 0;
}

static void
fini(void)
{
	snomask_modes['W'] = 0;
}

DECLARE_MODULE_AV2(sno_whois, init, fini, NULL, NULL, whois_hfnlist, NULL, NULL, sno_desc);

void
show_whois(hook_data_client *data)
{
	client::client *source_p = data->client;
	client::client *target_p = data->target;

	if(MyClient(target_p) &&
#ifdef OPERONLY
	   IsOper(target_p) &&
#endif
	   (source_p != target_p) &&
	   (target_p->snomask & snomask_modes['W']))
	{
		sendto_one_notice(target_p,
				":*** Notice -- %s (%s@%s) is doing a whois on you [%s]",
				source_p->name,
				source_p->username, source_p->host,
				source_p->servptr->name);
	}
}
