#pragma once

#include <stdarg.h>
#include <map>
#include <memory>
#include <string>
#include <boost/property_tree/ptree.hpp>
#include <boost/log/trivial.hpp>
#include "socket-moderator.h"

typedef struct socket_moderator_s socket_moderator_t;

typedef boost::property_tree::ptree prop_t;

struct tcp2udp_ctx_s
{
	socket_moderator_t *ss;
	std::map<int, std::weak_ptr<struct udp_socket>> udp_socket_pool;

	~tcp2udp_ctx_s();
};

void logv(enum boost::log::trivial::severity_level level, const char *fmt, va_list args);
inline void log(enum boost::log::trivial::severity_level level, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	logv(level, fmt, args);
	va_end(args);
}
#define LOG(level, fmt, ...) log(boost::log::trivial::level, fmt, ##__VA_ARGS__)

std::string u8_to_str(const uint8_t *data, size_t size);
inline std::string u8_to_str(const std::vector<uint8_t> data)
{
	return u8_to_str(data.data(), data.size());
}
