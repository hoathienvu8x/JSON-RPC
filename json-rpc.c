#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <netdb.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <json-rpc.h>
#include <pthread.h>

#define MAXEPOLLSIZE  (1000)
#define BACKLOG       (200)

static int make_non_blocking(int fd) {
  int flags, s;
  flags = fcntl(fd, F_GETFL, 0);
  if(flags == -1) {
    #ifndef NDEBUG
    perror("fcntl");
    #endif
    return -1;
  }
  flags |= O_NONBLOCK;
  s = fcntl(fd, F_SETFL, flags);
  if(s == -1) {
    #ifndef NDEBUG
    perror("fcntl");
    #endif
    return -1;
  }
  return 0;
}

static int add_to_epoll(int pid, int fd, uint32_t events) {
  struct epoll_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.events = events;
  ev.data.fd = fd;
  if (epoll_ctl(pid, EPOLL_CTL_ADD, fd, &ev) < 0) {
    return -1;
  }
  return 0;
}

static int remove_from_epoll(int pid, int fd) {
  if (epoll_ctl(pid, EPOLL_CTL_DEL, fd, NULL) < 0) {
    return -1;
  }
  return 0;
}

static void jrpc_send_error(
  struct jrpc_connection * conn, int code, const char * message,
  JSON_Value * id, int hdr, int version
) {
  JSON_Value * root = json_value_init_object();
  if (!root) return;
  if (version) {
    json_object_set_string(json_object(root), "jsonrpc", JRPC_VERSION);
  }
  json_object_set_value(
    json_object(root), "error", json_value_init_object()
  );
  json_object_dotset_number(json_object(root), "error.code", code);
  json_object_dotset_string(json_object(root), "error.message", message);
  json_object_set_value(json_object(root), "id", json_value_deep_copy(id));
  #ifndef NDEBUG
  char * msg = json_serialize_to_string_pretty(root);
  #else
  char * msg = json_serialize_to_string(root);
  #endif
  if (hdr) {
    char s[1024] = {0};
    int len = sprintf(
      s, "HTTP/1.1 200 OK\r\nContent-Type: application/json; charset=utf-8"
      "\r\nContent-Length: %ld\r\n\r\n", strlen(msg)
    );
    if (len <= 0) goto done;
    s[len] = '\0';
    char * buf = s;
    int left = (int)strlen(s);
    do {
      int rc = send(conn->fd, buf, left, 0);
      if (rc < 0) goto done;
      left -= rc;
      buf += rc;
    } while (left > 0);
  }
  char * buf = msg;
  int left = (int)strlen(buf);
  do {
    int rc = send(conn->fd, buf, left, 0);
    if (rc < 0) break;
    left -= rc;
    buf += rc;
  } while (left > 0);

done:
  json_free_serialized_string(msg);
  json_value_free(root);
}

static void jrpc_send_response(
  struct jrpc_connection * conn, JSON_Value * data, JSON_Value * id,
  int hdr, int version
) {
  JSON_Value * root = json_value_init_object();
  if (!root) return;
  if (version) {
    json_object_set_string(json_object(root), "jsonrpc", JRPC_VERSION);
  }
  json_object_set_value(
    json_object(root), "result", json_value_deep_copy(data)
  );
  json_object_set_value(json_object(root), "id", json_value_deep_copy(id));
  #ifndef NDEBUG
  char * msg = json_serialize_to_string_pretty(root);
  #else
  char * msg = json_serialize_to_string(root);
  #endif
  if (hdr) {
    char s[1024] = {0};
    int len = sprintf(
      s, "HTTP/1.1 200 OK\r\nContent-Type: application/json; charset=utf-8"
      "\r\nContent-Length: %ld\r\n\r\n", strlen(msg)
    );
    if (len <= 0) goto done;
    s[len] = '\0';
    char * buf = s;
    int left = (int)strlen(s);
    do {
      int rc = send(conn->fd, buf, left, 0);
      if (rc < 0) goto done;
      left -= rc;
      buf += rc;
    } while (left > 0);
  }
  char * buf = msg;
  int left = (int)strlen(buf);
  do {
    int rc = send(conn->fd, buf, left, 0);
    if (rc < 0) break;
    left -= rc;
    buf += rc;
  } while (left > 0);

done:
  json_free_serialized_string(msg);
  json_value_free(root);
}

