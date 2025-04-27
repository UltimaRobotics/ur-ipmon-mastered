/**
 * @file config.h
 * @brief Configuration handler for IP Monitor application
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <time.h>

typedef struct {
    char *ip_address;    // IP address to monitor
    int interval;        // Monitoring interval in seconds
    bool is_active;      // Whether monitoring is active
    int timeout;         // Timeout for ping in milliseconds
} IPConfig;

typedef struct {
    IPConfig *ips;       // Array of IP configurations
    int ip_count;        // Number of IPs to monitor
    int default_interval; // Default monitoring interval
    int default_timeout; // Default timeout 
    char *filename;      // Filename of the config for reloading
    time_t last_modified; // Last modification time of the config file
} Config;

/**
 * @brief Load configuration from a JSON file
 * 
 * @param filename Path to the configuration file
 * @return Config* Pointer to the loaded configuration, NULL on error
 */
Config* load_config(const char *filename);

/**
 * @brief Free resources allocated for configuration
 * 
 * @param config Pointer to the configuration to free
 */
void free_config(Config *config);

/**
 * @brief Check if the configuration file has been modified
 * 
 * @param config Pointer to the current configuration
 * @return true if the file has been modified, false otherwise
 */
bool config_has_changed(Config *config);

/**
 * @brief Reload the configuration if the file has been modified
 * 
 * @param config Pointer to a pointer to the configuration (can be updated)
 * @return true if the configuration was reloaded, false otherwise
 */
bool reload_config_if_changed(Config **config);

#endif /* CONFIG_H */
