#include "config.h"
#include "common.h"
#include "capture_RPiHQ.h"

//#include <sys/types.h>
#include <iomanip>
#include <cstring>
#include <sstream>
//include <iostream>
//#include <cstdio>

using namespace std;

#define KNRM "\x1B[0m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"
#define KYEL "\x1B[33m"
#define KBLU "\x1B[34m"
#define KMAG "\x1B[35m"
#define KCYN "\x1B[36m"
#define KWHT "\x1B[37m"

#define US_IN_MS 1000                     // microseconds in a millisecond
#define MS_IN_SEC 1000                    // milliseconds in a second
#define US_IN_SEC (US_IN_MS * MS_IN_SEC)  // microseconds in a second

//-------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------

//char nameCnt[128];
char const *image_file_name = "image.jpg";
bool b_main = true;
//ol b_display = false;
std::string day_or_night;
int exposures_counter = 0;	// how many valid pictures have we taken so far?

// Some command-line and other option definitions needed outside of main():
int tty				= 0;	// 1 if we're on a tty (i.e., called from the shell prompt).
#define NOT_SET			  -1	// signifies something isn't set yet
#define DEFAULT_NOTIFICATION_IMAGES 1
int notification_images		= DEFAULT_NOTIFICATION_IMAGES;

//bool b_saving_image = false;

//-------------------------------------------------------------------------------------------------------
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

std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
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
/*
void *Display(void *params)
{
	cv::Mat *pImg = (cv::Mat *)params;
	cvNamedWindow("video", 1);
	while (b_display)
	{
		cvShowImage("video", pImg);
		cvWaitKey(100);
	}
	cvDestroyWindow("video");
	printf("Display thread over\n");
	return (void *)0;
}
*/

// Exit the program gracefully.
void closeUp(int e)
{
	static int closing_up = 0;		// indicates if we're in the process of exiting.
	// For whatever reason, we're sometimes called twice, but we should only execute once.
	if (closing_up) return;

	closing_up = 1;

	// If we're not on a tty assume we were started by the service.
	// Unfortunately we don't know if the service is stopping us, or restarting us.
	// If it was restarting there'd be no reason to copy a notification image since it
	// will soon be overwritten.  Since we don't know, always copy it.
	if (notification_images) {
		system("scripts/copy_notification_image.sh NotRunning &");
		// Sleep to give it a chance to print any messages so they (hopefully) get printed
		// before the one below.  This is only so it looks nicer in the log file.
		sleep(3);
	}

	printf("     ***** Stopping AllSky *****\n");
	exit(e);
}

