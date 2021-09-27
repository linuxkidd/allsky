#include "capture.h"
#include "config.h"
#include "opencv.h"
#include "common.h"

//-------------------------------------------------------------------------------------------------------
void *retval;
int camera_controls_count           = 0;
int camera_id               = 0;
pthread_t thread_display = 0;
pthread_t thread_save       = 0;
int exposures_counter         = 0; // how many valid pictures have we taken so far?
int gain_current_value          = NOT_SET;

cv::Mat pRgb;
std::vector<int> compression_parameters;

bool b_save_run = false, b_saving_image = false;
pthread_mutex_t mtx_SaveImg;
pthread_cond_t cond_save_start;

// These are global so they can be used by other routines.
ASI_CONTROL_CAPS ControlCaps;
ASI_BOOL exposure_auto_current_value = ASI_FALSE; // is Auto Exposure currently on or off?

bool continuous_exposure    = true;       // Renamed and logic inverted from 'use_new_exposure_algorithm'
bool b_main = true, b_display = false;

// Some command-line and other option definitions needed outside of main():
bool notification_images      = DEFAULT_NOTIFICATION_IMAGES;
char const *image_file_name         = DEFAULT_IMAGE_FILENAME;
char const *time_format       = DEFAULT_TIME_FORMAT;
int exposure_day_us           = DEFAULT_EXPOSURE_DAY_US;
int exposure_day_auto       = DEFAULT_EXPOSURE_DAY_AUTO; // is it on or off for daylight?
int delay_day_ms                 = DEFAULT_DELAY_DAY_MS;        // Delay in milliseconds.
int delay_night_ms               = DEFAULT_DELAY_NIGHT_MS;      // Delay in milliseconds.
int exposure_night_max_us      = DEFAULT_EXPOSURE_NIGHT_MAX_US;
int gain_transition_time       = DEFAULT_GAIN_TRANSITION_TIME;
long exposure_camera_max_us = NOT_SET; // camera's max auto exposure in us

#ifdef USE_HISTOGRAM
int histogram_box_height_px = DEFAULT_HISTO_BOX_WIDTH_PX; // 500 px x 500 px box.  Must be a multiple of 2.
int histogram_box_width_px = DEFAULT_HISTO_BOX_HEIGHT_PX;

// % from left/top side that the center of the box is.  0.5 == the center of the image's X/Y axis
float histogram_box_center_from_left_pct = DEFAULT_HISTO_BOX_FROM_LEFT;
float histogram_box_center_from_top_pct  = DEFAULT_HISTO_BOX_FROM_TOP;
#endif // USE_HISTOGRAM

char retCodeBuffer[100];

long exposure_actual_us = 0; // actual exposure taken, per the camera
long gain_actual                 = 0; // actual gain used, per the camera
long temperature_actual                 = 0; // actual sensor temp, per the camera
ASI_BOOL bAuto = ASI_FALSE;          // "auto" flag returned by ASIGetControlValue, when we don't care what it is

ASI_BOOL was_exposure_auto = ASI_FALSE;
long buffer_size          = NOT_SET;

bool gain_adjust_enable          = false; // Should we adjust the gain?  Set by user on command line.
bool gain_adjust_current   = false; // Adjusting it right now?
int gain_total_adjust      = 0;     // The total amount to adjust gain.
int gain_adjust_per_image_value   = 0;     // Amount of gain to adjust each image
int gain_transition_images = 0;
int gain_changes_count       = 0; // This is reset at every day/night and night/day transition.
int signal_received            = 0; // did we get a SIGINT (from keyboard) or SIGTERM (from service)?
//-------------------------------------------------------------------------------------------------------

// Make sure we don't try to update a non-updateable control, and check for errors.
ASI_ERROR_CODE setControl(int camera_id, ASI_CONTROL_TYPE control, long value, ASI_BOOL make_auto)
{
    ASI_ERROR_CODE ret = ASI_SUCCESS;
    int i;
    for (i = 0; i < camera_controls_count && i <= control; i++) // controls are sorted 1 to n
    {
        ret = ASIGetControlCaps(camera_id, i, &ControlCaps);

#ifdef USE_HISTOGRAM
        // Keep track of the camera's max auto exposure so we don't try to exceed it.
        if (ControlCaps.ControlType == ASI_AUTO_MAX_EXP && exposure_camera_max_us == NOT_SET)
        {
            // MaxValue is in MS so convert to microseconds
            exposure_camera_max_us = ControlCaps.MaxValue * US_IN_MS;
        }
#endif

        if (ControlCaps.ControlType == control)
        {
            if (ControlCaps.IsWritable)
            {
                if (value > ControlCaps.MaxValue)
                {
                    sprintf(debugText,
                            "WARNING: Value of %ld greater than max value allowed (%ld) for control '%s' (#%d).\n",
                            value, ControlCaps.MaxValue, ControlCaps.Name, ControlCaps.ControlType);
                    displayDebugText(debugText, 1);
                    value = ControlCaps.MaxValue;
                }
                else if (value < ControlCaps.MinValue)
                {
                    sprintf(debugText,
                            "WARNING: Value of %ld less than min value allowed (%ld) for control '%s' (#%d).\n", value,
                            ControlCaps.MinValue, ControlCaps.Name, ControlCaps.ControlType);
                    displayDebugText(debugText, 1);
                    value = ControlCaps.MinValue;
                }
                if (make_auto == ASI_TRUE && ControlCaps.IsAutoSupported == ASI_FALSE)
                {
                    sprintf(debugText, "WARNING: control '%s' (#%d) doesn't support auto mode.\n", ControlCaps.Name,
                            ControlCaps.ControlType);
                    displayDebugText(debugText, 1);
                    make_auto = ASI_FALSE;
                }
                ret = ASISetControlValue(camera_id, control, value, make_auto);
            }
            else
            {
                sprintf(debugText, "ERROR: ControlCap: '%s' (#%d) not writable; not setting to %ld.\n",
                        ControlCaps.Name, ControlCaps.ControlType, value);
                displayDebugText(debugText, 0);
                ret = ASI_ERROR_INVALID_MODE; // this seemed like the closest error
            }
            return ret;
        }
    }
    sprintf(debugText, "NOTICE: Camera does not support ControlCap # %d; not setting to %ld.\n", control, value);
    displayDebugText(debugText, 3);
    return ASI_ERROR_INVALID_CONTROL_TYPE;
}

//-------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------

// Create Hex value from RGB
unsigned long createRGB(int r, int g, int b)
{
    return ((r & 0xff) << 16) + ((g & 0xff) << 8) + (b & 0xff);
}

void cvText(cv::Mat &img, const char *text, int x, int y, double font_size, int font_weight, int font_smoothing_options, int font_number,
            int font_color[], int imgtype, int font_outline)
{
    if (imgtype == ASI_IMG_RAW16)
    {
        unsigned long font_color16 = createRGB(font_color[2], font_color[1], font_color[0]);
        if (font_outline)
        {
            cv::putText(img, text, cv::Point(x, y), font_number, font_size, cv::Scalar(0, 0, 0), font_weight + 4, font_smoothing_options);
        }
        cv::putText(img, text, cv::Point(x, y), font_number, font_size, font_color16, font_weight, font_smoothing_options);
    }
    else
    {
        if (font_outline)
        {
            cv::putText(img, text, cv::Point(x, y), font_number, font_size, cv::Scalar(0, 0, 0, 255), font_weight + 4,
                        font_smoothing_options);
        }
        cv::putText(img, text, cv::Point(x, y), font_number, font_size,
                    cv::Scalar(font_color[0], font_color[1], font_color[2], 255), font_weight, font_smoothing_options);
    }
}

void *SaveImgThd(void *para)
{
    while (b_save_run)
    {
        pthread_mutex_lock(&mtx_SaveImg);
        pthread_cond_wait(&cond_save_start, &mtx_SaveImg);

        if (signal_received)
        {
            // we got a signal to exit, so don't save the (probably incomplete) image
            pthread_mutex_unlock(&mtx_SaveImg);
            break;
        }

        b_saving_image = true;
        if (pRgb.data)
        {
            imwrite(image_file_name, pRgb, compression_parameters);
            if (day_or_night == "NIGHT")
            {
                system("scripts/saveImageNight.sh &");
            }
            else
            {
                system("scripts/saveImageDay.sh &");
            }
        }
        else
        {
            // This can happen if the program is closed before the first picture.
            displayDebugText("----- SaveImgThd(): pRgb.data is null\n", 2);
        }
        b_saving_image = false;
        pthread_mutex_unlock(&mtx_SaveImg);
    }

    return (void *)0;
}

// Display ASI errors in human-readable format
char *getRetCode(ASI_ERROR_CODE code)
{
    std::string ret;
    if (code == ASI_SUCCESS)
        ret = "ASI_SUCCESS";
    else if (code == ASI_ERROR_INVALID_INDEX)
        ret = "ASI_ERROR_INVALID_INDEX";
    else if (code == ASI_ERROR_INVALID_ID)
        ret = "ASI_ERROR_INVALID_ID";
    else if (code == ASI_ERROR_INVALID_CONTROL_TYPE)
        ret = "ASI_ERROR_INVALID_CONTROL_TYPE";
    else if (code == ASI_ERROR_CAMERA_CLOSED)
        ret = "ASI_ERROR_CAMERA_CLOSED";
    else if (code == ASI_ERROR_CAMERA_REMOVED)
        ret = "ASI_ERROR_CAMERA_REMOVED";
    else if (code == ASI_ERROR_INVALID_PATH)
        ret = "ASI_ERROR_INVALID_PATH";
    else if (code == ASI_ERROR_INVALID_FILEFORMAT)
        ret = "ASI_ERROR_INVALID_FILEFORMAT";
    else if (code == ASI_ERROR_INVALID_SIZE)
        ret = "ASI_ERROR_INVALID_SIZE";
    else if (code == ASI_ERROR_INVALID_IMGTYPE)
        ret = "ASI_ERROR_INVALID_IMGTYPE";
    else if (code == ASI_ERROR_OUTOF_BOUNDARY)
        ret = "ASI_ERROR_OUTOF_BOUNDARY";
    else if (code == ASI_ERROR_TIMEOUT)
        ret = "ASI_ERROR_TIMEOUT";
    else if (code == ASI_ERROR_INVALID_SEQUENCE)
        ret = "ASI_ERROR_INVALID_SEQUENCE";
    else if (code == ASI_ERROR_BUFFER_TOO_SMALL)
        ret = "ASI_ERROR_BUFFER_TOO_SMALL";
    else if (code == ASI_ERROR_VIDEO_MODE_ACTIVE)
        ret = "ASI_ERROR_VIDEO_MODE_ACTIVE";
    else if (code == ASI_ERROR_EXPOSURE_IN_PROGRESS)
        ret = "ASI_ERROR_EXPOSURE_IN_PROGRESS";
    else if (code == ASI_ERROR_GENERAL_ERROR)
        ret = "ASI_ERROR_GENERAL_ERROR";
    else if (code == ASI_ERROR_END)
        ret = "ASI_ERROR_END";
    else if (code == -1)
        ret = "Non-ASI ERROR";
    else
        ret = "UNKNOWN ASI ERROR";

    sprintf(retCodeBuffer, "%d (%s)", (int)code, ret.c_str());
    return (retCodeBuffer);
}

