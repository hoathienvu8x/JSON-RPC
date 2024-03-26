CC = gcc
CFLAGS = -std=gnu99 -Wall -Wextra -Werror -pedantic -fPIC
LDFLAGS = -I. -ldl -lpthread -lm -DJRPC_PROCEDURES=\"$(shell pwd)/procs\" \
	-DJRPC_PORT=1234

ifeq ($(build),release)
	CFLAGS += -O3
	LDFLAGS += -DNDEBUG=1 -DBUILDDATE=$(shell date +%s)
else
	CFLAGS += -O0 -g
endif
RM = rm -rf

OBJECTS = parson.o procedure.o json-rpc.o
OBJECTS := $(addprefix objects/,$(OBJECTS))

all: objects parson $(OBJECTS)

json-rpc: objects objects/rpc.o $(OBJECTS)
	@$(CC) objects/rpc.o $(OBJECTS) -o $@ $(LDFLAGS)
	@$(RM) objects/rpc.o

rpc: objects objects/remote.o objects/parson.o
	@$(CC) objects/remote.o objects/parson.o -o $@ $(LDFLAGS)
	@$(RM) objects/remote.o objects/parson.o

objects:
	@mkdir -p objects

parson:
	@curl -sL 'https://raw.githubusercontent.com/kgabis/parson/master/parson.h' -o parson.h
	@curl -sL 'https://raw.githubusercontent.com/kgabis/parson/master/parson.c' -o parson.c

procs/%.so: objects/%.o objects/parson.o
	@$(CC) -shared $< objects/parson.o -o $@

objects/%.o: %.c
	@$(CC) -c $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	@$(RM) $(OBJECTS) json-rpc