// Build capture command to capture the image from the HQ camera
void RPiHQcapture(int camera_auto_focus, int asiAutoExposure, int camrea_exposure, int asiAutoGain, int white_balance_auto, double camrea_gain, int bin, double white_balance_red, double white_balance_blue, int image_rotation, int image_flip, int gamma, int image_brightness, int quality, const char* image_file_name, int time, int showDetails, const char* image_text, int font_size, int font_color, int background, int darkframe)
{
	sprintf(debugText, "capturing image in file %s\n", image_file_name);
	displayDebugText(debugText, 3);

	// Ensure no rraspistill process is still running
	string kill = "ps -ef|grep raspistill| grep -v color|awk '{print $2}'|xargs kill -9 1> /dev/null 2>&1";

	// Define char variable
	char kcmd[kill.length() + 1];

	// Convert command to character variable
	strcpy(kcmd, kill.c_str());

	sprintf(debugText, "Command: %s\n", kcmd);
	displayDebugText(debugText, 3);

	// Execute raspistill command
	system(kcmd);

	stringstream ss;

/*
	static char time_buffer[40];
	const struct std::tm *tm_ptr;
	std::time_t now;

	now = std::
time ( NULL );
	tm_ptr = std::localtime ( &now );

	std::strftime ( time_buffer, 40, "%d %B %Y %I:%M:%S %p", tm_ptr );
*/

	ss << image_file_name;

	// Define strings for raspistill command string and
	string command = "nice raspistill --nopreview --thumb none --output " + ss.str() + " --burst -st ";

	// Define strings for roi (used for binning) string
	string roi;

	if (bin > 3)
	{
		bin = 3;
	}

	if (bin < 1)
	{
		bin = 1;
	}

	// Check for binning 1x1 is selected
	if (bin==1)
	{
		// Select binning 1x1 (4060 x 3056 pixels)
		roi = "--mode 3 ";
	}

	// Check if binning 2x2 is selected
	else if (bin==2)
	{
		// Select binning 2x2 (2028 x 1520 pixels)
		roi = "--mode 2  --width 2028 --height 1520 ";
	}

	// Check if binning 3x3 is selected
	else
	{
		// Select binning 4x4 (1012 x 760 pixels)
		roi = "--mode 4 --width 1012 --height 760 ";
	}

	// Append binning window
	command += roi;

	if (camrea_exposure < 1)
	{
		camrea_exposure = 1;
	}

	if (camrea_exposure > 240000000)
	{
		camrea_exposure = 240000000;
	}

	// Exposure time
	string shutter;

	// Check if automatic determined exposure time is selected

	if (asiAutoExposure)
	{
		shutter = "--exposure auto ";
	}

	// Set exposure time
	else if (camrea_exposure)
	{
		ss.str("");
		ss << camrea_exposure;
		shutter = "--exposure off --shutter " + ss.str() + " ";
	}

	// Add exposure time setting to raspistill command string
	command += shutter;

	if (camera_auto_focus)
	{
		command += "--focus ";
	}

	// Anolog Gain
	string gain;

	// Check if auto gain is selected
	if (asiAutoGain)
	{
		// Set analog gain to 1
		gain = "--analoggain 1 ";
	}

	// Set manual analog gain setting
	else if (camrea_gain) {
		if (camrea_gain < 1)
		{
			camrea_gain = 1;
		}

		if (camrea_gain > 16)
		{
			camrea_gain = 16;
		}

		ss.str("");
		ss << camrea_gain;
		gain = "--analoggain " + ss.str() + " ";
	}

	// Add gain setting to raspistill command string
	command += gain;

	// White balance
	string awb;

	// Check if R and B component are given
	if (!white_balance_auto) {
		if (white_balance_red < 0.1)
		{
			white_balance_red = 0.1;
		}

		if (white_balance_red > 10)
		{
			white_balance_red = 10;
		}

		if (white_balance_blue < 0.1)
		{
			white_balance_blue = 0.1;
		}

		if (white_balance_blue > 10)
		{
			white_balance_blue = 10;
		}

		ss.str("");
		ss << white_balance_red;
		awb  = "--awb off --awbgains " + ss.str();

		ss.str("");
		ss << white_balance_blue;
		awb += "," + ss.str() + " ";
	}

	// Use automatic white balance
	else
	{
		awb = "--awb auto ";
	}

	// Add white balance setting to raspistill command string
	command += awb;

	// Check if rotation is at least 0 degrees
	if (image_rotation != 0 && image_rotation != 90 && image_rotation != 180 && image_rotation != 270)
	{
		// Set rotation to 0 degrees
		image_rotation = 0;
	}

	// check if rotation is needed
	if (image_rotation!=0) {
		ss.str("");
		ss << image_rotation;

		// Add white balance setting to raspistill command string
		command += "--rotation "  + ss.str() + " ";
	}

	// Flip image
	string flip = "";

	// Check if flip is selected
	if (image_flip == 1 || image_flip == 3)
	{
		// Set horizontal flip
		flip += "--hflip ";
	}
	if (image_flip == 2 || image_flip == 3)
	{
		// Set vertical flip
		flip += "--vflip ";
	}

	// Add flip info to raspistill command string
	command += flip;

	//Gamma correction (saturation)
	string saturation;

	// Check if gamma correction is set
	if (gamma < -100)
	{
		gamma = -100;
	}

	if (gamma > 100)
	{
		gamma = 100;
	}

	if (gamma)
	{
		ss.str("");
		ss << gamma;
		saturation = "--saturation "+ ss.str() + " ";
	}

	// Add gamma correction info to raspistill command string
	command += saturation;

	// Brightness
	string brightness;

	if (image_brightness < 0)
	{
		image_brightness = 0;
	}

	if (image_brightness > 100)
	{
		image_brightness = 100;
	}

	// check if brightness setting is set
	if (image_brightness!=50)
	{
		ss.str("");
		ss << image_brightness;
		brightness = "--brightness " + ss.str() + " ";
	}

	// Add brightness info to raspistill command string
	command += brightness;

	// Quality
	string squality;

	if (quality < 0)
	{
		quality = 0;
	}

	if (quality > 100)
	{
		quality = 100;
	}

	ss.str("");
	ss << quality;
	squality = "--quality " + ss.str() + " ";

	// Add image quality info to raspistill command string
	command += squality;

	if (!darkframe) {
		if (showDetails)
			command += "-a 1104 ";

		if (time==1)
			command += "-a 1036 ";

		if (strcmp(image_text, "") != 0) {
			ss.str("");
	//		ss << ReplaceAll(image_text, std::string(" "), std::string("_"));
			ss << image_text;
			command += "-a \"" + ss.str() + "\" ";
		}

		if (font_size < 6)
			font_size = 6;

		if (font_size > 160)
			font_size = 160;

		ss.str("");
		ss << font_size;

		if (font_color < 0)
			font_color = 0;

		if (font_color > 255)
			font_color = 255;

		std::stringstream C;
		C  << std::setfill ('0') << std::setw(2) << std::hex << font_color;

		if (background < 0)
			background = 0;

		if (background > 255)
			background = 255;

		std::stringstream B;
		B  << std::setfill ('0') << std::setw(2) << std::hex << background;

		command += "-ae " + ss.str() + ",0x" + C.str() + ",0x8080" + B.str() + " ";
	}

	// Define char variable
	char cmd[command.length() + 1];

	// Convert command to character variable
	strcpy(cmd, command.c_str());

	sprintf(debugText, "Capture command: %s\n", cmd);
	displayDebugText(debugText, 1);

	// Execute raspistill command
	if (system(cmd) == 0) exposures_counter++;
}