int bytesPerPixel(ASI_IMG_TYPE imageType)
{
    switch (imageType)
    {
        case ASI_IMG_RGB24:
            return 3;
            break;
        case ASI_IMG_RAW16:
            return 2;
            break;
        case ASI_IMG_RAW8:
        case ASI_IMG_Y8:
        default:
            return 1;
    }
}

#ifdef USE_HISTOGRAM
// As of July 2021, ZWO's SDK (version 1.9) has a bug where autoexposure daylight shots'
// exposures jump all over the place.  One is way too dark and the next way too light, etc.
// As a workaround, our histogram code replaces ZWO's code auto-exposure mechanism.
// We look at the mean brightness of an X by X rectangle in image, and adjust exposure based on that.

void computeHistogram(unsigned char *imageBuffer, int my_image_width, int my_image_height, ASI_IMG_TYPE imageType, int *histogram)
{
    int h, i;
    unsigned char *b = imageBuffer;

    // Clear the histogram array.
    for (h = 0; h < 256; h++)
    {
        histogram[h] = 0;
    }

    // Different image types have a different number of bytes per pixel.
    int bpp = bytesPerPixel(imageType);
    my_image_width *= bpp;
    int roi_x1 = (my_image_width * histogram_box_center_from_left_pct) - (histogram_box_height_px * bpp / 2);
    int roi_x2 = roi_x1 + (bpp * histogram_box_height_px);
    int roi_y1 = (my_image_height * histogram_box_center_from_top_pct) - (histogram_box_width_px / 2);
    int roi_y2 = roi_y1 + histogram_box_width_px;

    // Start off and end on a logical pixel boundries.
    roi_x1 = (roi_x1 / bpp) * bpp;
    roi_x2 = (roi_x2 / bpp) * bpp;

    // For RGB24, data for each pixel is stored in 3 consecutive bytes: blue, green, red.
    // For all image types, each row in the image contains one row of pixels.
    // bpp doesn't apply to rows, just columns.
    switch (imageType)
    {
        case ASI_IMG_RGB24:
        case ASI_IMG_RAW8:
        case ASI_IMG_Y8:
            for (int y = roi_y1; y < roi_y2; y++)
            {
                for (int x = roi_x1; x < roi_x2; x += bpp)
                {
                    i         = (my_image_width * y) + x;
                    int total = 0;
                    for (int z = 0; z < bpp; z++)
                    {
                        // For RGB24 this averages the blue, green, and red pixels.
                        total += b[i + z];
                    }
                    int avg = total / bpp;
                    histogram[avg]++;
                }
            }
            break;
        case ASI_IMG_RAW16:
            for (int y = roi_y1; y < roi_y2; y++)
            {
                for (int x = roi_x1; x < roi_x2; x += bpp)
                {
                    i = (my_image_width * y) + x;
                    int pixel_value;
                    // This assumes the image data is laid out in big endian format.
                    // We are going to grab the most significant byte
                    // and use that for the histogram value ignoring the
                    // least significant byte so we can use the 256 value histogram array.
                    // If t's acutally little endian then add a +1 to the array subscript for b[i].
                    pixel_value = b[i];
                    histogram[pixel_value]++;
                }
            }
            break;
        default:
            sprintf(debugText, "*** ERROR: Received unspported value for ASI_IMG_TYPE: %d\n", imageType);
            displayDebugText(debugText, 0);
    }
}

int calculateHistogramMean(int *histogram)
{
    int mean_bin = 0;
    int a = 0, b = 0;
    for (int h = 0; h < 256; h++)
    {
        a += (h + 1) * histogram[h];
        b += histogram[h];
    }

    if (b == 0)
    {
        sprintf(debugText, "*** ERROR: calculateHistogramMean(): b==0\n");
        displayDebugText(debugText, 0);
        return (0);
    }

    mean_bin = a / b - 1;
    return mean_bin;
}
#endif

ASI_ERROR_CODE takeOneExposure(int my_camera_id, long exposureTimeMicroseconds, unsigned char *imageBuffer, long my_image_width,
                               long my_image_height, // where to put image and its size
                               ASI_IMG_TYPE my_image_type)
{
    if (imageBuffer == NULL)
    {
        return (ASI_ERROR_CODE)-1;
    }

    ASI_ERROR_CODE status;
    // ZWO recommends timeout = (exposure*2) + 500 ms
    // After some discussion, we're doing +5000ms to account for delays induced by
    // USB contention, such as that caused by heavy USB disk IO
    long timeout = ((exposureTimeMicroseconds * 2) / US_IN_MS) + 5000; // timeout is in ms

    sprintf(debugText, "  > Exposure set to %'ld us (%'.2f ms), timeout: %'ld ms\n", exposureTimeMicroseconds,
            (float)exposureTimeMicroseconds / US_IN_MS, timeout);
    displayDebugText(debugText, 2);

    setControl(my_camera_id, ASI_EXPOSURE, exposureTimeMicroseconds, exposure_auto_current_value);

    if (continuous_exposure)
    {
        status = ASI_SUCCESS;
    }
    else
    {
        status = ASIStartVideoCapture(cameraId);
    }

    if (status == ASI_SUCCESS)
    {
        status = ASIGetVideoData(my_camera_id, imageBuffer, buffer_size, timeout);
        if (status != ASI_SUCCESS)
        {
            sprintf(debugText, "  > ERROR: Failed getting image, status = %s\n", getRetCode(status));
            displayDebugText(debugText, 0);
        }
        else
        {
            ASIGetControlValue(my_camera_id, ASI_EXPOSURE, &exposure_actual_us, &was_exposure_auto);
            sprintf(debugText, "  > Got image @ exposure: %'ld us (%'.2f ms)\n", exposure_actual_us,
                    (float)exposure_actual_us / US_IN_MS);
            displayDebugText(debugText, 2);

            // If this was a manual exposure, make sure it took the correct exposure.
            if (was_exposure_auto == ASI_FALSE && exposureTimeMicroseconds != exposure_actual_us)
            {
                sprintf(debugText,
                        "  > WARNING: not correct exposure (requested: %'ld us, actual: %'ld us, diff: %'ld)\n",
                        exposureTimeMicroseconds, exposure_actual_us,
                        exposure_actual_us - exposureTimeMicroseconds);
                displayDebugText(debugText, 0);
                status = (ASI_ERROR_CODE)-1;
            }
            ASIGetControlValue(my_camera_id, ASI_GAIN, &gain_actual, &bAuto);
            ASIGetControlValue(my_camera_id, ASI_TEMPERATURE, &temperature_actual, &bAuto);
        }

        if (!continuous_exposure)
            ASIStopVideoCapture(my_camera_id);
    }
    else
    {
        sprintf(debugText, "  > ERROR: Not fetching exposure data because status is %s\n", getRetCode(status));
        displayDebugText(debugText, 0);
    }

    return status;
}

// Exit the program gracefully.
void closeUp(int e)
{
    static int closing_up = 0; // indicates if we're in the process of exiting.
    // For whatever reason, we're sometimes called twice, but we should only execute once.
    if (closing_up)
        return;

    closing_up = 1;

    ASIStopVideoCapture(camera_id);

    // Seems to hang on ASICloseCamera() if taking a picture when the signal is sent,
    // until the exposure finishes, then it never returns so the remaining code doesn't
    // get executed.  Don't know a way around that, so don't bother closing the camera.
    // Prior versions of allsky didn't do any cleanup, so it should be ok not to close the camera.
    //    ASICloseCamera(camera_id);

    if (b_display)
    {
        b_display = 0;
        pthread_join(thread_display, &retval);
    }

    if (b_save_run)
    {
        b_save_run = false;
        pthread_mutex_lock(&mtx_SaveImg);
        pthread_cond_signal(&cond_save_start);
        pthread_mutex_unlock(&mtx_SaveImg);
        pthread_join(thread_save, 0);
    }

    // Unfortunately we don't know if the service is stopping us, or restarting us.
    // If it was restarting there'd be no reason to copy a notification image since it
    // will soon be overwritten.  Since we don't know, always copy it.
    if (notification_images)
    {
        system("scripts/copy_notification_image.sh NotRunning &");
        // Sleep to give it a chance to print any messages so they (hopefully) get printed
        // before the one below.  This is only so it looks nicer in the log file.
        sleep(3);
    }

    printf("     ***** Stopping AllSky *****\n");
    exit(e);
}

