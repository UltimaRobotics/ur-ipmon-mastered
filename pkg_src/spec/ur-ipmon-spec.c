#include "../include/config.h"
#include "../include/logger.h"
#include "../include/monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h>
#include <errno.h>

#include "ur-rpc-template.h"

#include "../include/thread_manager.h"
#include "../include/utils.h"
#include <time.h>  

#if !defined(_POSIX_C_SOURCE) || (_POSIX_C_SOURCE < 199309L)
extern int nanosleep(const struct timespec *req, struct timespec *rem);
#endif

thread_manager_t manager;
volatile sig_atomic_t running = 1;

static Monitor *g_monitor = NULL;
static Config *g_config = NULL;

static void signal_handler(int signo);
static void print_usage(const char *program_name);
static void cleanup(void);
char *create_config_file(const char *config);

void* function_ipmon_single(void *args) {
    char *config_value = (char*)args;
    unsigned int thread_id = 0;

    for (unsigned int i = 0; i < thread_get_count(&manager); i++) {
        unsigned int *ids = (unsigned int *)malloc(thread_get_count(&manager) * sizeof(unsigned int));
        int count = thread_get_all_ids(&manager, ids, thread_get_count(&manager));
        for (int j = 0; j < count; j++) {
            thread_info_t info;
            if (thread_get_info(&manager, ids[j], &info) == 0) {
                if (info.arg == args) {
                    thread_id = ids[j];
                    break;
                }
            }
        }
        free(ids);
        if (thread_id != 0) {
            break;
        }
    }
    char *config_file = create_config_file(config_value);
    char *log_file = NULL;
    int display_interval = 1;  
    LogLevel log_level = LOG_ERROR;
    
    log_message(LOG_INFO, "Starting IP Monitor. Loading configuration from %s", config_file);
    Config *config = load_config(config_file);
    if (!config) {
        log_message(LOG_ERROR, "Failed to load configuration. Exiting.");
        exit(EXIT_FAILURE);
    }
    
    g_monitor = init_monitor(config);
    if (!g_monitor) {
        log_message(LOG_ERROR, "Failed to initialize monitor. Exiting.");
        free_config(config);
        exit(EXIT_FAILURE);
    }
    
    log_message(LOG_INFO, "Starting monitoring of %d IP addresses", g_monitor->ip_count);
    if (start_monitoring(g_monitor) != 0) {
        log_message(LOG_ERROR, "Failed to start monitoring. Exiting.");
        free_monitor(g_monitor);
        free_config(config);
        exit(EXIT_FAILURE);
    }
    
    g_config = config;
    time_t last_config_check = time(NULL);
    const int config_check_interval = 5; 
    
    log_message(LOG_INFO, "Monitoring started. Displaying status every %d seconds", display_interval);
    log_message(LOG_INFO, "Dynamic configuration enabled. Checking for changes every %d seconds", config_check_interval);
    
    while (g_monitor->running) {
        display_status(g_monitor);
        thread_check_pause(&manager, thread_id);
        time_t current_time = time(NULL);
        if (current_time - last_config_check >= config_check_interval) {
            if (reload_config_if_changed(&g_config)) {
                log_message(LOG_INFO, "Configuration has changed, updating monitor");
                stop_monitoring(g_monitor);
                free_monitor(g_monitor);
                g_monitor = init_monitor(g_config);
                if (!g_monitor) {
                    log_message(LOG_ERROR, "Failed to reinitialize monitor after config change");
                    exit(EXIT_FAILURE);
                }
                if (start_monitoring(g_monitor) != 0) {
                    log_message(LOG_ERROR, "Failed to restart monitoring after config change");
                    free_monitor(g_monitor);
                    exit(EXIT_FAILURE);
                }
                log_message(LOG_INFO, "Monitoring restarted with new configuration");
            }
            last_config_check = current_time;
        }
        sleep(display_interval);
    }
    exit(EXIT_FAILURE);
}

static void cleanup(void) {
    if (g_monitor) {
        free_monitor(g_monitor);
        g_monitor = NULL;
    }
    if (g_config) {
        free_config(g_config);
        g_config = NULL;
    }
    close_logger();
}

char *create_config_file(const char *config) {
    if (config == NULL) {
        return NULL;
    }
    char path[255];
    snprintf(path, sizeof(path), "/tmp/ipmon-config-XXXXXX");
    int fd = mkstemp(path);
    if (fd == -1) { return NULL;  }
    size_t len = strlen(config);
    const char *p = config;
    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n < 0) {
            if (errno == EINTR) { continue; }
            close(fd);unlink(path);
            return NULL;
        } else if (n == 0) {
            close(fd);unlink(path);
            return NULL;
        }
        len -= n;
        p += n;
    }
    if (close(fd) == -1) {
        unlink(path);return NULL;
    }
    char *result = strdup(path);
    if (!result) {
        unlink(path);return NULL;
    }
    return result;
}


void *function_heartbeat(void *args){
    MqttThreadContext* context = (MqttThreadContext*)args;
    char *heartbeat_topic = context->config_base.heartbeat_topic;
    char *heartbeat_message = "ipmon_heartbeat";
    unsigned int thread_id = 0;

    for (unsigned int i = 0; i < thread_get_count(&manager); i++) {
        unsigned int *ids = (unsigned int *)malloc(thread_get_count(&manager) * sizeof(unsigned int));
        int count = thread_get_all_ids(&manager, ids, thread_get_count(&manager));
        for (int j = 0; j < count; j++) {
            thread_info_t info;
            if (thread_get_info(&manager, ids[j], &info) == 0) {
                if (info.arg == args) {
                    thread_id = ids[j];
                    break;
                }
            }
        }
        free(ids);
        if (thread_id != 0) {
            break;
        }
    }
    while (context->mqtt_monitor.running) {
        thread_check_pause(&manager, thread_id);
        int rc = mosquitto_publish(context->mosq, NULL, heartbeat_topic, strlen(heartbeat_message), heartbeat_message, 0, false);
        if (rc != MOSQ_ERR_SUCCESS) {
            fprintf(stderr, "Failed to publish heartbeat message: %s\n", mosquitto_strerror(rc));
        }
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 1000000000; // 1 second
        nanosleep(&ts, NULL);
    }

}