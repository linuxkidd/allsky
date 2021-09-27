#ifndef CONFIG_H

#define CONFIG_H
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "INIReader.h"
#include "common.h"

#define KNRM "\x1B[0m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"
#define KYEL "\x1B[33m"
#define KBLU "\x1B[34m"
#define KMAG "\x1B[35m"
#define KCYN "\x1B[36m"
#define KWHT "\x1B[37m"

#define USE_HISTOGRAM                     // use the histogram code as a workaround to ZWO's bug

#define US_IN_MS   1000                    // microseconds in a millisecond
#define MS_IN_SEC  1000                    // milliseconds in a second
#define US_IN_SEC  (US_IN_MS * MS_IN_SEC)  // microseconds in a second

#define NOT_SET                          -1                 // signifies something isn't set yet

#define AUTO_IMAGE_TYPE_VALUE                  99                 // needs to match what's in the camera_settings.json file
#define DEFAULT_BINNING_DAY              1                  // binning during the day probably isn't too useful...
#define DEFAULT_BINNING_NIGHT            1
#define DEFAULT_BRIGHTNESS_TARGET        50                 // target brightness for auto-exposure and auto-gain
#define DEFAULT_CAPTURE_DAYTIME          0
#define DEFAULT_COOLER_ENABLED           0
#define DEFAULT_COOLER_TARGET_TEMP_C     0
#define DEFAULT_DEBUG_LEVEL              1
#define DEFAULT_DELAY_DAY_MS             (5 * MS_IN_SEC)    //  5 seconds
#define DEFAULT_DELAY_NIGHT_MS           (10 * MS_IN_SEC)   // 10 seconds
#define DEFAULT_EXPOSURE_DAY_AUTO        1
#define DEFAULT_EXPOSURE_DAY_US          500                // microseconds - good starting point for daytime exposures
#define DEFAULT_EXPOSURE_NIGHT_AUTO      1
// There is no max day autoexposure since daylight exposures are always pretty short.
#define DEFAULT_EXPOSURE_NIGHT_MAX_US    (20 * US_IN_MS)    // 20 seconds
#define DEFAULT_EXPOSURE_NIGHT_US        (10 * US_IN_SEC)   // 10 seconds
#define DEFAULT_FONT_NUMBER              0
#define DEFAULT_FONT_SIZE                 7
#define DEFAULT_GAIN_DAY                 0
#define DEFAULT_GAIN_NIGHT               150
#define DEFAULT_GAIN_NIGHT_AUTO          0
#define DEFAULT_GAIN_NIGHT_MAX           200
#define DEFAULT_GAIN_TRANSITION_TIME     5                 // user specifies minutes
#define DEFAULT_GAMMA                    50                // not supported by all cameras
#define DEFAULT_IMAGE_FILENAME           "image.jpg"
#define DEFAULT_IMAGE_FLIP               "N"               // N - None, H - Horizontal, V - Vertical, B - Both
#define DEFAULT_IMAGE_HEIGHT_PX          0
#define DEFAULT_IMAGE_TYPE               1
#define DEFAULT_IMAGE_WIDTH_PX           0
#define DEFAULT_LATITUDE                 "60.7N" //GPS Coordinates of Whitehorse, Yukon where the code was created
#define DEFAULT_LOCALE                   "en_US.UTF-8"
#define DEFAULT_LONGITUDE                "135.05W"
#define DEFAULT_NOTIFICATION_IMAGES      1
#define DEFAULT_FONT_OUTLINE              0
// angle of the sun with the horizon
// (0=sunset, -6=civil twilight, -12=nautical twilight, -18=astronomical twilight)
#define DEFAULT_SOLAR_ANGLE              "-6"
#define DEFAULT_TEXT_LINE_HEIGHT_PX      30
#define DEFAULT_FONT_SMOOTHING      0
#define DEFAULT_TEXT_FONT_WEIGHT          1
#define DEFAULT_TEXT_OFFSET_FROM_LEFT_PX 15
#define DEFAULT_TEXT_OFFSET_FROM_TOP_PX  25
#define DEFAULT_TIME_FORMAT              "%Y%m%d %H:%M:%S"  // format the time should be displayed in
#define DEFAULT_TIME_SHOW                1
#define DEFAULT_USB_BANDWIDTH            40
#define DEFAULT_USB_BANDWIDTH_AUTO       0
#define DEFAULT_WHITE_BALANCE_AUTO       0
#define DEFAULT_WHITE_BALANCE_BLUE       85
#define DEFAULT_WHITE_BALANCE_RED        65

#define MAX_BRIGHTNESS                   600
#define SHORT_EXPOSURE                   30000
#define FONT_SIZE_SMALL_MULTIPLIER         0.08

#ifdef USE_HISTOGRAM
#define DEFAULT_HISTO_BOX_WIDTH_PX       500
#define DEFAULT_HISTO_BOX_HEIGHT_PX      500
#define DEFAULT_HISTO_BOX_FROM_LEFT      0.5
#define DEFAULT_HISTO_BOX_FROM_TOP       0.5
#endif

void convertColor(std::string colortmp, int *fontary);
void convertFont(std::string fontName);
void parse_commandline(int argc, char *argv[]);
int parse_ini();

extern void displayDebugText(const char * text, int requiredLevel);
extern void waitToFix(char const *msg);

extern bool notification_images;
extern bool continuous_exposure;
extern char const *angle;
extern char const *image_file_name;
extern char const *font_names[];
extern char const *image_extra_text_file_name;
extern char const *image_text;
extern char const *latitude;
extern char const *longitude;
extern char const *time_format;
extern char debugText[500];
extern char *line;
extern const char *locale;
extern double font_size;
extern int usb_bandwidth_auto;
extern int auto_white_balance;
extern int usb_bandwidth;
extern int cooler_enabled;
extern int exposure_day_auto;
extern int brightness_target_day;
extern int exposure_day_us;
extern int gain_day;
extern int image_flip;
extern double gamma;
extern int exposure_night_auto;
extern int gain_night_auto;
extern int brightness_target_night;
extern int gain_night;
extern int exposure_night_max_us;
extern int gain_night_max;
extern int white_balance_blue;
extern int white_balance_red;
extern int binning_current;
extern int brightness_current;
extern int delay_current;
extern int binning_day;
extern int delay_day_ms;
extern int capture_daytime;
extern int extra_file_age;
extern int font_color[3];
extern int font_count;
extern int font_number;
extern int gain_transition_time;
extern int height;
extern int image_type;
extern std::string image_ext;
extern std::string image_name;
extern int text_line_height_px;
extern int text_offset_from_left_px;
extern int text_offset_from_top_px;
extern int font_smoothing;
extern int font_weight;
extern int binning_night;
extern int delay_night_ms;
extern int font_outline;
extern int preview;
extern int quality;
extern int brightness_show;
extern int exposure_show;
extern int gain_show;
extern int temperature_show;
extern int time_show;
extern int font_color_small[3];
extern int width;
extern long exposure_night_us;
extern long cooler_target_temp_c;
extern long buffer_size;
extern long exposure_camera_max_us;
extern long exposure_current_value;

#ifdef USE_HISTOGRAM
extern float histogram_box_center_from_left_pct;
extern float histogram_box_center_from_top_pct;
extern int histogram_box_height_px;
extern int histogram_box_width_px;
extern int histogram_box_show;
#endif


#endif // CONFIG_H