// Reset the gain transition variables for the first transition image.
// This is called when the program first starts and at the beginning of every day/night transition.
// "day_or_night" is the new value, e.g., if we just transitioned from day to night, it's "NIGHT".
bool resetGainTransitionVariables(int dayGain, int nightGain)
{
    // Many of the "xxx" messages below will go away once we're sure gain transition works.
    sprintf(debugText, "xxx resetGainTransitionVariables(%d, %d) called at %s\n", dayGain, nightGain,
            day_or_night.c_str());
    displayDebugText(debugText, 2);

    if (gain_adjust_enable == false)
    {
        // determineGainChange() will never be called so no need to set any variables.
        sprintf(debugText, "xxx will not adjust gain - gain_adjust_enable == false\n");
        displayDebugText(debugText, 2);
        return (false);
    }

    if (exposures_counter == 0)
    {
        // we don't adjust when the program first starts since there's nothing to transition from
        sprintf(debugText, "xxx will not adjust gain right now - exposures_counter == 0\n");
        displayDebugText(debugText, 2);
        return (false);
    }

    // Determine the amount to adjust gain per image.
    // Do this once per day/night or night/day transition (i.e., gain_changes_count == 0).
    // First determine how long an exposure and delay is, in seconds.
    // The user specifies the transition period in seconds,
    // but day exposure is in microseconds, night max is in milliseconds,
    // and delays are in milliseconds, so convert to seconds.
    float time_total_sec;
    if (day_or_night == "DAY")
    {
        time_total_sec = (exposure_day_us / US_IN_SEC) + (delay_day_ms / MS_IN_SEC);
        sprintf(debugText, "xxx time_total_sec=%.1fs, exposure_day_us=%'dus , daydelay=%'dms\n", time_total_sec,
                exposure_day_us, delay_day_ms);
        displayDebugText(debugText, 2);
    }
    else // NIGHT
    {
        // At nightime if the exposure is less than the max, we wait until max has expired,
        // so use it instead of the exposure time.
        time_total_sec = (exposure_night_max_us / MS_IN_SEC) + (delay_night_ms / MS_IN_SEC);
        sprintf(debugText, "xxx time_total_sec=%.1fs, exposure_night_max_us=%'dms, delay_night_ms=%'dms\n", time_total_sec,
                exposure_night_max_us, delay_night_ms);
        displayDebugText(debugText, 2);
    }

    gain_transition_images = ceil(gain_transition_time / time_total_sec);
    if (gain_transition_images == 0)
    {
        sprintf(debugText,
                "*** INFORMATION: Not adjusting gain - your 'gaintransitiontime' (%d seconds) is less than the time to "
                "take one image plus its delay (%.1f seconds).\n",
                gain_transition_time, time_total_sec);
        displayDebugText(debugText, 0);

        return (false);
    }

    gain_total_adjust    = nightGain - dayGain;
    gain_adjust_per_image_value = ceil(gain_total_adjust / gain_transition_images); // spread evenly
    if (gain_adjust_per_image_value == 0)
    {
        gain_adjust_per_image_value = gain_total_adjust;
    }
    else
    {
        // Since we can't adust gain by fractions, see if there's any "left over" after gain_transition_images.
        // For example, if gain_total_adjust is 7 and we're adjusting by 3 each of 2 times,
        // we need an extra transition to get the remaining 1 ((7 - (3 * 2)) == 1).
        if (gain_transition_images * gain_adjust_per_image_value < gain_total_adjust)
            gain_transition_images++; // this one will get the remaining amount
    }

    sprintf(debugText,
            "xxx gain_transition_images=%d, gain_transition_time=%ds, gain_adjust_per_image_value=%d, gain_total_adjust=%d\n",
            gain_transition_images, gain_transition_time, gain_adjust_per_image_value, gain_total_adjust);
    displayDebugText(debugText, 2);

    return (true);
}

// Determine the change in gain needed for smooth transitions between night and day.
// Gain during the day is usually 0 and at night is usually > 0.
// If auto exposure is on for both, the first several night frames may be too bright at night
// because of the sudden (often large) increase in gain, or too dark at the night-to-day
// transition.
// Try to mitigate that by changing the gain over several images at each transition.

int determineGainChange(int dayGain, int nightGain)
{
    if (gain_changes_count > gain_transition_images || gain_total_adjust == 0)
    {
        // no more changes needed in this transition
        sprintf(debugText, "  xxxx No more gain changes needed.\n");
        displayDebugText(debugText, 2);
        gain_adjust_current = false;
        return (0);
    }

    gain_changes_count++;
    int amt; // amount to adjust gain on next picture
    if (day_or_night == "DAY")
    {
        // During DAY, want to start out adding the full gain adjustment minus the increment on the first image,
        // then DECREASE by gain_total_adjust each exposure.
        // This assumes night gain is > day gain.
        amt = gain_total_adjust - (gain_adjust_per_image_value * gain_changes_count);
        if (amt < 0)
        {
            amt             = 0;
            gain_total_adjust = 0; // we're done making changes
        }
    }
    else // NIGHT
    {
        // During NIGHT, want to start out (nightGain-gain_adjust_per_image_value),
        // then DECREASE by gain_adjust_per_image_value each time, until we get to "nightGain".
        // This last image was at dayGain and we wen't to increase each image.
        amt = (gain_adjust_per_image_value * gain_changes_count) - gain_total_adjust;
        if (amt > 0)
        {
            amt             = 0;
            gain_total_adjust = 0; // we're done making changes
        }
    }

    sprintf(debugText, "  xxxx Adjusting %s gain by %d on next picture to %d; will be gain change # %d of %d.\n",
            day_or_night.c_str(), amt, amt + gain_current_value, gain_changes_count, gain_transition_images);
    displayDebugText(debugText, 2);
    return (amt);
}

//-------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    signal(SIGINT, IntHandle);
    signal(SIGTERM, IntHandle); // The service sends SIGTERM to end this program.
    pthread_mutex_init(&mtx_SaveImg, 0);
    pthread_cond_init(&cond_save_start, 0);

    //-------------------------------------------------------------------------------------------------------
    //-------------------------------------------------------------------------------------------------------
    int font_numbers[] = { cv::FONT_HERSHEY_SIMPLEX,        cv::FONT_HERSHEY_PLAIN,         cv::FONT_HERSHEY_DUPLEX,
                       cv::FONT_HERSHEY_COMPLEX,        cv::FONT_HERSHEY_TRIPLEX,       cv::FONT_HERSHEY_COMPLEX_SMALL,
                       cv::FONT_HERSHEY_SCRIPT_SIMPLEX, cv::FONT_HERSHEY_SCRIPT_COMPLEX };
    char const *font_names[] = { // Character representation of names for clarity:
                                "SIMPLEX", "PLAIN",         "DUPEX",          "COMPLEX",
                                "TRIPLEX", "COMPLEX_SMALL", "SCRIPT_SIMPLEX", "SCRIPT_COMPLEX" };

    int font_count = sizeof(font_names) / sizeof(font_names[0]);

    char time_buffer[128]               = { 0 };
    char tmp_buffer[128]               = { 0 };
    char tmp_buffer2[50]               = { 0 };
    char const *bayer[]             = { "RG", "BG", "GR", "GB" };
    bool end_of_night                 = false;
    int i;
    ASI_ERROR_CODE camera_return_code; // used for return code from ASI functions.

    // Some settings have both day and night versions, some have only one version that applies to both,
    // and some have either a day OR night version but not both.
    // For settings with both versions we keep a "current" variable (e.g., "binning_current") that's either the day
    // or night version so the code doesn't always have to check if it's day or night.
    // The settings have either "day" or "night" in the name.
    // In theory, almost every setting could have both day and night versions (e.g., width & height),
    // but the chances of someone wanting different versions.

    const char *locale          = DEFAULT_LOCALE;

    // All the font settings apply to both day and night.
    int font_number              = DEFAULT_FONT_NUMBER;

    int text_offset_from_left_px                  = DEFAULT_TEXT_OFFSET_FROM_LEFT_PX;
    int text_offset_from_top_px                  = DEFAULT_TEXT_OFFSET_FROM_TOP_PX;
    int text_line_height_px         = DEFAULT_TEXT_LINE_HEIGHT_PX;
    char const *image_text         = "";
    char const *image_extra_text_file_name    = "";
    int extra_file_age            = 0; // 0 disables it
    char text_buffer[1024]       = { 0 };
    double font_size             = DEFAULT_FONT_SIZE;

    int font_weight              = DEFAULT_TEXT_FONT_WEIGHT;
    int font_outline             = DEFAULT_FONT_OUTLINE;
    int font_color[3]            = { 255, 0, 0 };
    int font_color_small[3]      = { 0, 0, 255 };
    int font_smoothing_options[3]        = { cv::LINE_AA, 8, 4 };
    int font_smoothing               = DEFAULT_FONT_SMOOTHING;

    int image_width              = DEFAULT_IMAGE_WIDTH_PX;
    int sensor_width_px            = image_width;
    int image_height             = DEFAULT_IMAGE_HEIGHT_PX;
    int sensor_height_px           = image_height;

    int binning_day                  = DEFAULT_BINNING_DAY;
    int binning_night                = DEFAULT_BINNING_NIGHT;
    int binning_current              = NOT_SET;

    int image_type              = DEFAULT_IMAGE_TYPE;

    int usb_bandwidth           = DEFAULT_USB_BANDWIDTH;
    int usb_bandwidth_auto      = 0; // is Auto Bandwidth on or off?

    long exposure_night_us      = DEFAULT_EXPOSURE_NIGHT_US;
    long exposure_current_value        = NOT_SET;
    int exposure_night_auto     = DEFAULT_EXPOSURE_NIGHT_AUTO; // is it on or off for nighttime?
    // exposure_auto_current_value is global so is defined outside of main()

    int gain_day                = DEFAULT_GAIN_DAY;
    int gain_day_auto           = 0; // is Auto Gain on or off for daytime?
    int gain_night              = DEFAULT_GAIN_NIGHT;
    int gain_night_auto         = DEFAULT_GAIN_NIGHT_AUTO; // is Auto Gain on or off for nighttime?
    int gain_night_max          = DEFAULT_GAIN_NIGHT_MAX;
    ASI_BOOL gain_auto_current_value    = ASI_FALSE;

    int delay_current            = NOT_SET;

    int white_balance_red       = DEFAULT_WHITE_BALANCE_RED;
    int white_balance_blue      = DEFAULT_WHITE_BALANCE_BLUE;
    int auto_white_balance      = DEFAULT_WHITE_BALANCE_AUTO; // is Auto White Balance on or off?

    int gamma                   = DEFAULT_GAMMA;

    int brightness_target_day   = DEFAULT_BRIGHTNESS_TARGET;
    int brightness_target_night = DEFAULT_BRIGHTNESS_TARGET;
    int brightness_current       = NOT_SET;

    char const *latitude        = DEFAULT_LATITUDE;
    char const *longitude       = DEFAULT_LONGITUDE;
    // angle of the sun with the horizon
    // (0=sunset, -6=civil twilight, -12=nautical twilight, -18=astronomical twilight)
    char const *angle           = DEFAULT_SOLAR_ANGLE;

    int preview                 = 0;
    int time_show                = DEFAULT_TIME_SHOW;
    int darkframe               = 0;
    char const *temperature_unit        = "C"; // Celsius

    int temperature_show                = 0;
    int exposure_show            = 0;
    int gain_show                = 0;
    int brightness_show          = 0;
#ifdef USE_HISTOGRAM
    int histogram_mean_show           = 0;
    int histogram_attempts_count_max    = 15; // max number of times we'll try for a better histogram mean
    int histogram_box_show        = 0;
