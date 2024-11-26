#ifndef METRICS_H
#define METRICS_H

#include <stddef.h>
#include <time.h>

#define LOG_FILE "/var/log/pop3_access.log"

struct metrics {
  size_t currentConnections;
  size_t totalConnections;
  size_t bytesSent;
  size_t bytesReceived;
  size_t totalCommands;
  size_t failedCommands;
  time_t serverStartTime;
  size_t activeUsers;
};

void metrics_init();
void metrics_new_connection();
void metrics_connection_closed();
void metrics_bytes_sent(size_t bytes);
void metrics_get_metrics(struct metrics * metrics);
void log_command(const char *username, const char *command, const char *response);
void metrics_set_active_users(int i);
void metrics_set_commands(int i);
void metric_set_failed_commands(int i);

#endif //METRICS_H
