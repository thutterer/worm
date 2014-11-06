#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

void error(const char *msg)
{
  perror(msg);
  exit(1);
}

class network {
  public:
    network(void);
    ~network(void);
    void send_input(int, int);
    void send_playground(int*, int max_x, int max_y);
    void send_int(int score);
    void receive_playground(int*, int max_x, int max_y);
    void receive_input(int &mov_x, int &mov_y);
    void receive_int(int &score);

    bool is_connected;
    struct addrinfo hints;
    struct addrinfo *result;
    int s, fd;
    char port[5];
};

class server : public network {
  public:
    server(char* p);
};

class client : public network {
  public:
    client(char* p, char* ip);
    char server_ip_hostname[20];
};

network::network(void) {
  memset (&hints, 0, sizeof (struct addrinfo));
  hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
  hints.ai_protocol = 0;          /* Any protocol */
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;
}

network::~network(void) {
  close (fd);
}

server::server(char* p) {
  strncpy (this->port, p, 5);
  s = getaddrinfo (NULL, this->port, &hints, &result);
  //s = getaddrinfo (NULL, "4567", &hints, &result);
  if (s != 0)	{
    perror ("getaddrinfo");
    exit (1);
  }
  struct addrinfo *r;
  for (r = result; r != NULL; r = r->ai_next) {
    fd = socket (r->ai_family, r->ai_socktype, r->ai_protocol);
    if (fd == -1) continue;
    if (bind (fd, r->ai_addr, r->ai_addrlen) == 0) break;
    close (fd);
  }
  if (!r) {
    perror ("socket/bind");
    exit (1);
  }
  freeaddrinfo (result);
  if (listen (fd, 1) == -1) {
    perror ("listen");
    exit (1);
  }
  if ((fd = accept (fd, NULL, NULL)) == -1) {
    perror ("error");
    exit (1);
  }
  is_connected = true;
}

client::client(char* p, char* ip) {
  strncpy (this->port, p, 5);
  strncpy (this->server_ip_hostname, ip, 20);
  s = getaddrinfo (this->server_ip_hostname, this->port, &hints, &result);
  //s = getaddrinfo ("localhost", "4567", &hints, &result);
  if (s != 0) exit (1);
  struct addrinfo *r;
  for (r = result; r != NULL; r = r->ai_next) {
    fd = socket (r->ai_family, r->ai_socktype, r->ai_protocol);
    if (fd == -1) continue;
    if (connect (fd, r->ai_addr, r->ai_addrlen) == 0) break;
    close (fd);
  }
  if (!r) exit (1);
  freeaddrinfo (result);
  is_connected = true;
}

void network::send_input(int input_x, int input_y) {
  write (fd, &input_x, 4);
  write (fd, &input_y, 4);
}

void network::receive_input(int &input_x, int &input_y) {
  read (fd, &input_x, 4);
  read (fd, &input_y, 4);
}

void network::send_int(int score) {
  write (fd, &score, 4);
}

void network::receive_int(int &score) {
  read (fd, &score, 4);
}

void network::send_playground(int* pground, int max_x, int max_y) {
  write (fd, pground, max_x*max_y*4);
}

void network::receive_playground(int* pground, int max_x, int max_y) {
  read (fd, pground, max_x*max_y*4);
}
