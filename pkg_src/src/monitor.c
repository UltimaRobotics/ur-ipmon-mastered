/**
 * @file monitor.c
 * @brief Implementation of IP monitoring functionality
 */

#include "../include/monitor.h"
#include "../include/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <sys/time.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>

#define PACKET_SIZE 64
#define MAX_WAIT_TIME 5 /* seconds */
#define FAILED_THRESHOLD 3

typedef struct {
    MonitoredIP *ip;
    Monitor *monitor;
} ThreadArgs;

// Utility function to calculate checksum for ICMP packet
static unsigned short calculate_checksum(unsigned short *addr, int len) {
    int nleft = len;
    int sum = 0;
    unsigned short *w = addr;
    unsigned short answer = 0;

    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1) {
        *(unsigned char *)(&answer) = *(unsigned char *)w;
        sum += answer;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    answer = ~sum;
    return answer;
}

int check_ip(const char *ip_address, int timeout) {
    FILE *fp;
    char command[256];
    char output[1024];
    struct timeval start, end;
    long rtt_ms = -1;
    
    // Create ping command with timeout
    snprintf(command, sizeof(command), "ping -c 1 -W %d %s 2>&1", 
             (timeout + 999) / 1000,  // Convert milliseconds to seconds, rounding up
             ip_address);
    
    // Record start time
    gettimeofday(&start, NULL);
    
    // Execute ping command
    fp = popen(command, "r");
    if (fp == NULL) {
        log_message(LOG_ERROR, "Failed to execute ping command: %s", strerror(errno));
        return -1;
    }
    
    // Read output
    size_t output_len = 0;
    while (fgets(output + output_len, sizeof(output) - output_len, fp) != NULL) {
        output_len = strlen(output);
        if (output_len >= sizeof(output) - 1)
            break;
    }
    
    // Record end time
    gettimeofday(&end, NULL);
    
    // Close command pipe
    int status = pclose(fp);
    
    // Check if ping was successful
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        // Parse time from output if possible
        char *time_pos = strstr(output, "time=");
        if (time_pos) {
            float ping_time;
            if (sscanf(time_pos, "time=%f", &ping_time) == 1) {
                rtt_ms = (long)(ping_time);
            } else {
                // If we can't parse the time, calculate it from our timestamps
                rtt_ms = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
            }
        } else {
            // Calculate time from our timestamps
            rtt_ms = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
        }
        
        log_message(LOG_DEBUG, "Ping to %s successful, time: %ld ms", ip_address, rtt_ms);
        return (int)rtt_ms;
    } else {
        log_message(LOG_DEBUG, "Ping to %s failed", ip_address);
        return -1;
    }
}

static void *monitor_ip_thread(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    MonitoredIP *ip = args->ip;
    Monitor *monitor = args->monitor;
    free(args);

    while (monitor->running && ip->is_active) {
        // Check the IP address
        int response_time = check_ip(ip->ip_address, ip->timeout);
        
        // Update status
        ip->last_checked = time(NULL);
        ip->response_time_ms = response_time;
        
        if (response_time >= 0) {
            if (ip->status != STATUS_UP) {
                log_message(LOG_INFO, "IP %s is UP (response time: %d ms)", 
                            ip->ip_address, response_time);
            }
            ip->status = STATUS_UP;
            ip->failures = 0;
        } else {
            ip->failures++;
            if (ip->failures >= FAILED_THRESHOLD) {
                if (ip->status != STATUS_DOWN) {
                    log_message(LOG_WARNING, "IP %s is DOWN (failed %d times)", 
                                ip->ip_address, ip->failures);
                }
                ip->status = STATUS_DOWN;
            }
        }
        
        // Sleep for the configured interval
        sleep(ip->interval);
    }
    
    return NULL;
}

