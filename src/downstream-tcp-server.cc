#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "downstream-tcp-server.h"
#include "downstream-tcp.h"
#include "socket-moderator.h"

static int tcp_server_set_fds(fd_set *read_fds, fd_set *write_fds, fd_set *except_fds, void *data)
{
	return static_cast<downstream_tcp_server *>(data)->set_fds(read_fds, write_fds, except_fds);
}

static int tcp_server_process(fd_set *read_fds, fd_set *write_fds, fd_set *except_fds, void *data)
{
	return static_cast<downstream_tcp_server *>(data)->process(read_fds, write_fds, except_fds);
}

const static socket_info_s info = {
	.set_fds = tcp_server_set_fds,
	.process = tcp_server_process,
};

static int listen_port(int port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		exit(1);
	}

	int opt = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));

	struct sockaddr_in me;
	memset(&me, 0, sizeof(me));
	me.sin_port = htons(port);
	int ret = bind(fd, (const sockaddr *)&me, sizeof(me));
	if (ret) {
		perror("bind");
		exit(1);
	}

	listen(fd, 16);

	return fd;
}

downstream_tcp_server::downstream_tcp_server(tcp2udp_ctx_s &ctx_, const prop_t &prop, std::shared_ptr<class upstream> u_)
	: ctx(ctx_) , u(u_)
{
	int port = prop.get<int>("port", 5678);

	fd = listen_port(port);
	LOG(debug, "Listening port %d", port);

	socket_moderator_add(ctx.ss, &info, this);
}

downstream_tcp_server::~downstream_tcp_server()
{
	socket_moderator_remove(ctx.ss, this);
	close(fd);
}

// callback functions from socket_moderator_t
int downstream_tcp_server::set_fds(fd_set *read_fds, fd_set *, fd_set *)
{
	for (auto it = clients.begin(); it != clients.end(); ) {
		if ((*it)->is_closed())
			it = clients.erase(it);
		else
			it++;
	}

	FD_SET(fd, read_fds);
	return fd + 1;
}

int downstream_tcp_server::process(fd_set *read_fds, fd_set *, fd_set *)
{
	if (!FD_ISSET(fd, read_fds))
		return 0;

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	socklen_t len_addr = sizeof(addr);

	int sock = accept(fd, (sockaddr *)&addr, &len_addr);
	if (sock < 0) {
		perror("accept");
		return 0;
	}

	char addr_name[64];
	LOG(info, "Connected from %s:%d fd=%d", inet_ntop(AF_INET, &addr.sin_addr, addr_name, sizeof(addr_name)), ntohs(addr.sin_port), sock);

	std::shared_ptr<downstream_tcp> client(new downstream_tcp(ctx, u, sock));

	clients.push_back(client);

	return 0;
}
