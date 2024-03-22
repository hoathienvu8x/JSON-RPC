#include <parson.h>
#include <string.h>
#include <stdio.h>
#include <procedure.h>

JSON_Value * hello_eval(JSON_Value * params, JSON_Value * id) {
  (void)id;
  const char * name = json_object_get_string(json_object(params), "name");
  if (!name) return NULL;
  char s[1024] = {0};
  sprintf(s, "Halo %s", name);
  JSON_Value * obj = json_value_init_string(s);
  return obj;
}

int jrpc_procedure_info(struct jrpc_procedure * proc) {
  proc->name = strdup("hello");
  if (proc->name == NULL) return -1;
  proc->description = strdup("Hello procedure");
  proc->procedure = hello_eval;
  return 0;
}
