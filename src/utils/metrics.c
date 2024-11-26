#include <string.h>
#include "../include/metrics.h"
#include <stdio.h>
#include <time.h>
#include <errno.h>

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
    
    FILE *log = fopen(LOG_FILE, "a+");
    if (log == NULL) {
        fprintf(stderr, "ERROR: No se pudo abrir pop3_access.log: %s\n", strerror(errno));
        return;
    }
    
    fprintf(log, "[%s] User: %-15s | Command: %-10s | Response: %s\n", 
            timestamp, 
            username ? username : "anonymous", 
            command ? command : "unknown", 
            response ? response : "no response");
    fclose(log);
}

void metrics_set_active_users(int i){
  metrics.activeUsers += i;
}

void metrics_set_commands(int i){
  metrics.totalCommands += i;
}

void metric_set_failed_commands(int i){
  metrics.failedCommands += i;
}