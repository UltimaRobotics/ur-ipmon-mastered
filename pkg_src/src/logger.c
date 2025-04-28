/**
 * @file logger.c
 * @brief Implementation of logging functionality
 */

#include "../include/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

static FILE *log_file = NULL;
static LogLevel current_log_level = LOG_INFO;
static int is_stdout = 0;

int init_logger(const char *filename) {
    // If NULL, use stdout
    if (!filename) {
        log_file = stdout;
        is_stdout = 1;
        return 0;
    }
    
    log_file = fopen(filename, "a");
    if (!log_file) {
        fprintf(stderr, "Failed to open log file: %s\n", filename);
        return -1;
    }
    
    is_stdout = 0;
    return 0;
}

void close_logger(void) {
    if (log_file && !is_stdout) {
        fclose(log_file);
        log_file = NULL;
    }
}

void set_log_level(LogLevel level) {
    current_log_level = level;
}

void log_message(LogLevel level, const char *format, ...) {
    #ifdef _DEBUG
    // Skip if level is below current setting
    if (level < current_log_level) {
        return;
    }
    
    // If logger is not initialized, initialize with stdout
    if (!log_file) {
        init_logger(NULL);
    }
    
    // Get current time
    time_t now = time(NULL);
    struct tm *time_info = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);
    
    // Level as string
    const char *level_str;
    switch (level) {
        case LOG_DEBUG:
            level_str = "DEBUG";
            break;
        case LOG_INFO:
            level_str = "INFO";
            break;
        case LOG_WARNING:
            level_str = "WARNING";
            break;
        case LOG_ERROR:
            level_str = "ERROR";
            break;
        default:
            level_str = "UNKNOWN";
    }
    
    // Print timestamp and level
    fprintf(log_file, "[%s] [%s] ", time_str, level_str);
    
    // Print formatted message
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    
    // Add newline and flush
    fprintf(log_file, "\n");
    fflush(log_file);
    #endif
}
