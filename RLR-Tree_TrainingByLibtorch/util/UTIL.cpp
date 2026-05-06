#include "UTIL.h"

double thread_real_time(){
    timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}
