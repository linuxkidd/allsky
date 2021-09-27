#include "common.h"

//-------------------------------------------------------------------------------------------------------
char debugText[500]; // buffer to hold debug messages displayed by displayDebugText()
std::string dayOrNight;
//-------------------------------------------------------------------------------------------------------

// Return the numeric time.
timeval getTimeval()
{
    timeval curTime;
    gettimeofday(&curTime, NULL);
    return (curTime);
}

// Format a numeric time as a string.
char *formatTime(timeval t, char const *tf)
{
    static char TimeString[128];
    strftime(TimeString, 80, tf, localtime(&t.tv_sec));
    return (TimeString);
}

// Return the current time as a string.  Uses both functions above.
char *getTime(char const *tf)
{
    return (formatTime(getTimeval(), tf));
}

double timeDiff(int64 start, int64 end)
{
    double frequency = cv::getTickFrequency();
    return (double)(end - start) / frequency; // in Microseconds
}

std::string ReplaceAll(std::string str, const std::string &from, const std::string &to)
{
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

std::string exec(const char *cmd)
{
    std::tr1::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
    if (!pipe)
        return "ERROR";
    char buffer[128];
    std::string result = "";
    while (!feof(pipe.get()))
    {
        if (fgets(buffer, 128, pipe.get()) != NULL)
        {
            result += buffer;
        }
    }
    return result;
}

void *Display(void *params)
{
    cv::Mat *para = (cv::Mat *)params;
    int w         = para->cols;
    int h         = para->rows;

    cv::Mat pImg(h, w, (int)para->type(), (uchar *)para->data);
    cv::namedWindow("video", 1);
    while (b_display)
    {
        cv::imshow("video", pImg);
        cv::waitKey(100);
    }
    cv::destroyWindow("video");
    printf("Display thread over\n");
    return (void *)0;
}

int roundTo(int n, int roundTo)
{
    int a = (n / roundTo) * roundTo; // Smaller multiple
    int b = a + roundTo;             // Larger multiple
    return (n - a > b - n) ? b : a;  // Return of closest of two
}

void IntHandle(int i)
{
    signal_received = 1;
    closeUp(0);
}

void displayDebugText(const char *text, int requiredLevel)
{
    if (debug_level >= requiredLevel)
    {
        printf("%s", text);
    }
}

// A user error was found.  Wait for the user to fix it.
void waitToFix(char const *msg)
{
    printf("**********\n");
    printf(msg);
    printf("\n");
    printf("*** After fixing, ");
    if (tty)
    {
        printf("restart allsky.sh.\n");
    }
    else
    {
        printf("restart the allsky service.\n");
    }
    if (notification_images)
    {
        system("scripts/copy_notification_image.sh Error &");
    }
    sleep(5); // give time for image to be copied
    printf("*** Sleeping until you fix the problem.\n");
    printf("**********\n");
    sleep(100000); // basically, sleep forever until the user fixes this.
}

// Calculate if it is day or night
void calculateDayOrNight(const char *latitude, const char *longitude, const char *angle)
{
    char sunwaitCommand[128];
    // don't need "exit" or "set".
    sprintf(sunwaitCommand, "sunwait poll angle %s %s %s", angle, latitude, longitude);
    dayOrNight = exec(sunwaitCommand);
    dayOrNight.erase(std::remove(dayOrNight.begin(), dayOrNight.end(), '\n'), dayOrNight.end());

    if (dayOrNight != "DAY" && dayOrNight != "NIGHT")
    {
        sprintf(debugText, "*** ERROR: dayOrNight isn't DAY or NIGHT, it's '%s'\n", dayOrNight.c_str());
        waitToFix(debugText);
        closeUp(2);
    }
}

// Calculate how long until nighttime.
int calculateTimeToNightTime(const char *latitude, const char *longitude, const char *angle)
{
    std::string t;
    char sunwaitCommand[128]; // returns "hh:mm, hh:mm" (sunrise, sunset)
    sprintf(sunwaitCommand, "sunwait list angle %s %s %s | awk '{print $2}'", angle, latitude, longitude);
    t = exec(sunwaitCommand);
    t.erase(std::remove(t.begin(), t.end(), '\n'), t.end());

    int h = 0, m = 0, secs;
    sscanf(t.c_str(), "%d:%d", &h, &m);
    secs = (h * 60 * 60) + (m * 60);

    char *now = getTime("%H:%M");
    int hNow = 0, mNow = 0, secsNow;
    sscanf(now, "%d:%d", &hNow, &mNow);
    secsNow = (hNow * 60 * 60) + (mNow * 60);

    // Handle the (probably rare) case where nighttime is tomorrow
    if (secsNow > secs)
    {
        return (secs + (60 * 60 * 24) - secsNow);
    }
    else
    {
        return (secs - secsNow);
    }
}

// Simple function to make flags easier to read for humans.
char const *yesNo(int flag)
{
    if (flag)
    {
        return ("yes");
    }
    else
    {
        return ("no");
    }
}

void printCredits()
{
    setlinebuf(stdout); // Line buffer output so entries appear in the log immediately.
    printf("\n");
    printf("%s ******************************************\n", KGRN);
    printf("%s *** Allsky Camera Software v0.8 | 2021 ***\n", KGRN);
    printf("%s ******************************************\n\n", KGRN);
    printf("\%sCapture images of the sky with a Raspberry Pi and an ASI Camera\n", KGRN);
    printf("\n");
    printf("%sAdd -h or -help for available options\n", KYEL);
    printf("\n");
    printf("\%sAuthor: ", KNRM);
    printf("Thomas Jacquin - <jacquin.thomas@gmail.com>\n\n");
    printf("\%sContributors:\n", KNRM);
    printf("-Knut Olav Klo\n");
    printf("-Daniel Johnsen\n");
    printf("-Yang and Sam from ZWO\n");
    printf("-Robert Wagner\n");
    printf("-Michael J. Kidd - <linuxkidd@gmail.com>\n");
    printf("-Chris Kuethe\n\n");
    printf("-Eric Claeys\n");
}

void printHelp()
{
    printf("%sAvailable Arguments:\n", KYEL);
    printf(" -c | --conf <str>  : The config file to use. - No default (default settings output below)\n");
    printf(" -d | --debug <int> : Debug Level, 0 to 9\n");
    printf(" -h | --help        : Display this help and exit.\n");
    printf(" -p | --preview     : Display a preview of the image on screen. Requires a GUI to be running.\n");

    printf("%s", KGRN);
    printf("\nDEFAULT Settings:\n");
    printf("=================\n");
    printf("\nGlobal Settings:\n");
    printf("================\n");
    printf("              Image Type: %s\n  - 0 = RAW8,  1 = RGB24,  2 = RAW16,  3 = Y8", DEFAULT_IMAGE_TYPE);
    printf("         Image File Name: %s\n", DEFAULT_IMAGE_FILENAME);
    printf("          Width x Height: %d x %d\n", DEFAULT_IMAGE_WIDTH_PX, DEFAULT_IMAGE_HEIGHT_PX);
    printf("          Cooler Enabled: %s\n", yesNo(DEFAULT_COOLER_ENABLED));
    printf("      Target Temperature: %d C\n", DEFAULT_COOLER_TARGET_TEMP_C);
    printf("                 Quality: %d for jpg, %d for png\n", 95, 3);
    printf("                   Gamma: %d\n", DEFAULT_GAMMA);
    printf("      Auto White Balance: %s\n", yesNo(DEFAULT_WHITE_BALANCE_AUTO));
    printf("       Red White Balance: %d%% Manual Offset\n", DEFAULT_WHITE_BALANCE_RED);
    printf("      Blue White Balance: %d%% Manual Offset\n", DEFAULT_WHITE_BALANCE_BLUE);
    printf("           USB Bandwidth: %d\n", DEFAULT_USB_BANDWIDTH);
    printf("      Auto USB Bandwidth: %s\n", yesNo(DEFAULT_USB_BANDWIDTH_AUTO));
    printf("         Daytime capture: %s\n", yesNo(DEFAULT_CAPTURE_DAYTIME));
    printf("              Flip Image: %s  - N = None, H = Horizontal, V = Vertical, B = Both\n", DEFAULT_IMAGE_FLIP);
    printf("                Latitude: %s\n", DEFAULT_LATITUDE);
    printf("               Longitude: %s\n", DEFAULT_LONGITUDE);
    printf("           Sun Elevation: %s\n", DEFAULT_SOLAR_ANGLE);
    printf("                  Locale: %s\n", DEFAULT_LOCALE);
    printf("     Notification Images: %s\n", yesNo(DEFAULT_NOTIFICATION_IMAGES));
#ifdef USE_HISTOGRAM
    printf("           Histogram Box: %d px Wide x %d px High, Centered %0.0f%% from Left, %0.0f%% from Top\n",
           DEFAULT_HISTO_BOX_WIDTH_PX, DEFAULT_HISTO_BOX_HEIGHT_PX, DEFAULT_HISTO_BOX_FROM_LEFT * 100,
           DEFAULT_HISTO_BOX_FROM_TOP * 100);
#endif // USE_HISTOGRAM
    printf("             Debug Level: %d\n", DEFAULT_DEBUG_LEVEL);

    printf("\n\n");
    printf("                Settings: Day / Night\n");
    printf("  ===========================================\n");
    printf("                 Binning: %d / %d\n", DEFAULT_BINNING_DAY, DEFAULT_BINNING_NIGHT);
    printf("    Inter-Exposure Delay: %'dms /  %'dms\n", DEFAULT_DELAY_DAY_MS, DEFAULT_DELAY_NIGHT_MS);
    printf("           Auto Exposure: %s / %s\n", yesNo(DEFAULT_EXPOSURE_DAY_AUTO), yesNo(DEFAULT_EXPOSURE_NIGHT_AUTO));
    printf("                Exposure: %'1.3fms / %'1.0fms\n", (float)DEFAULT_EXPOSURE_DAY_US / US_IN_MS,
           round(DEFAULT_EXPOSURE_NIGHT_US / US_IN_MS));
    printf("            Max Exposure: n/a  / %'dms\n", DEFAULT_EXPOSURE_NIGHT_MAX_US / US_IN_MS);
    printf("               Auto Gain: n/a  / %s\n", yesNo(DEFAULT_GAIN_NIGHT_AUTO));
    printf("                    Gain: n/a  / %d\n", DEFAULT_GAIN_NIGHT);
    printf("                Max Gain: n/a  / %d\n", DEFAULT_GAIN_NIGHT_MAX);
    printf("       Brightness Target: %d / %d\n  -- Used by Auto-Exposure / Auto-Gain", DEFAULT_BRIGHTNESS_TARGET,
           DEFAULT_BRIGHTNESS_TARGET);
    printf("    Gain Transition Time: %'d seconds\n", DEFAULT_GAIN_TRANSITION_TIME);
    printf("     ^^ Only used with manual gain when the gain setting differs day/night\n");

    printf("\n\nOverlay Settings:\n");
    printf("=================\n");
    printf("      Dark Frame Capture: %s  -- Yes = disables overlays\n", yesNo(0));
    printf("            Text Overlay: %s\n", "[none]");
    printf("     Text Extra Filename: %s\n", "[none]");
    printf(" Text Extra Filename Age: %d seconds\n", 0);
    printf("        Text Line Height: %dpx\n", DEFAULT_TEXT_LINE_HEIGHT_PX);
    printf("           Text Position: %dpx left, %dpx top\n", DEFAULT_TEXT_OFFSET_FROM_LEFT_PX,
           DEFAULT_TEXT_OFFSET_FROM_TOP_PX);
    printf("               Font Name: %d (%s)\n", font_numbers[DEFAULT_FONT_NUMBER], font_names[DEFAULT_FONT_NUMBER]);
    printf("              Font Color: %d, %d, %d (R, G, B)\n", font_color[0], font_color[1], font_color[2]);
    printf("        Small Font Color: %d, %d, %d (R, G, B)\n", font_color_small[0], font_color_small[1], font_color_small[2]);
    printf("          Font Smoothing: %d\n", font_smoothing_options[DEFAULT_FONT_SMOOTHING]);
    printf("               Font Size: %1.1f\n", DEFAULT_FONT_SIZE);
    printf("          Font Thickness: %d\n", 1);
    printf("            Font Outline: %s\n", yesNo(DEFAULT_FONT_OUTLINE));

#ifdef USE_HISTOGRAM
    printf("      Show Histogram Box: %s  -- Outline around box used for Auto-Exposure calculation\n", yesNo(0));
    printf("     Show Histogram Mean: %s\n", yesNo(0));
#endif
    printf("               Show Time: %s (format: %s)\n", yesNo(DEFAULT_TIME_SHOW), DEFAULT_TIME_FORMAT);
    printf("        Show Temperature: %s\n", yesNo(0));
    printf("        Temperature Unit: %s\n", "C");
    printf("           Show Exposure: %s\n", yesNo(0));
    printf("               Show Gain: %s\n", yesNo(0));
    printf("         Show Brightness: %s\n", yesNo(0));
    printf("                 Preview: %s\n", yesNo(0));
    printf("%s\n", KNRM);
    printf("%sUsage:\n", KRED);
    printf(" ./capture -width 640 -height 480 -nightexposure 5000000 -gamma 50 -type 1 -nightbin 1 -filename "
           "Lake-Laberge.PNG\n\n");
}