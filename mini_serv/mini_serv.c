#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

size_t			id;
struct sockaddr_in	servaddr, clientaddr;
socklen_t		len = sizeof(clientaddr);
fd_set			curr_sock, cpy_read, cpy_write;
int			sockfd, client_fd, max_fd;
char			info[42], buf[1025];
char			*msg[65000];
int			keep_fd[65000];

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

void	fatal_error(void)
{
	write(2, "Fatal error\n", strlen("Fatal error\n"));
	close(sockfd);
	exit(1);
}

void	send_message(int send_fd, char *msg)
{
	for (int fd = 0; fd <= max_fd; fd++)
	{
		if (fd != send_fd && FD_ISSET(fd, &cpy_write))
			send(fd, msg, strlen(msg), 0);
	}
}

int	main(int argc, char **argv)
{
	if (argc != 2)
	{
		write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
		exit(1);
	}
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
		fatal_error();
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(argv[1]));
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		fatal_error();
	if (listen(sockfd, 1024) != 0)
		fatal_error();
	bzero(&info, sizeof(info));
	bzero(&msg, sizeof(msg));
	FD_ZERO(&curr_sock);
	FD_SET(sockfd, &curr_sock);
	max_fd = sockfd;
	while (1)
	{
		cpy_read = cpy_write = curr_sock;
		if (select(max_fd + 1, &cpy_read, &cpy_write, NULL, NULL) < 0)
			fatal_error();
		for (int fd = 0; fd <= max_fd; fd++)
		{
			if (!FD_ISSET(fd, &cpy_read))
				continue ;
			if (fd == sockfd)
			{
				client_fd = accept(sockfd, (struct sockaddr *)&clientaddr, &len);
				if (client_fd < 0)
					fatal_error();
				if (client_fd > max_fd)
					max_fd = client_fd;
				keep_fd[client_fd] = id;
				id++;
				bzero(&info, sizeof(info));
				sprintf(info, "server: client %d just arrived\n", keep_fd[client_fd]);
				send_message(keep_fd[client_fd], info);
				FD_SET(client_fd, &curr_sock);
				break ;
			}
			else
			{
				int let = recv(fd, buf, 1024, 0);
				if (let <= 0)
				{
					bzero(&info, sizeof(info));
					sprintf(info, "server: client %d just left\n", keep_fd[fd]);
					send_message(fd, info);
					close(fd);
					msg[fd] = NULL;
					FD_CLR(fd, &curr_sock);
					break ;
				}
				buf[let] = '\0';
				msg[fd] = str_join(msg[fd], buf);
				for (char *tmp = NULL; extract_message(&msg[fd], &tmp); )
				{
					bzero(&info, sizeof(info));
					sprintf(info, "client %d: ", keep_fd[fd]);
					send_message(fd, info);
					send_message(fd, tmp);
					tmp = NULL;
				}
			}
		}
	}
	return (0);
}
