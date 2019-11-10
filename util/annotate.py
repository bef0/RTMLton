#!/usr/bin/env python
from __future__ import print_function
import re

import sys

def lwl(n, l):  # line with label  (linenum, line)
    return """  fprintf(stderr, "[%d %x {} {}]\\n", PTHREAD_NUM, *(int*)(gcStateAddress+64));  """.format(n, l.strip())

def line(n, l):  #  (linenum, line)
    l = re.sub(r"[\\\"]", " ", l.strip())
    return """  fprintf(stderr, "[%d %x {}] {}\\n", PTHREAD_NUM, *(int*)(gcStateAddress+64));  """.format(n, l)


# currentFrame_Offset = 64

ln = 0
with open(sys.argv[1], "r") as f:
    for l in f:
        l = l.strip()
        print(l)
        ln += 1
        if "ChunkSwitch" in l:
            break

    for l in f:
        ln += 1
        l = l.strip()
        if l.startswith("L_"):
            print("{} {}".format(l, lwl(ln, l)))
        elif 'case ' in l:
            print("{} {}".format(l, lwl(ln, l)))
        elif 'default:' in l:
            print("{} {}".format(l, lwl(ln, l)))
        else:
            print("{} {}".format(line(ln, l), l))

