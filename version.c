#include <parson.h>
#include <string.h>
#include <stdio.h>
#include <procedure.h>

JSON_Value * version_eval(JSON_Value * params, JSON_Value * id) {
  (void)id;
  (void)params;
  JSON_Value * obj = json_value_init_string(JRPC_VERSION);
  return obj;
}

int jrpc_procedure_info(struct jrpc_procedure * proc) {
  proc->name = strdup("version");
  if (proc->name == NULL) return -1;
  proc->description = strdup("version procedure");
  proc->procedure = version_eval;
  return 0;
}
