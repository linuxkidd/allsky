#ifndef CAPTURE_H

#define CAPTURE_H

#include "config.h"
#include "opencv.h"
#include "include/ASICamera2.h"
#include <sys/time.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <string>
#include <iostream>
#include <cstdio>
#include <tr1/memory>
#include <ctime>
#include <stdlib.h>
#include <signal.h>
#include <fstream>
#include <locale.h>

void *retval;
int gotSignal            = 0;		// did we get a SIGINT (from keyboard) or SIGTERM (from service)?
int iNumOfCtrl           = 0;
int CamNum               = 0;
pthread_t thread_display = 0;
pthread_t hthdSave       = 0;
int numExposures         = 0;	// how many valid pictures have we taken so far?
int currentGain          = NOT_SET;

// Some command-line and other option definitions needed outside of main():
int tty = 0;	// 1 if we're on a tty (i.e., called from the shell prompt).
int notificationImages     = DEFAULT_NOTIFICATIONIMAGES;
char const *fileName       = DEFAULT_FILENAME;
char const *timeFormat     = DEFAULT_TIMEFORMAT;
int asiDayExposure         = DEFAULT_ASIDAYEXPOSURE;
int asiDayAutoExposure     = DEFAULT_DAYAUTOEXPOSURE;	// is it on or off for daylight?
int dayDelay               = DEFAULT_DAYDELAY;	// Delay in milliseconds.
int nightDelay             = DEFAULT_NIGHTDELAY;	// Delay in milliseconds.
int asiNightMaxExposure    = DEFAULT_ASINIGHTMAXEXPOSURE;
int gainTransitionTime     = DEFAULT_GAIN_TRANSITION_TIME;

#ifdef USE_HISTOGRAM
long cameraMaxAutoExposureUS  = NOT_SET;	// camera's max auto exposure in us

int histogramBoxSizeX         = DEFAULT_BOX_SIZEX;     // 500 px x 500 px box.  Must be a multiple of 2.
int histogramBoxSizeY         = DEFAULT_BOX_SIZEY;

// % from left/top side that the center of the box is.  0.5 == the center of the image's X/Y axis
float histogramBoxPercentFromLeft = DEFAULT_BOX_FROM_LEFT;
float histogramBoxPercentFromTop = DEFAULT_BOX_FROM_TOP;
#endif	// USE_HISTOGRAM

#endif // CAPTURE_H