#include <string.h>
#include "../include/metrics.h"

static struct metrics metrics;

void metrics_init() {
  memset(&metrics, 0, sizeof(metrics));
}

void metrics_new_connection() {
  metrics.currentConnections++;
  metrics.totalConnections++;
}

void metrics_connection_closed() {
  metrics.currentConnections--;
}

void metrics_bytes_sent(size_t bytes) {
  metrics.bytesSent += bytes;
}

void metrics_get_metrics(struct metrics * metricsCopy) {
  memcpy(metricsCopy, &metrics, sizeof(struct metrics));
}