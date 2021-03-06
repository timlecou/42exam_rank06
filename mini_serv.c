#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

typedef	struct			s_client
{
	int					fd;
	int					id;
	struct	s_client	*next;
}						t_client;

int			g_id;
int			sockfd;

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

int		get_max_fd(t_client *list)
{
	int		i;
	
	i = -1;
	while (list != NULL)
	{
		if (list->fd > i)
			i = list->fd;
		list = list->next;
	}
	if (i == -1)
		i = sockfd;
	return (i);
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

void	fatal_error(void)
{
	write(2, "Fatal error\n", 12);
	if (sockfd != -1)
		close(sockfd);
	exit(1);
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

void	send_all(t_client *list, char *str, int fd, fd_set *set)
{
	size_t	len = strlen(str);

	while (list != NULL)
	{
		if (FD_ISSET(list->fd, set) && list->fd != fd)
			send(list->fd, str, len, 0);
		list = list->next;
	}
}

int		extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int		i = 0;

	if (*buf == 0)
		return (0);
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n' || (*buf)[i + 1] == '\0')
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

int		main(int argc, char **argv)
{
	int		port, connfd, id, i;
	ssize_t	size = 0;
	struct	sockaddr_in	servaddr;
	t_client	*tmp;
	t_client	*clients = NULL;
	char		str[256];
	char		buff[4096];
	char		*message = NULL;
	char		*msg = NULL;
	char		*tmpchar = NULL;
	fd_set		set_read;
	fd_set		set_write;
	fd_set		fds;

	if (argc != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	port = atoi(argv[1]);

	if (port <= 0)
		fatal_error();

	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = htonl(2130706433);

	if ((bind(sockfd, (const struct sockaddr*)&servaddr, sizeof(servaddr))) != 0)
		fatal_error();
	if (listen(sockfd, 10) != 0)
		fatal_error();

	g_id = 0;
	FD_ZERO(&fds);
	FD_SET(sockfd, &fds);

	while (1)
	{
		set_read = fds;
		set_write = fds;
		select(get_max_fd(clients) + 1, &set_read, &set_write, NULL, NULL);
		if (FD_ISSET(sockfd, &set_read))	//add new connection
		{
			connfd = accept(sockfd, NULL, NULL);
			if (connfd >= 0)
			{
				id = add_client(&clients, connfd);
				fcntl(connfd, F_SETFL, O_NONBLOCK); //line to remove when get corrected
				FD_SET(connfd, &fds);
				sprintf(str, "server: client %d just arrived\n", id);
				send_all(clients, str, connfd, &set_write);
			}
		}

		tmp = clients;
		while (tmp != NULL)
		{
			id = tmp->id;
			connfd = tmp->fd;
			tmp = tmp->next;
			if (FD_ISSET(connfd, &set_read))
			{
				i = 0;
				while ((size = recv(connfd, buff, 4095, 0)) > 0)
				{
					buff[size] = '\0';
					i += size;
					message = str_join(message, buff);
				}
				if (i == 0)
				{
					id = remove_client(&clients, connfd);
					if (id != -1)
					{
						sprintf(str, "server: client %d just left\n", id);
						send_all(clients, str, connfd, &set_write);
					}
					FD_CLR(connfd, &fds);
				}
				else if (i > 0)
				{
					msg = NULL;
					while (extract_message(&message, &msg))
					{
						if (!(tmpchar = malloc(sizeof(char) * (14 + strlen(msg)))))
							fatal_error();
						sprintf(tmpchar, "client %d: %s", id, msg);
						send_all(clients, tmpchar, connfd, &set_write);
						free(msg);
						free(tmpchar);
						tmpchar = NULL;
						msg = NULL;
					}
				}
				free(message);
				message = NULL;
			}
		}
	}
	return (0);
}
