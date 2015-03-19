/*
 * tcp_epoll.h
 *
 *  Created on: Nov 10, 2014
 *      Author: liao
 */

#ifndef TCP_EPOLL_H_
#define TCP_EPOLL_H_

#include "sys/epoll.h"
#define SS_WRITE_BUFFER_SIZE 4096
#define SS_READ_BUFFER_SIZE 4096

#define WRITE_CONN_ALIVE 0
#define WRITE_CONN_CLOSE 1
#define WRITE_CONN_CONTINUE 2

#define READ_OVER 0
#define READ_CONTINUE 1

class EpollContext {
public:
	void *ptr;
	int fd;
	std::string client_ip;
};

class EpollSocketWatcher {
public:
	virtual int on_accept(EpollContext &epoll_context) = 0;

	virtual int on_readable(EpollContext &epoll_context, char *read_buffer, int buffer_size, int read_size) = 0;

	/**
	 * return :
	 * if return value == 1, we will close the connection
	 * if return value == 2, we will continue to write
	 */
	virtual int on_writeable(EpollContext &epoll_context) = 0;

	virtual int on_close(EpollContext &epoll_context) = 0;
};

class EpollSocket {
private:

	int setNonblocking(int fd);

	int accept_socket(int sockfd, std::string &client_ip);

	int listen_on(int port, int backlog);

	int close_and_release(int &epollfd, epoll_event &event, EpollSocketWatcher &socket_watcher);

	int handle_accept_event(int &epollfd, epoll_event &event, EpollSocketWatcher &socket_watcher);

	int handle_readable_event(int &epollfd, epoll_event &event, EpollSocketWatcher &socket_watcher);

	int handle_writeable_event(int &epollfd, epoll_event &event, EpollSocketWatcher &socket_watcher);

public:

	int start_epoll(int port, EpollSocketWatcher &socket_watcher, int backlog, int max_events);

};

#endif /* TCP_EPOLL_H_ */
