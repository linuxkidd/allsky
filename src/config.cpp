/*
 * Command Line Arguments Parser
 * part of the Allsky project
 * https://github.com/thomasjacquin/allsky
 *
 */

#include "config.h"

//-------------------------------------------------------------------------------------------------------
char const *config_file;
int debugLevel;
bool help;
//-------------------------------------------------------------------------------------------------------


void parse_commandline(int argc, char *argv[])
{
    debugLevel = 0;

    // -h[elp] doesn't take an argument, but the "for" loop assumes every option does,
    // so check separately, assuming the option is the first one.
    // If it's not the first option, we'll find it in the "for" loop.
    if (strncmp(argv[1], "-h",2) == 0 || strncmp(argv[1], "--help",6) == 0)
    {
        help = 1;
        return;
    }

    // Many of the argument names changed to allow day and night values.
    // However, still check for the old names in case the user didn't update their
    // settings.json file.  The old names should be removed below in a future version.
    for (int i=1 ; i < argc - 1 ; i++)
    {
        // Check again in case "-h" isn't the first option.
        if (strncmp(argv[i], "-c",2) == 0 || strncmp(argv[i], "--conf",6) == 0)
        {
            // In case the text is null and isn't quoted, check if the next argument
            // starts with a "-".  If so, the text is null, otherwise it's the text.
            if ((char)argv[i + 1][0] != '-') {
                config_file = argv[++i];
            }
        }
        else if (strncmp(argv[i], "-d",2) == 0 || strncmp(argv[i], "--debug",7) == 0)
        {
            debugLevel = atoi(argv[++i]);
        }
        else if (strncmp(argv[i], "-h",2) == 0 || strncmp(argv[i], "--help",6) == 0)
        {
            help = 1;
        }
    }
    return;
}