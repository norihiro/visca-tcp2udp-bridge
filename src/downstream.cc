#include "downstream.h"
#include "upstream.h"

downstream::downstream(std::shared_ptr<class upstream> u_)
{
	u = u_;
}

void downstream::send_packet(const uint8_t *data, size_t size)
{
	if (!u)
		return;

	u->send_packet(shared_from_this(), data, size);
}
