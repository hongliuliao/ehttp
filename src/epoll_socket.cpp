/*
 * tcp_epoll.cpp
 *
 *  Created on: Nov 10, 2014
 *      Author: liao
 */
#include <cerrno>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>

#include "simple_log.h"
#include "epoll_socket.h"

int EpollSocket::setNonblocking(int fd) {
	int flags;

	if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
};

int EpollSocket::listen_on(int port, int backlog) {
	int sockfd; /* listen on sock_fd, new connection on new_fd */

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	struct sockaddr_in my_addr; /* my address information */
	memset (&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET; /* host byte order */
	my_addr.sin_port = htons(port); /* short, network byte order */
	my_addr.sin_addr.s_addr = INADDR_ANY; /* auto-fill with my IP */

	int opt = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	if (bind(sockfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) == -1) {
		perror("bind");
		exit(1);
	}

	if (listen(sockfd, backlog) == -1) {
		perror("listen");
		exit(1);
	}

	LOG_INFO("start Server Socket on port : %d", port);
	return sockfd;
}

int EpollSocket::accept_socket(int sockfd) {
	int new_fd;
	struct sockaddr_in their_addr; /* connector's address information */
	socklen_t sin_size = sizeof(struct sockaddr_in);

	if ((new_fd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size)) == -1) {
		perror("accept");
		return -1;
	}

	LOG_DEBUG("server: got connection from %s\n", inet_ntoa(their_addr.sin_addr));
	return new_fd;
}

int EpollSocket::start_epoll(int port, EpollSocketHandler &socket_handler, int backlog) {
	int sockfd = this->listen_on(port, backlog);

	struct epoll_event ev;
	int epollfd = epoll_create(10);
	if (epollfd == -1) {
		perror("epoll_create");
		exit(EXIT_FAILURE);
	}

	ev.events = EPOLLIN;
	ev.data.fd = sockfd;
	if(epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
		perror("epoll_ctl: listen_sock");
		exit(EXIT_FAILURE);
	}

	epoll_event events[10];

	while(1) {
		int fds_num = epoll_wait(epollfd, events, 10, -1);
		if(fds_num == -1) {
			perror("epoll_pwait");
			exit(EXIT_FAILURE);
		}

		for (int i = 0; i < fds_num; i++) {
			if(events[i].data.fd == sockfd) {
				int conn_sock = accept_socket(sockfd);
				setNonblocking(conn_sock);
				LOG_DEBUG("get accept socket which listen fd:%d, conn_sock_fd:%d", sockfd, conn_sock);

				EpollContext *epoll_context = new EpollContext();
				epoll_context->fd = conn_sock;

				socket_handler.on_accept(*epoll_context);

				epoll_event conn_sock_ev;
				conn_sock_ev.events = EPOLLIN | EPOLLET;
				conn_sock_ev.data.ptr = epoll_context;

				if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &conn_sock_ev) == -1) {
				   perror("epoll_ctl: conn_sock");
				   exit(EXIT_FAILURE);
				}

			} else if(events[i].events & EPOLLIN ){ // readable
				EpollContext *epoll_context = (EpollContext *) events[i].data.ptr;
				int fd = epoll_context->fd;

				int buffer_size = 1024;
				char read_buffer[buffer_size];
				memset(read_buffer, 0, buffer_size);
				int read_size = 0;

				while((read_size = recv(fd, read_buffer, buffer_size, 0)) > 0) {
					LOG_DEBUG("read success which read content:%s", read_buffer);

					int ret = socket_handler.on_readable(*epoll_context, read_buffer, buffer_size, read_size);
					if(ret != 0) {
						close_and_release(epollfd, events[i], socket_handler);
						continue;
					}
				}

				if(read_size == 0 || (read_size == -1 && errno != EAGAIN)) {
					LOG_DEBUG("read_size not normal which size:%d", read_size);
					close_and_release(epollfd, events[i], socket_handler);
					continue;
				}

				events[i].events = EPOLLOUT | EPOLLET;
				epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &events[i]);

			} else if(events[i].events & EPOLLOUT) { // writeable
				EpollContext *epoll_context = (EpollContext *) events[i].data.ptr;
				int fd = epoll_context->fd;
				LOG_DEBUG("start write data");

				int ret = socket_handler.on_writeable(*epoll_context);
				if(ret != 0) {
					close_and_release(epollfd, events[i], socket_handler);
					continue;
				}

				events[i].events = EPOLLIN | EPOLLET;
				epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &events[i]);
			} else {
				LOG_INFO("unkonw events :%d", events[i].events);
				continue;
			}
		}
	}
}

int EpollSocket::close_and_release(int &epollfd, epoll_event &epoll_event, EpollSocketHandler &socket_handler) {
	LOG_INFO("connect close");

	EpollContext *hc = (EpollContext *) epoll_event.data.ptr;

	socket_handler.on_close(*hc);

	int fd = hc->fd;
	epoll_event.events = EPOLLIN | EPOLLOUT | EPOLLET;
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &epoll_event);

	if(epoll_event.data.ptr != NULL) {
		delete (EpollContext *) epoll_event.data.ptr;
		epoll_event.data.ptr = NULL;
	}

	int ret = close(fd);
	LOG_DEBUG("connect close complete which fd:%d, ret:%d", fd, ret);
	return 0;
}
