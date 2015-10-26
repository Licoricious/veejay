#ifndef AVCOMMON_H
#define AVCOMMON_H
#include <libavutil/version.h>
#define LIBAVUTIL_VERSION_CHECK( a, b, c, d, e ) \
    ( (LIBAVUTIL_VERSION_MICRO <  100 && LIBAVUTIL_VERSION_INT >= AV_VERSION_INT( a, b, c ) ) || \
      (LIBAVUTIL_VERSION_MICRO >= 100 && LIBAVUTIL_VERSION_INT >= AV_VERSION_INT( a, d, e ) ) )

#if LIBAVUTIL_VERSION_CHECK(51,45,0,74,100)
#ifndef PIX_FMT_YUVA422P
#define PIX_FMT_YUVA422P AV_PIX_FMT_YUVA422P
#endif
#endif
#endif
