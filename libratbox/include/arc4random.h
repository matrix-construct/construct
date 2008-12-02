

#if !defined(HAVE_OPENSSL) && !defined(HAVE_GNUTLS) && !defined(HAVE_ARC4RANDOM)
void arc4random_stir(void);
uint32_t arc4random(void);
void arc4random_addrandom(uint8_t *dat, int datlen);
#endif
