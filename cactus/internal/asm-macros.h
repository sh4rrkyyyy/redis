#define CONCAT_AUX(x, y) x ## y
#define CONCAT(x, y) CONCAT_AUX(x, y)

#if defined(__APPLE__)

#define NAME(name) CONCAT(_, name)
#define TYPE_FUNCTION(name)

#else

#define NAME(name) name
#define TYPE_FUNCTION(name) .type name, @function

#endif
