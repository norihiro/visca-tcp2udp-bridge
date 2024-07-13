#include <vector>
#include "tcp2udp.h"

void logv(enum boost::log::trivial::severity_level level, const char *fmt, va_list args)
{
	va_list args_cp;
	va_copy(args_cp, args);
	int len = vsnprintf(NULL, 0, fmt, args_cp);
	va_end(args_cp);
	if (len < 0)
		len = 4095;

	std::vector<char> buf(len + 1);
	len = vsnprintf(buf.data(), ((size_t)len) + 1, fmt, args);

	BOOST_LOG_STREAM_WITH_PARAMS(boost::log::trivial::logger::get(),
			        (boost::log::keywords::severity = level)) << buf.data();
}

std::string u8_to_str(const uint8_t *data, size_t size)
{
	std::string ret;
	char sz[8];
	while (size--) {
		snprintf(sz, sizeof(sz), " %02X", 0xFF & *data++);
		if (ret.size())
			ret += sz;
		else
			ret += sz + 1;
	}
	return ret;
}
