#include <string.h>
#include "../include/metrics.h"
#include <stdio.h>
#include <time.h>

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

void log_command(const char *username, const char *command, const char *response) {
    time_t now;
    struct tm *tm_info;
    char timestamp[26];
    
    time(&now);
    tm_info = localtime(&now);
    strftime(timestamp, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    
    FILE *log = fopen("pop3_access.log", "a");
    if (log != NULL) {
        fprintf(log, "[%s] User: %s, Command: %s, Response: %s\n", 
                timestamp, username ? username : "anonymous", 
                command ? command : "unknown", 
                response ? response : "no response");
        fclose(log);
    }
}