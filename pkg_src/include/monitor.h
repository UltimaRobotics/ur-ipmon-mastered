/**
 * @file monitor.h
 * @brief IP Monitoring functionality
 */

#ifndef MONITOR_H
#define MONITOR_H

#include "config.h"
#include <stdbool.h>
#include <time.h>

typedef enum {
    STATUS_UNKNOWN,
    STATUS_UP,
    STATUS_DOWN
} IPStatus;

typedef struct {
    char *ip_address;       // IP address being monitored
    IPStatus status;        // Current status
    time_t last_checked;    // Last time this IP was checked
    int response_time_ms;   // Last response time in milliseconds
    int failures;           // Number of consecutive failures
    bool is_active;         // Whether monitoring is active
    int interval;           // Monitoring interval in seconds
    int timeout;            // Timeout in milliseconds
} MonitoredIP;

typedef struct {
    MonitoredIP *ips;       // Array of monitored IPs
    int ip_count;           // Number of IPs being monitored
    bool running;           // Whether monitoring is running
} Monitor;

/**
 * @brief Initialize the monitor with configuration
 * 
 * @param config Configuration to use
 * @return Monitor* Initialized monitor, NULL on error
 */
Monitor* init_monitor(Config *config);

/**
 * @brief Free resources allocated for the monitor
 * 
 * @param monitor Monitor to free
 */
void free_monitor(Monitor *monitor);

/**
 * @brief Start the monitoring process
 * 
 * @param monitor Monitor to start
 * @return int 0 on success, -1 on error
 */
int start_monitoring(Monitor *monitor);

/**
 * @brief Stop the monitoring process
 * 
 * @param monitor Monitor to stop
 */
void stop_monitoring(Monitor *monitor);

/**
 * @brief Check a specific IP address
 * 
 * @param ip_address IP address to check
 * @param timeout Timeout in milliseconds
 * @return int Response time in ms if reachable, -1 if unreachable
 */
int check_ip(const char *ip_address, int timeout);

/**
 * @brief Get a display-friendly string for the IP status
 * 
 * @param status Status to convert
 * @return const char* String representation of status
 */
const char* get_status_string(IPStatus status);

/**
 * @brief Display the current status of all monitored IPs
 * 
 * @param monitor Monitor containing the IPs
 */
void display_status(Monitor *monitor);

#endif /* MONITOR_H */
