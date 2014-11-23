/*
 * tcp_epoll.h
 *
 *  Created on: Nov 10, 2014
 *      Author: liao
 */

#ifndef TCP_EPOLL_H_
#define TCP_EPOLL_H_

#include "sys/epoll.h"

class EpollContext {
public:
	void *ptr;
	int fd;
};

class EpollSocketWatcher {
public:
	virtual int on_accept(EpollContext &epoll_context) = 0;

	virtual int on_readable(EpollContext &epoll_context, char *read_buffer, int buffer_size, int read_size) = 0;

	/**
	 * return : if return value !=0 , we will close the connection
	 */
	virtual int on_writeable(EpollContext &epoll_context) = 0;

	virtual int on_close(EpollContext &epoll_context) = 0;
};

class EpollSocket {
private:

	int setNonblocking(int fd);

	int accept_socket(int sockfd);

	int listen_on(int port, int backlog);

	int close_and_release(int &epollfd, epoll_event &epoll_event, EpollSocketWatcher &socket_watcher);

public:

	int start_epoll(int port, EpollSocketWatcher &socket_watcher, int backlog = 10);

};

#endif /* TCP_EPOLL_H_ */
