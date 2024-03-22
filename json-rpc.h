#ifndef __JSON_RPC_H
#define __JSON_RPC_H

#include <procedure.h>

struct jrpc_connection {
  int fd;
};

struct jrpc_server {
  int fd;
  int pid;
  int total;
  int stop;
  struct jrpc_procedure * procedures;
};

int jrpc_server_init(struct jrpc_server * srv, const char * host, int port);
void jrpc_server_destroy(struct jrpc_server * srv);
void jrpc_server_add_procedure(struct jrpc_server * srv, struct jrpc_procedure * proc);
void jrpc_server_remove_procedure(struct jrpc_server * srv, const char * name);
void jrpc_server_listen(struct jrpc_server * srv);
void jrpc_server_stop(struct jrpc_server * srv);
const char * jrpc_server_error(int code);

#endif