// Simple function to make flags easier to read for humans.
char const *yes = "1 (yes)";
char const *no  = "0 (no)";
char const *yesNo(int flag)
{
    if (flag)
        return(yes);
    else
        return(no);
}


//-------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
	signal(SIGINT, IntHandle);
	signal(SIGTERM, IntHandle);	// The service sends SIGTERM to end this program.
/*
	int font_numbers[] = { CV_FONT_HERSHEY_SIMPLEX,        CV_FONT_HERSHEY_PLAIN,         CV_FONT_HERSHEY_DUPLEX,
					   CV_FONT_HERSHEY_COMPLEX,        CV_FONT_HERSHEY_TRIPLEX,       CV_FONT_HERSHEY_COMPLEX_SMALL,
					   CV_FONT_HERSHEY_SCRIPT_SIMPLEX, CV_FONT_HERSHEY_SCRIPT_COMPLEX };
	int font_number = 0;
	int iStrLen;
	int text_offset_from_left_px = 15, text_offset_from_top_px = 25;
*/
	char const *image_text   = "";
	char const *param     = "";
	double font_size       = 32;
/*
	int font_weight         = 1;
	int font_outline       = 0;
*/
	int font_color         = 255;
	int background        = 0;
/*
	int font_color_small[3] = { 0, 0, 255 };
	int font_smoothing_options[3]       = { CV_AA, 8, 4 };
	int font_smoothing        = 0;
	char buf[1024]    = { 0 };
	char time_buffer[128] = { 0 };
	char tmp_buffer[128] = { 0 };
*/
	int width             = 0;
	int height            = 0;
	int binning_day            = 1;
	int binning_night          = 2;
	int binning_current        = NOT_SET;
	int exposure_day_us    = 32;	// milliseconds
	int exposure_night_us  = 60000000;
	int exposure_current_value   = NOT_SET;
	int exposure_night_auto = 0;
	int exposure_day_auto= 1;
	int exposure_auto_current_value = 0;
	int camera_auto_focus      = 0;
	double gain_night   = 4;
	double gain_day     = 1;
	double gain_current_value    = NOT_SET;
	int gain_night_auto  = 0;
	int gain_day_auto    = 0;
	int gain_auto_current_value   = NOT_SET;
	int white_balance_auto        = 0;
	int delay_night_ms        = 10;   // Delay in milliseconds. Default is 10ms
	int delay_day_ms          = 15000; // Delay in milliseconds. Default is 15 seconds
	int delay_current      = NOT_SET;
	double white_balance_red         = 2.5;
	double white_balance_blue         = 2;
	int gamma          = 50;
	int brightness_target_day  = 50;
	int brightness_target_night= 50;
	int brightness_current = NOT_SET;
	int image_flip           = 0;
	int image_rotation       = 0;
	char const *latitude  = "52.57N"; //GPS Coordinates of Limmen, Netherlands where this code was altered
	char const *longitude = "4.70E";
	char const *angle     = "0"; // angle of the sun with the horizon (0=sunset, -6=civil twilight, -12=nautical twilight, -18=astronomical twilight)
	//int preview           = 0;
	int time              = 0;
	int showDetails       = 0;
	int darkframe         = 0;
	int capture_daytime    = 0;
	int help              = 0;
	int quality           = 90;

	int i;
	//id *retval;
	bool end_of_night    = false;
	//hread_t hthdSave = 0;

	//-------------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------------
	setlinebuf(stdout);   // Line buffer output so entries appear in the log immediately.
	printf("\n");
	printf("%s ******************************************\n", KGRN);
	printf("%s *** Allsky Camera Software v0.8 | 2021 ***\n", KGRN);
	printf("%s ******************************************\n\n", KGRN);
	printf("\%sCapture images of the sky with a Raspberry Pi and an ZWO ASI or RPi HQ camera\n", KGRN);
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
	printf("-Rob Musquetier\n\n");

	// The newer "allsky.sh" puts quotes around arguments so we can have spaces in them.
	// If you are running the old allsky.sh, set this to false:
	bool argumentsQuoted = true;

	if (argc > 0)
	{
		sprintf(debugText, "Found %d parameters...\n", argc - 1);
		displayDebugText(debugText, 3);

		// -h[elp] doesn't take an argument, but the "for" loop assumes every option does,
       		// so check separately, assuming the option is the first one.
		// If it's not the first option, we'll find it in the "for" loop.
		if (strcmp(argv[0], "-h") == 0 || strcmp(argv[0], "-help") == 0)
		{
			help = 1;
			i = 1;
		}
		else
		{
			i = 0;
		}

		for (i = 0; i < argc - 1; i++)
		{
			sprintf(debugText, "Processing argument: %s\n\n", argv[i]);
			displayDebugText(debugText, 3);

			// Check again in case "-h" isn't the first option.
			if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-help") == 0)
			{
				help = 1;
			}
			else if (strcmp(argv[i], "-width") == 0)
			{
				width = atoi(argv[++i]);
			}
			else if (strcmp(argv[i], "-height") == 0)
			{
				height = atoi(argv[++i]);
			}
/*
			else if (strcmp(argv[i], "-type") == 0)
			{
				image_type = atoi(argv[++i]);
			}
*/
			else if (strcmp(argv[i], "-quality") == 0)
			{
				quality = atoi(argv[++i]);
			}
			// check for old names as well - the "||" part is the old name
			else if (strcmp(argv[i], "-nightexposure") == 0 || strcmp(argv[i], "-exposure") == 0)
			{
				exposure_night_us = atoi(argv[++i]) * US_IN_MS;
			}

			else if (strcmp(argv[i], "-nightautoexposure") == 0 || strcmp(argv[i], "-autoexposure") == 0)
			{
				exposure_night_auto = atoi(argv[++i]);
			}

			else if (strcmp(argv[i], "-autofocus") == 0)
			{
				camera_auto_focus = atoi(argv[++i]);
			}
			// xxxx Day gain isn't settable by the user.  Should it be?
			else if (strcmp(argv[i], "-nightgain") == 0 || strcmp(argv[i], "-gain") == 0)
			{
				gain_night = atof(argv[++i]);
			}
			else if (strcmp(argv[i], "-nightautogain") == 0 || strcmp(argv[i], "-autogain") == 0)
			{
				gain_night_auto = atoi(argv[++i]);
			}
			else if (strcmp(argv[i], "-gamma") == 0)
			{
				gamma = atoi(argv[++i]);
			}
			else if (strcmp(argv[i], "-brightness") == 0)// old "-brightness" applied to day and night
			{
				brightness_target_day = atoi(argv[++i]);
				brightness_target_night = brightness_target_day;
			}
			else if (strcmp(argv[i], "-daybrightness") == 0)
			{
				brightness_target_day = atoi(argv[++i]);
			}
			else if (strcmp(argv[i], "-nightbrightness") == 0)
			{
				brightness_target_night = atoi(argv[++i]);
			}
 			else if (strcmp(argv[i], "-daybin") == 0)
            		{
                		binning_day = atoi(argv[++i]);
            		}
			else if (strcmp(argv[i], "-nightbin") == 0 || strcmp(argv[i], "-bin") == 0)
			{
				binning_night = atoi(argv[++i]);
			}
			else if (strcmp(argv[i], "-nightdelay") == 0 || strcmp(argv[i], "-delay") == 0)
			{
				delay_night_ms = atoi(argv[++i]);
			}
			else if (strcmp(argv[i], "-daydelay") == 0 || strcmp(argv[i], "-daytimeDelay") == 0)
			{
				delay_day_ms = atoi(argv[++i]);
			}
			else if (strcmp(argv[i], "-awb") == 0)
			{
				white_balance_auto = atoi(argv[++i]);
			}
			else if (strcmp(argv[i], "-wbr") == 0)
			{
				white_balance_red = atof(argv[++i]);
			}
			else if (strcmp(argv[i], "-wbb") == 0)
			{
				white_balance_blue = atof(argv[++i]);
			}

			// Check for text parameter
			else if (strcmp(argv[i], "-text") == 0)
			{
				if (argumentsQuoted)
				{
					image_text = argv[++i];
				}
				else
				{
					// Get first param
					param = argv[i + 1];

					// Space character
					const char *space = " ";

					// Temporary text buffer
					char buffer[1024]; // <- danger, only storage for 1024 characters.

					// First word flag
					int j = 0;

					// Loop while next parameter doesn't start with a - character
					while (strncmp(param, "-", 1) != 0)
					{
						// Copy Text into buffer
						strncpy(buffer, image_text, sizeof(buffer));

						// Add a space after each word (skip for first word)
						if (j)
							strncat(buffer, space, sizeof(buffer));

						// Add parameter
						strncat(buffer, param, sizeof(buffer));

						// Copy buffer into image_text variable
						image_text = buffer;

						// Flag first word is entered
						j = 1;

						// Get next parameter
						param = argv[++i];
					}
				}
			}
/*
			else if (strcmp(argv[i], "-textx") == 0)
			{
				text_offset_from_left_px = atoi(argv[++i]);
			}
			else if (strcmp(argv[i], "-texty") == 0)
			{
				text_offset_from_top_px = atoi(argv[++i]);
			}
			else if (strcmp(argv[i], "-font_numbers") == 0)
			{
				font_number = atoi(argv[++i]);
			}
*/
			else if (strcmp(argv[i], "-background") == 0)
			{
				background = atoi(argv[++i]);
			}
			else if (strcmp(argv[i], "-font_color") == 0)
			{
				font_color = atoi(argv[++i]);
			}
/*
			else if (strcmp(argv[i], "-smallfont_color") == 0)
			{
				if (argumentsQuoted)
				{
					sscanf(argv[++i], "%d %d %d", &font_color_small[0], &font_color_small[1], &font_color_small[2]);
				}
				else
				{
					font_color_small[0] = atoi(argv[++i]);
					font_color_small[1] = atoi(argv[++i]);
					font_color_small[2] = atoi(argv[++i]);
				}
			}
			else if (strcmp(argv[i], "-fonttype") == 0)
			{
				font_smoothing = atoi(argv[++i]);
			}
*/
			else if (strcmp(argv[i], "-font_size") == 0)
			{
				font_size = atof(argv[++i]);
			}
/*
			else if (strcmp(argv[i], "-fontline") == 0)
			{
				font_weight = atoi(argv[++i]);
			}
			else if (strcmp(argv[i], "-font_outline") == 0)
			{
				font_outline = atoi(argv[++i]);
				if (font_outline != 0)
					font_outline = 1;
			}
*/
			else if (strcmp(argv[i], "-rotation") == 0)
			{
				image_rotation = atoi(argv[++i]);
			}
			else if (strcmp(argv[i], "-flip") == 0)
			{
				image_flip = atoi(argv[++i]);
			}
			else if (strcmp(argv[i], "-filename") == 0)
			{
				image_file_name = (argv[++i]);
			}
			else if (strcmp(argv[i], "-latitude") == 0)
			{
				latitude = argv[++i];
			}
			else if (strcmp(argv[i], "-longitude") == 0)
			{
				longitude = argv[++i];
			}
			else if (strcmp(argv[i], "-angle") == 0)
			{
				angle = argv[++i];
			}
/*
			else if (strcmp(argv[i], "-preview") == 0)
			{
				preview = atoi(argv[++i]);
			}
*/
			else if (strcmp(argv[i], "-time_show") == 0 || strcmp(argv[i], "-time") == 0)
			{
				time = atoi(argv[++i]);
			}

			else if (strcmp(argv[i], "-darkframe") == 0)
			{
				darkframe = atoi(argv[++i]);
			}
			else if (strcmp(argv[i], "-showDetails") == 0)
			{
				showDetails = atoi(argv[++i]);
			}
			else if (strcmp(argv[i], "-daytime") == 0)
			{
				capture_daytime = atoi(argv[++i]);
			}
			else if (strcmp(argv[i], "-notificationimages") == 0)
			{
				notification_images = atoi(argv[++i]);
			}
			else if (strcmp(argv[i], "-tty") == 0)
			{
				tty = atoi(argv[++i]);
			}
		}
	}

	if (help == 1)
	{
		printf("%sAvailable Arguments:\n", KYEL);
		printf(" -width                             - Default = Camera Max Width\n");
		printf(" -height                            - Default = Camera Max Height\n");
		printf(" -nightexposure                     - Default = 5000000 - Time in us (equals to 5 sec)\n");
		printf(" -nightautoexposure                 - Default = 0 - Set to 1 to enable auto Exposure\n");
		printf(" -autofocus                         - Default = 0 - Set to 1 to enable auto Focus\n");
		printf(" -nightgain                         - Default = 1 (1 - 16)\n");
		printf(" -nightautogain                     - Default = 0 - Set to 1 to enable auto Gain at night\n");
		printf(" -gamma                             - Default = 50 (-100 till 100)\n");
		printf(" -brightness                        - Default = 50 (0 till 100)\n");
		printf(" -awb                               - Default = 0 - Auto White Balance (0 = off)\n");
		printf(" -wbr                               - Default = 2 - White Balance Red  (0 = off)\n");
		printf(" -wbb                               - Default = 2 - White Balance Blue (0 = off)\n");
		printf(" -daybin                            - Default = 1 - binning OFF (1x1), 2 = 2x2, 3 = 3x3 binning\n");
		printf(" -nightbin                          - Default = 1 - same as -daybin but for nighttime\n");
		printf(" -nightdelay                        - Default = 10 - Delay between images in milliseconds - %d = 1 sec.\n", MS_IN_SEC);
		printf(" -daydelay                          - Default = 5000 - Delay between images in milliseconds - 5000 = 5 sec.\n");
		printf(" -type = Image Type                 - Default = 0 - 0 = RAW8,  1 = RGB24,  2 = RAW16\n");
		printf(" -quality                           - Default = 70%%, 0%% (poor) 100%% (perfect)\n");
		printf(" -filename                          - Default = image.jpg\n");
		printf(" -rotation                          - Default = 0 degrees - Options 0, 90, 180 or 270\n");
		printf(" -flip                              - Default = 0 - 0 = Orig, 1 = Horiz, 2 = Verti, 3 = Both\n");
		printf("\n");
		printf(" -text                              - Default =      - Character/Text Overlay. Use Quotes.  Ex. -c "
			   "\"Text Overlay\"\n");
//		printf(" -textx                             - Default = 15   - Text Placement Horizontal from LEFT in Pixels\n");
//		printf(" -texty = Text Y                    - Default = 25   - Text Placement Vertical from TOP in Pixels\n");
//		printf(" -font_numbers = Font Name              - Default = 0    - Font Types (0-7), Ex. 0 = simplex, 4 = triplex, 7 = script\n");
		printf(" -font_color = Font Color            - Default = 255  - Text gray scale color  (0 - 255)\n");
		printf(" -background= Font Color            - Default = 0  - Backgroud gray scale color (0 - 255)\n");
//		printf(" -smallfont_color = Small Font Color - Default = 0 0 255  - Text red (BGR)\n");
//		printf(" -fonttype = Font Type              - Default = 0    - Font Line Type,(0-2), 0 = AA, 1 = 8, 2 = 4\n");
		printf(" -font_size                          - Default = 32  - Text Font Size (range 6 - 160, 32 default)\n");
//		printf(" -fontline                          - Default = 1    - Text Font Line Thickness\n");
		printf("\n");
		printf("\n");
		printf(" -latitude                          - Default = 60.7N (Whitehorse)   - Latitude of the camera.\n");
		printf(" -longitude                         - Default = 135.05W (Whitehorse) - Longitude of the camera\n");
		printf(" -angle                             - Default = -6 - Angle of the sun below the horizon. -6=civil "
			   "twilight, -12=nautical twilight, -18=astronomical twilight\n");
		printf("\n");
		// printf(" -preview                           - set to 1 to preview the captured images. Only works with a Desktop Environment\n");
		printf(" -time                              - Adds the time to the image.\n");
		printf(" -darkframe                         - Set to 1 to grab dark frame and cover your camera\n");
		printf(" -showDetails                       - Set to 1 to display the metadata on the image\n");
		printf(" -notificationimages                - Set to 1 to enable notification images, for example, 'Camera is off during day'.\n");
		printf(" -debuglevel                        - Default = 0. Set to 1,2 or 3 for more debugging information.\n");

		printf("%sUsage:\n", KRED);
		printf(" ./capture_RPiHQ -width 640 -height 480 -nightexposure 5000000 -gamma 50 -nightbin 1 -filename Lake-Laberge.JPG\n\n");
	}

	printf("%s", KNRM);

	int image_width_max = 4096;
	int image_height_max = 3040;
	double pixel_size_microns = 1.55;

	printf("- Resolution: %dx%d\n", image_width_max, image_height_max);
	printf("- Pixel Size: %1.2fmicrons\n", pixel_size_microns);
	printf("- Supported Bin: 1x, 2x and 3x\n");

	if (darkframe)
	{
		// To avoid overwriting the optional notification inage with the dark image,
		// during dark frames we use a different file name.
		image_file_name = "dark.jpg";
	}

	//-------------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------------

	printf("%s", KGRN);
	printf("\nCapture Settings:\n");
	printf(" Resolution (before any binning): %dx%d\n", width, height);
	printf(" Quality: %d\n", quality);
	printf(" Exposure (night): %1.0fms\n", round(exposure_night_us / US_IN_MS));
	printf(" Auto Exposure (night): %s\n", yesNo(exposure_night_auto));
	printf(" Auto Focus: %s\n", yesNo(camera_auto_focus));
	printf(" Gain (night): %1.2f\n", gain_night);
	printf(" Auto Gain (night): %s\n", yesNo(gain_night_auto));
	printf(" Brightness (day): %d\n", brightness_target_day);
	printf(" Brightness (night): %d\n", brightness_target_night);
	printf(" Gamma: %d\n", gamma);
	printf(" Auto White Balance: %s\n", yesNo(white_balance_auto));
	printf(" WB Red: %1.2f\n", white_balance_red);
	printf(" WB Blue: %1.2f\n", white_balance_blue);
	printf(" Binning (day): %d\n", binning_day);
	printf(" Binning (night): %d\n", binning_night);
	printf(" Delay (day): %dms\n", delay_day_ms);
	printf(" Delay (night): %dms\n", delay_night_ms);
	printf(" Text Overlay: %s\n", image_text);
