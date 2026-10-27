#include "web_client.h"

int web_read_pi(client_context_t* client, int arg);
int web_write_pi(client_context_t* client, int arg);

int web_read_ps(client_context_t* client, int arg);
int web_write_ps(client_context_t* client, int arg);

int web_read_dps(client_context_t* client, int arg);
int web_write_dps(client_context_t* client, int arg);

int web_read_dpss(client_context_t* client, int arg);
int web_write_dpss(client_context_t* client, int arg);

int web_read_parse(client_context_t* client, int arg);
int web_write_parse(client_context_t* client, int arg);

int web_read_pty(client_context_t* client, int arg);
int web_write_pty(client_context_t* client, int arg);

int web_read_tp(client_context_t* client, int arg);
int web_write_tp(client_context_t* client, int arg);

int web_read_ta(client_context_t* client, int arg);
int web_write_ta(client_context_t* client, int arg);

int web_read_di(client_context_t* client, int arg);
int web_write_di(client_context_t* client, int arg);

int web_read_ms(client_context_t* client, int arg);
int web_write_ms(client_context_t* client, int arg);

int web_read_text(client_context_t* client, int arg);
int web_write_text(client_context_t* client, int arg);

int web_read_af(client_context_t* client, int arg);
int web_write_af(client_context_t* client, int arg);

int web_read_echo(client_context_t* client, int arg);
int web_write_echo(client_context_t* client, int arg);

int web_read_rdson(client_context_t* client, int arg);
int web_write_rdson(client_context_t* client, int arg);

int web_read_pilot(client_context_t* client, int arg);
int web_write_pilot(client_context_t* client, int arg);

int web_read_phase(client_context_t* client, int arg);
int web_write_phase(client_context_t* client, int arg);

int web_read_scrollspeed(client_context_t* client, int arg);
int web_write_scrollspeed(client_context_t* client, int arg);

void web_proc_save(client_context_t* client);
void web_proc_reset(client_context_t* client);