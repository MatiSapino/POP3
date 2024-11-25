#ifndef METRICS_H
#define METRICS_H

#include <stddef.h>

struct metrics {
  size_t currentConnections;
  size_t totalConnections;
  size_t bytesSent;
};

void metrics_init();
void metrics_new_connection();
void metrics_connection_closed();
void metrics_bytes_sent(size_t bytes);
void metrics_get_metrics(struct metrics * metrics);

#endif //METRICS_H
