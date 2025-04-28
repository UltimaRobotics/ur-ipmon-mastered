/**
 * @file main.c
 * @brief Main entry point for IP Monitor application
 */
#include "stdio.h"
#include "stdlib.h"

#include "string.h"
#include "signal.h"
#include "unistd.h"
#include "../include/thread_manager.h"
#include "../include/utils.h"

#include "ur-ipmon-spec.h"
#include "ur-rpc-template.h"


thread_manager_t manager;
volatile sig_atomic_t running = 1;

static void signal_handler(int signo) {
    log_message(LOG_INFO, "Received signal %d. Stopping monitoring...", signo);
    if (g_monitor) {
        stop_monitoring(g_monitor);
    }
    exit(EXIT_SUCCESS);
}

void on_message(struct mosquitto* mosq, void* userdata, const struct mosquitto_message* message) {
    MqttThreadContext* context_temp = (MqttThreadContext*)userdata;
    Config* config = &context_temp->config_base;
    pthread_mutex_lock(&context_temp->mutex);
    for (int i = 0; i < context_temp->config_additional.json_added_subs.topics_num; i++) {
        if (strcmp(message->topic, context_temp->config_additional.json_added_subs.topics[i]) == 0) {
            #ifdef _DEBUG_MODE
            printf("Received message on custom topic %s: %.*s\n",message->topic, message->payloadlen, (char*)message->payload);
            #endif
            if (strcmp(message->topic,)){

            }
            break;
        }
    }
    pthread_mutex_unlock(&context_temp->mutex);
}

int main (int argc, char *argv[]){
    if (argc != 3) {
        return EXIT_FAILURE;
    }
    context = malloc(sizeof(MqttThreadContext));
    memset(context, 0, sizeof(MqttThreadContext));
    pthread_mutex_init(&context->mutex, NULL);

    context->config_paths.base_config_path = strdup(argv[1]);
    context->config_paths.custom_config_path = strdup(argv[2]);

    context->config_base = parse_base_config(argv[1]);
    context->config_additional = parse_custom_topics(argv[2]);

    context->mqtt_monitor.last_activity = time(NULL);
    atomic_init(&context->mqtt_monitor.running, false);
    atomic_init(&context->mqtt_monitor.healthy, false);
    atomic_init(&context->health_monitor.running, false);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&context->mqtt_monitor.thread_id, &attr, 
                      mqtt_thread_func, context) != 0) {
        fprintf(stderr, "Failed to create MQTT thread\n");
        goto cleanup;
    }

    if (pthread_create(&context->health_monitor.thread_id, &attr, 
                      health_monitor_func, context) != 0) {
        fprintf(stderr, "Failed to create health monitor thread\n");
        atomic_store(&context->mqtt_monitor.running, false);
        goto cleanup;
    }

    pthread_attr_destroy(&attr);

    if (thread_manager_init(&manager, 10) != 0) {
        ERROR_LOG("Failed to initialize thread manager");
        return 1;
    }
    cleanup:
    atomic_store(&context->mqtt_monitor.running, false);
    atomic_store(&context->health_monitor.running, false);
    
    set_log_level(LOG_LEVEL_INFO);
    signal(SIGINT, signal_handler);
    if (thread_manager_init(&manager, 10) != 0) {
        ERROR_LOG("Failed to initialize thread manager");
        return 1;
    }
    INFO_LOG("Thread manager initialized");
    
    int choice;
    unsigned int thread_id;

    int *thread_num = (int *)malloc(sizeof(int));
    *thread_num = thread_get_count(&manager) + 1;
                
    unsigned int new_id;
    if (thread_create(&manager, function_heartbeat, thread_num, &new_id) > 0) {
        INFO_LOG("Created heartbeat thread with ID %u", new_id);
    } else {
        ERROR_LOG("Failed to heartbeat worker thread");
        free(thread_num);
    }
    
    while (running) {
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 100000000; 
        nanosleep(&ts, NULL);
    }

    sleep(1);
    
    free_base_config(&context->config_base);
    free_custom_topics(&context->config_additional);
    free(context->config_paths.base_config_path);
    free(context->config_paths.custom_config_path);
    pthread_mutex_destroy(&context->mutex);
    thread_manager_destroy(&manager);
    free(context);
    cleanup_threads();
    return EXIT_SUCCESS;
}
