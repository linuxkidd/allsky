#!/usr/bin/env python3

import argparse,importlib,json,numpy as np,os,sys
import PythonMagick as Magick

'''
NOTE: `valid_module_paths` must be an array, and the order specified dictates the order of search for a named module.
It is expected that the 'user' supplied modules is searched first, and thus comes before the distributed modules path.
This permits the user to copy and modify a distributed module, or create an entirely new replacement for a distributed
module, thus giving the user total control.
'''
valid_module_paths = ["/etc/allsky/modules", "/home/pi/allsky/scripts/modules"]

for vmp in valid_module_paths:
    sys.path.append(os.path.abspath(vmp))

# This is a dummy module for shared variables.
import shared as s


def set_font(params):
    if os.path.exists(params['font']):
        if os.path.isfile(params['font']):
            s.font=params['font']
        else:
            print("Font specified ( {0} ) is not a file.".format(params['font']))
    else:
        print("Font specified ( {0} ) does not exist.".format(params['font']))

def load_image(params):
    try:
        print("Loading image {0}...".format(s.args.image))
        s.image = Magick.Image(s.args.image)
    except:
        print("Cannot load {0}".format(s.args.image))
        exit(99)

def save_image(params):
    outfile=""
    overwrite = False
    if 'overwrite' in params:
        overwrite = params['overwrite']

    if 'path' in params:
        outfile=params['path']
        if 'filename' in params:
            outfile=os.path.join(params['path'],params['filename'])
        else:
            outfile=os.path.join(params['path'],os.path.basename(s.args.image))
    elif 'filename' in params:
        outfile=os.path.join(os.path.dirname(s.args.image),params['filename'])
    elif overwrite:
        outfile=s.args.image
    else:
        print("Error: No filename, path or overwrite parameter provided.  Aborting image save.")
        return

    if os.path.exists(outfile) and not overwrite:
        print("Error: File {0} exists, but overwrite is False or unspecified.".format(outfile))
        return

    try:
        print("Saving to {0}".format(outfile))
        s.image.write(outfile)
    except:
        print("Failed to save image {0}".format(outfile))
        exit(97)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-c", "--config", default = "/etc/allsky/config.json", help="Config file to use.  Default: /etc/allsky/config.json")
    parser.add_argument("-i", "--image",  type=str, help="Image to proces ( Required for day or night ).")
    parser.add_argument("-p", "--path",   type=str, help="Path to image directory to process ( Required for for endOfNight ).")
    parser.add_argument("-t", "--tod",    type=str, choices = ['day','night','endOfNight'], default = "night", help="Time of day.  One of: day, night, or endOfNight.  Default: night")
    s.args = parser.parse_args()

    if s.args.image:
        if not os.path.exists(s.args.image):
            print("Sorry, image file specified ( {0} ) does not exist.".format(s.args.image))
            exit(98)
        if not os.path.isfile(s.args.image):
            print("Sorry, image file specified ( {0} ) is not a file.".format(s.args.image))
            exit(98)

    if s.args.path:
        if not os.path.exists(s.args.path):
            print("Sorry, path specified ( {0} ) does not exist.".format(s.args.path))
            exit(98)
        if not os.path.isdir(s.args.path):
            print("Sorry, path specified ( {0} ) does not exist.".format(s.args.path))
            exit(98)

    print("Loading config...",flush=True)
    try:
        with open(s.args.config,'r') as config:
            try:
                s.conf=json.load(config)
            except json.JSONDecodeError as err:
                print("Error: {0}".format(err))
                exit(2)
    except:
        print("Failed to open {0}".format(s.args.config))
        exit(1)

    try:
        with open("{0}/postprocessing_{1}.json".format(s.conf['CONFIG_PATH'], s.args.tod)) as recipe_file:
            try:
                s.recipe=json.load(recipe_file)
            except json.JSONDecodeError as err:
                print("Error parsing {0}/postprocessing_{1}.json: {2}".format(s.conf['CONFIG_PATH'], s.args.tod, err))
                exit(3)
    except:
        print("Failed to open {0}/postprocessing_{1}.json".format(s.conf['CONFIG_PATH'], s.args.tod))
        exit(4)

    if s.args.tod == 'day' or s.args.tod == 'night':
        if not s.args.image:
            print("Sorry, you must provide an image ( using -i / --image ) for day or night processing.")
            exit(5)
        load_image('')
        print("Image dimensions: {0} x {1}".format(s.image.size().width(), s.image.size().height()))

    for s.step in s.recipe:
        if s.step['module'] not in globals():
            try:
                '''
                This section expects module python to be present in /etc/allsky/modules/, or /home/pi/allsky/scripts/modules.
                Module files should be named 'allsky_MODULE.py', where MODULE is the name of the module.
                Assuming step['module']=="resize" for example, the below is equivalent to:
                    from allsky_resize import resize
                and expects allsky_resize.py to be present in /etc/allsky/modules/ (this path has priority), or /home/pi/allsky/scripts/modules.
                '''
                print("Attempting to load allsky_{0}.py".format(s.step['module']))
                _temp = importlib.import_module("allsky_{0}".format(s.step['module']))
                globals()[s.step['module']] = getattr(_temp,s.step['module'])
            except:
                print("Failed to import module allsky_{0}.py in one of ( {1} ). Exiting.".format(s.step['module'], ", ".join(valid_module_paths)))
                exit(6)

        globals()[s.step['module']](s.step['arguments'])
