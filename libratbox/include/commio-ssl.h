int rb_setup_ssl_server(const char *cert, const char *keyfile, const char *dhfile);
int rb_init_ssl(void);

int rb_ssl_listen(rb_fde_t *F, int backlog);
int rb_init_prng(const char *path, prng_seed_t seed_type);

int rb_get_random(void *buf, size_t length);
const char *rb_get_ssl_strerror(rb_fde_t *F);
void rb_ssl_start_accepted(rb_fde_t *new_F, ACCB *cb, void *data, int timeout);
void rb_ssl_start_connected(rb_fde_t *F, CNCB *callback, void *data, int timeout);
void rb_connect_tcp_ssl(rb_fde_t *F, struct sockaddr *dest, struct sockaddr *clocal, int socklen, CNCB *callback, void *data, int timeout);
void rb_ssl_accept_setup(rb_fde_t *F, rb_fde_t *new_F, struct sockaddr *st, int addrlen);
void rb_ssl_shutdown(rb_fde_t *F);
ssize_t rb_ssl_read(rb_fde_t *F, void *buf, size_t count);
ssize_t rb_ssl_write(rb_fde_t *F, const void *buf, size_t count);


