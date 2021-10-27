#!/usr/bin/env python3

import argparse,json,os,sys,re,subprocess

def findZWO():
    import usb.core,usb.util
    zwo_found = False
    try:
        dev = usb.core.find(manufacturer="ZWO")
    except:
        zwo_found = False
    else:
        if dev is None:
            zwo_found = False
        else:
            if "ASI.*$".match(dev.product):
                zwo_found = True
    return zwo_found

def findRPiHQ():
    picam_found = False
    from picamera import PiCamera
    try:
        picam = PiCamera()  
    except:
        picam_found = False
    else:
        if picam.revision == 'imx477':
            picam_found = True
        picam.close()
    return picam_found

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-c", "--config", default = "/etc/allsky/config.json", help="Config file to use.  Default: /etc/allsky/config.json")
    parser.add_argument("--preview",      action='store_true',                 help="Enable Image Preview" )
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

    if conf['CAMERA'] != "ZWO" and conf['CAMERA'] != "RPiHQ":
        print("Sorry, cameara {0} not supported.  Only ZWO or RPiHQ are valid".format(conf['CAMERA']))
        exit(3)

    print("Checking for {0} camera...".format(conf['CAMERA']))
    cam_found = globals()["find{0}".format(conf['CAMERA'])]()

    if cam_found:
        print("...found {0} camera.".format(conf['CAMERA']))
        camconf={}
        print("Loading {0}/settings_{1}.json...".format(conf['CONFIG_PATH'],conf['CAMERA']))
        try:
            with open("{0}/settings_{1}.json".format(conf['CONFIG_PATH'],conf['CAMERA']),'r') as camera_config:
                try:
                    camconf=json.load(camera_config)
                except json.JSONDecodeError as err:
                    print("Error: {0}".format(err))
                    exit(4)
        except:
            print("Failed to open {0}/settings_{1}.json".format(conf['CONFIG_PATH'],conf['CAMERA']))
            exit(5)

        capture_args = ["./capture_{0}".format(conf['CAMERA'])]
        for x in camconf:
            capture_args.extend(["-{0}".format(x),camconf[x]])

        if sys.__stdin__.isatty():
            capture_args.extend(["-tty","1"])
        else:
            capture_args.extend(["-tty","0"])

        if args.preview:
            capture_args.extend(["-preview","1"])
        if int(camconf['debuglevel']) > 0:
            print("Starting capture_{0}".format(conf['CAMERA']))
        if int(camconf['debuglevel']) > 2:
            print("Command line: {0}".format(" ".join(capture_args)))
        try:
            subprocess.run(capture_args)
        except:
            print("Failed to run (or unclean exit): {0}".format(" ".join(capture_args)))
            exit(6)
    else:
        print("Camera {0} not found.".format(conf['CAMERA']))
        exit(99)
