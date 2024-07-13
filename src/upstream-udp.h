#pragma once

#include <cstdint>
#include <vector>
#include <deque>
#include <list>
#include <netinet/in.h>
#include "tcp2udp.h"
#include "upstream.h"

class upstream_udp : public upstream
{
	struct send_queue_s {
		std::shared_ptr<class downstream> c;
		std::vector<uint8_t> data;
		uint32_t to_ack = 0;
		uint32_t to_comp = 0;
		uint8_t n_sent = 0;
		bool ack_received = false;
		bool comp_received = false;
		uint8_t socket = 0;

		uint32_t seq() const
		{
			if (data.size() < 8)
				return 0;
			return (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
		}
		void set_seq(uint32_t seq)
		{
			if (data.size() < 8)
				data.resize(8);
			data[4] = (seq >> 24) & 0xFF;
			data[5] = (seq >> 16) & 0xFF;
			data[6] = (seq >> 8) & 0xFF;
			data[7] = (seq >> 0) & 0xFF;
		}

		void create_from_data(uint32_t seq, const uint8_t *data, size_t size);
	};

	in_addr_t host;
	std::shared_ptr<struct udp_socket> socket;
	std::deque<send_queue_s> send_queue;
	uint32_t seq_next = 0;
	uint16_t socket_available = 0;
	uint16_t n_socket = 1;

	bool can_send_packet() const;
	void send_packet(struct send_queue_s &);

public:
	upstream_udp(tcp2udp_ctx_s &ctx, const prop_t &);
	~upstream_udp();
	void send_packet(std::shared_ptr<class downstream> c, const uint8_t *data, size_t size) override;

	// callback functions from socket_moderator_t
	uint32_t timeout_us();
	int process(uint8_t *data, size_t size);

	friend struct udp_socket;
};

struct udp_socket
{
	int fd;
	int port;
	socket_moderator_t *ss;

	std::list<upstream_udp *> uu;

	udp_socket(tcp2udp_ctx_s &ctx, int port);
	~udp_socket();

	int sendto(upstream_udp &u, uint8_t *data, size_t size);

	// callback functions from socket_moderator_t
	int set_fds(fd_set *read_fds, fd_set *write_fds, fd_set *except_fds);
	uint32_t timeout_us();
	int process(fd_set *read_fds, fd_set *write_fds, fd_set *except_fds);
};
