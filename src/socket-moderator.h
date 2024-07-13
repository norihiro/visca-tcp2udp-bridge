#pragma once

#include <sys/select.h>
#include "tcp2udp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*socket_info_set_fds_cb)(fd_set *read_fds, fd_set *write_fds, fd_set *except_fds, void *data);
typedef int (*socket_info_process_cb)(fd_set *read_fds, fd_set *write_fds, fd_set *except_fds, void *data);

struct socket_info_s
{
	socket_info_set_fds_cb set_fds;
	uint32_t (*timeout_us)(void *data);
	socket_info_process_cb process;
};

struct socket_moderator_s *socket_moderator_create();
void socket_moderator_destroy(struct socket_moderator_s *);
int socket_moderator_mainloop(struct socket_moderator_s *);

void socket_moderator_add(struct socket_moderator_s *s, const struct socket_info_s *info, void *data);
void socket_moderator_remove(struct socket_moderator_s *s, void *data);

#ifdef __cplusplus
}
#endif
