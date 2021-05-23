#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef	struct			s_client
{
	int					fd;
	int					id;
	struct	s_client	*next;
}						t_client;

int		g_id;
int		sockfd;
int		max_fd;

void	clear_client(t_client *list)
{
	t_client	*tmp;

	if (list != NULL)
	{
		tmp = list->next;
		close(list->fd);
		free(list);
		list = tmp;
	}
}

int		add_client(t_client **list, int fd)
{
	t_client	*new;
	t_client	*tmp;

	if ((new = malloc(sizeof(t_client))) == NULL || list == NULL)
	{
		if (list != NULL)
			clear_client(*list);
		write(2, "Fatal error\n", 12);
		close(sockfd);
		exit(1);
	}
	new->fd = fd;
	new->id = g_id++;
	new->next = NULL;
	if (*list == NULL)
		*list = new;
	else
	{
		tmp = *list;
		while (tmp->next != NULL)
			tmp = tmp->next;
		tmp->next = new;
	}
	return (new->id);
}

int		remove_client(t_client **list, int fd)
{
	t_client	*prev = NULL;
	t_client	*tmp;
	int			id = -1;

	if (list == NULL)
	{
		write(2, "Fatal error\n", 12);
		close(sockfd);
		exit(1);
	}
	tmp = *list;
	if (tmp != NULL && tmp->fd == fd)
	{
		*list = tmp->next;
		id = tmp->id;
		close(tmp->fd);
		free(tmp);
	}
	else
	{
		while (tmp != NULL && tmp->fd != fd)
		{
			prev = tmp;
			tmp = tmp->next;
		}
		if (tmp != NULL)
		{
			prev->next = tmp->next;
			id = tmp->id;
			close(tmp->fd);
			free(tmp);
		}
	}
	return (id);
}

void	send_all(t_client *list, char *str, int fd)
{
	size_t	len = strlen(str);

	while (list != NULL)
	{
		if (list->fd != fd)
			send(list->fd, str, len, 0);
		list = list->next;
	}
	write(1, str, len);
}

int		extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int		i = 0;

	if (*buf == 0)
		return (0);
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			if (!(newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1))))
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

void	init_fdset(fd_set *set, t_client *list)
{
	FD_ZERO(set);
	max_fd = sockfd;
	while (list != NULL)
	{
		FD_SET(list->fd, set);
		if (max_fd < list->fd)
			max_fd = list->fd;
		list = list->next;
	}
	FD_SET(sockfd, set);
}

int		main(int argc, char **argv)
{
	int		port, ret, connfd, id;
	ssize_t	size = 0;
	//struct	timeval	timeout;
	struct	sockaddr_in	servaddr;//, cli;
	t_client	*clients = NULL;
	t_client	*tmp;
	char		str[500000];
	char		*buff = NULL;
	char		*msg = NULL;
	fd_set		set_read;
	//socklen_t	len;

	if (argc != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		write(2, "Fatal error\n", 12);
		close(sockfd);
		exit(1);
	}
	port = atoi(argv[1]);
	bzero(&servaddr, sizeof(servaddr));

	if (port <= 0)
	{
		write(2, "Fatal error\n", 12);
		close(sockfd);
		exit(1);
	}

	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = htonl(2130706433);

	if ((bind(sockfd, (const struct sockaddr*)&servaddr, sizeof(servaddr))) != 0)
	{
		write(2, "Fatal error\n", 12);
		close(sockfd);
		exit(1);
	}
	if (listen(sockfd, 10) != 0)
	{
		write(2, "Fatal error\n", 12);
		close(sockfd);
		exit(1);
	}

	g_id = 0;
	//len = sizeof(cli);
	//timeout.tv_sec = 5;
	//timeout.tv_usec = 0;

	while (1)
	{
		init_fdset(&set_read, clients);
		ret = select(max_fd + 1, &set_read, NULL, NULL, NULL);
		if (ret > 0)
		{
			if (FD_ISSET(sockfd, &set_read))
			{
				connfd = accept(sockfd, NULL, NULL);
				if (connfd >= 0)
				{
					id = add_client(&clients, connfd);
					if (max_fd < connfd)
						max_fd = connfd;
					sprintf(str, "server: client %d just arrived\n", id);
					send_all(clients, str, connfd);
				}
			}
			else
			{
				tmp = clients;
				while (tmp != NULL)
				{
					id = tmp->id;
					connfd = tmp->fd;
					tmp = tmp->next;
					if (FD_ISSET(connfd, &set_read))
					{
						if (!(buff = (char*)malloc(500000)))
						{
							write(2, "Fatal error\n", 12);
							close(sockfd);
							exit(1);
						}
						size = recv(connfd, buff, 500000, 0);
						buff[size] = '\0';
						if (size == 0)
						{
							id = remove_client(&clients, connfd);
							if (id != -1)
							{
								sprintf(str, "server: client %d just left\n", id);
								send_all(clients, str, connfd);
							}
						}
						else if (size != 0)
						{
							msg = NULL;
							while (extract_message(&buff, &msg))
							{
								sprintf(str, "client %d: %s", id, msg);
								send_all(clients, str, connfd);
								free(msg);
							}
						}
						free(buff);
						buff = NULL;
					}
				}
			}
		}
	}
	return (0);
}
