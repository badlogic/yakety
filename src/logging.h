#ifndef LOGGING_H
#define LOGGING_H

// Initialize logging system
void log_init(void);

// Cleanup logging system
void log_cleanup(void);

// Logging functions
void log_info(const char *format, ...);
void log_error(const char *format, ...);
void log_debug(const char *format, ...);

#endif// LOGGING_H