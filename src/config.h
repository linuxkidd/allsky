#ifndef CONFIG_H

#define CONFIG_H
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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

#define DEFAULT_LOCALE           "en_US.UTF-8"
#define DEFAULT_FONTNUMBER       0
#define DEFAULT_ITEXTX           15
#define DEFAULT_ITEXTY           25
#define DEFAULT_ITEXTLINEHEIGHT  30
#define DEFAULT_FONTSIZE         7
#define SMALLFONTSIZE_MULTIPLIER 0.08
#define DEFAULT_LINEWIDTH        1
#define DEFAULT_OUTLINEFONT      0
#define DEFAULT_LINENUMBER       0

#define DEFAULT_WIDTH            0
#define DEFAULT_HEIGHT           0

#define DEFAULT_DAYBIN           1  // binning during the day probably isn't too useful...
#define DEFAULT_NIGHTBIN         1

#define DEFAULT_IMAGE_TYPE       1
#define AUTO_IMAGE_TYPE         99	// needs to match what's in the camera_settings.json file

#define DEFAULT_ASIBANDWIDTH    40

// There is no max day autoexposure since daylight exposures are always pretty short.
#define DEFAULT_ASINIGHTEXPOSURE (5 * US_IN_SEC)	// 5 seconds

#define DEFAULT_NIGHTAUTOEXPOSURE 1
#define DEFAULT_ASIDAYGHTGAIN    0
#define DEFAULT_ASINIGHTGAIN     150
#define DEFAULT_NIGHTAUTOGAIN    0
#define DEFAULT_ASINIGHTMAXGAIN  200
#define DEFAULT_ASIWBR           65
#define DEFAULT_ASIWBB           85
#define DEFAULT_AUTOWHITEBALANCE 0
#define DEFAULT_ASIGAMMA         50		// not supported by all cameras
#define DEFAULT_BRIGHTNESS       50
#define MAX_BRIGHTNESS           600
#define DEFAULT_LATITUDE         "60.7N" //GPS Coordinates of Whitehorse, Yukon where the code was created
#define DEFAULT_LONGITUDE        "135.05W"

// angle of the sun with the horizon
// (0=sunset, -6=civil twilight, -12=nautical twilight, -18=astronomical twilight)
#define DEFAULT_ANGLE            "-6"
#define DEFAULT_SHOWTIME 1
#define DEFAULT_DAYTIMECAPTURE   0

void parse_commandline(int argc, char *argv[]);

#endif // CONFIG_H