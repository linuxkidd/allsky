/*
 * Command Line Arguments Parser
 * part of the Allsky project
 * https://github.com/thomasjacquin/allsky
 *
 */

#include "config.h"

//-------------------------------------------------------------------------------------------------------
char const *config_file;
int debug_level = DEFAULT_DEBUG_LEVEL;
bool help;
//-------------------------------------------------------------------------------------------------------


void parse_commandline(int argc, char *argv[])
{
    // Many of the argument names changed to allow day and night values.
    // However, still check for the old names in case the user didn't update their
    // settings.json file.  The old names should be removed below in a future version.
    for (int i=1 ; i < argc - 1 ; i++)
    {
        // Check again in case "-h" isn't the first option.
        if (strncmp(argv[i], "-c",3) == 0 || strncmp(argv[i], "--conf",7) == 0)
        {
            // In case the text is null and isn't quoted, check if the next argument
            // starts with a "-".  If so, the text is null, otherwise it's the text.
            if ((char)argv[i + 1][0] != '-') {
                config_file = argv[++i];
            }
        }
        else if (strncmp(argv[i], "-d",3) == 0 || strncmp(argv[i], "--debug",8) == 0)
        {
            debug_level = atoi(argv[++i]);
        }
        else if (strncmp(argv[i], "-h",3) == 0 || strncmp(argv[i], "--help",7) == 0)
        {
            help = 1;
        }
        else if (strncmp(argv[i], "-p",3) == 0 || strncmp(argv[i], "--preview",10) == 0)
        {
            preview = 1;
        } else {
            help = 1;
        }
    }
    return;
}

