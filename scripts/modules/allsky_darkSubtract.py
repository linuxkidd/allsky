'''
allsky_darkSubtract.py

Part of allsky postprocess.py modules.
https://github.com/thomasjacquin/allsky

This module will perform dark frame subtraction from a single image.

Expected parameters:
darks_path  == Path to dark frames
temperature == Sensor Temperature

'''
import os
import PythonMagick as Magick

import shared as s

def darkSubtract(params):
    extension = s.args.image[-3:]

    if 'darks_path' not in params:
        print("darks_path not set in params.")
        return
    elif not os.path.isdir(params['darks_path']):
        print("darks_dir isn't a directory.")
        return

    temperature = -999
    if 'temperature' in params:
        temperature = params['temperature']
    elif os.path.exists("temperature.txt"):
        try:
            with open('temperature.txt','r') as temperature_file:
                temperature = temperature_file.readline().strip()
        except:
            print("An error occurred reading temperature.txt")
            return
    else:
        print("Temperature not found in params or temperature.txt.")
        return

    dark_file = ""
    if os.path.exists("{0}/{1}.{2}".format(params['darks_path'],temperature,extension)):
        dark_file = "{0}/{1}.{2}".format(params['darks_path'],temperature,extension)
    else:
        prev_delta=999
        prev_filename=""
        for dir_item in filter(lambda x: os.path.isfile(x) and x.endswith(".{0}".format(extension)), os.listdir(param['darks_path'])):
            delta = abs(temperature - int(dir_item.split(".")[0]))
            if delta < prev_delta:
                prev_delta = delta
                prev_filename = dir_item
            else:
                dark_file = prev_filename
                break

    if not dark_file:
        print("No dark files found.")
        return

    try:
        s.dark = Magick.image(dark_file)
    except:
        print("Cannot load dark file {0}".format(dark_file))
        return

    s.image.composite(s.dark, x.image.size().width(), s.image.size().height(), Magick.CompositeOperator.SubtractCompositeOp)

'''
	# Update the current image - don't rename it.
	convert "${CURRENT_IMAGE}" "${DARK}" -compose minus_src -composite -type TrueColor "${CURRENT_IMAGE}"
	if [ $? -ne 0 ]; then
		# Exit since we don't know the state of ${CURRENT_IMAGE}.
		echo "*** ${ME}: ERROR: 'convert' of '${DARK}' failed"
		exit 4
	fi
fi
'''