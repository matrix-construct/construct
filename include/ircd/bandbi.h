#ifndef INCLUDED_bandbi_h
#define INCLUDED_bandbi_h

void init_bandb(void);

typedef enum
{
	BANDB_KLINE,
	BANDB_DLINE,
	BANDB_XLINE,
	BANDB_RESV,
	LAST_BANDB_TYPE
} bandb_type;

void bandb_add(bandb_type, struct Client *source_p, const char *mask1,
	       const char *mask2, const char *reason, const char *oper_reason, int perm);
void bandb_del(bandb_type, const char *mask1, const char *mask2);
void bandb_rehash_bans(void);
#endif
