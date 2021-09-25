#ifndef CONFIG_H

#define CONFIG_H

#define KNRM "\x1B[0m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"
#define KYEL "\x1B[33m"
#define KBLU "\x1B[34m"
#define KMAG "\x1B[35m"
#define KCYN "\x1B[36m"
#define KWHT "\x1B[37m"

#define USE_HISTOGRAM                     // use the histogram code as a workaround to ZWO's bug

#define US_IN_MS 1000                     // microseconds in a millisecond
#define MS_IN_SEC 1000                    // milliseconds in a second
#define US_IN_SEC (US_IN_MS * MS_IN_SEC)  // microseconds in a second

#define NOT_SET -1	// signifies something isn't set yet

#define DEFAULT_NOTIFICATIONIMAGES 1
#define DEFAULT_FILENAME     "image.jpg"
#define DEFAULT_TIMEFORMAT   "%Y%m%d %H:%M:%S"	// format the time should be displayed in
#define DEFAULT_ASIDAYEXPOSURE   500	// microseconds - good starting point for daytime exposures
#define DEFAULT_DAYAUTOEXPOSURE  1
#define DEFAULT_DAYDELAY     (5 * MS_IN_SEC)	// 5 seconds
#define DEFAULT_NIGHTDELAY   (10 * MS_IN_SEC)	// 10 seconds
#define DEFAULT_ASINIGHTMAXEXPOSURE  (10 * US_IN_MS)	// 10 ms
#define DEFAULT_GAIN_TRANSITION_TIME 5		// user specifies minutes

#ifdef USE_HISTOGRAM
#define DEFAULT_BOX_SIZEX       500
#define DEFAULT_BOX_SIZEY       500
#define DEFAULT_BOX_FROM_LEFT   0.5
#define DEFAULT_BOX_FROM_TOP    0.5
#endif

#endif // CONFIG_H