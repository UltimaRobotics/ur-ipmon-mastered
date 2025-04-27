/**
 * @file logger.h
 * @brief Logging utilities for IP Monitor application
 */

#ifndef LOGGER_H
#define LOGGER_H

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} LogLevel;

/**
 * @brief Initialize the logger
 * 
 * @param log_file Path to the log file, NULL for stdout
 * @return int 0 on success, -1 on error
 */
int init_logger(const char *log_file);

/**
 * @brief Close the logger and free resources
 */
void close_logger(void);

/**
 * @brief Log a message with the specified level
 * 
 * @param level Logging level
 * @param format Format string (printf-like)
 * @param ... Variable arguments
 */
void log_message(LogLevel level, const char *format, ...);

/**
 * @brief Set the minimum log level
 * 
 * @param level Minimum level to log
 */
void set_log_level(LogLevel level);

#endif /* LOGGER_H */
