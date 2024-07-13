#pragma once

#include <cstdint>
#include <memory>

class downstream : public std::enable_shared_from_this<downstream>
{
	std::shared_ptr<class upstream> u;

public:
	downstream(std::shared_ptr<class upstream> u_);
	virtual ~downstream() {};
	virtual void packet_from_camera(const uint8_t *data, size_t size) = 0;

	void send_packet(const uint8_t *data, size_t size);
};
