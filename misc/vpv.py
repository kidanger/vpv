#!/usr/bin/env python3
# coding: utf-8

import os
import sys
import tempfile
import numpy as np


def vpv(*args):
    '''
        *args should be a list containing numpy arrays or strings
        arrays should be of shape ([nframes, ]height, width[, nchannels])
        torch.Tensor as converted as numpy arrays of the same shape
    '''
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
        if type(o).__module__ is 'torch':
            o = o.cpu().detach().numpy()

        if isinstance(o, str):
            cmd += ' ' + o
        else:
            name = '{}/{}.npy'.format(dir, j)
            np.save(name, o)
            cmd = cmd + ' ' + name
            j += 1

    cmd = '({}; rm -rf "{}") &'.format(cmd, dir)
    if not isnumbered or newwindow:
        print(cmd)
        os.system(cmd)
    elif isnumbered:
        print('vpv #', num, ' updated.')


# make so the vpv module is callable
sys.modules[__name__] = vpv
