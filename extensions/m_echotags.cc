using namespace ircd;

static void m_echotags(struct MsgBuf *msgbuf_p, client::client *client_p, client::client *source_p, int parc, const char *parv[]);

struct Message echotags_msgtab = {
	"ECHOTAGS", 0, 0, 0, 0,
	{ mg_ignore, {m_echotags, 0}, mg_ignore, mg_ignore, mg_ignore, {m_echotags, 0} }
};

mapi_clist_av1 echotags_clist[] = { &echotags_msgtab, NULL };

static const char echotags_desc[] = "A test module for tags";

DECLARE_MODULE_AV2(echotags, NULL, NULL, echotags_clist, NULL, NULL, NULL, NULL, echotags_desc);

static void
m_echotags(struct MsgBuf *msgbuf_p, client::client *client_p, client::client *source_p, int parc, const char *parv[])
{
	sendto_one_notice(source_p, ":*** You sent %zu tags.", msgbuf_p->n_tags);

	for (size_t i = 0; i < msgbuf_p->n_tags; i++)
	{
		struct MsgTag *tag = &msgbuf_p->tags[i];

		if (tag->value)
			sendto_one_notice(source_p, ":*** %zu: %s => %s", i, tag->key, tag->value);
		else
			sendto_one_notice(source_p, ":*** %zu: %s", i, tag->key);
	}
}


