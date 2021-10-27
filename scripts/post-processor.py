#!/usr/bin/env python3

import argparse,json,os,sys
sys.path.append(os.path.abspath("/etc/allsky/modules"))
sys.path.append(os.path.abspath("/home/pi/allsky/scripts/modules"))

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-c", "--config", default = "/etc/allsky/config.json", help="Config file to use.  Default: /etc/allsky/config.json")
    parser.add_argument("-t", "--time", type=str, choices = ['day','night','endOfNight'], default = "night", help="Time, either day, night, or endOfNight.  Default: night")
    args = parser.parse_args()

    print("Loading config...",flush=True)
    try:
        with open(args.config,'r') as config:
            try:
                conf=json.load(config)
            except json.JSONDecodeError as err:
                print("Error: {0}".format(err))
                exit(2)
    except:
        print("Failed to open {0}".format(args.config))
        exit(1)

    try:
        with open("{0}/postprocessing_{1}.json".format(conf['CONFIG_PATH'], args.time)) as recipe_file:
            try:
                recipe=json.load(recipe_file)
            except json.JSONDecodeError as err:
                print("Error parsing {0}/postprocessing_{1}.json: {2}".format(conf['CONFIG_PATH'], args.time, err))
                exit(3)
    except:
        print("Failed to open {0}/postprocessing_{1}.json".format(conf['CONFIG_PATH'], args.time))
        exit(4)
    
    for step in recipe:
        if step['module'] not in globals():
            try:
                '''
                This section expects module python to be present in /etc/allsky/modules/, or /home/pi/allsky/scripts/modules.
                Module files should be named 'allsky_MODULE.py', where MODULE is the name of the module.
                Assuming step['module']=="resize" for example, the below is equivalent to:
                    from allsky_resize import resize
                and expects allsky_resize.py to be present in /etc/allsky/modules/ (this path has priority), or /home/pi/allsky/scripts/modules.
                '''
                print("Attempting to load allsky_{0}.py".format(step['module']))
                _temp = importlib.import_module("allsky_{0}".format(step['module']))
                globals()[step['module']] = getattr(_temp,step['module'])
            except:
                print("Failed to import module allsky_{0}.py in either /etc/allsky/modules/ or /home/pi/allsky/scripts/modules. Exiting.".format(step['module']))
                exit(5)

        globals()[step['module']](step['arguments'])
