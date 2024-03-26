#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <fcntl.h>
#include <sys/epoll.h>
#include <parson.h>

#define BUFFER_SIZE (4096)

int main(int argc, char ** argv) {
  if (argc < 3) {
    printf("Usage: %s <hostname> <port>\n", argv[0]);
    return 1;
  }
  int fd, port, rc = 0;
  struct sockaddr_in addr;
  struct hostent * srv;
  char buff[BUFFER_SIZE] = {0};
  JSON_Value * req = NULL;
  port = atoi(argv[2]);
  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    printf("Failed to open socket\n");
    return -1;
  }
  srv = gethostbyname(argv[1]);
  if (srv == NULL) {
    printf("No such host\n");
    rc = -1;
    goto out;
  }
  memset(&addr, 0, sizeof(addr));
  memcpy(
    (void *)srv->h_addr, (void *)&addr.sin_addr.s_addr, srv->h_length
  );
  addr.sin_port = htons(port);
  if ((rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr))) < 0) {
    printf("Could not connect to server\n");
    goto out;    
  }
  req = json_value_init_object();
  if (!req) {
    printf("Could not make request\n");
    goto out;
  }
  json_object_set_string(json_object(req), "jsonrpc", "2.0");
  json_object_set_string(json_object(req), "method", "hello");
  json_object_set_value(
    json_object(req), "params", json_value_init_object()
  );
  json_object_dotset_string(json_object(req), "params.name", "Join");
  rc = (int)json_serialize_to_buffer(req, buff, BUFFER_SIZE);
  if (rc < 0) {
    printf("Failed to make payload\n");
    goto out;
  }
  printf("%d (%s)\n",rc,buff);
  buff[strlen(buff)] = '\0';
  printf("Sending\n\n%s\n", buff);
  rc = send(fd, &buff[0], strlen(buff), 0);
  printf("Sent %d bytes\n", rc);
  if (rc < 0) {
    printf("Faild to call procedure\n");
    goto out;
  }
  memset(&buff, 0, sizeof(buff));
  rc = recv(fd, &buff[0], BUFFER_SIZE, 0);
  if (rc <= 0) {
    printf("Failed to read result\n");
    goto out;
  }
  buff[rc] = '\0';
  printf("Result (%d bytes)\n\n%s\n", rc, buff);
out:
  close(fd);
  if (req) json_value_free(req);
  return rc;
}
