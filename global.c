#include <stdio.h>
#include <time.h>
#include "range.h"
#include "buffer.h"
#include "global.h"

struct settings global_settings = { 8, 0, 0, 0 };

buffer_t *global_buffer;

int global_running = 1;