static jrpc_eval * jrpc_server_get_procedure(
  struct jrpc_server * srv, const char * name
) {
  int i;
  for (i = 0; i < srv->total; i++) {
    if (strcmp(srv->procedures[i].name, name) == 0) {
      return &(srv->procedures[i].procedure);
    }
  }
  return NULL;
}

static JSON_Value * jrpc_list_procedures(
  struct jrpc_server * srv, JSON_Value * params
) {
  JSON_Value * lst = json_value_init_array();
  if (!lst) return NULL;
  int pg = (int)json_object_get_number(json_object(params), "page");
  int limit = (int)json_object_get_number(json_object(params), "limit");
  if (pg <= 0) {
    pg = 1;
  }
  if (limit <= 0) {
    limit = 5;
  }
  if (pg == 1) {
    limit -= 2;
  }
  int i = (pg - 1) * limit;
  int end = i + limit + 1;
  if (end > srv->total) {
    end = srv->total;
  }
  if (pg == 1) {
    JSON_Value * it = json_value_init_object();
    json_object_set_string(
      json_object(it), "name", "list_procedures"
    );
    json_object_set_string(
      json_object(it), "description", "List procedures"
    );
    json_array_append_value(json_array(lst), it);
    it = json_value_init_object();
    json_object_set_string(
      json_object(it), "name", "get_procedure"
    );
    json_object_set_string(
      json_object(it), "description", "Get procedure info"
    );
    json_array_append_value(json_array(lst), it);
  }
  for (; i < end; i++) {
    JSON_Value * it = json_value_init_object();
    if (!it) continue;
    json_object_set_string(
      json_object(it), "name", srv->procedures[i].name
    );
    json_object_set_string(
      json_object(it), "description", srv->procedures[i].description
    );
    json_array_append_value(json_array(lst), it);
  }
  return lst;
}

static JSON_Value * jrpc_get_procedure(
  struct jrpc_server * srv, JSON_Value * params
) {
  const char * name = json_object_get_string(json_object(params), "name");
  if (!name) return NULL;
  int i;
  for (i = 0; i < srv->total; i++) {
    if (strcmp(name, srv->procedures[i].name) == 0) {
      JSON_Value * obj = json_value_init_object();
      if (!obj) return NULL;
      json_object_set_string(
        json_object(obj), "name", srv->procedures[i].name
      );
      json_object_set_string(
        json_object(obj), "description", srv->procedures[i].description
      );
      return obj;
    }
  }
  return NULL;
}

struct jrpc_param {
  struct jrpc_server * srv;
  struct jrpc_connection * conn;
};

