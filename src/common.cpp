#include "common.h"

//-------------------------------------------------------------------------------------------------------
char debugText[500];		// buffer to hold debug messages displayed by displayDebugText()
std::string dayOrNight;
//-------------------------------------------------------------------------------------------------------



// Return the numeric time.
timeval getTimeval()
{
    timeval curTime;
    gettimeofday(&curTime, NULL);
    return(curTime);
}

// Format a numeric time as a string.
char *formatTime(timeval t, char const *tf)
{
    static char TimeString[128];
    strftime(TimeString, 80, tf, localtime(&t.tv_sec));
    return(TimeString);
}

// Return the current time as a string.  Uses both functions above.
char *getTime(char const *tf)
{
    return(formatTime(getTimeval(), tf));
}

double timeDiff(int64 start, int64 end)
{
	double frequency = cv::getTickFrequency();
	return (double)(end - start) / frequency;	// in Microseconds
}

std::string ReplaceAll(std::string str, const std::string& from, const std::string& to)
{
	size_t start_pos = 0;
	while((start_pos = str.find(from, start_pos)) != std::string::npos) {
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
    int w=para->cols;
    int h=para->rows;
    
    cv::Mat pImg(h,w,(int)para->type(),(uchar *)para->data);
    cv::namedWindow("video", 1);
    while (bDisplay)
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
    int a = (n / roundTo) * roundTo;	// Smaller multiple
    int b = a + roundTo;		// Larger multiple
    return (n - a > b - n)? b : a;	// Return of closest of two
}

void IntHandle(int i)
{
    gotSignal = 1;
    closeUp(0);
}

void displayDebugText(const char * text, int requiredLevel) {
    if (debugLevel >= requiredLevel) {
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
    if (notificationImages)
    {
        system("scripts/copy_notification_image.sh Error &");
    }
    sleep(5);	// give time for image to be copied
    printf("*** Sleeping until you fix the problem.\n");
    printf("**********\n");
    sleep(100000);	// basically, sleep forever until the user fixes this.
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
    char sunwaitCommand[128];	// returns "hh:mm, hh:mm" (sunrise, sunset)
    sprintf(sunwaitCommand, "sunwait list angle %s %s %s | awk '{print $2}'", angle, latitude, longitude);
    t = exec(sunwaitCommand);
    t.erase(std::remove(t.begin(), t.end(), '\n'), t.end());

    int h=0, m=0, secs;
    sscanf(t.c_str(), "%d:%d", &h, &m);
    secs = (h*60*60) + (m*60);

    char *now = getTime("%H:%M");
    int hNow=0, mNow=0, secsNow;
    sscanf(now, "%d:%d", &hNow, &mNow);
    secsNow = (hNow*60*60) + (mNow*60);

    // Handle the (probably rare) case where nighttime is tomorrow
    if (secsNow > secs)
    {
        return(secs + (60*60*24) - secsNow);
    }
    else
    {
        return(secs - secsNow);
    }
}

// Simple function to make flags easier to read for humans.
char const *yesNo(int flag)
{
    if (flag)
    {
        return("yes");
    }
    else
    {
        return("no");
    }
}

void printCredits()
{
    setlinebuf(stdout);   // Line buffer output so entries appear in the log immediately.
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
}

void printHelp()
{
    printf("%sAvailable Arguments:\n", KYEL);
    printf(" -width                             - Default = %d = Camera Max Width\n", DEFAULT_WIDTH);
    printf(" -height                            - Default = %d = Camera Max Height\n", DEFAULT_HEIGHT);
    printf(" -daytime                           - Default = %d - Set to 1 to enable daytime images\n", DEFAULT_DAYTIMECAPTURE);
    printf(" -dayexposure                       - Default = %'d - Time in us (equals to %.4f sec)\n", DEFAULT_ASIDAYEXPOSURE, (float)DEFAULT_ASIDAYEXPOSURE/US_IN_SEC);
    printf(" -nightexposure                     - Default = %'d - Time in us (equals to %.4f sec)\n", DEFAULT_ASINIGHTEXPOSURE, (float)DEFAULT_ASINIGHTEXPOSURE/US_IN_SEC);
    printf(" -nightmaxexposure                  - Default = %'d - Time in ms (equals to %.1f sec)\n", DEFAULT_ASINIGHTMAXEXPOSURE, (float)DEFAULT_ASINIGHTMAXEXPOSURE/US_IN_MS);

    printf(" -dayautoexposure                   - Default = %d - Set to 1 to enable daytime auto Exposure\n", DEFAULT_DAYAUTOEXPOSURE);
    printf(" -nightautoexposure                 - Default = %d - Set to 1 to enable nighttime auto Exposure\n", DEFAULT_NIGHTAUTOEXPOSURE);
    printf(" -nightgain                         - Default = %d\n", DEFAULT_ASINIGHTGAIN);
    printf(" -nightmaxgain                      - Default = %d\n", DEFAULT_ASINIGHTMAXGAIN);
    printf(" -nightautogain                     - Default = %d - Set to 1 to enable nighttime auto gain\n", DEFAULT_NIGHTAUTOGAIN);
    printf(" -gaintransitiontime                - Default = %'d - Seconds to transition gain from day-to-night or night-to-day.  Set to 0 to disable\n", DEFAULT_GAIN_TRANSITION_TIME);
    printf(" -coolerEnabled                     - Set to 1 to enable cooler (works on cooled cameras only)\n");
    printf(" -targetTemp                        - Target temperature in degrees C (works on cooled cameras only)\n");
    printf(" -gamma                             - Default = %d\n", DEFAULT_ASIGAMMA);
    printf(" -daybrightness                     - Default = %d (range: 0 - 600)\n", DEFAULT_BRIGHTNESS);
    printf(" -nightbrightness                   - Default = %d (range: 0 - 600)\n", DEFAULT_BRIGHTNESS);
    printf(" -wbr                               - Default = %d   - manual White Balance Red\n", DEFAULT_ASIWBR);
    printf(" -wbb                               - Default = %d   - manual White Balance Blue\n", DEFAULT_ASIWBB);
    printf(" -autowhitebalance                  - Default = %d - Set to 1 to enable auto White Balance\n", DEFAULT_AUTOWHITEBALANCE);
    printf(" -daybin                            - Default = %d    - 1 = binning OFF (1x1), 2 = 2x2 binning, 4 = 4x4 binning\n", DEFAULT_DAYBIN);
    printf(" -nightbin                          - Default = %d    - 1 = binning OFF (1x1), 2 = 2x2 binning, 4 = 4x4 binning\n", DEFAULT_NIGHTBIN);
    printf(" -dayDelay                          - Default = %'d   - Delay between daytime images in milliseconds - 5000 = 5 sec.\n", DEFAULT_DAYDELAY);
    printf(" -nightDelay                        - Default = %'d   - Delay between night images in milliseconds - %d = 1 sec.\n", DEFAULT_NIGHTDELAY, MS_IN_SEC);
    printf(" -type = Image Type                 - Default = %d    - 0 = RAW8,  1 = RGB24,  2 = RAW16,  3 = Y8\n", DEFAULT_IMAGE_TYPE);
    printf(" -quality                           - Default PNG=3, JPG=95, Values: PNG=0-9, JPG=0-100\n");
    printf(" -usb = USB Speed                   - Default = %d   - Values between 40-100, This is "
            "BandwidthOverload\n", DEFAULT_ASIBANDWIDTH);
    printf(" -autousb                           - Default = 0 - Set to 1 to enable auto USB Speed\n");
    printf(" -filename                          - Default = %s\n", DEFAULT_FILENAME);
    printf(" -flip                              - Default = 0    - 0 = Orig, 1 = Horiz, 2 = Verti, 3 = Both\n");
    printf("\n");
    printf(" -text                              - Default = \"\"   - Character/Text Overlay\n");
    printf(" -extratext                         - Default = \"\"   - Full Path to extra text to display\n");
    printf(" -extratextage                      - Default = 0  - If the extra file is not updated after this many seconds its contents will not be displayed. Set to 0 to disable\n");
    printf(" -textlineheight                    - Default = %d   - Text Line Height in Pixels\n", DEFAULT_ITEXTLINEHEIGHT);
    printf(" -textx = Text X                    - Default = %d   - Text Placement Horizontal from LEFT in pixels\n", DEFAULT_ITEXTX);
    printf(" -texty = Text Y                    - Default = %d   - Text Placement Vertical from TOP in pixels\n", DEFAULT_ITEXTY);
    printf(" -fontname = Font Name              - Default = %d   - Font Types (0-7), Ex. 0 = simplex, 4 = triplex, 7 = script\n", DEFAULT_FONTNUMBER);
    printf(" -fontcolor = Font Color            - Default = 255 0 0  - Text blue (BGR)\n");
    printf(" -smallfontcolor = Small Font Color - Default = 0 0 255  - Text red (BGR)\n");
    printf(" -fonttype = Font Type              - Default = %d    - Font Line Type,(0-2), 0 = AA, 1 = 8, 2 = 4\n", DEFAULT_LINENUMBER);
    printf(" -fontsize                          - Default = %d    - Text Font Size\n", DEFAULT_FONTSIZE);
    printf(" -fontline                          - Default = %d    - Text Font Line Thickness\n", DEFAULT_LINEWIDTH);
    printf(" -outlinefont                       - Default = %d    - TSet to 1 to enable outline font\n", DEFAULT_OUTLINEFONT);
    //printf(" -bgc = BG Color                    - Default =      - Text Background Color in Hex. 00ff00 = Green\n");
    //printf(" -bga = BG Alpha                    - Default =      - Text Background Color Alpha/Transparency 0-100\n");
    printf("\n");
    printf("\n");
    printf(" -latitude                          - Default = %7s (Whitehorse) - Latitude of the camera.\n", DEFAULT_LATITUDE);
    printf(" -longitude                         - Default = %7s (Whitehorse) - Longitude of the camera\n", DEFAULT_LONGITUDE);
    printf(" -angle                             - Default = %s - Angle of the sun below the horizon.\n", DEFAULT_ANGLE);
    printf("   -6=civil twilight\n   -12=nautical twilight\n   -18=astronomical twilight\n");
    printf("\n");
    printf(" -locale                            - Default = %s - Your locale, used to determine your thousands separator and decimal point. If you don't know it, type 'locale' at a command prompt.\n", DEFAULT_LOCALE);
    printf(" -notificationimages                - Set to 1 to enable notification images, for example, 'Camera is off during day'.\n");
#ifdef USE_HISTOGRAM
    printf(" -histogrambox                      - Default = %d %d %0.2f %0.2f (box width X, box width y, X offset percent (0-100), Y offset (0-100)\n", DEFAULT_BOX_SIZEX, DEFAULT_BOX_SIZEY, DEFAULT_BOX_FROM_LEFT * 100, DEFAULT_BOX_FROM_TOP * 100);
    printf(" -showhistogrambox                  - Set to 1 to view an outline of the histogram box. Useful to help determine what parameters to use with -histogrambox.\n");
#endif
    printf(" -darkframe                         - Set to 1 to disable time and text overlay and take dark frames instead.\n");
    printf(" -preview                           - Set to 1 to preview the captured images. Only works with a Desktop Environment\n");
    printf(" -time                              - Set to 1 to add the time to the image. Combine with Text X and Text Y for placement\n");
    printf(" -timeformat                        - Format the optional time is displayed in; default is '%s'\n", DEFAULT_TIMEFORMAT);
    printf(" -showDetails (obsolete)            - Set to 1 to display sensor temp, exposure length, and gain metadata on the image.\n");
    printf(" -showTemp                          - Set to 1 to display the camera sensor temperature on the image.\n");
    printf(" -temptype                          - How to display temperature: 'C'elsius, 'F'ahrenheit, or 'B'oth.\n");
    printf(" -showExposure                      - Set to 1 to display the exposure length on the image.\n");
    printf(" -showGain                          - Set to 1 to display the gain on the image.\n");
    printf(" -showBrightness                    - Set to 1 to display the brightness on the image, if not the default.\n");
#ifdef USE_HISTOGRAM
    printf(" -showHistogram                     - Set to 1 to display the histogram mean on the image.\n");
#endif
    printf(" -debugLevel                        - Default = 0. Set to 1,2 or 3 for more debugging information.\n");
    printf("%sUsage:\n", KRED);
    printf(" ./capture -width 640 -height 480 -nightexposure 5000000 -gamma 50 -type 1 -nightbin 1 -filename Lake-Laberge.PNG\n\n");
}