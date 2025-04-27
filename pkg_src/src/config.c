/**
 * @file config.c
 * @brief Implementation of configuration loading and handling
 */

#include "../include/config.h"
#include "../include/logger.h"
#include "../include/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define DEFAULT_INTERVAL 5  // Default interval: 5 seconds
#define DEFAULT_TIMEOUT 1000 // Default timeout: 1000 milliseconds (1 second)
#define CONFIG_CHECK_INTERVAL 5 // Check for config changes every 5 seconds

Config* load_config(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        log_message(LOG_ERROR, "Failed to open configuration file: %s", filename);
        return NULL;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Read the file content
    char *json_data = (char*)malloc(file_size + 1);
    if (!json_data) {
        log_message(LOG_ERROR, "Memory allocation failed for config file content");
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(json_data, 1, file_size, file);
    fclose(file);

    if (read_size != file_size) {
        log_message(LOG_ERROR, "Failed to read the entire config file");
        free(json_data);
        return NULL;
    }

    json_data[file_size] = '\0';

    // Parse JSON
    cJSON *root = cJSON_Parse(json_data);
    free(json_data);

    if (!root) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr) {
            log_message(LOG_ERROR, "JSON parsing error near: %s", error_ptr);
        } else {
            log_message(LOG_ERROR, "JSON parsing error");
        }
        return NULL;
    }

    // Allocate config structure
    Config *config = (Config*)malloc(sizeof(Config));
    if (!config) {
        log_message(LOG_ERROR, "Memory allocation failed for config");
        cJSON_Delete(root);
        return NULL;
    }

    // Set defaults
    config->default_interval = DEFAULT_INTERVAL;
    config->default_timeout = DEFAULT_TIMEOUT;
    config->ips = NULL;
    config->ip_count = 0;
    config->filename = strdup(filename);
    
    // Get the file's last modification time
    struct stat file_stat;
    if (stat(filename, &file_stat) == 0) {
        config->last_modified = file_stat.st_mtime;
    } else {
        log_message(LOG_WARNING, "Could not get file modification time for %s", filename);
        config->last_modified = time(NULL);
    }

    // Get global settings if present
    cJSON *settings = cJSON_GetObjectItem(root, "settings");
    if (settings) {
        cJSON *interval = cJSON_GetObjectItem(settings, "default_interval");
        if (interval && cJSON_IsNumber(interval)) {
            config->default_interval = interval->valueint;
        }
        
        cJSON *timeout = cJSON_GetObjectItem(settings, "default_timeout");
        if (timeout && cJSON_IsNumber(timeout)) {
            config->default_timeout = timeout->valueint;
        }
    }

    // Get IP addresses
    cJSON *ips_array = cJSON_GetObjectItem(root, "ip_addresses");
    if (!ips_array || !cJSON_IsArray(ips_array)) {
        log_message(LOG_ERROR, "Configuration must contain 'ip_addresses' array");
        free(config->filename);
        free(config);
        cJSON_Delete(root);
        return NULL;
    }

    config->ip_count = cJSON_GetArraySize(ips_array);
    if (config->ip_count == 0) {
        log_message(LOG_WARNING, "No IP addresses found in configuration");
        config->ips = NULL;
    } else {
        config->ips = (IPConfig*)malloc(config->ip_count * sizeof(IPConfig));
        if (!config->ips) {
            log_message(LOG_ERROR, "Memory allocation failed for IP configurations");
            free(config->filename);
            free(config);
            cJSON_Delete(root);
            return NULL;
        }

        for (int i = 0; i < config->ip_count; i++) {
            cJSON *ip_item = cJSON_GetArrayItem(ips_array, i);
            
            if (cJSON_IsString(ip_item)) {
                // Simple format: just the IP address string
                config->ips[i].ip_address = strdup(ip_item->valuestring);
                config->ips[i].interval = config->default_interval;
                config->ips[i].timeout = config->default_timeout;
                config->ips[i].is_active = true;
            } else if (cJSON_IsObject(ip_item)) {
                // Complex format: object with IP and settings
                cJSON *ip = cJSON_GetObjectItem(ip_item, "ip");
                if (!ip || !cJSON_IsString(ip)) {
                    log_message(LOG_ERROR, "IP item must contain 'ip' field");
                    // Clean up previously allocated items
                    for (int j = 0; j < i; j++) {
                        free(config->ips[j].ip_address);
                    }
                    free(config->ips);
                    free(config->filename);
                    free(config);
                    cJSON_Delete(root);
                    return NULL;
                }
                
                config->ips[i].ip_address = strdup(ip->valuestring);
                
                // Get custom interval if present
                cJSON *interval = cJSON_GetObjectItem(ip_item, "interval");
                if (interval && cJSON_IsNumber(interval)) {
                    config->ips[i].interval = interval->valueint;
                } else {
                    config->ips[i].interval = config->default_interval;
                }
                
                // Get custom timeout if present
                cJSON *timeout = cJSON_GetObjectItem(ip_item, "timeout");
                if (timeout && cJSON_IsNumber(timeout)) {
                    config->ips[i].timeout = timeout->valueint;
                } else {
                    config->ips[i].timeout = config->default_timeout;
                }
                
                // Get active state if present
                cJSON *active = cJSON_GetObjectItem(ip_item, "active");
                if (active && cJSON_IsBool(active)) {
                    config->ips[i].is_active = cJSON_IsTrue(active);
                } else {
                    config->ips[i].is_active = true;
                }
            } else {
                log_message(LOG_ERROR, "Invalid IP item format at index %d", i);
                // Clean up previously allocated items
                for (int j = 0; j < i; j++) {
                    free(config->ips[j].ip_address);
                }
                free(config->ips);
                free(config->filename);
                free(config);
                cJSON_Delete(root);
                return NULL;
            }
        }
    }

    cJSON_Delete(root);
    log_message(LOG_INFO, "Configuration loaded successfully with %d IP addresses", config->ip_count);
    return config;
}

void free_config(Config *config) {
    if (!config) {
        return;
    }
    
    if (config->ips) {
        for (int i = 0; i < config->ip_count; i++) {
            free(config->ips[i].ip_address);
        }
        free(config->ips);
    }
    
    if (config->filename) {
        free(config->filename);
    }
    
    free(config);
}

bool config_has_changed(Config *config) {
    if (!config || !config->filename) {
        return false;
    }
    
    struct stat file_stat;
    if (stat(config->filename, &file_stat) != 0) {
        log_message(LOG_ERROR, "Could not stat config file: %s", config->filename);
        return false;
    }
    
    // Check if the file has been modified since we last loaded it
    if (file_stat.st_mtime > config->last_modified) {
        log_message(LOG_INFO, "Configuration file has been modified");
        return true;
    }
    
    return false;
}

bool reload_config_if_changed(Config **config) {
    if (!config || !*config || !(*config)->filename) {
        return false;
    }
    
    if (!config_has_changed(*config)) {
        return false;
    }
    
    // Load the new configuration
    Config *new_config = load_config((*config)->filename);
    if (!new_config) {
        log_message(LOG_ERROR, "Failed to reload configuration, keeping existing configuration");
        return false;
    }
    
    // Free the old configuration and replace it with the new one
    free_config(*config);
    *config = new_config;
    
    log_message(LOG_INFO, "Configuration reloaded successfully with %d IP addresses", (*config)->ip_count);
    return true;
}
