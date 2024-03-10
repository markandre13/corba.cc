#pragma once

#include <cstdint>

void ignore_sig_pipe();
int create_listen_socket(const char *hostname, uint16_t port);
int connect_to(const char *host, uint16_t port);
int set_non_block(int fd);
int set_no_delay(int fd);