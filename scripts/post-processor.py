#!/usr/bin/env python3

import argparse,importlib,json,numpy as np,os,sys

'''
NOTE: `valid_module_paths` must be an array, and the order specified dictates the order of search for a named module.
It is expected that the 'user' supplied modules is searched first, and thus comes before the distributed modules path.
This permits the user to copy and modify a distributed module, or create an entirely new replacement for a distributed
module, thus giving the user total control.
'''
valid_module_paths = ["/etc/allsky/modules", "/home/pi/allsky/scripts/modules"]

for vmp in valid_module_paths:
    sys.path.append(os.path.abspath(vmp))

def reload_image():
    print("Importing OpenCV")
    import cv2
    try:
        print("Loading image {0}...".format(args.image))
        globals()['image'] = cv2.imread(args.image, 0)
    except:
        print("Cannot load {0}".format(args.image))
        exit(99)

def save_image(outfile):
    try:
        cv2.imwrite(outfile, globals()['image'])
    except:
        print("Failed to save image {0}".format(outfile))
        exit(97)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-c", "--config", default = "/etc/allsky/config.json", help="Config file to use.  Default: /etc/allsky/config.json")
    parser.add_argument("-i", "--image",  type=str, required=True, help="Image to proces.")
    parser.add_argument("-t", "--time",   type=str, choices = ['day','night','endOfNight'], default = "night", help="Time, either day, night, or endOfNight.  Default: night")
    args = parser.parse_args()

    if not os.path.isfile(args.image):
        print("Sorry, image file specified ( {0} ) is not a file.".format(args.image))
        exit(98)

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

    reload_image()
    print(image.shape)

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
                print("Failed to import module allsky_{0}.py in one of ( {1} ). Exiting.".format(step['module'], ", ".join(valid_module_paths)))
                exit(5)

        globals()[step['module']](step['arguments'])
