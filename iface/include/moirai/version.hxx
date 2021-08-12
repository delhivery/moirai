#ifndef PROJECT_VERSION_HXX
#define PROJECT_VERSION_HXX

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#ifndef NAME
#error Missing definition PROJECT_NAME
#else
#define PROJECT_NAME STR(NAME)
#endif

#ifndef VERSION_TWEAK
#define VERSION_TWEAK ""
#endif

#define PROJECT_VERSION_STRING                                                 \
  (STR(VERSION_MAJOR) "." STR(VERSION_MINOR) "." STR(VERSION_PATCH)            \
     STR(VERSION_TWEAK))
#endif
