#ifndef COMMON_H
#define COMMON_H

#include <cstdio>
#include <cstring>
#include <ctime>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/time.h>
#include <sys/types.h>
#include <tr1/memory>
#include <unistd.h>

#include "config.h"
#include "opencv.h"


extern bool b_display;
extern bool notification_images;
extern bool tty;
extern int debug_level;
extern int font_numbers[];
extern int font_smoothing_options[3];
extern int signal_received;
extern void closeUp(int e);
extern char const *font_names[];

char const *yesNo(int flag);
char *formatTime(timeval t, char const *tf);
char *getTime(char const *tf);
double timeDiff(int64 start, int64 end);
int calculateTimeToNightTime(const char *latitude, const char *longitude, const char *angle);
int roundTo(int n, int roundTo);
std::string exec(const char *cmd);
std::string ReplaceAll(std::string str, const std::string& from, const std::string& to);
timeval getTimeval();
void calculateDayOrNight(const char *latitude, const char *longitude, const char *angle);
void displayDebugText(const char * text, int requiredLevel);
void *Display(void *params);
void IntHandle(int i);
void printCredits();
void printHelp();
void waitToFix(char const *msg);

#endif