#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <json-rpc.h>
#include <procedure.h>
#include <time.h>

#ifndef JRPC_PROCEDURES
#define JRPC_PROCEDURES "procs"
#endif

#ifndef JRPC_PORT
#define JRPC_PORT (1234)
#endif

static int jrpc_load_procedures(struct jrpc_server * srv);

int main(int argc, char ** argv) {
  (void)argc;
  (void)argv;
  #ifdef BUILDDATE
  time_t timestamp = BUILDDATE;
  char s[100] = {0};
  if (strftime(s, 100, "%d/%m/%Y @%H:%M", localtime(&timestamp)) > 0) {
    printf("build: %s\n", s);
  }
  #endif
  int rc = 0;
  struct jrpc_server srv;
  if (jrpc_server_init(&srv, NULL, JRPC_PORT) < 0) {
    return -1;
  }
  if (jrpc_load_procedures(&srv) < 0) {
    rc = -1;
    goto out;
  }
  jrpc_server_listen(&srv);

out:
  jrpc_server_destroy(&srv);
  return rc;
}

static int jrpc_load_procedures(struct jrpc_server * srv) {
  DIR *d;
  struct dirent *dp;
  d = opendir(JRPC_PROCEDURES);
  if (!d) return -1;
  char fpath[512] = {0};
  while ((dp = readdir(d))) {
    if (strstr(dp->d_name + strlen(dp->d_name) - 3, ".so") != NULL) {
      memset(&fpath, 0, sizeof(fpath));
      sprintf(fpath, "%s/%s", JRPC_PROCEDURES, dp->d_name);
      struct jrpc_procedure proc;
      if (jrpc_load_procedure(fpath, &proc) == 0) {
        jrpc_server_add_procedure(srv, &proc);
      }
    }
  }
  closedir(d);
  if (srv->total == 0) return -1;
  return 0;
}
