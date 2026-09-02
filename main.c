#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

typedef struct s_clients
{
	int id;
	char *buf;
} Clients;

Clients clients[424242];
char recv_buffer[424242];
fd_set readfd, writefd, setfd;

int maxfds = 0;
int client_id = 0;
int sockfd;



int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void fatal(char *msg)
{
	write(2, msg, strlen(msg));
	write(2, "\n", strlen("\n"));
	exit(1);
}

void update_maxfds(int newclient)
{
	if (newclient > maxfds)
		maxfds = newclient;
}

void send_message(int s_fd, char *msg)
{
	for (int fd = 0; fd <= maxfds; fd++)
	{
		if ((FD_ISSET(fd, &setfd)) && (fd != sockfd) && (fd != s_fd))
			send(fd, msg, strlen(msg), 0);
	}
}

void register_client()
{
	int client = accept(sockfd, NULL, NULL);
	
	if (client < 0)
		fatal("Fatal error");
	FD_SET(client, &setfd);
	clients[client].id = client_id++;
	update_maxfds(client);

	char msg[100];
	bzero(&msg, sizeof(msg));
	sprintf(msg, "server: client %d just arrived\n" , clients[client].id);
	send_message(client, msg);
}

int main(int ac, char** av) {
	struct sockaddr_in servaddr; 

	if (ac != 2)
		fatal("Wrong number of arguments");
	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) 
		fatal("Fatal error");
	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1])); 

	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		fatal("Fatal error");
	
	if (listen(sockfd, 1024) != 0) 
		fatal("Fatal error");

	bzero(&clients, sizeof(clients));
	FD_ZERO(&setfd);
	FD_SET(sockfd, &setfd);
	update_maxfds(sockfd);
	while (1)
	{
		readfd = writefd = setfd;
		
		if (select(maxfds +1 , &readfd, &writefd, NULL , NULL) <= 0)
			continue;
		
		for (int fd = 0; fd <= maxfds; fd++)
		{
			if (FD_ISSET(fd, &readfd))
			{
				if (fd == sockfd)
				{
					register_client();
				}
				else
				{
					bzero(&recv_buffer, sizeof(recv_buffer));
					int size = recv(fd, recv_buffer, 424241, 0);
					if (size <= 0)
					{
						char msg[100];

						bzero(&msg, sizeof(msg));
						if (clients[fd].buf != NULL)
						{
							free(clients[fd].buf);
							clients[fd].buf = NULL;
						}
						close(fd);
						FD_CLR(fd, &setfd);
						sprintf(msg, "server: client %d just left\n", clients[fd].id);
						send_message(fd, msg);
					}
					else
					{
						char *msg = NULL;
						clients[fd].buf = str_join(clients[fd].buf, recv_buffer);
						while(extract_message(&clients[fd].buf, &msg))
						{
							char tmp[ strlen(msg)+ 50];
							sprintf(tmp, "client %d: %s", clients[fd].id, msg);
							send_message(fd,tmp);
							free(msg);
						}
					}
				}
			}
		}

	}
}