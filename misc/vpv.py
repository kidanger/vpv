#!/usr/bin/env python3
# coding: utf-8

import os
import sys
import tempfile

def write_img(img, path):
    from PIL import Image
    im = Image.fromarray(img)
    im.save(path, "TIFF")

def vpv(*args):
    cmd = 'vpv'
    dir = os.path.join(tempfile.gettempdir(), 'vpvpython')

    if args and isinstance(args[0], int):
        num = args[0]
        dir = dir + '_' + str(num)
        cmd = 'env WATCH=1 ' + cmd
        args = args[1:]
        isnumbered = True
    else:
        isnumbered = False

    if not os.path.exists(dir):
        os.makedirs(dir)
        newwindow = True
    else:
        newwindow = False

    j = 1
    for o in args:
        if isinstance(o, str):
            cmd += ' ' + o
        else:
            if len(o.shape) <= 3 or o.shape[3] == 1:  # image
                name = '{}/{}.tiff'.format(dir, j)
                write_img(o, name);
            else:  # video
                name = '{}/{}.tiff'.format(dir, j)

                if not os.path.exists(name):
                    os.mkdir(name)

                for k in range(o.shape[3]):
                    write_img(o[:,:,:,k], '{}/{}.tiff'.format(name, k))

            cmd = cmd + ' ' + name
            j += 1

    cmd = '({}; rm -rf "{}") &'.format(cmd, dir)
    print(isnumbered, newwindow)
    if not isnumbered or newwindow:
        print(cmd)
        os.system(cmd)
    elif isnumbered:
        print('vpv #', num, ' updated.')

# make so the vpv module is callable
sys.modules[__name__] = vpv

