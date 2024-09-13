#!/usr/bin/env python

from datetime import datetime as dt
from datetime import timedelta
from glob import glob
import re
import matplotlib.pyplot as plt
import numpy as np
import matplotlib.cm as cm
from netCDF4 import Dataset
from h5py import File
import argparse
import os
import json
from struct import unpack
import subprocess
import os
import sys

# converts datetime object to list for use with JSON files 
def dtToList(datetime):
    if type(datetime) != dt
        raise TypeError("Argument to dtToList() must be type dt")
    dt_list = [datetime.year, datetime.month, datetime.day, datetime.hour, datetime.minute, datetime.second]
    return dt_list

# parses command-line arguments
def parse_args():

    parser = argparse.ArgumentParser(description = 'Process number of restarts and time interval for Aether run')
    parser.add_argument('-restarts', type=int, required=True, help='number of restarts')
    parser.add_argument('-minutes', type=int, required=True, help='number of minutes from start time')

    args = parser.parse_args()

    return args
    

# Function runs Aether repeatedly by splitting a chunk of time into a specified number of restarts
# restarts and minutes must be integers
def runRestarts(restarts, minutes):
    try:
        with open('aether.json', 'r') as file:
            aether = json.load(file)
    except FileNotFoundError as e:
        print("Error: no aether.json file available.", e)
        sys.exit(1)
    restartDuration = minutes / (restarts+1)
    
    startTime_list = aether.get("StartTime")
    if startTime_list is None:
        raise KeyError("Error: StartTime key not found in aether.json file", e)
    try:
        startTime = dt(*startTime_list)
    except ValueError as e:
        print("Error: invalid values for StartTime", e)
        sys.exit(1)
    except TypeError as e:
        print("Error: unable to convert StartTime to datetime object. Check type and number of values.", e)
        sys.exit(1)
    
    startRun = startTime
    endRun = 0
    for i in range(0,restarts+1):
        endRun = startRun + timedelta(minutes=restartDuration)
        try:
            startRun_list = dtToList(startRun)
            endRun_list = dtToList(endRun)
        except TypeError as e:
            print(f"Error: {e}")
            sys.exit(1)
        aether['StartTime'] = startRun_list
        aether['EndTime'] = endRun_list
        with open(f'aether{i+1}.json', 'w') as file:
            json.dump(aether, file)
        
        subprocess.run(['cp',f'./aether{i+1}.json','./aether.json'])
        subprocess.run(['./aether'])

        try:
            os.chdir('UA')
            os.rename('restartOut',f'restartOut{i+1}')
        except FileNotFoundError as e:
            print("Error: UA or restartOut directories not found. Check file paths.", e)
            sys.exit(1)
        os.mkdir('restartOut')
        subprocess.run(['rm','-rf','restartIn'])
        subprocess.run(['ln','-s',f'restartOut{i+1}','restartIn'])
        os.chdir('..')
        startRun = endRun

    try:
        startRun_list = dtToList(startTime)
        endRun_list = dtToList(endRun)
    except TypeError as e:
        print(f"Error: {e}")
        sys.exit(1)
    aether['StartTime'] = startRun_list
    aether['EndTime'] = endRun_list
    with open('../run.whole/aetherwhole.json', 'w') as file:
        json.dump(aether, file)
        

        
        

# main code
# manipulates directories, parses arguments, runs restart function, and performs whole run
try:
    os.chdir('../tests/restarts')
    subprocess.run(['rm', '-rf', 'run.halves'])
    subprocess.run(['rm', '-rf', 'run.whole'])
    subprocess.run(['cp','-R', '../../share/run', './run.halves'])
    subprocess.run(['cp','-R', '../../share/run', './run.whole'])
    os.chdir('run.halves')
except FileNotFoundError as e:
    print("Error: at least one of the directory operations failed. Check your file paths.", e)
    sys.exit(1)

args = parse_args()

try:
    runRestarts(args.restarts, args.minutes)
except KeyError as e:
    print(f"Error: {e}")
    sys.exit(1)

os.chdir('../run.whole')
subprocess.run(['cp','./aetherwhole.json','./aether.json'])
subprocess.run(['./aether'])





