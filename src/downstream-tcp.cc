#include <sys/socket.h>
#include "downstream-tcp.h"
#include "socket-moderator.h"

static int downstream_tcp_set_fds(fd_set *read_fds, fd_set *write_fds, fd_set *except_fds, void *data)
{
	return static_cast<downstream_tcp *>(data)->set_fds(read_fds, write_fds, except_fds);
}

static int downstream_tcp_process(fd_set *read_fds, fd_set *write_fds, fd_set *except_fds, void *data)
{
	return static_cast<downstream_tcp *>(data)->process(read_fds, write_fds, except_fds);
}

const static socket_info_s info = {
	.set_fds = downstream_tcp_set_fds,
	.process = downstream_tcp_process,
};

downstream_tcp::downstream_tcp(tcp2udp_ctx_s &ctx, std::shared_ptr<class upstream> u_, int sock)
	: downstream(u_)
{
	fd = sock;
	ss = ctx.ss;
	socket_moderator_add(ctx.ss, &info, this);
}

downstream_tcp::~downstream_tcp()
{
	socket_moderator_remove(ss, this);
	if (fd >= 0)
		close(fd);
}

void downstream_tcp::packet_from_camera(const uint8_t *data, size_t size)
{
	send(fd, data, size, 0);
}

int downstream_tcp::set_fds(fd_set *read_fds, fd_set *, fd_set *)
{
	if (fd < 0)
		return 0;
	FD_SET(fd, read_fds);
	return fd + 1;
}

int downstream_tcp::process(fd_set *read_fds, fd_set *, fd_set *)
{
	if (fd < 0 || !FD_ISSET(fd, read_fds))
		return 0;

	constexpr uint32_t n_buf = 1024;
	uint32_t n_begin = buf_to_camera.size();
	buf_to_camera.resize(n_begin + n_buf);

	ssize_t ret = recv(fd, buf_to_camera.data() + n_begin, n_buf, MSG_DONTWAIT);
	if (ret <= 0) {
		buf_to_camera.resize(n_begin);
		LOG(info, "Closed fd=%d", fd);
		close(fd);
		fd = -1;
		return 0;
	}
	buf_to_camera.resize(n_begin + ret);

	LOG(debug, "tcp(fd=%d) recv %s", fd, u8_to_str(buf_to_camera.data() + n_begin, ret).c_str());

	uint32_t processed = 0;
	for (uint32_t i = 0; i < buf_to_camera.size(); i++) {
		if (buf_to_camera[i] == 0xFF) {
			send_packet(buf_to_camera.data() + processed, i - processed + 1);
			processed = i + 1;
		}
	}
	if (processed)
		buf_to_camera.erase(buf_to_camera.begin(), buf_to_camera.begin() + processed);

	return 0;
}

bool downstream_tcp::is_closed() const
{
	return fd < 0;
}