Monitor* init_monitor(Config *config) {
    if (!config || !config->ips || config->ip_count <= 0) {
        log_message(LOG_ERROR, "Invalid configuration for monitor initialization");
        return NULL;
    }
    
    Monitor *monitor = (Monitor *)malloc(sizeof(Monitor));
    if (!monitor) {
        log_message(LOG_ERROR, "Memory allocation failed for monitor");
        return NULL;
    }
    
    monitor->ip_count = config->ip_count;
    monitor->ips = (MonitoredIP *)malloc(config->ip_count * sizeof(MonitoredIP));
    if (!monitor->ips) {
        log_message(LOG_ERROR, "Memory allocation failed for monitored IPs");
        free(monitor);
        return NULL;
    }
    
    // Initialize each monitored IP
    for (int i = 0; i < config->ip_count; i++) {
        monitor->ips[i].ip_address = strdup(config->ips[i].ip_address);
        monitor->ips[i].status = STATUS_UNKNOWN;
        monitor->ips[i].last_checked = 0;
        monitor->ips[i].response_time_ms = -1;
        monitor->ips[i].failures = 0;
        monitor->ips[i].is_active = config->ips[i].is_active;
        monitor->ips[i].interval = config->ips[i].interval;
        monitor->ips[i].timeout = config->ips[i].timeout;
    }
    
    monitor->running = false;
    
    return monitor;
}

void free_monitor(Monitor *monitor) {
    if (!monitor) {
        return;
    }
    
    if (monitor->ips) {
        for (int i = 0; i < monitor->ip_count; i++) {
            free(monitor->ips[i].ip_address);
        }
        free(monitor->ips);
    }
    
    free(monitor);
}

int start_monitoring(Monitor *monitor) {
    if (!monitor || !monitor->ips) {
        log_message(LOG_ERROR, "Invalid monitor for starting");
        return -1;
    }
    
    monitor->running = true;
    pthread_t *threads = (pthread_t *)malloc(monitor->ip_count * sizeof(pthread_t));
    if (!threads) {
        log_message(LOG_ERROR, "Memory allocation failed for threads");
        return -1;
    }
    
    // Create a thread for each IP
    for (int i = 0; i < monitor->ip_count; i++) {
        if (!monitor->ips[i].is_active) {
            log_message(LOG_INFO, "Skipping inactive IP: %s", monitor->ips[i].ip_address);
            continue;
        }
        
        ThreadArgs *args = (ThreadArgs *)malloc(sizeof(ThreadArgs));
        if (!args) {
            log_message(LOG_ERROR, "Memory allocation failed for thread arguments");
            free(threads);
            return -1;
        }
        
        args->ip = &monitor->ips[i];
        args->monitor = monitor;
        
        int result = pthread_create(&threads[i], NULL, monitor_ip_thread, args);
        if (result != 0) {
            log_message(LOG_ERROR, "Failed to create thread for IP %s: %s", 
                      monitor->ips[i].ip_address, strerror(result));
            free(args);
            free(threads);
            return -1;
        }
        
        // Detach the thread so we don't need to join it
        pthread_detach(threads[i]);
    }
    
    free(threads);
    return 0;
}

void stop_monitoring(Monitor *monitor) {
    if (!monitor) {
        return;
    }
    
    // Set the running flag to false to stop all threads
    monitor->running = false;
    
    // Give threads time to clean up
    sleep(1);
}

const char* get_status_string(IPStatus status) {
    switch (status) {
        case STATUS_UNKNOWN:
            return "UNKNOWN";
        case STATUS_UP:
            return "UP";
        case STATUS_DOWN:
            return "DOWN";
        default:
            return "INVALID";
    }
}

void display_status(Monitor *monitor) {
    if (!monitor || !monitor->ips) {
        printf("No IPs being monitored\n");
        return;
    }
    
    printf("\n=== IP Monitoring Status ===\n");
    printf("%-20s %-10s %-15s %-20s\n", "IP Address", "Status", "Response Time", "Last Checked");
    printf("----------------------------------------------------------------\n");
    
    for (int i = 0; i < monitor->ip_count; i++) {
        MonitoredIP *ip = &monitor->ips[i];
        
        char time_str[30] = "Never";
        if (ip->last_checked > 0) {
            struct tm *time_info = localtime(&ip->last_checked);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);
        }
        
        char response_str[20];
        if (ip->status == STATUS_UP) {
            snprintf(response_str, sizeof(response_str), "%d ms", ip->response_time_ms);
        } else {
            strcpy(response_str, "N/A");
        }
        
        printf("%-20s %-10s %-15s %-20s%s\n", 
               ip->ip_address, 
               get_status_string(ip->status), 
               response_str,
               time_str,
               ip->is_active ? "" : " (inactive)");
    }
    
    printf("----------------------------------------------------------------\n");
}
