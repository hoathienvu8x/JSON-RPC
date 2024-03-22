#ifndef __JRPC_PROCEDURE_H
#define __JRPC_PROCEDURE_H

#define PARSON_INDENT_STR "  "
#include <parson.h>

#define JRPC_VERSION "2.0"

#define JRPC_PARSE_ERROR      -32700
#define JRPC_INVALID_REQUEST  -32600
#define JRPC_METHOD_NOT_FOUND -32601
#define JRPC_INVALID_PARAMS   -32602
#define JRPC_INTERNAL_ERROR   -32603

typedef JSON_Value * (*jrpc_eval)(JSON_Value *, JSON_Value *);

struct jrpc_procedure {
  char * name;
  char * description;
  jrpc_eval procedure;
};

int jrpc_load_procedure(const char * filepath, struct jrpc_procedure * proc);

#endif
