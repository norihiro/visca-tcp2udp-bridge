#include <cstdbool>
#include <cstdint>
#include <list>
#include <signal.h>
#include "tcp2udp.h"
#include "downstream-tcp-server.h"
#include "upstream-udp.h"
#include "socket-moderator.h"

extern "C" {
bool got_interrupted = false;
}

static void sigint_handler(int)
{
	got_interrupted = true;
}

struct tcp2udp_pair
{
	prop_t prop_tcp;
	prop_t prop_udp;

	std::shared_ptr<downstream_tcp_server> tcp_inst;
	std::shared_ptr<upstream_udp> udp_inst;
};

int main(int argc, char **argv)
{
	got_interrupted = false;
	signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigint_handler);

	tcp2udp_ctx_s ctx;
	ctx.ss = socket_moderator_create();

	std::list<tcp2udp_pair> instances;

	for (int i = 1; i < argc; i++) {
		char *ai = argv[i];
		char *saveptr = NULL;
		int tcp_port = atoi(strtok_r(ai, ":", &saveptr));
		char *udp_host = strtok_r(NULL, ":", &saveptr);
		int udp_port = atoi(strtok_r(NULL, ":", &saveptr));

		tcp2udp_pair inst;
		inst.prop_tcp.put("port", tcp_port);
		inst.prop_udp.put("host", udp_host);
		inst.prop_udp.put("port", udp_port);

		inst.udp_inst = std::shared_ptr<upstream_udp>(new upstream_udp(ctx, inst.prop_udp));
		inst.tcp_inst = std::shared_ptr<downstream_tcp_server>(new downstream_tcp_server(ctx, inst.prop_tcp, inst.udp_inst));

		instances.push_back(inst);
	}

	int ret = socket_moderator_mainloop(ctx.ss);

	instances.clear();

	return ret;
}

tcp2udp_ctx_s::~tcp2udp_ctx_s()
{
	if (ss)
		socket_moderator_destroy(ss);
}