#endif
    int capture_daytime          = DEFAULT_CAPTURE_DAYTIME; // are we capturing daytime pictures?

    int help                    = 0;
    int quality                 = NOT_SET;
    int image_flip              = 0;
    int cooler_enabled          = 0;
    long cooler_target_temp_c   = 0;
    std::string image_ext       = "jpg";
    std::string image_name      = "image";
    //-------------------------------------------------------------------------------------------------------
    //-------------------------------------------------------------------------------------------------------
    printCredits();

    if (argc > 1)
    {
        // Note: argv[1] is the name of the program, so argc must be > 1 for any options to be present.
        parse_commandline(argc, argv);
    }

    if (help == 1)
    {
        printHelp();
        exit(1);
    }

    if (strncmp(config_file,"",1) != 0)
    {
        int pret=parse_ini();
        if(pret) {
            exit(pret);
        }
    }
    printf("%s\n", KNRM);
    setlocale(LC_NUMERIC, locale);

    const char *imagetype = "";
    if (strncmp(image_ext.c_str(), "jpg",4) == 0 || strncmp(image_ext.c_str(), "jpeg",5) == 0)
    {
        if (image_type == ASI_IMG_RAW16)
        {
            waitToFix("*** ERROR: RAW16 images only work with .png files; either change the Image Type or the Filename.\n");
            exit(99);
        }

        imagetype = "jpg";
        compression_parameters.push_back(cv::IMWRITE_JPEG_QUALITY);
        if (quality == NOT_SET)
        {
            quality = 95;
        } else if (quality > 100)
        {
            quality = 100;
        }
    }
    else if (strncmp(image_ext.c_str(), "png", 4) == 0)
    {
        imagetype = "png";
        compression_parameters.push_back(cv::IMWRITE_PNG_COMPRESSION);
        if (quality == NOT_SET)
        {
            quality = 3;
        } else if (quality > 9)
        {
            quality = 9;
        }
    }
    compression_parameters.push_back(quality);

    if (darkframe)
    {
        // To avoid overwriting the optional notification inage with the dark image,
        // during dark frames we use a different file name.
        static char darkFilename[200];
        sprintf(darkFilename, "dark.%s", imagetype);
        image_file_name = darkFilename;
    }

    int camera_count = ASIGetNumOfConnectedCameras();
    if (camera_count <= 0)
    {
        printf("*** ERROR: No Connected Camera...\n");
        // Don't wait here since it's possible the camera is physically connected
        // but the software doesn't see it and the USB bus needs to be reset.
        closeUp(1); // If there are no cameras we can't do anything.
    }

    printf("\nListing Attached Cameras%s:\n", camera_count == 1 ? "" : " (using first one)");

    ASI_CAMERA_INFO ASICameraInfo;

    for (i = 0; i < camera_count; i++)
    {
        ASIGetCameraProperty(&ASICameraInfo, i);
        printf("  - %d %s\n", i, ASICameraInfo.Name);
    }

    camera_return_code = ASIOpenCamera(camera_id);
    if (camera_return_code != ASI_SUCCESS)
    {
        printf("*** ERROR opening camera, check that you have root permissions! (%s)\n", getRetCode(camera_return_code));
        closeUp(1); // Can't do anything so might as well exit.
    }

    printf("\n%s Information:\n", ASICameraInfo.Name);
    int image_width_max, image_height_max;
    double pixel_size_microns;
    image_width_max  = ASICameraInfo.MaxWidth;
    image_height_max = ASICameraInfo.MaxHeight;
    pixel_size_microns  = ASICameraInfo.PixelSize;
    printf("  - Resolution:%dx%d\n", image_width_max, image_height_max);
    printf("  - Pixel Size: %1.1fmicrons\n", pixel_size_microns);
    printf("  - Supported Bin: ");
    for (int i = 0; i < 16; ++i)
    {
        if (ASICameraInfo.SupportedBins[i] == 0)
        {
            break;
        }
        printf("%d ", ASICameraInfo.SupportedBins[i]);
    }
    printf("\n");

    if (ASICameraInfo.IsColorCam)
    {
        printf("  - Color Camera: bayer pattern:%s\n", bayer[ASICameraInfo.BayerPattern]);
    }
    else
    {
        printf("  - Mono camera\n");
    }
    if (ASICameraInfo.IsCoolerCam)
    {
        printf("  - Camera with cooling capabilities\n");
    }

    const char *ver = ASIGetSDKVersion();
    printf("  - SDK version %s\n", ver);

    camera_return_code = ASIInitCamera(camera_id);
    if (camera_return_code == ASI_SUCCESS)
    {
        printf("  - Initialise Camera OK\n");
    }
    else
    {
        printf("*** ERROR: Unable to initialise camera: %s\n", getRetCode(camera_return_code));
        closeUp(1); // Can't do anything so might as well exit.
    }

    ASIGetNumOfControls(camera_id, &camera_controls_count);
    if (debug_level >= 3) // this is really only needed for debugging
    {
        printf("Control Caps:\n");
        for (i = 0; i < camera_controls_count; i++)
        {
            ASIGetControlCaps(camera_id, i, &ControlCaps);
            printf("- %s:\n", ControlCaps.Name);
            printf("   - MinValue        = %ld\n", ControlCaps.MinValue);
            printf("   - MaxValue        = %ld\n", ControlCaps.MaxValue);
            printf("   - DefaultValue    = %ld\n", ControlCaps.DefaultValue);
            printf("   - IsAutoSupported = %d\n",  ControlCaps.IsAutoSupported);
            printf("   - IsWritable      = %d\n",  ControlCaps.IsWritable);
            printf("   - ControlType     = %d\n",  ControlCaps.ControlType);
        }
    }

    if (image_width == 0 || image_height == 0)
    {
        image_width  = image_width_max;
        image_height = image_height_max;
    }
    sensor_width_px  = image_width;
    sensor_height_px = image_height;

    ASIGetControlValue(camera_id, ASI_TEMPERATURE, &temperature_actual, &bAuto);
    printf("- Sensor temperature: %0.2f\n", (float)temperature_actual / 10.0);

    // Handle "auto" image_type.
    if (image_type == AUTO_IMAGE_TYPE_VALUE)
    {
        // If it's a color camera, create color pictures.
        // If it's a mono camera use RAW16 if the image file is a .png, otherwise use RAW8.
        // There is no good way to handle Y8 automatically so it has to be set manually.
        if (ASICameraInfo.IsColorCam)
            image_type = ASI_IMG_RGB24;
        else if (strncmp(imagetype, "png", 4) == 0)
            image_type = ASI_IMG_RAW16;
        else // jpg
            image_type = ASI_IMG_RAW8;
    }

    const char *image_type_name; // displayed in output
    if (image_type == ASI_IMG_RAW16)
    {
        image_type_name = "ASI_IMG_RAW16";
    }
    else if (image_type == ASI_IMG_RGB24)
    {
        image_type_name = "ASI_IMG_RGB24";
    }
    else if (image_type == ASI_IMG_RAW8)
    {
        // Color cameras should use Y8 instead of RAW8.  Y8 is the mono mode for color cameras.
        if (ASICameraInfo.IsColorCam)
        {
            image_type = ASI_IMG_Y8;
            image_type_name      = "ASI_IMG_Y8 (not RAW8 for color cameras)";
        }
        else
        {
            image_type_name = "ASI_IMG_RAW8";
        }
    }
    else
    {
        image_type_name = "ASI_IMG_Y8";
    }

    //-------------------------------------------------------------------------------------------------------
    //-------------------------------------------------------------------------------------------------------

    printf("%s", KGRN);
    printf("\nCapture Settings:\n");
    printf(    "===============\n");
    printf("\n\nGlobal Settings:\n");
    printf(    "================\n");
    printf("              Image Type: %s\n", image_type_name);
    printf("         Image File Name: %s\n", image_file_name);
    printf("       Sensor Resolution: %dx%d\n", image_width, image_height);
    printf("          Cooler Enabled: %s\n", yesNo(cooler_enabled));
    printf("      Target Temperature: %ldC\n", cooler_target_temp_c);
    printf("                 Quality: %d\n", quality);
    printf("                   Gamma: %d\n", gamma);
    printf("      Auto White Balance: %s\n", yesNo(auto_white_balance));
    printf("       Red White Balance: %d%% Manual Offset\n", white_balance_red);
    printf("      Blue White Balance: %d%% Manual Offset\n", white_balance_blue);
    printf("           USB Bandwidth: %d\n", usb_bandwidth);
    printf("      Auto USB Bandwidth: %s\n", yesNo(usb_bandwidth_auto));
    printf("         Daytime capture: %s\n", yesNo(capture_daytime));
    printf("              Flip Image: %d\n", image_flip);
    printf("                Latitude: %s\n", latitude);
    printf("               Longitude: %s\n", longitude);
    printf("           Sun Elevation: %s\n", angle);
    printf("                  Locale: %s\n", locale);
    printf("     Notification Images: %s\n", yesNo(notification_images));
#ifdef USE_HISTOGRAM
    printf("           Histogram Box: %d %d %0.0f %0.0f\n", histogram_box_height_px, histogram_box_width_px,
           histogram_box_center_from_left_pct * 100, histogram_box_center_from_top_pct * 100);
#endif // USE_HISTOGRAM
    printf("             Debug Level: %d\n", debug_level);

    printf("\n\n");
    printf("                Settings: Day / Night\n");
    printf("  ===========================================\n");
    printf("                 Binning: %d / %d\n", binning_day, binning_night);
    printf("    Inter-Exposure Delay: %'dms /  %'dms\n", delay_day_ms, delay_night_ms);
    printf("           Auto Exposure: %s / %s\n", yesNo(exposure_day_auto), yesNo(exposure_night_auto));
    printf("                Exposure: %'1.3fms / %'1.0fms\n", (float)exposure_day_us / US_IN_MS, round(exposure_night_us / US_IN_MS));
    printf("            Max Exposure: n/a  / %'dms\n", exposure_night_max_us);
    printf("               Auto Gain: n/a  / %s\n", yesNo(gain_night_auto));
    printf("                    Gain: n/a  / %d\n", gain_night);
    printf("                Max Gain: n/a  / %d\n", gain_night_max);
    printf("       Brightness Target: %d / %d\n  -- Used by Auto-Exposure / Auto-Gain", brightness_target_day, brightness_target_night);
    printf("    Gain Transition Time: %'d seconds\n", gain_transition_time);
    printf("     ^^ Only used with manual gain when the gain setting differs day/night\n");

    printf("\n\nOverlay Settings:\n");
    printf(    "=================\n");
    printf("      Dark Frame Capture: %s  -- Yes = disables overlays\n", yesNo(darkframe));
    printf("            Text Overlay: %s\n", image_text[0] == '\0' ? "[none]" : image_text);
    printf("     Text Extra Filename: %s\n", image_extra_text_file_name[0] == '\0' ? "[none]" : image_extra_text_file_name);
    printf(" Text Extra Filename Age: %d\n", extra_file_age);
    printf("        Text Line Height: %dpx\n", text_line_height_px);
    printf("           Text Position: %dpx left, %dpx top\n", text_offset_from_left_px, text_offset_from_top_px);
    printf("               Font Name: %d (%s)\n", font_numbers[font_number], font_names[font_number]);
    printf("              Font Color: %d , %d, %d (R, G, B)\n", font_color[0], font_color[1], font_color[2]);
    printf("        Small Font Color: %d , %d, %d (R, G, B)\n", font_color_small[0], font_color_small[1], font_color_small[2]);
    printf("          Font Line Type: %d\n", font_smoothing_options[font_smoothing]);
    printf("               Font Size: %1.1f\n", font_size);
    printf("          Font Thickness: %d\n", font_weight);
    printf("            Font Outline: %s\n", yesNo(font_outline));