int parse_ini()
{
    INIReader reader(config_file);
    if ( reader.ParseError() < 0 )
    {
        snprintf(debugText,499,"Could not load %s.\n",config_file);
        displayDebugText(debugText,0);
        return 1;
    }

    // Global Options
    image_ext            = reader.GetString( "global",      "image_extension", "jpg");
    image_name           = reader.GetString( "global",           "image_name", "image");
    std::string fname    = image_name + "." + image_ext;
    image_file_name             = fname.c_str();
    longitude            = reader.GetString( "global",            "longitude", DEFAULT_LONGITUDE).c_str();
    latitude             = reader.GetString( "global",             "latitude", DEFAULT_LATITUDE).c_str();
    angle                = reader.GetString( "global",                "angle", DEFAULT_SOLAR_ANGLE).c_str();
    notification_images   = reader.GetBoolean( "webui",  "notification_images", "1");
#ifdef USE_HISTOGRAM
    std::string histogramBox = reader.GetString("advanced", "histogram_box", "500 500 50 50");
    sscanf(histogramBox.c_str(), "%d %d %f %f", &histogram_box_height_px, &histogram_box_width_px, &histogram_box_center_from_left_pct, &histogram_box_center_from_top_pct);
    histogram_box_center_from_left_pct /= 100; // user enters 0-100
    histogram_box_center_from_top_pct /= 100;
    histogram_box_show = reader.GetBoolean("overlay", "histogram_show", 0);
#endif // USE_HISTORGRAM

    // Capture options that apply to both Day and Night
    continuous_exposure     = reader.GetBoolean( "capture",          "continuous_exposure","0");
    locale                  = reader.GetString( "advanced",                      "locale", DEFAULT_LOCALE).c_str();
    height                  = reader.GetInteger( "capture",                   "height_px", DEFAULT_IMAGE_HEIGHT_PX);
    width                   = reader.GetInteger( "capture",                    "width_px", DEFAULT_IMAGE_WIDTH_PX);
    image_type              = reader.GetInteger( "capture",                        "type", DEFAULT_IMAGE_TYPE);
    quality                 = reader.GetInteger( "capture",                     "quality", NOT_SET);
    gamma                   = reader.GetInteger( "capture",                       "gamma", DEFAULT_GAMMA);
    auto_white_balance      = reader.GetBoolean( "capture",          "white_balance_auto", DEFAULT_WHITE_BALANCE_AUTO);
    white_balance_red       = reader.GetInteger( "capture",                      "wb_red", DEFAULT_WHITE_BALANCE_RED);
    white_balance_blue      = reader.GetInteger( "capture",                     "wb_blue", DEFAULT_WHITE_BALANCE_BLUE);
    std::string fliptmp     = reader.GetString(  "capture",                        "flip", DEFAULT_IMAGE_FLIP);
    usb_bandwidth           = reader.GetInteger( "capture",                         "usb", DEFAULT_USB_BANDWIDTH);
    usb_bandwidth_auto      = reader.GetBoolean( "capture",                    "usb_auto", DEFAULT_USB_BANDWIDTH_AUTO);
    cooler_enabled          = reader.GetBoolean( "capture",              "cooler_enabled", DEFAULT_COOLER_ENABLED);
    cooler_target_temp_c    = reader.GetReal(    "capture",        "cooler_target_temp_c", DEFAULT_COOLER_TARGET_TEMP_C);

    // All Night Specific Options:
    exposure_night_auto     = reader.GetBoolean("capture.night",          "exposure_auto", DEFAULT_EXPOSURE_NIGHT_AUTO);
    exposure_night_us       = reader.GetInteger("capture.night",            "exposure_ms", DEFAULT_EXPOSURE_NIGHT_US) * US_IN_MS;
    exposure_night_max_us   = reader.GetInteger("capture.night",        "exposure_max_ms", DEFAULT_EXPOSURE_NIGHT_MAX_US) * US_IN_MS;
    gain_night_auto         = reader.GetBoolean("capture.night",              "gain_auto", DEFAULT_GAIN_NIGHT_AUTO);
    gain_night              = reader.GetInteger("capture.night",                   "gain", DEFAULT_GAIN_NIGHT);
    gain_night_max          = reader.GetInteger("capture.night",               "gain_max", DEFAULT_GAIN_NIGHT_MAX);
    gain_transition_time      = reader.GetInteger("capture.night", "gain_transition_time_s", DEFAULT_GAIN_TRANSITION_TIME);
    brightness_target_night = reader.GetInteger("capture.night",             "brightness", DEFAULT_BRIGHTNESS_TARGET);
    binning_night                = reader.GetInteger("capture.night",                "binning", DEFAULT_BINNING_NIGHT);
    delay_night_ms              = reader.GetInteger("capture.night",                  "delay", DEFAULT_DELAY_NIGHT_MS);

    // All Day Specific Options:
    capture_daytime          = reader.GetBoolean("capture.day",                   "enable", DEFAULT_CAPTURE_DAYTIME);
    exposure_day_auto       = reader.GetBoolean("capture.day",            "exposure_auto", DEFAULT_EXPOSURE_DAY_AUTO) * 60;
    exposure_day_us         = reader.GetInteger("capture.day",              "exposure_ms", DEFAULT_EXPOSURE_DAY_US/1000) * US_IN_MS;
    brightness_target_day   = reader.GetInteger("capture.day",               "brightness", DEFAULT_BRIGHTNESS_TARGET);
    binning_day                  = reader.GetInteger("capture.day",                  "binning", DEFAULT_BINNING_DAY);
    delay_day_ms                = reader.GetInteger("capture.day",                    "delay", DEFAULT_DELAY_DAY_MS);

    // Overlay Options:
    ImgText                 = reader.GetString(  "overlay",                        "text", "").c_str();
    image_extra_text_file_name            = reader.GetString(  "overlay",             "text_extra_file", "").c_str();
    extra_file_age            = reader.GetInteger( "overlay",   "text_extra_file_max_age_s", 0);
    text_line_height_px         = reader.GetInteger( "overlay",         "text_line_height_px", DEFAULT_TEXT_LINE_HEIGHT_PX);
    text_offset_from_left_px                  = reader.GetInteger( "overlay",                   "text_x_px", DEFAULT_TEXT_OFFSET_FROM_LEFT_PX);
    text_offset_from_top_px                  = reader.GetInteger( "overlay",                   "text_y_px", DEFAULT_TEXT_OFFSET_FROM_TOP_PX);
    std::string font_numbers    = reader.GetString(  "overlay",                   "font_name", "SIMPLEX");
    std::string lgclrtmp    = reader.GetString(  "overlay",              "font_color_rgb", "255 0 0");
    std::string smclrtmp    = reader.GetString(  "overlay",        "font_small_color_rgb", "0 0 255");
    font_smoothing              = reader.GetInteger( "overlay",              "font_line_type", DEFAULT_FONT_SMOOTHING);
    font_size                = reader.GetInteger( "overlay",                   "font_size", DEFAULT_FONT_SIZE);
    font_weight               = reader.GetInteger( "overlay",         "font_line_thickness", DEFAULT_TEXT_FONT_WEIGHT);
    font_outline             = reader.GetBoolean( "overlay",                "font_outline", DEFAULT_FONT_OUTLINE);
    time_show                = reader.GetBoolean( "overlay",                   "time_show", DEFAULT_TIME_SHOW);
    time_format              = reader.GetString(  "overlay",                 "time_format", DEFAULT_TIME_FORMAT).c_str();
    temperature_show                = reader.GetBoolean( "overlay",                   "temp_show", "0");
    exposure_show            = reader.GetBoolean( "overlay",               "exposure_show", "0");
    gain_show                = reader.GetBoolean( "overlay",                   "gain_show", "0");
    brightness_show          = reader.GetBoolean( "overlay",             "brightness_show", "0");

    convertColor(lgclrtmp,font_color);
    convertColor(smclrtmp,font_color_small);
    convertFont(font_numbers);
    convertFlip(fliptmp);

    return 0;
}

void convertFlip(std::string flipname) {
    bool found_flip = false;
    const char *flip_options[] = { "N", "H", "V", "B" };
    for(int i=0;i<4;i++)
    {
        if(strncmp(flipname.c_str(),flip_options[i],2)==0)
        {
            image_flip = i;
            found_flip = true;
        }
    }
    if(!found_flip) {
        snprintf(debugText,499,"Warning: Flip option '%s' not one of 'N', 'H', 'V', 'B'.  Continuing with no flip.",flipname);
        image_flip = 0;
        displayDebugText(debugText,0);
    }
}

void convertColor(std::string colortmp, int *fontary)
{
    const char *colorpattern;

    if(strncmp(colortmp.c_str(),"#",1) && sizeof(colortmp)>=7)
    {
        colorpattern = "#%02X%02X%02X";
        
    }
    else
    {
        colorpattern = "%d %d %d";
    }
    sscanf(colortmp.c_str(), colorpattern, &fontary[0], &fontary[1], &fontary[2]);
}

void convertFont(std::string fontName)
{
    bool foundfont = false;
    for(int i=0; i<font_count; i++)
    {
        if(strncmp(font_names[i],fontName.c_str(),16))
        {
            font_number = i;
            foundfont = true;
        }
    }
    if(!foundfont)
    {
        snprintf(debugText,16,"Sorry, font name %s is not valid.",fontName);
        waitToFix(debugText);
    }
}