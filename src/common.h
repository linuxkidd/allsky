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


extern bool bDisplay;
extern int gotSignal;
extern bool tty;
extern bool notificationImages;
extern void closeUp(int e);
extern int debugLevel;

timeval getTimeval();
char *formatTime(timeval t, char const *tf);
char *getTime(char const *tf);
double timeDiff(int64 start, int64 end);
std::string ReplaceAll(std::string str, const std::string& from, const std::string& to);
std::string exec(const char *cmd);
void *Display(void *params);
int roundTo(int n, int roundTo);
void IntHandle(int i);
void displayDebugText(const char * text, int requiredLevel);
void waitToFix(char const *msg);
void calculateDayOrNight(const char *latitude, const char *longitude, const char *angle);
int calculateTimeToNightTime(const char *latitude, const char *longitude, const char *angle);
char const *yesNo(int flag);
void printCredits();
void printHelp();

#endif