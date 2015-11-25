#if defined(_MSC_VER) && _MSC_VER < 1800
#define PRId32   "I32d"
#define PRId64   "I64d"
#else
#include <inttypes.h>
#endif