//	printf(" Text Position: %dpx left, %dpx top\n", text_offset_from_left_px, text_offset_from_top_px);
//	printf(" Font Name:  %d\n", font_numbers[font_number]);
	printf(" Font Color: %d\n", font_color);
	printf(" Font Background Color: %d\n", background);
//	printf(" Small Font Color: %d , %d, %d\n", font_color_small[0], font_color_small[1], font_color_small[2]);
//	printf(" Font Line Type: %d\n", font_smoothing_options[font_smoothing]);
	printf(" Font Size: %1.1f\n", font_size);
//	printf(" Font Line: %d\n", font_weight);
//	printf(" Outline Font : %s\n", yesNo(font_outline));
	printf(" Rotation: %d\n", image_rotation);
	printf(" Flip Image: %d\n", image_flip);
	printf(" Filename: %s\n", image_file_name);
	printf(" Latitude: %s\n", latitude);
	printf(" Longitude: %s\n", longitude);
	printf(" Sun Elevation: %s\n", angle);
	// printf(" Preview: %s\n", yesNo(preview));
	printf(" Time: %s\n", yesNo(time));
	printf(" Show Details: %s\n", yesNo(showDetails));
	printf(" Darkframe: %s\n", yesNo(darkframe));
	printf(" Notification Images: %s\n", yesNo(notification_images));

	// Show selected camera type
	printf(" Camera: Raspberry Pi HQ camera\n");

	printf("%s", KNRM);

	// Initialization
	std::string last_day_or_night;
	int message_no_daytime_shown = 0; // Have we displayed "not taking picture during day" message, if applicable?

	while (b_main)
	{
		printf("\n");

		// Find out if it is currently DAY or NIGHT
		calculateDayOrNight(latitude, longitude, angle);

// Next line is present for testing purposes
// day_or_night.assign("NIGHT");

		last_day_or_night = day_or_night;

// Next lines are present for testing purposes
sprintf(debugText, "Daytimecapture: %d\n", capture_daytime);
displayDebugText(debugText, 3);

		printf("\n");

		if (darkframe)
		{
			// We're doing dark frames so turn off autoexposure and autogain, and use
			// nightime gain, delay, exposure, and brightness to mimic a nightime shot.
			exposure_auto_current_value = 0;
			gain_auto_current_value = 0;
			gain_current_value = gain_night;
			delay_current = delay_night_ms;
			exposure_current_value = exposure_night_us;
			brightness_current = brightness_target_night;
			binning_current = binning_night;

 			displayDebugText("Taking dark frames...\n", 0);
			if (notification_images) {
				system("scripts/copy_notification_image.sh DarkFrames &");
			}
		}

		else if (day_or_night == "DAY")
		{
			if (end_of_night == true)		// Execute end of night script
			{
				system("scripts/end_of_night.sh &");

				// Reset end of night indicator
				end_of_night = false;

				message_no_daytime_shown = 0;
			}

// Next line is present for testing purposes
// capture_daytime = 1;

			// Check if images should not be captured during day-time
			if (capture_daytime != 1)
			{
				// Only display messages once a day.
				if (message_no_daytime_shown == 0) {
					if (notification_images) {
						system("scripts/copy_notification_image.sh CameraOffDuringDay &");
					}
					sprintf(debugText, "It's daytime... we're not saving images.\n%s\n",
						tty ? "Press Ctrl+C to stop" : "Stop the allsky service to end this process.");
					displayDebugText(debugText, 0);
					message_no_daytime_shown = 1;

					// sleep until almost nighttime, then wake up and sleep a short time
					int seconds_till_night = calculateTimeToNightTime(latitude, longitude, angle);
					sleep(seconds_till_night - 10);
				}
				else
				{
					// Shouldn't need to sleep more than a few times before nighttime.
					sleep(5);
				}

				// No need to do any of the code below so go back to the main loop.
				continue;
			}

			// Images should be captured during day-time
			else
			{
				// Inform user
				char const *x;
				if (exposures_counter > 0)	// so it's easier to see in log file
					x = "\n==========\n";
				else
					x = "";
				sprintf(debugText, "%s=== Starting daytime capture ===\n%s", x, x);
				displayDebugText(debugText, 0);

				// set daytime settings
				exposure_auto_current_value = exposure_day_auto;
				gain_auto_current_value = gain_day_auto;
				gain_current_value = gain_day;
				delay_current = delay_day_ms;
				exposure_current_value = exposure_day_us;
				brightness_current = brightness_target_day;
				binning_current = binning_day;

				// Inform user
				sprintf(debugText, "Saving %d ms exposed images with %d seconds delays in between...\n\n", exposure_current_value * US_IN_MS, delay_current / MS_IN_SEC);
				displayDebugText(debugText, 0);
			}
		}

		else	// NIGHT
		{
			char const *x;
			if (exposures_counter > 0)	// so it's easier to see in log file
				x = "\n==========\n";
			else
				x = "";
			sprintf(debugText, "%s=== Starting nighttime capture ===\n%s", x, x);
			displayDebugText(debugText, 0);

			// Set nighttime settings
			exposure_auto_current_value = exposure_night_auto;
			gain_auto_current_value = gain_night_auto;
			gain_current_value = gain_night;
			delay_current = delay_night_ms;
			exposure_current_value = exposure_night_us;
			brightness_current = brightness_target_night;
			binning_current = binning_night;

			// Inform user
			sprintf(debugText, "Saving %d seconds exposure images with %d ms delays in between...\n\n", (int)round(exposure_current_value / US_IN_SEC), delay_night_ms);
			displayDebugText(debugText, 0);
		}

		// Adjusting variables for chosen binning
		width  = image_width_max / binning_current;
		height = image_height_max / binning_current;
//		text_offset_from_left_px    = text_offset_from_left_px / binning_current;
//		text_offset_from_top_px    = text_offset_from_top_px / binning_current;
//		font_size  = font_size / binning_current;
//		font_weight = font_weight / binning_current;

		// Inform user
		if (tty)
			printf("Press Ctrl+Z to stop\n\n");	// xxx ECC: Ctrl-Z stops a process, it doesn't kill it
		else
			printf("Stop the allsky service to end this process.\n\n");

		// Wait for switch day time -> night time or night time -> day time
		while (b_main && last_day_or_night == day_or_night)
		{
			// Inform user
			sprintf(debugText, "Capturing & saving image...\n");
			displayDebugText(debugText, 0);

			// Capture and save image
			RPiHQcapture(camera_auto_focus, exposure_auto_current_value, exposure_current_value, gain_auto_current_value, white_balance_auto, gain_current_value, binning_current, white_balance_red, white_balance_blue, image_rotation, image_flip, gamma, brightness_current, quality, image_file_name, time, showDetails, image_text, font_size, font_color, background, darkframe);

			// Check for night time
			if (day_or_night == "NIGHT")
			{
				// Preserve image during night time
				system("scripts/saveImageNight.sh &");
			}
			else
			{
				// Upload and resize image when configured
				system("scripts/saveImageDay.sh &");
			}

			// Inform user
			sprintf(debugText, "Capturing & saving %s done, now wait %d seconds...\n", darkframe ? "dark frame" : "image", delay_current / MS_IN_SEC);
			displayDebugText(debugText, 0);

			// Sleep for a moment
			usleep(delay_current * US_IN_MS);

			// Check for day or night based on location and angle
			calculateDayOrNight(latitude, longitude, angle);

// Next line is present for testing purposes
// day_or_night.assign("NIGHT");

			// ECC: why bother with the check below for DAY/NIGHT?
			// Check if it is day time
			if (day_or_night=="DAY")
			{
				// Check started capturing during day time
				if (last_day_or_night=="DAY")
				{
					sprintf(debugText, "Check for day or night: DAY (waiting for changing DAY into NIGHT)...\n");
					displayDebugText(debugText, 2);
				}

				// Started capturing during night time
				else
				{
					sprintf(debugText, "Check for day or night: DAY (waiting for changing NIGHT into DAY)...\n");
					displayDebugText(debugText, 2);
				}
			}

			else	// NIGHT
			{
				// Check started capturing during day time
				if (last_day_or_night=="DAY")
				{
					sprintf(debugText, "Check for day or night: NIGHT (waiting for changing DAY into NIGHT)...\n");
					displayDebugText(debugText, 2);
				}

				// Started capturing during night time
				else
				{
					sprintf(debugText, "Check for day or night: NIGHT (waiting for changing NIGHT into DAY)...\n");
					displayDebugText(debugText, 2);
				}
			}

			printf("\n");
		}

		// Check for night situation
		if (last_day_or_night == "NIGHT")
		{
			// Flag end of night processing is needed
			end_of_night = true;
		}
	}

	closeUp(0);
}