static void * jrpc_execute(void * ptr) {
  struct jrpc_param * p = (struct jrpc_param *)ptr;
  JSON_Value * obj = NULL;
  char * buff = NULL;
  ssize_t rc, plen = 0, len = 0;
  while (1) {
    if (plen >= len) {
      char * tmp;
      len += 4096;
      tmp = realloc(buff, len);
      if (NULL == tmp) break;
      buff = tmp;
    }
    rc = recv(p->conn->fd, buff + plen, 4096, 0);
    if (rc == -1) {
      if(errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      } else {
        free(buff);
        return NULL;
      }
    } else if (rc == 0) {
      goto done;
    } else {
      plen += rc;
    }
  }
  buff[plen] = '\0';
  int hdr = 1, version = 0;
  char * payload = strstr(buff, "\r\n\r\n");
  if (payload) {
    payload += 4;
  } else {
    payload = buff;
    hdr = 0;
  }
  JSON_Value * id = NULL;
  if (strlen(payload) == 0) {
    goto fail;
  }
  obj = json_parse_string(payload);
  if (!obj || json_value_get_type(obj) != JSONObject) {
    goto fail;
  }
  id = json_object_get_value(json_object(obj), "id");
  if (
    id && json_value_get_type(id) != JSONString &&
    json_value_get_type(id) != JSONNumber
  ) {
    id = NULL;
  }
  if (json_object_has_value(json_object(obj), "jsonrpc")) {
    version = 1;
  }
  const char * method = json_object_get_string(json_object(obj), "method");
  if (!method || strlen(method) == 0) goto fail;
  JSON_Value * params = json_object_get_value(json_object(obj), "params");
  if (
    params && json_value_get_type(params) != JSONArray &&
    json_value_get_type(params) != JSONObject
  ) {
    goto fail;
  }
  JSON_Value * data = NULL;
  if (strcmp(method, "list_procedures") == 0) {
    data = jrpc_list_procedures(p->srv, params);
  } else if (strcmp(method, "get_procedure") == 0) {
    data = jrpc_get_procedure(p->srv, params);
    if (!data) {
      jrpc_send_error(
        p->conn, JRPC_METHOD_NOT_FOUND,
        "Method not found.", id, hdr, version
      );
      goto done;
    }
  } else {
    jrpc_eval * func = jrpc_server_get_procedure(p->srv, method);
    if (!func) {
      jrpc_send_error(
        p->conn, JRPC_METHOD_NOT_FOUND,
        "Method not found.", id, hdr, version
      );
      goto done;
    }
    data = (*func)(params, id);
  }
  if (!data) {
    jrpc_send_error(
      p->conn, JRPC_INTERNAL_ERROR,
      "JSON-RPC internal error.", id, hdr, version
    );
    goto done;
  }
  jrpc_send_response(p->conn, data, id, hdr, version);
  json_value_free(data);
  goto done;

fail:
  jrpc_send_error(
    p->conn, JRPC_INVALID_REQUEST,
    "The JSON sent is not a valid Request object.", id, hdr, version
  );
done:
  if (obj) json_value_free(obj);
  if (buff) free(buff);
  (void)remove_from_epoll(p->srv->pid, p->conn->fd);
  close(p->conn->fd);
  return NULL;
}

int jrpc_server_init(
  struct jrpc_server * srv, const char * host, int port
) {
  int rc, fd, pid, yes = 1;
  struct addrinfo hints, * info, * p;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  char sport[332] = {0};
  sprintf(sport, "%d", port);
  rc = getaddrinfo(host, sport, &hints, &info);
  if (rc != 0) return -1;
  for (p = info; p != NULL; p = p->ai_next) {
    fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (fd == -1) continue;
    if (make_non_blocking(fd) < 0) {
      close(fd);
      continue;
    }
    rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    if (rc == -1) {
      close(fd);
      continue;
    }
    rc = bind(fd, p->ai_addr, p->ai_addrlen);
    if (rc == -1) {
      close(fd);
      continue;
    }
    break;
  }
  if (p == NULL) {
    goto fail;
  }
  pid = epoll_create1(0);
  if (pid < 0) goto fail;
  srv->fd = fd;
  srv->pid = pid;
  srv->stop = 1;
  srv->total = 0;
  srv->procedures = NULL;
  freeaddrinfo(info);
  return 0;

fail:
  freeaddrinfo(info);
  return -1;
}

#define prepare(p) do { if (p) { free(p); } p = NULL; } while(0)
static void jrpc_procedure_destroy(struct jrpc_procedure * proc) {
  prepare(proc->name);
  prepare(proc->description);  
}

