#ifndef MOIRAI_VERSION_HXX
#define MOIRAI_VERSION_HXX

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define MOIRAI_VERSION_STRING                                                  \
  (STR(MOIRAI_VERSION_MAJOR) "." STR(MOIRAI_VERSION_MINOR) "." STR(            \
    MOIRAI_VERSION_PATCH) MOIRAI_VERSION_TWEAK)
#endif
