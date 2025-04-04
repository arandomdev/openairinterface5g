#include "bictr.h"
#include <math.h>
#include <gmt/gmt.h>

unsigned int delay_samples(double fs, double delay) {
    return (int)round(fs * delay);
}

void bictr_initialize(bictr_desc_t *desc) {
    desc->gmtAPI = GMT_Create_Session("BICTR", 2, 0, NULL);
}

void bictr_free(bictr_desc_t *desc) {
    GMT_Destroy_Session(desc->gmtAPI);
}

#ifdef BICTR_TEST_EXECUTABLE
int main(int argc, char const *argv[])
{
    printf("Hello World\n");
    return 0;
}

#endif