void jrpc_server_destroy(struct jrpc_server * srv) {
  srv->stop = 1;
  if (remove_from_epoll(srv->pid, srv->fd) == 0) {
    close(srv->fd);
  }
  if (!srv->procedures) return;
  int i;
  for (i = 0; i < srv->total; i++) {
    jrpc_procedure_destroy(&(srv->procedures[i]));
  }
  free(srv->procedures);
}
void jrpc_server_add_procedure(struct jrpc_server * srv, struct jrpc_procedure * proc) {
  int i = srv->total++;
  if (!srv->procedures) {
    srv->procedures = (struct jrpc_procedure *)calloc(
      1, sizeof(struct jrpc_procedure)
    );
    if (!srv->procedures) return;
  } else {
    struct jrpc_procedure * ptr = (struct jrpc_procedure *)realloc(
      srv->procedures, srv->total * sizeof(struct jrpc_procedure)
    );
    if (!ptr) return;
    srv->procedures = ptr;
  }
  srv->procedures[i].name = strdup(proc->name);
  if (srv->procedures[i].name == NULL) return;
  srv->procedures[i].description = strdup(proc->description);
  srv->procedures[i].procedure = proc->procedure;
}

void jrpc_server_remove_procedure(struct jrpc_server * srv, const char * name) {
  if (!srv->procedures) return;
  int i, found = 0;
  for (i = 0; i < srv->total; i++) {
    if (found) {
      srv->procedures[i - 1] = srv->procedures[i];
    } else if (!strcmp(name, srv->procedures[i].name)) {
      found = 1;
      jrpc_procedure_destroy(&(srv->procedures[i]));
    }
  }
  if (found) {
    srv->total--;
    if (srv->total) {
      struct jrpc_procedure * ptr = (struct jrpc_procedure *)realloc(
        srv->procedures, srv->total * sizeof(struct jrpc_procedure)
      );
      if (!ptr) return;
      srv->procedures = ptr;
    } else {
      srv->procedures = NULL;
    }
  }
}

void jrpc_server_listen(struct jrpc_server * srv) {
  int rc, fds, i, fd;
  rc = listen(srv->fd, BACKLOG);
  if (rc < 0) return;
  struct epoll_event events[MAXEPOLLSIZE];
  memset(&events, 0, sizeof(events));
  rc = add_to_epoll(srv->pid, srv->fd, EPOLLIN | EPOLLET);
  if (rc < 0) return;
  srv->stop = 0;
  struct sockaddr_storage cli;
  socklen_t clen;
  while(1) {
    fds = epoll_wait(srv->pid, events, MAXEPOLLSIZE, -1);
    if (fds == -1) {
      break;
    }
    if (srv->stop) break;
    for (i = 0; i < fds; i++) {
      if (events[i].data.fd == srv->fd) {
        clen = sizeof(cli);
        fd = accept(events[i].data.fd, (struct sockaddr *)&cli, &clen);
        if (fd == -1) {
          if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
          break;
        }
        if(make_non_blocking(fd) < 0) {
          close(fd);
          continue;
        }
        if (add_to_epoll(srv->pid, fd, EPOLLIN | EPOLLET) < 0) {
          close(fd);
        }
      } else {
        if (!(events[i].events & (EPOLLERR | EPOLLHUP))) {
          struct jrpc_connection conn = {
            .fd = events[i].data.fd
          };
          struct jrpc_param args = (struct jrpc_param) {
            .srv = srv,
            .conn = &conn
          };
          pthread_t th;
          if (pthread_create(&th, NULL, jrpc_execute, &args) != 0) {
            (void)remove_from_epoll(srv->pid, events[i].data.fd);
            close(events[i].data.fd);
          } else {
            pthread_detach(th);
          }
        } else {
          (void)remove_from_epoll(srv->pid, events[i].data.fd);
          close(events[i].data.fd);
        }
      }
    }
  }
}

void jrpc_server_stop(struct jrpc_server * srv) {
  srv->stop = 1;
}

const char * jrpc_server_error(int code) {
  if (code == JRPC_PARSE_ERROR) return "Parse error";
  if (code == JRPC_INVALID_REQUEST) return "Invalid Request";
  if (code == JRPC_METHOD_NOT_FOUND) return "Method not found";
  if (code == JRPC_INVALID_PARAMS) return "Invalid params";
  if (code == JRPC_INTERNAL_ERROR) return "Internal error";
  if (code >= -32099 && code <= -32000) return "Server error";
  return "Unknown error";
}
