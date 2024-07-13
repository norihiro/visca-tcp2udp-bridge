#pragma once

#include <list>
#include "tcp2udp.h"

class downstream_tcp_server
{
	int fd;
	tcp2udp_ctx_s &ctx;
	std::shared_ptr<class upstream> u;
	std::list<std::shared_ptr<class downstream_tcp>> clients;

public:
	downstream_tcp_server(tcp2udp_ctx_s &, const prop_t &, std::shared_ptr<class upstream>);
	~downstream_tcp_server();

	// callback functions from socket_moderator_t
	int set_fds(fd_set *read_fds, fd_set *write_fds, fd_set *except_fds);
	int process(fd_set *read_fds, fd_set *write_fds, fd_set *except_fds);
};
