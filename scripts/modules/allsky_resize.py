'''
allsky_resize.py

Part of allsky postprocess.py modules.
https://github.com/thomasjacquin/allsky

This module will resize the image, but only a single image.

Expected parameters:
x == New Width
y == New Height
keep_aspect == Boolean to keep aspect ratio ( default True )
'''
import shared as s

def resize(params):
    if 'keep_aspect' not in params:
        params['keep_aspect']=True
    size = s.image.size()
    if params['x'] < size.width() and params['y'] < size.height():
        if params['keep_aspect']:
            s.image.sample("{0}x{1}".format(params['x'],params['y']))
        else:
            s.image.sample("!{0}x{1}".format(params['x'],params['y']))