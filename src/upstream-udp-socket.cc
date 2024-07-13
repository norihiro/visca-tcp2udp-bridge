#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "upstream-udp.h"
#include "socket-moderator.h"

static int udp_socket_set_fds(fd_set *read_fds, fd_set *write_fds, fd_set *except_fds, void *data)
{
	return static_cast<udp_socket *>(data)->set_fds(read_fds, write_fds, except_fds);
}

static uint32_t udp_socket_timeout_us(void *data)
{
	return static_cast<udp_socket *>(data)->timeout_us();
}

static int udp_socket_process(fd_set *read_fds, fd_set *write_fds, fd_set *except_fds, void *data)
{
	return static_cast<udp_socket *>(data)->process(read_fds, write_fds, except_fds);
}

const static socket_info_s info = {
	.set_fds = udp_socket_set_fds,
	.timeout_us = udp_socket_timeout_us,
	.process = udp_socket_process,
};

udp_socket::udp_socket(tcp2udp_ctx_s &ctx, int port_)
{
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	port = port_;

	struct sockaddr_in me;
	memset(&me, 0, sizeof(me));
	me.sin_port = htons(port_);
	int ret = bind(fd, (const sockaddr *)&me, sizeof(me));
	if (ret) {
		perror("bind udp");
		exit(1);
	}

	ss = ctx.ss;
	socket_moderator_add(ctx.ss, &info, this);
}

udp_socket::~udp_socket()
{
	socket_moderator_remove(ss, this);
	close(fd);
}

int udp_socket::sendto(upstream_udp &u, uint8_t *data, size_t size)
{
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = u.host;

	ssize_t ret = ::sendto(fd, data, size, 0, (sockaddr *)&addr, sizeof(addr));
	return (int)ret;
}

int udp_socket::set_fds(fd_set *read_fds, fd_set *, fd_set *)
{
	FD_SET(fd, read_fds);
	return fd + 1;
}

uint32_t udp_socket::timeout_us()
{
	auto ret = std::numeric_limits<uint32_t>::max();

	for (auto u : uu) {
		uint32_t t = u->timeout_us();
		if (t < ret)
			ret = t;
	}

	return ret;
}

int udp_socket::process(fd_set *read_fds, fd_set *, fd_set *)
{
	if (!FD_ISSET(fd, read_fds))
		return 0;

	uint8_t buf[1024];
	struct sockaddr_in src_addr;
	socklen_t addrlen = sizeof(src_addr);
	memset(&src_addr, 0, sizeof(src_addr));
	ssize_t ret = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&src_addr, &addrlen);
	if (ret <= 0)
		return 0;

	for (auto u : uu) {
		if (src_addr.sin_addr.s_addr != u->host)
			continue;

		return u->process(buf, (size_t)ret);
	}

	return 0;
}
