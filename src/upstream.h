#pragma once

#include <cstdint>
#include <memory>

class upstream
{
public:
	virtual void send_packet(std::shared_ptr<class downstream> c, const uint8_t *data, size_t size) = 0;
};
