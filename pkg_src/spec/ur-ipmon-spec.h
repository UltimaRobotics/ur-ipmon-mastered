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

#define IPMON_ACTION_TOPIC "ur-ipmon/actions"
#define IPMON_RESULT_TOPIC "ur-ipmon/results"

void *function_ipmon_single(void *args);
void *function_heartbeat(void *args);