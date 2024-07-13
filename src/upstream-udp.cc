#include <list>
#include <limits>
#include <arpa/inet.h>
#include "tcp2udp.h"
#include "platform.h"
#include "upstream-udp.h"
#include "downstream.h"

namespace {
std::shared_ptr<struct udp_socket> get_socket(tcp2udp_ctx_s &ctx, int port)
{
	if (ctx.udp_socket_pool.count(port)) {
		if (std::shared_ptr<struct udp_socket> cand = ctx.udp_socket_pool[port].lock())
			return cand;
	}

	std::shared_ptr<struct udp_socket> ret(new udp_socket(ctx, port));
	ctx.udp_socket_pool[port] = ret;
	return ret;
}
}

upstream_udp::upstream_udp(tcp2udp_ctx_s &ctx, const prop_t &prop)
{
	host = inet_addr(prop.get<std::string>("host").c_str());
	auto port = prop.get<int>("port", 52381);
	socket = get_socket(ctx, port);

	socket->uu.push_back(this);

	/* Send a control command RESET */
	struct send_queue_s s;
	seq_next = 1;
	s.data = std::vector<uint8_t>({0x02, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01});
	send_packet(s);
	send_queue.push_back(s);

	const uint8_t cmd_version_inq[] = {0x81, 0x09, 0x00, 0x02, 0xFF};
	send_packet(nullptr, cmd_version_inq, sizeof(cmd_version_inq));
}

upstream_udp::~upstream_udp()
{
	if (socket)
		std::erase(socket->uu, this);
}

inline static bool is_payload_device_setting(const uint8_t *buf)
{
	// IF_Clear
	if (buf[1] == 0x01 && buf[2] == 0x00 && buf[3] == 0x01 && buf[4] == 0xFF)
		return true;

	// CAM_VersionInq
	if (buf[1] == 0x09 && buf[2] == 0x00 && buf[3] == 0x02 && buf[4] == 0xFF)
		return true;

	return false;
}

void upstream_udp::send_queue_s::create_from_data(uint32_t seq, const uint8_t *d, size_t size)
{
	data.resize(size + 8);

	if (size > 4 && is_payload_device_setting(d)) {
		/* Device setting command */
		data[0] = 0x01;
		data[1] = 0x20;
	} else if ((d[0] & 0xF0) == 0x80 && d[1] == 0x01) {
		/* Execution command */
		data[0] = 0x01;
		data[1] = 0x00;
	} else if ((d[0] & 0xF0) == 0x80 && d[1] == 0x09) {
		/* Inquiry command */
		data[0] = 0x01;
		data[1] = 0x10;
	} else {
		LOG(warning, "Unidentified type for 0x%02X 0x%02X\n", d[0], d[1]);
	}

	data[2] = (size >> 8) & 0xFF;
	data[3] = (size >> 0) & 0xFF;

	set_seq(seq);

	memcpy(data.data() + 8, d, size);
}

void upstream_udp::send_packet(std::shared_ptr<class downstream> c, const uint8_t *data, size_t size)
{
	struct send_queue_s s;
	s.create_from_data(seq_next++, data, size);
	s.c = c;

	if (can_send_packet())
		send_packet(s);

	send_queue.push_back(s);
}

void upstream_udp::send_packet(struct send_queue_s &s)
{
	LOG(debug, "udp(fd=%d) send type: 0x%02X%02X seq: %u payload: %s (retry %d)", socket->fd, s.data[0], s.data[1], s.seq(), u8_to_str(s.data.data() + 8, s.data.size() - 8).c_str(), s.n_sent);

	socket->sendto(*this, s.data.data(), s.data.size());

	/*
	 * TODO: Instead of the fixed re-send timeout, define the timeout for each command type.
	 * Current timeout is defined by the slowest command `set_power`.
	 */
	s.n_sent++;
	auto t = os_gettime_us();
	s.to_ack = t + 150'000 * 2; // saw 140ms delay until Ack for pantilt command.
	s.to_comp = t + 5'000'000;
	if (!s.to_ack)
		s.to_ack++;
	if (!s.to_comp)
		s.to_comp++;
}

