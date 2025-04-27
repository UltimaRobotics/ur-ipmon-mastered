/**
 * @file main.c
 * @brief Main entry point for IP Monitor application
 */

#include "../include/config.h"
#include "../include/logger.h"
#include "../include/monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>

// Global variables for signal handling and cleanup
static Monitor *g_monitor = NULL;
static Config *g_config = NULL;

// Function prototypes
static void signal_handler(int signo);
static void print_usage(const char *program_name);
static void cleanup(void);

int main(int argc, char *argv[]) {
    // Default values
    char *config_file = "config.json";
    char *log_file = NULL;
    int display_interval = 5;  // Display status every 5 seconds
    LogLevel log_level = LOG_INFO;
    
    // Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "c:l:d:v:h")) != -1) {
        switch (opt) {
            case 'c':
                config_file = optarg;
                break;
            case 'l':
                log_file = optarg;
                break;
            case 'd':
                display_interval = atoi(optarg);
                if (display_interval <= 0) {
                    fprintf(stderr, "Display interval must be positive\n");
                    return EXIT_FAILURE;
                }
                break;
            case 'v':
                if (strcmp(optarg, "debug") == 0) {
                    log_level = LOG_DEBUG;
                } else if (strcmp(optarg, "info") == 0) {
                    log_level = LOG_INFO;
                } else if (strcmp(optarg, "warning") == 0) {
                    log_level = LOG_WARNING;
                } else if (strcmp(optarg, "error") == 0) {
                    log_level = LOG_ERROR;
                } else {
                    fprintf(stderr, "Invalid log level: %s\n", optarg);
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }
                break;
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    // Initialize logger
    if (init_logger(log_file) != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return EXIT_FAILURE;
    }
    
    // Set log level
    set_log_level(log_level);
    
    // Setup signal handlers
    if (signal(SIGINT, signal_handler) == SIG_ERR || 
        signal(SIGTERM, signal_handler) == SIG_ERR) {
        log_message(LOG_ERROR, "Failed to set up signal handlers");
        cleanup();
        return EXIT_FAILURE;
    }
    
    // Register cleanup function to be called at exit
    atexit(cleanup);
    
    // Load configuration
    log_message(LOG_INFO, "Starting IP Monitor. Loading configuration from %s", config_file);
    Config *config = load_config(config_file);
    if (!config) {
        log_message(LOG_ERROR, "Failed to load configuration. Exiting.");
        return EXIT_FAILURE;
    }
    
    // Initialize monitor
    g_monitor = init_monitor(config);
    if (!g_monitor) {
        log_message(LOG_ERROR, "Failed to initialize monitor. Exiting.");
        free_config(config);
        return EXIT_FAILURE;
    }
    
    // Start monitoring
    log_message(LOG_INFO, "Starting monitoring of %d IP addresses", g_monitor->ip_count);
    if (start_monitoring(g_monitor) != 0) {
        log_message(LOG_ERROR, "Failed to start monitoring. Exiting.");
        free_monitor(g_monitor);
        free_config(config);
        return EXIT_FAILURE;
    }
    
    // Keep the config for dynamic reloading
    g_config = config;
    
    // Variables for configuration checking
    time_t last_config_check = time(NULL);
    const int config_check_interval = 5; // Check for config changes every 5 seconds
    
    // Main loop - display status every display_interval seconds
    log_message(LOG_INFO, "Monitoring started. Displaying status every %d seconds", display_interval);
    log_message(LOG_INFO, "Dynamic configuration enabled. Checking for changes every %d seconds", config_check_interval);
    
    while (g_monitor->running) {
        display_status(g_monitor);
        
        // Check if configuration has changed
        time_t current_time = time(NULL);
        if (current_time - last_config_check >= config_check_interval) {
            if (reload_config_if_changed(&g_config)) {
                log_message(LOG_INFO, "Configuration has changed, updating monitor");
                
                // Stop the current monitoring
                stop_monitoring(g_monitor);
                free_monitor(g_monitor);
                
                // Initialize and start new monitoring with updated config
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
    
    return EXIT_SUCCESS;
}

static void signal_handler(int signo) {
    log_message(LOG_INFO, "Received signal %d. Stopping monitoring...", signo);
    if (g_monitor) {
        stop_monitoring(g_monitor);
    }
    
    // Exit will call cleanup() due to atexit registration
    exit(EXIT_SUCCESS);
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

static void print_usage(const char *program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -c <file>    Configuration file (default: config.json)\n");
    printf("  -l <file>    Log file (default: stdout)\n");
    printf("  -d <seconds> Display update interval (default: 5)\n");
    printf("  -v <level>   Log level: debug, info, warning, error (default: info)\n");
    printf("  -h           Display this help message\n");
}
