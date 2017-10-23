#!/usr/bin/python3
#
# Copyright (C) 2017 Dell Inc.
#
# Licensed under the GNU General Public License Version 2
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
import os
import sys

directory = os.path.dirname(sys.argv[0])
TARGET=os.getenv('OS')
if TARGET == '':
    print("Missing OS environment variable")
    sys.exit(1)

if (len(sys.argv) < 3):
    print("Missing input and output file")
    sys.exit(1)

deps = []
with open (os.path.join(directory,"dependencies.txt"), 'r') as rfd:
    header = rfd.readline().split(',')
    pos = -1
    control_pos = -1
    for i in range(0,len(header)):
        if header[i].strip() == TARGET:
            pos = i
        elif header[i].strip() == 'debian-control':
            control_pos = i
        if pos > 0 and control_pos > 0:
            break
    if pos == -1:
        print("Unknown OS: %s" % TARGET)
        sys.exit(1)
    if control_pos == -1:
        print("Missing control section")
        sys.exit(1)
    for line in rfd.readlines():
       dep = line.split(',')[pos].strip()
       control_dep = line.split(',')[control_pos].strip()
       if dep == '':
           continue
       if control_dep:
           if control_dep == '*':
               deps.append(dep)
           else:
               deps.append("%s %s" % (dep, control_dep))

input = sys.argv[1]
if not os.path.exists(input):
    print("Missing input file %s" % input)
    sys.exit(1)

with open(input, 'r') as rfd:
    lines = rfd.readlines()

deps.sort()
output = sys.argv[2]
with open(output, 'w') as wfd:
    for line in lines:
        if line.startswith("Build-Depends: %%%DYNAMIC%%%"):
            wfd.write("Build-Depends:\n")
            for i in range(0, len(deps)):
                wfd.write("\t%s,\n" % deps[i])
        else:
            wfd.write(line)
