#ifndef CAPTURE_H

#define CAPTURE_H
#include <fstream>
#include <locale.h>
#include <signal.h>
#include <sys/stat.h>
#include "config.h"
#include "opencv.h"
#include "common.h"
#include "include/ASICamera2.h"

extern std::string day_or_night;
extern char debugText[500];
extern char const *config_file;
extern int debug_level;
extern bool help;

ASI_ERROR_CODE setControl(int camera_id, ASI_CONTROL_TYPE control, long value, ASI_BOOL make_auto);
unsigned long createRGB(int r, int g, int b);
void cvText(cv::Mat &img, const char *text, int x, int y, double font_size, int font_weight, int font_smoothing_options, int font_numbers,
            int font_color[], int imgtype, int font_outline);
void *SaveImgThd(void *para);
char *getRetCode(ASI_ERROR_CODE code);
int bytesPerPixel(ASI_IMG_TYPE imageType);

#ifdef USE_HISTOGRAM
void computeHistogram(unsigned char *imageBuffer, int width, int height, ASI_IMG_TYPE imageType, int *histogram);
int calculateHistogramMean(int *histogram);
#endif // USE_HISTOGRAM

ASI_ERROR_CODE takeOneExposure(
        int cameraId,
        long exposureTimeMicroseconds,
        unsigned char *imageBuffer, long width, long height,  // where to put image and its size
        ASI_IMG_TYPE imageType);

void closeUp(int e);
bool resetGainTransitionVariables(int dayGain, int nightGain);
int determineGainChange(int dayGain, int nightGain);
int main(int argc, char *argv[]);

#endif // CAPTURE_H