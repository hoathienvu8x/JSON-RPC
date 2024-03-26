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
  int fd, pid, port, rc = 0;
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
  bzero((char *)&addr, sizeof(addr));
  bcopy ( srv->h_addr, &addr.sin_addr, srv->h_length );
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if ((rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr))) < 0) {
    printf("Could not connect to server\n");
    goto out;    
  }
  rc = fcntl(fd, F_GETFL, 0);
  if (rc < 0) {
    printf("Get socket flags failed\n");
    goto out;
  }
  if ((rc = fcntl(fd, F_SETFL, rc | O_NONBLOCK)) < 0) {
    printf("Set nonblocking failed\n");
    goto out;
  }
  pid = epoll_create(1);
  if (pid < 0) {
    rc = -1;
    printf("Create epoll error\n");
    goto out;
  }
  struct epoll_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
  ev.data.fd = fd;
  if ((rc = epoll_ctl(pid, EPOLL_CTL_ADD, fd, &ev)) < 0) {
    printf("Add socket to epoll failed\n");
    goto out;
  }
  struct epoll_event evs[1];
  while (1) {
    int fds = epoll_wait(pid, evs, 1, -1);
    int i;
    for (i = 0; i < fds; i++) {
      if (evs[i].events & EPOLLOUT) {
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
        buff[rc] = '\0';
        rc = send(evs[i].data.fd, &buff[0], strlen(buff), 0);
        if (rc < 0) {
          printf("Faild to call procedure\n");
          goto out;
        }
      } else if (evs[i].events & EPOLLIN) {
        memset(&buff, 0, sizeof(buff));
        rc = recv(evs[i].data.fd, buff, BUFFER_SIZE, 0);
        epoll_ctl(pid, EPOLL_CTL_DEL, evs[i].data.fd, &ev);
        if (rc <= 0) {
          printf("Failed to read result\n");
          goto out;
        }
        buff[rc] = '\0';
        printf("Result\n\n%s\n", buff);
      }
    }
  }
out:
  epoll_ctl(pid, EPOLL_CTL_DEL, fd, NULL);
  close(fd);
  if (req) json_value_free(req);
  return rc;
}
