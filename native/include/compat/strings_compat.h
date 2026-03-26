#pragma once

#ifdef _WIN32
    #include <string.h>

    #ifndef strcasecmp
        #define strcasecmp _stricmp
    #endif

    #ifndef strncasecmp
        #define strncasecmp _strnicmp
    #endif
#else
    #include <strings.h>
#endif
