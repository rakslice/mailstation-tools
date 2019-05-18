#include "progname.h"


#include <stdlib.h>

#ifdef __linux__
const char * getprogname(void) {
        return getenv("_");
}
#endif