#ifdef USE_HISTOGRAM
    printf("      Show Histogram Box: %s  -- Outline around box used for Auto-Exposure calculation\n", yesNo(histogram_box_show));
    printf("     Show Histogram Mean: %s\n", yesNo(histogram_mean_show));
#endif
    printf("               Show Time: %s (format: %s)\n", yesNo(time_show), time_format);
    printf("        Show Temperature: %s\n", yesNo(temperature_show));
    printf("        Temperature Unit: %s\n", temperature_unit);
    printf("           Show Exposure: %s\n", yesNo(exposure_show));
    printf("               Show Gain: %s\n", yesNo(gain_show));
    printf("         Show Brightness: %s\n", yesNo(brightness_show));
    printf("                 Preview: %s\n", yesNo(preview));
    printf("%s\n", KNRM);

    //-------------------------------------------------------------------------------------------------------
    //-------------------------------------------------------------------------------------------------------
    // These configurations apply to both day and night.
    // Other calls to setControl() are done after we know if we're in daytime or nighttime.
    setControl(camera_id, ASI_BANDWIDTHOVERLOAD, usb_bandwidth, usb_bandwidth_auto == 1 ? ASI_TRUE : ASI_FALSE);
    setControl(camera_id, ASI_HIGH_SPEED_MODE, 0, ASI_FALSE); // ZWO sets this in their program
    setControl(camera_id, ASI_WB_R, white_balance_red, auto_white_balance == 1 ? ASI_TRUE : ASI_FALSE);
    setControl(camera_id, ASI_WB_B, white_balance_blue, auto_white_balance == 1 ? ASI_TRUE : ASI_FALSE);
    setControl(camera_id, ASI_GAMMA, gamma, ASI_FALSE);
    setControl(camera_id, ASI_FLIP, image_flip, ASI_FALSE);

    if (ASICameraInfo.IsCoolerCam)
    {
        camera_return_code = setControl(camera_id, ASI_COOLER_ON, cooler_enabled == 1 ? ASI_TRUE : ASI_FALSE, ASI_FALSE);
        if (camera_return_code != ASI_SUCCESS)
        {
            printf("%s", KRED);
            printf(" WARNING: Could not enable cooler: %s, but continuing without it.\n", getRetCode(camera_return_code));
            printf("%s", KNRM);
        }
        camera_return_code = setControl(camera_id, ASI_TARGET_TEMP, cooler_target_temp_c, ASI_FALSE);
        if (camera_return_code != ASI_SUCCESS)
        {
            printf("%s", KRED);
            printf(" WARNING: Could not set cooler temperature: %s, but continuing without it.\n",
                   getRetCode(camera_return_code));
            printf("%s", KNRM);
        }
    }

    if (preview == 1)
    {
        b_display = 1;
        pthread_create(&thread_display, NULL, Display, (void *)&pRgb);
    }

    if (!b_save_run)
    {
        b_save_run = true;
        if (pthread_create(&thread_save, 0, SaveImgThd, 0) != 0)
        {
            b_save_run = false;
        }
    }

    // Initialization
    int exit_code              = 0; // Exit code for main()
    int error_count             = 0; // Number of errors in a row.
    int error_count_max             = 2; // Max number of errors in a row before we exit
    int text_offset_from_left_pre_binning_px        = text_offset_from_left_px;
    int text_offset_from_top_pre_binning_px        = text_offset_from_top_px;
    int font_size_pre_binning      = font_size;
    int font_weight_pre_binning     = font_weight;
    int message_no_daytime_shown = 0; // Have we displayed "not taking picture during day" message, if applicable?
    int gain_change_amount            = 0; // how much to change gain up or down

    // If autogain is on, our adjustments to gain will get overwritten by the camera
    // so don't transition.
    // gain_transition_time of 0 means don't adjust gain.
    // No need to adjust gain if day and night gain are the same.
    if (gain_day_auto == 1 || gain_night_auto == 1)
    {
        gain_adjust_enable = false;
        displayDebugText("Info: Will NOT adjust gain at transitions due to auto-gain in use.",1);
    }
    else if(gain_transition_time == 0)
    {
        gain_adjust_enable = false;
        displayDebugText("Info: Will NOT adjust gain at transitions due Gain Transition Time of 0.",1);
    }
    else if( gain_day == gain_night )
    {
        gain_adjust_enable = false;
        displayDebugText("Info: Will NOT adjust gain at transitions due to Day and Night gain being equal.",1);
    }
    else if( darkframe == 1)
    {
        gain_adjust_enable = false;
        displayDebugText("Info: Will NOT adjust gain at transitions due Dark Frame Capture.",1);
    }
    else
    {
        gain_adjust_enable = true;
        snprintf(debugText,499,"Info: Will adjust gain at day/night transition over the course of %'d seconds.", gain_transition_time);
        displayDebugText(debugText,1);
    }

    displayDebugText("Press Ctrl+C or stop the service to exit.\n\n",0);

    if (continuous_exposure)
    {
        camera_return_code = ASIStartVideoCapture(camera_id);
        if (camera_return_code != ASI_SUCCESS)
        {
            snprintf(debugText,499,"*** ERROR: Unable to start video capture: %s\n", getRetCode(camera_return_code));
            displayDebugText(debugText,0);
            closeUp(99);
        }
    }

    while (b_main)
    {
        std::string last_day_or_night;

        // Find out if it is currently DAY or NIGHT
        calculateDayOrNight(latitude, longitude, angle);

        if (!darkframe)
            gain_adjust_current = resetGainTransitionVariables(gain_day, gain_night);

        last_day_or_night = day_or_night;
        if (darkframe)
        {
            // We're doing dark frames so turn off autoexposure and autogain, and use
            // nightime gain, delay, max exposure, bin, and brightness to mimic a nightime shot.
            exposure_auto_current_value = ASI_FALSE;
            setControl(camera_id, ASI_EXPOSURE, exposure_current_value, exposure_auto_current_value);
            exposure_night_auto = 0;
            gain_auto_current_value      = ASI_FALSE;
            // Don't need to set ASI_AUTO_MAX_GAIN since we're not using auto gain
            setControl(camera_id, ASI_GAIN, gain_night, ASI_FALSE);
            gain_current_value       = gain_night;
            delay_current      = delay_night_ms;
            exposure_current_value   = exposure_night_max_us * US_IN_MS;
            binning_current        = binning_night;
            brightness_current = brightness_target_night;

            displayDebugText("Taking dark frames...\n", 0);

            if (notification_images)
            {
                system("scripts/copy_notification_image.sh DarkFrames &");
            }
        }
        else if (day_or_night == "DAY")
        {
            // Setup the daytime capture parameters
            if (end_of_night == true) // Execute end of night script
            {
                sprintf(text_buffer, "Processing end of night data\n");
                displayDebugText(text_buffer, 0);
                system("scripts/end_of_night.sh &");
                end_of_night            = false;
                message_no_daytime_shown = 0;
            }

            if (capture_daytime != 1)
            {
                // Only display messages once a day.
                if (message_no_daytime_shown == 0)
                {
                    if (notification_images)
                    {
                        system("scripts/copy_notification_image.sh CameraOffDuringDay &");
                    }
                    displayDebugText("It's daytime... we're not saving images.\nPress Ctrl+C or stop the service to end this process.\n", 0);
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
            else
            {
                sprintf(text_buffer, "==========\n=== Starting daytime capture ===\n==========\n");
                displayDebugText(text_buffer, 0);
                sprintf(text_buffer, "Saving images with delay of %'d ms (%d sec)\n\n", delay_day_ms, delay_day_ms / MS_IN_SEC);
                displayDebugText(text_buffer, 0);
#ifdef USE_HISTOGRAM
                // Don't use camera auto exposure since we mimic it ourselves.
                if (exposure_day_auto == 1)
                {
                    sprintf(text_buffer, "Turning off daytime auto-exposure to use histogram exposure.\n");
                    displayDebugText(text_buffer, 2);
                    exposure_auto_current_value = ASI_FALSE;
                }
#else
                exposure_auto_current_value = exposure_day_auto ? ASI_TRUE : ASI_FALSE;
#endif
                brightness_current = brightness_target_day;
                delay_current      = delay_day_ms;
                binning_current        = binning_day;

                // If we went from Night to Day, then exposure_current_value will be the last night
                // exposure so leave it if we're using auto-exposure so there's a seamless change from
                // Night to Day, i.e., if the exposure was fine a minute ago it will likely be fine now.
                // On the other hand, if this program just started or we're using manual exposures,
                // use what the user specified.
                if (exposures_counter == 0 || exposure_day_auto == ASI_FALSE)
                {
                    exposure_current_value = exposure_day_us;
                }
                else
                {
                    sprintf(text_buffer, "Using last night exposure of %'ld us (%'.2lf ms)\n", exposure_current_value,
                            (float)exposure_current_value / US_IN_MS);
                    displayDebugText(text_buffer, 2);
                }
#ifndef USE_HISTOGRAM
                setControl(camera_id, ASI_EXPOSURE, exposure_current_value, exposure_auto_current_value);
#endif
                setControl(camera_id, ASI_AUTO_MAX_EXP, exposure_camera_max_us / US_IN_MS, ASI_FALSE); // need ms
                gain_current_value = gain_day; // must come before determineGainChange() below
                if (gain_adjust_current)
                {
                    // we did some nightime images so adjust gain
                    gain_changes_count = 0;
                    gain_change_amount     = determineGainChange(gain_day, gain_night);
                }
                else
                {
                    gain_change_amount = 0;
                }
                gain_auto_current_value = gain_day_auto ? ASI_TRUE : ASI_FALSE;
                setControl(camera_id, ASI_GAIN, gain_current_value + gain_change_amount, gain_auto_current_value);
                // We don't have a separate asiDayMaxGain, so set to night one
                setControl(camera_id, ASI_AUTO_MAX_GAIN, gain_night_max, ASI_FALSE);
            }
        }
        else // NIGHT
        {
            sprintf(text_buffer, "==========\n=== Starting nighttime capture ===\n==========\n");
            displayDebugText(text_buffer, 0);

            // Setup the night time capture parameters
            if (exposure_night_auto == 1)
            {
                exposure_auto_current_value = ASI_TRUE;
                setControl(camera_id, ASI_AUTO_MAX_EXP, exposure_night_max_us, ASI_FALSE);
                printf("Saving auto exposed night images with delay of %'d ms (%d sec)\n\n", delay_night_ms,
                       delay_night_ms / MS_IN_SEC);
            }
            else
            {
                exposure_auto_current_value = ASI_FALSE;
                printf("Saving %ds manual exposure night images with delay of %'d ms (%d sec)\n\n",
                       (int)round(exposure_current_value / US_IN_SEC), delay_night_ms, delay_night_ms / MS_IN_SEC);
            }

            brightness_current = brightness_target_night;
            delay_current      = delay_night_ms;
            binning_current        = binning_night;
            if (exposures_counter == 0 || exposure_night_auto == ASI_FALSE)
            {
                exposure_current_value = exposure_night_us;
            }
#ifndef USE_HISTOGRAM
            setControl(camera_id, ASI_EXPOSURE, exposure_current_value, exposure_auto_current_value);
#endif
            gain_current_value = gain_night; // must come before determineGainChange() below
            if (gain_adjust_current)
            {
                // we did some daytime images so adjust gain
                gain_changes_count = 0;
                gain_change_amount     = determineGainChange(gain_day, gain_night);
            }
            else
            {
                gain_change_amount = 0;
            }
            gain_auto_current_value = gain_night_auto ? ASI_TRUE : ASI_FALSE;
            setControl(camera_id, ASI_GAIN, gain_current_value + gain_change_amount, gain_auto_current_value);
            setControl(camera_id, ASI_AUTO_MAX_GAIN, gain_night_max, ASI_FALSE);
        }

        // Adjusting variables for chosen binning
        image_height     = sensor_height_px / binning_current;
        image_width      = sensor_width_px / binning_current;
        text_offset_from_left_px     = text_offset_from_left_pre_binning_px / binning_current;
        text_offset_from_top_px     = text_offset_from_top_pre_binning_px / binning_current;
        font_size   = font_size_pre_binning / binning_current;
        font_weight  = font_weight_pre_binning / binning_current;
        buffer_size = image_width * image_height * bytesPerPixel((ASI_IMG_TYPE)image_type);
        if (exposures_counter > 0 && binning_day != binning_night)
        {
            // No need to print after first time if the binning didn't change.
            sprintf(debugText, "Buffer size: %ld\n", buffer_size);
            displayDebugText(debugText, 2);
        }

        if (image_type == ASI_IMG_RAW16)
        {
            pRgb.create(cv::Size(image_width, image_height), CV_16UC1);
        }
        else if (image_type == ASI_IMG_RGB24)
        {
            pRgb.create(cv::Size(image_width, image_height), CV_8UC3);
        }
        else // RAW8 and Y8
        {
            pRgb.create(cv::Size(image_width, image_height), CV_8UC1);
        }

        camera_return_code = ASISetROIFormat(camera_id, image_width, image_height, binning_current, (ASI_IMG_TYPE)image_type);
        if (camera_return_code)
        {
            printf("ASISetROIFormat(%d, %dx%d, %d, %d) = %s\n", camera_id, image_width, image_height, binning_current, image_type,
                   getRetCode(camera_return_code));
            closeUp(1);
        }
        setControl(camera_id, ASI_BRIGHTNESS, brightness_current, ASI_FALSE); // ASI_BRIGHTNESS == ASI_OFFSET

        // Here and below, indent sub-messages with "  > " so it's clear they go with the un-indented line.
        // This simply makes it easier to see things in the log file.

        // As of April 2021 there's a bug that causes the first 3 images to be identical,
        // so take 3 short ones but don't save them.
        // On the ASI178MC the shortest time is 10010 us; it may be higher on other cameras,
        // so use a higher value like 30,000 us to be safe.
        // Only do this once.
        if (exposures_counter == 0)
        {
            displayDebugText("===Taking 3 images to clear buffer...\n", 2);
            // turn off auto exposure
            ASI_BOOL exposure_auto_saved_value = exposure_auto_current_value;
            exposure_auto_current_value        = ASI_FALSE;
            for (i = 1; i <= 3; i++)
            {
                // don't count these as "real" exposures, so don't increment exposures_counter.
                camera_return_code =
                    takeOneExposure(camera_id, SHORT_EXPOSURE, pRgb.data, image_width, image_height, (ASI_IMG_TYPE)image_type);
                if (camera_return_code != ASI_SUCCESS)
                {
                    sprintf(debugText, "buffer clearing exposure %d failed: %s\n", i, getRetCode(camera_return_code));
                    displayDebugText(debugText, 0);
                    error_count++;
                    sleep(2); // sometimes sleeping keeps errors from reappearing
                }
            }
            if (error_count >= error_count_max)
            {
                b_main    = false;
                exit_code = 2;
                break;
            }

            // Restore correct exposure times and auto-exposure mode.
            exposure_auto_current_value = exposure_auto_saved_value;
            setControl(camera_id, ASI_EXPOSURE, exposure_current_value, exposure_auto_current_value);
            sprintf(debugText, "...DONE.  Reset exposure to %'ld us\n", exposure_current_value);
            displayDebugText(debugText, 2);
            // END of bug code
        }

        int mean = 0;

        while (b_main && last_day_or_night == day_or_night)
        {
            // date/time is added to many log entries to make it easier to associate them
            // with an image (which has the date/time in the filename).
            timeval t;
            t = getTimeval();
            char exposureStart[128];
            char f[10] = "%F %T";
            sprintf(exposureStart, "%s", formatTime(t, f));
            sprintf(text_buffer, "STARTING EXPOSURE at: %s\n", exposureStart);
            displayDebugText(text_buffer, 0);

            // Get start time for overlay.  Make sure it has the same time as exposureStart.
            if (time_show == 1)
                sprintf(time_buffer, "%s", formatTime(t, time_format));

            camera_return_code = takeOneExposure(camera_id, exposure_current_value, pRgb.data, image_width, image_height, (ASI_IMG_TYPE)image_type);
            if (camera_return_code == ASI_SUCCESS)
            {
                error_count = 0;
                exposures_counter++;

#ifdef USE_HISTOGRAM
                int histogram_used = 0; // did we use the histogram method?
                // We don't use this at night since the ZWO bug is only when it's light outside.
                if (day_or_night == "DAY" && exposure_day_auto && !darkframe &&
                    exposure_current_value <= exposure_camera_max_us)
                {
                    int histogram_acceptable_min;
                    int histogram_acceptable_max;
                    int mean_low_threshold;
                    int mean_low;
                    int round_to_me_us;

                    histogram_used = 1; // we are using the histogram code on this exposure
                    int histogram[256];
                    computeHistogram(pRgb.data, image_width, image_height, (ASI_IMG_TYPE)image_type, histogram);
                    mean = calculateHistogramMean(histogram);
                    // "last_OK_exposure" is the exposure time of the last OK
                    // image (i.e., mean < 255).
                    // The intent is to keep track of the last OK exposure in case the final
                    // exposure we calculate is no good, we can go back to the last OK one.
                    long last_OK_exposure = exposure_current_value;

                    int historgram_attempts_count = 0;
                    long exposure_new = 0;

                    int exposure_minimum      = 100;
                    long exposure_min_temp = exposure_minimum;
                    long exposure_max_temp = exposure_night_max_us * US_IN_MS;

                    // Got these by trial and error.  They are more-or-less half the max of 255.
                    histogram_acceptable_min = 120;
                    histogram_acceptable_max = 136;
                    mean_low_threshold          = 5;
                    mean_low                = 15;

                    round_to_me_us = 5; // round exposures to this many microseconds

                    if (brightness_target_day != DEFAULT_BRIGHTNESS_TARGET)
                    {
                        // Adjust brightness based on brightness_target_day.
                        // The default value has no adjustment.
                        // The only way we can do this easily is via adjusting the exposure.
                        // We could apply a stretch to the image, but that's more difficult.
                        // Sure would be nice to see how ZWO handles this variable.
                        // We asked but got a useless reply.
                        // Values below the default make the image darker; above make it brighter.

                        float exposure_adjustment_pct = 0.0, multiples_count;

                        // Adjustments of DEFAULT_BRIGHTNESS_TARGET up or down make the image this much darker/lighter.
                        // Don't want the max brightness to give pure white.
                        //xxx May have to play with this number, but it seems to work ok.
                        float exposure_adjustment_amount_per_multiple = 0.12; // 100 * this number is the percent to change

                        // The amount doesn't change after being set, so only display once.
                        static int message_displayed = 0;
                        if (message_displayed == 0)
                        {
                            // Determine the adjustment amount - only done once.
                            // See how many multiples we're different.
                            // If asiDayBrightnes < DEFAULT_BRIGHTNESS_TARGET the multiples_count will be negative,
                            // which is ok - it just means the multiplier will be less than 1.
                            multiples_count       = (brightness_target_day - DEFAULT_BRIGHTNESS_TARGET) / DEFAULT_BRIGHTNESS_TARGET;
                            exposure_adjustment_pct = 1 + (multiples_count * exposure_adjustment_amount_per_multiple);
                            sprintf(text_buffer, "  > >>> Adjusting exposure %.1f%% for daybrightness\n",
                                    (exposure_adjustment_pct - 1) * 100);
                            displayDebugText(text_buffer, 2);
                            message_displayed = 1;
                        }

                        // Now adjust the variables
                        exposure_minimum *= exposure_adjustment_pct;
                        mean_low_threshold *= exposure_adjustment_pct;
                        mean_low *= exposure_adjustment_pct;
                        histogram_acceptable_min *= exposure_adjustment_pct;
                        histogram_acceptable_max *= exposure_adjustment_pct;
                    }

                    while ((mean < histogram_acceptable_min || mean > histogram_acceptable_max) &&
                           ++histogram_attempts_count <= histogram_attempts_count_max)
                    {
                        sprintf(text_buffer,
                                "  > Attempt %i,  current exposure %'ld us,  mean %d,  temp min exposure %ld us,  "
                                "exposure_max_temp %'ld us",
                                histogram_attempts_count, exposure_current_value, mean, exposure_min_temp, exposure_max_temp);
                        displayDebugText(text_buffer, 2);

                        std::string why; // Why did we adjust the exposure?  For debugging
                        int num = 0;
                        if (mean >= 254)
                        {
                            exposure_new     = exposure_current_value * 0.4;
                            exposure_max_temp = exposure_current_value - round_to_me_us;
                            why             = "mean >= max";
                            num             = 254;
                        }
                        else
                        {
                            //  The code below takes into account how far off we are from an acceptable mean.
                            //  There's probably a simplier way to do this, like adjust by some multiple of
                            //  how far of we are.  That exercise is left to the reader...
                            last_OK_exposure = exposure_current_value;
                            if (mean < mean_low_threshold)
                            {
                                // The cameras don't appear linear at this low of a level,
                                // so really crank it up to get into the linear area.
                                exposure_new     = exposure_current_value * 20;
                                exposure_min_temp = exposure_current_value + round_to_me_us;
                                why             = "mean < mean_low_threshold";
                                num             = mean_low_threshold;
                            }
                            else if (mean < mean_low)
                            {
                                exposure_new     = exposure_current_value * 5;
                                exposure_min_temp = exposure_current_value + round_to_me_us;
                                why             = "mean < mean_low";
                                num             = mean_low;
                            }
                            else if (mean < (histogram_acceptable_min * 0.6))
                            {
                                exposure_new     = exposure_current_value * 2.5;
                                exposure_min_temp = exposure_current_value + round_to_me_us;
                                why             = "mean < (histogram_acceptable_min * 0.6)";
                                num             = histogram_acceptable_min * 0.6;
                            }
                            else if (mean < histogram_acceptable_min)
                            {
                                exposure_new     = exposure_current_value * 1.1;
                                exposure_min_temp = exposure_current_value + round_to_me_us;
                                why             = "mean < histogram_acceptable_min";
                                num             = histogram_acceptable_min;
                            }
                            else if (mean > (histogram_acceptable_max * 1.6))
                            {
                                exposure_new     = exposure_current_value * 0.7;
                                exposure_max_temp = exposure_current_value - round_to_me_us;
                                why             = "mean > (histogram_acceptable_max * 1.6)";
                                num             = (histogram_acceptable_max * 1.6);
                            }
                            else if (mean > histogram_acceptable_max)
                            {
                                exposure_new     = exposure_current_value * 0.9;
                                exposure_max_temp = exposure_current_value - round_to_me_us;
                                why             = "mean > histogram_acceptable_max";
                                num             = histogram_acceptable_max;
                            }
                        }

                        exposure_new = roundTo(exposure_new, round_to_me_us);
                        exposure_new = std::max(exposure_min_temp, exposure_new);
                        exposure_new = std::min(exposure_new, exposure_max_temp);
                        exposure_new = std::max(exposure_min_temp, exposure_new);
                        exposure_new = std::min(exposure_new, exposure_camera_max_us);

                        sprintf(text_buffer, ",  new exposure %'ld us\n", exposure_new);
                        displayDebugText(text_buffer, 2);

                        if (exposure_new == exposure_current_value)
                        {
                            // We can't find a better exposure so stick with this one
                            // or the last OK one.  If the last exposure had a mean >= 254,
                            // use the most recent exposure that was OK.
                            if (mean >= 254 && 0)
                            { // xxxxxxxxxxxxxxxxxxxx This needs work so disabled
                                exposure_current_value = last_OK_exposure;
                                sprintf(text_buffer, "  > !!! Resetting to last OK exposure of '%ld us\n",
                                        exposure_current_value);
                                displayDebugText(text_buffer, 2);
                                takeOneExposure(camera_id, exposure_current_value, pRgb.data, image_width, image_height,
                                                (ASI_IMG_TYPE)image_type);
                                computeHistogram(pRgb.data, image_width, image_height, (ASI_IMG_TYPE)image_type, histogram);
                                mean = calculateHistogramMean(histogram);
                            }
                            break;
                        }

                        exposure_current_value = exposure_new;

                        sprintf(text_buffer, "  > !!! Retrying @ %'ld us because '%s (%d)'\n", exposure_current_value,
                                why.c_str(), num);
                        displayDebugText(text_buffer, 2);
                        takeOneExposure(camera_id, exposure_current_value, pRgb.data, image_width, image_height, (ASI_IMG_TYPE)image_type);
                        computeHistogram(pRgb.data, image_width, image_height, (ASI_IMG_TYPE)image_type, histogram);
                        mean = calculateHistogramMean(histogram);
                    }
                    if (histogram_attempts_count > histogram_attempts_count_max)
                    {
                        sprintf(text_buffer, "  > max histogram attempts reached - using exposure of %'ld us with mean %d\n",
                                exposure_current_value, mean);
                        displayDebugText(text_buffer, 2);
                    }
                    else if (histogram_attempts_count > 1)
                    {
                        sprintf(text_buffer, "  > Using exposure of %'ld us with mean %d\n", exposure_current_value, mean);
                        displayDebugText(text_buffer, 2);
                    }
                    else if (histogram_attempts_count == 1)
                    {
                        sprintf(
                            text_buffer,
                            "  > Current exposure of %'ld us with mean %d was ok - no additional histogram attempts needed.\n",
                            exposure_current_value, mean);
                        displayDebugText(text_buffer, 2);
                    }
                    exposure_actual_us = exposure_current_value;
                }
                else
                {
                    exposure_current_value = exposure_actual_us;
                }
#endif
                // If darkframe mode is off, add overlay text to the image
                if (!darkframe)
                {
                    int text_offset_vertical_current_px = 0;

                    if (time_show == 1)
                    {
                        // The time and image_text are in the larger font; everything else is in smaller font.
                        cvText(pRgb, time_buffer, text_offset_from_left_px, text_offset_from_top_px + (text_offset_vertical_current_px / binning_current), font_size * 0.1, font_weight,
                               font_smoothing_options[font_smoothing], font_numbers[font_number], font_color, image_type, font_outline);
                        text_offset_vertical_current_px += text_line_height_px;
                    }

                    if (image_text[0] != '\0')
                    {
                        cvText(pRgb, image_text, text_offset_from_left_px, text_offset_from_top_px + (text_offset_vertical_current_px / binning_current), font_size * 0.1, font_weight,
                               font_smoothing_options[font_smoothing], font_numbers[font_number], font_color, image_type, font_outline);
                        text_offset_vertical_current_px += text_line_height_px;
                    }

                    if (temperature_show == 1)
                    {
                        char C[20] = { 0 }, F[20] = { 0 };
                        if (strncmp(temperature_unit, "C", 2) == 0 || strncmp(temperature_unit, "B", 2) == 0)
                        {
                            sprintf(C, "  %.0fC", (float)temperature_actual / 10);
                        }
                        if (strncmp(temperature_unit, "F", 2) == 0 || strncmp(temperature_unit, "B", 2) == 0)
                        {
                            sprintf(F, "  %.0fF", (((float)temperature_actual / 10 * 1.8) + 32));
                        }
                        sprintf(tmp_buffer, "Sensor: %s %s", C, F);
                        cvText(pRgb, tmp_buffer, text_offset_from_left_px, text_offset_from_top_px + (text_offset_vertical_current_px / binning_current),
                               font_size * FONT_SIZE_SMALL_MULTIPLIER, font_weight, font_smoothing_options[font_smoothing],
                               font_numbers[font_number], font_color_small, image_type, font_outline);
                        text_offset_vertical_current_px += text_line_height_px;
                    }

                    if (exposure_show == 1)
                    {
                        // Indicate when the time to take the exposure is less than the reported exposure time
                        if (exposure_actual_us == exposure_current_value)
                        {
                            tmp_buffer2[0] = '\0';
                        }
                        else
                        {
                            sprintf(tmp_buffer2, " actual %'.2lf ms)", (double)exposure_actual_us / US_IN_MS);
                        }
                        if (exposure_actual_us >=
                            (1 * US_IN_SEC)) // display in seconds if >= 1 second, else in ms
                        {
                            sprintf(tmp_buffer, "Exposure: %'.2f s%s", (float)exposure_current_value / US_IN_SEC, tmp_buffer2);
                        }
                        else
                        {
                            sprintf(tmp_buffer, "Exposure: %'.2f ms%s", (float)exposure_current_value / US_IN_MS, tmp_buffer2);
                        }
                        // Indicate if in auto exposure mode.
                        if (exposure_auto_current_value == ASI_TRUE)
                        {
                            strcat(tmp_buffer, " (auto)");
                        }
                        cvText(pRgb, tmp_buffer, text_offset_from_left_px, text_offset_from_top_px + (text_offset_vertical_current_px / binning_current),
                               font_size * FONT_SIZE_SMALL_MULTIPLIER, font_weight, font_smoothing_options[font_smoothing],
                               font_numbers[font_number], font_color_small, image_type, font_outline);
                        text_offset_vertical_current_px += text_line_height_px;
                    }

                    if (gain_show == 1)
                    {
                        sprintf(tmp_buffer, "Gain: %ld", gain_actual);

                        // Indicate if in auto gain mode.
                        if (gain_auto_current_value == ASI_TRUE)
                        {
                            strcat(tmp_buffer, " (auto)");
                        }
                        // Indicate if in gain transition mode.
                        if (gain_change_amount != 0)
                        {
                            char x[20];
                            sprintf(x, " (adj: %+d)", gain_change_amount);
                            strcat(tmp_buffer, x);
                        }

                        cvText(pRgb, tmp_buffer, text_offset_from_left_px, text_offset_from_top_px + (text_offset_vertical_current_px / binning_current),
                               font_size * FONT_SIZE_SMALL_MULTIPLIER, font_weight, font_smoothing_options[font_smoothing],
                               font_numbers[font_number], font_color_small, image_type, font_outline);
                        text_offset_vertical_current_px += text_line_height_px;
                    }
                    if (gain_adjust_current)
                    {
                        // Determine if we need to change the gain on the next image.
                        // This must come AFTER the "gain_show" above.
                        gain_change_amount = determineGainChange(gain_day, gain_night);
                        setControl(camera_id, ASI_GAIN, gain_current_value + gain_change_amount, gain_auto_current_value);
                    }

                    if (brightness_show == 1)
                    {
                        sprintf(tmp_buffer, "Brightness: %d", brightness_current);
                        cvText(pRgb, tmp_buffer, text_offset_from_left_px, text_offset_from_top_px + (text_offset_vertical_current_px / binning_current),
                               font_size * FONT_SIZE_SMALL_MULTIPLIER, font_weight, font_smoothing_options[font_smoothing],
                               font_numbers[font_number], font_color_small, image_type, font_outline);
                        text_offset_vertical_current_px += text_line_height_px;
                    }

#ifdef USE_HISTOGRAM
                    if (histogram_mean_show && histogram_used)
                    {
                        sprintf(tmp_buffer, "Histogram mean: %d", mean);
                        cvText(pRgb, tmp_buffer, text_offset_from_left_px, text_offset_from_top_px + (text_offset_vertical_current_px / binning_current),
                               font_size * FONT_SIZE_SMALL_MULTIPLIER, font_weight, font_smoothing_options[font_smoothing],
                               font_numbers[font_number], font_color_small, image_type, font_outline);
                        text_offset_vertical_current_px += text_line_height_px;
                    }
                    if (histogram_box_show && histogram_used)
                    {
                        // Draw a rectangle where the histogram box is.
                        int lt = cv::LINE_AA, thickness = 2;
                        cv::Point from1, to1, from2, to2;
                        int X1 = (image_width * histogram_box_center_from_left_pct) - (histogram_box_height_px / 2);
                        int X2 = X1 + histogram_box_height_px;
                        int Y1 = (image_height * histogram_box_center_from_top_pct) - (histogram_box_width_px / 2);
                        int Y2 = Y1 + histogram_box_width_px;
                        // Put a black and white line one next to each other so they
                        // can be seen in day and night images.
                        // The black line is on the outside; the white on the inside.
                        // cv::line takes care of bytes per pixel.

                        // top lines
                        from1 = cv::Point(X1, Y1);
                        to1   = cv::Point(X2, Y1);
                        from2 = cv::Point(X1, Y1 + thickness);
                        to2   = cv::Point(X2, Y1 + thickness);
                        cv::line(pRgb, from1, to1, cv::Scalar(0, 0, 0), thickness, lt);
                        cv::line(pRgb, from2, to2, cv::Scalar(255, 255, 255), thickness, lt);

                        // right lines
                        from1 = cv::Point(X2, Y1);
                        to1   = cv::Point(X2, Y2);
                        from2 = cv::Point(X2 - thickness, Y1 + thickness);
                        to2   = cv::Point(X2 - thickness, Y2 - thickness);
                        cv::line(pRgb, from1, to1, cv::Scalar(0, 0, 0), thickness, lt);
                        cv::line(pRgb, from2, to2, cv::Scalar(255, 255, 255), thickness, lt);

                        // bottom lines
                        from1 = cv::Point(X1, Y2);
                        to1   = cv::Point(X2, Y2);
                        from2 = cv::Point(X1, Y2 - thickness);
                        to2   = cv::Point(X2, Y2 - thickness);
                        cv::line(pRgb, from1, to1, cv::Scalar(0, 0, 0), thickness, lt);
                        cv::line(pRgb, from2, to2, cv::Scalar(255, 255, 255), thickness, lt);

                        // left lines
                        from1 = cv::Point(X1, Y1);
                        to1   = cv::Point(X1, Y2);
                        from2 = cv::Point(X1 + thickness, Y1 + thickness);
                        to2   = cv::Point(X1 + thickness, Y2 - thickness);
                        cv::line(pRgb, from1, to1, cv::Scalar(0, 0, 0), thickness, lt);
                        cv::line(pRgb, from2, to2, cv::Scalar(255, 255, 255), thickness, lt);
                    }
#endif
                    /**
                     * Display extra text if required. The extra text is read from the provided file. If the
                     * age of the file exceeds the specified limit then the text in the file is not displayed
                     * this is to prevent situations where the code updating the text file stops working.
                     **/
                    if (image_extra_text_file_name[0] != '\0')
                    {
                        bool extra_file_use = true;
                        // Display these messages every time, since it's possible the user will correct the
                        // issue while we're running.
                        if (access(image_extra_text_file_name, F_OK) == -1)
                        {
                            extra_file_use = false;
                            displayDebugText("  > *** WARNING: Extra Text File Does Not Exist So Ignoring It\n", 1);
                        }
                        else if (access(image_extra_text_file_name, R_OK) == -1)
                        {
                            displayDebugText("  > *** ERROR: Cannot Read From Extra Text File So Ignoring It\n", 1);
                            extra_file_use = false;
                        }

                        if (extra_file_use)
                        {
                            FILE *fp = fopen(image_extra_text_file_name, "r");

                            if (fp != NULL)
                            {
                                bool b_add_extra = false;
                                if (extra_file_age > 0)
                                {
                                    struct stat buffer;
                                    if (stat(image_extra_text_file_name, &buffer) == 0)
                                    {
                                        struct tm extra_file_modified_time = *localtime(&buffer.st_mtime);

                                        time_t now          = time(NULL);
                                        double ageInSeconds = difftime(now, mktime(&extra_file_modified_time));
                                        sprintf(text_buffer, "  > Extra Text File (%s) Modified %.1f seconds ago",
                                                image_extra_text_file_name, ageInSeconds);
                                        displayDebugText(text_buffer, 1);
                                        if (ageInSeconds < extra_file_age)
                                        {
                                            displayDebugText(", so Using It\n", 1);
                                            b_add_extra = true;
                                        }
                                        else
                                        {
                                            displayDebugText(", so Ignoring\n", 1);
                                        }
                                    }
                                    else
                                    {
                                        displayDebugText("  > *** ERROR: Stat Of Extra Text File Failed !\n", 0);
                                    }
                                }
                                else
                                {
                                    // xxx Should really only display this once, maybe at program start.
                                    displayDebugText("  > Extra Text File Age Disabled So Displaying Anyway\n", 1);
                                    b_add_extra = true;
                                }
                                if (b_add_extra)
                                {
                                    char *line = NULL;
                                    size_t len = 0;
                                    int slen   = 0;
                                    while (getline(&line, &len, fp) != -1)
                                    {
                                        slen = strlen(line);
                                        if (slen >= 2 && (line[slen - 2] == 10 || line[slen - 2] == 13))
                                        { // LF, CR
                                            line[slen - 2] = '\0';
                                        }
                                        else if (slen >= 1 && (line[slen - 1] == 10 || line[slen - 1] == 13))
                                        {
                                            line[slen - 1] = '\0';
                                        }

                                        cvText(pRgb, line, text_offset_from_left_px, text_offset_from_top_px + (text_offset_vertical_current_px / binning_current),
                                               font_size * FONT_SIZE_SMALL_MULTIPLIER, font_weight, font_smoothing_options[font_smoothing],
                                               font_numbers[font_number], font_color_small, image_type, font_outline);
                                        text_offset_vertical_current_px += text_line_height_px;
                                    }
                                }
                                fclose(fp);
                            }
                            else
                            {
                                displayDebugText("  > *** WARNING: Failed To Open Extra Text File\n", 0);
                            }
                        }
                    }
                }

#ifndef USE_HISTOGRAM
                if (exposure_auto_current_value == ASI_TRUE)
                {
                    // Retrieve the current Exposure for smooth transition to night time
                    // as long as auto-exposure is enabled during night time
                    exposure_current_value = exposure_actual_us;
                }
#endif

                // Save the image
                if (b_saving_image == false)
                {
                    sprintf(text_buffer, "  > Saving image '%s' that started at %s", image_file_name, exposureStart);
                    displayDebugText(text_buffer, 0);

                    pthread_mutex_lock(&mtx_SaveImg);
                    // Display the time it took to save an image, for debugging.
                    int64 st = cv::getTickCount();
                    pthread_cond_signal(&cond_save_start);
                    int64 et = cv::getTickCount();
                    pthread_mutex_unlock(&mtx_SaveImg);

                    sprintf(text_buffer, "  (%.0f us)\n", timeDiff(st, et));
                    displayDebugText(text_buffer, 0);
                }
                else
                {
                    // Hopefully the user can use the time it took to save a file to disk
                    // to help determine why they are getting this warning.
                    // Perhaps their disk is very slow or their delay is too short.
                    sprintf(text_buffer, "  > WARNING: currently saving an image; can't save new one at %s.\n",
                            exposureStart);
                    displayDebugText(text_buffer, 0);
                }

                if (gain_night_auto == 1 && day_or_night == "NIGHT" && !darkframe)
                {
                    ASIGetControlValue(camera_id, ASI_GAIN, &gain_actual, &bAuto);
                    sprintf(text_buffer, "  > Auto Gain value: %ld\n", gain_actual);
                    displayDebugText(text_buffer, 1);
                }

                if (exposure_auto_current_value == ASI_TRUE)
                {
#ifndef USE_HISTOGRAM
                    if (day_or_night == "DAY")
                    {
                        exposure_current_value = exposure_actual_us;
                    }
#endif

                    // Delay applied before next exposure
                    if (day_or_night == "NIGHT" && exposure_night_auto == 1 &&
                        exposure_actual_us < (exposure_night_max_us * US_IN_MS) && !darkframe)
                    {
                        // If using auto-exposure and the actual exposure is less than the max,
                        // we still wait until we reach maxexposure, then wait for the delay period.
                        // This is important for a constant frame rate during timelapse generation.
                        // This doesn't apply during the day since we don't have a max time then.
                        int s = (exposure_night_max_us * US_IN_MS) - exposure_actual_us; // to get to max
                        s += delay_current * US_IN_MS; // Add standard delay amount
                        sprintf(text_buffer, "  > Sleeping: %'d ms\n", s / US_IN_MS);
                        displayDebugText(text_buffer, 0);
                        usleep(s); // usleep() is in microseconds
                    }
                    else
                    {
                        // Sleep even if taking dark frames so the sensor can cool between shots like it would
                        // do on a normal night.  With no delay the sensor may get hotter than it would at night.
                        sprintf(text_buffer, "  > Sleeping from %s exposure: %'d ms (%.0f sec)\n",
                                darkframe ? "dark frame" : "auto", delay_current, (float)delay_current / US_IN_MS);
                        displayDebugText(text_buffer, 0);
                        usleep(delay_current * US_IN_MS);
                    }
                }
                else
                {
                    std::string s;
                    if (darkframe)
                    {
                        s = "dark frame";
                    }
                    else
                    {
                        s = "manual";
                    }
#ifdef USE_HISTOGRAM
                    if (histogram_used == 1)
                    {
                        s = "histogram";
                    }
#endif
                    sprintf(text_buffer, "  > Sleeping from %s exposure: %'d ms\n", s.c_str(), delay_current);
                    displayDebugText(text_buffer, 0);
                    usleep(delay_current * US_IN_MS);
                }
                calculateDayOrNight(latitude, longitude, angle);
            }
            else
            {
                // Once takeOneExposure() fails with a timeout, it seems to always fail,
                // even with extremely large timeout values, so apparently ASI_ERROR_TIMEOUT doesn't
                // necessarily mean it's timing out.  I think it means the camera went away,
                // so exit which will cause us to be restarted.
                error_count++;
                sleep(2);
                if (error_count >= error_count_max)
                {
                    b_main    = false; // get out of inner and outer loop
                    exit_code = 2;
                }
            }
        }
        if (last_day_or_night == "NIGHT")
        {
            end_of_night = true;
        }
    }

    closeUp(exit_code);
}
