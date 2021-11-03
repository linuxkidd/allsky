'''
allsky_crop.py

Part of allsky postprocess.py modules.
https://github.com/thomasjacquin/allsky

This module will crop the image, but only a single image.

Expected parameters:
x == crop width
y == crop height
xOffset == X offset for start of crop
yOffset == Y offset for start of crop
'''
import shared as s
import PythonMagick as Magick

def crop(params):
    size = s.image.size()
    if params['x'] < size.width() and params['y'] < size.height():
        # Crop the image to specified size (width, height, xOffset, yOffset)
        s.image.crop( Magick.Geometry(params['x'],params['y'], params['xOffset'], params['yOffset']) )