uint32_t upstream_udp::timeout_us()
{
	uint32_t t = os_gettime_us();
	auto ret = std::numeric_limits<uint32_t>::max();

	for (auto &q : send_queue) {
		if (q.n_sent == 0)
			continue;

		if (!q.ack_received) {
			if (q.to_ack && (int32_t)(t - q.to_ack) >= 0) {
				LOG(debug, "Ack timeout send_queue=%zu socket_available=%d", send_queue.size(), socket_available);
				send_packet(q);
			}
			ret = std::min(ret, q.to_ack - t);
		}
		else if (!q.comp_received) {
			if (q.to_comp && (int32_t)(t - q.to_comp) >= 0) {
				LOG(debug, "Comp timeout send_queue=%zu socket_available=%d", send_queue.size(), socket_available);
				/* TODO: If it's a execution command, it's
				 * better to reply a dummy completion packet
				 * rather than sending the command again to the
				 * camera. */
				/* TODO: If the completion packet was lost,
				 * there is no way to know whether the command
				 * was ended. */
				send_packet(q);
			}
			ret = std::min(ret, q.to_comp - t);
		}
	}

	return ret;
}

bool upstream_udp::can_send_packet() const
{
	if (!socket_available)
		return false;

	/* If waiting ack, do not send more packet. */
	for (auto &q : send_queue) {
		if (q.n_sent && !q.ack_received)
			return false;
	}
	return true;
}

int upstream_udp::process(uint8_t *data, size_t size)
{
	uint16_t type = (data[0] << 8) | data[1];
	uint16_t length = (data[2] << 8) | data[3];
	uint32_t seq = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
	uint8_t *payload = data + 8;

	LOG(debug, "udp(fd=%d) recv type: 0x%04X seq: %u payload: %s", socket->fd, type, seq, u8_to_str(data + 8, length).c_str());

	struct send_queue_s *p = nullptr;
	for (auto &q : send_queue) {
		if (q.seq() == seq) {
			p = &q;
			break;
		}
	}

	if (!p)
		return 0;

	struct send_queue_s &f = *p;

	if (size < length + 8) {
		fprintf(stderr, "Error: Too short packet: got %u expect %u\n", size, length + 8);
		return 0;
	}
	if (seq != f.seq()) {
		fprintf(stderr, "Error: sequence number mismatch: got %u expect %u\n", seq, f.seq());
		return 0;
	}

	bool completed = false;

	if (length >= 1 && type == 0x0201 && payload[0] == 0x01) { /* control reply for RESET */
		completed = true;
		socket_available = (1 << n_socket) - 1;
	}
	else if (length >= 2 && type == 0x0200 && payload[0] == 0x0F && payload[1] == 0x01) {
		/* Sequence number error
		 * Let's assume the command was accepted by the camera.
		 */
		f.ack_received = true;
		f.to_ack = 0;
	}
	else if (type == 0x0111 && length == 3 && (payload[1] & 0xF0) == 0x40) { /* acknowledge */
		f.ack_received = true;
		f.socket = payload[1] & 0x0F;
		if (f.socket)
			socket_available &= ~(1 << (f.socket - 1));
		f.to_ack = 0;
	}
	else if (type == 0x0111 && length >= 3 && (payload[1] & 0xF0) == 0x50) { /* completion */
		f.comp_received = true;
		f.to_comp = 0;
		completed = true;
		int socket = payload[1] & 0x0F;
		if (socket)
			socket_available |= 1 << (socket - 1);

		if (f.data.size() == 8 + 5 && f.data[8] == 0x81 && f.data[9] == 0x09 && f.data[10] == 0x00 && f.data[11] == 0x02) {
			/* CAM_VersionInq */
			int ns = payload[8];
			if (ns > n_socket) {
				socket_available |= (1 << ns) - (1 << n_socket);
				n_socket = ns;
				LOG(debug, "got n_socket=%d socket_available=0x%X", ns, socket_available);
			} else if (ns < n_socket) {
				socket_available &= (1 << ns) - 1;
			}
		}
	}
	else if (length >= 3 && (payload[1] & 0xF0) == 0x60) { /* error */
		f.to_ack = 0;
		f.to_comp = 0;
		completed = true;
	}

	if (type == 0x0111 && f.c) {
		/* TODO: If ack was not received but this is completion (90 51 FF), return ack (90 41 FF). */
		f.c->packet_from_camera(payload, length);
	}

	if (completed) {
		for (auto it = send_queue.begin(); it != send_queue.end(); it++) {
			if (it->seq() != seq)
				continue;
			send_queue.erase(it);
			break;
		}
	}

	if (can_send_packet()) {
		for (auto &q : send_queue) {
			if (q.n_sent)
				continue;

			send_packet(q);
			break;
		}
	}

	return 0;
}
