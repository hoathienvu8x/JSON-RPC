#include <procedure.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>

typedef int (*jrpc_procedure_info)(struct jrpc_procedure *);
int jrpc_load_procedure(const char * filepath, struct jrpc_procedure * proc) {
  //void * plg = dlopen(filepath, RTLD_NOW | RTLD_LOCAL | RTLD_LAZY);
  void * plg = dlopen(filepath, RTLD_NOW | RTLD_GLOBAL | RTLD_LAZY);
  if (plg == NULL) return -1;
  jrpc_procedure_info fn = (jrpc_procedure_info)(intptr_t)dlsym(plg, "jrpc_procedure_info");
  if (!fn) return -1;
  return (*fn)(proc);
}
