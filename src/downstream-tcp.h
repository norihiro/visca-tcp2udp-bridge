#pragma once

#include <vector>
#include "tcp2udp.h"
#include "downstream.h"

class downstream_tcp : public downstream
{
	int fd;
	socket_moderator_t *ss;
	std::vector<uint8_t> buf_to_camera;

public:
	downstream_tcp(tcp2udp_ctx_s &ctx, std::shared_ptr<class upstream> u, int fd);
	~downstream_tcp() override;
	void packet_from_camera(const uint8_t *data, size_t size) override;

	// callback functions from socket_moderator_t
	int set_fds(fd_set *read_fds, fd_set *write_fds, fd_set *except_fds);
	int process(fd_set *read_fds, fd_set *write_fds, fd_set *except_fds);

	bool is_closed() const;
};
