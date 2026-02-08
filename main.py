#!/bin/python

import argparse
import os
import sys
import subprocess

# change this, if you are on windows:
VISUAL_STUDIO_AT = R"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"

exec = "agnes" 
if os.name == "nt":
    exec = exec + ".exe"

def build(with_build_log: bool):
    if not os.path.exists("build"):
        os.mkdir("build")
    
    source_file = os.path.abspath("test.c")
    
    if os.name == "nt": 
        subprocess.run(["build.bat", VISUAL_STUDIO_AT, source_file], capture_output=(not with_build_log))
    else:
        os.chdir("build")
        subprocess.run(["gcc", source_file, "-o", "{exec}"])
        os.chdir("..")        
argparser = argparse.ArgumentParser(
    prog="agnes", description="JSON parser."
)

argparser.add_argument("--restrict", type=str, default=None)

args = argparser.parse_args()
"""
argparser.add_argument("file")
argparser.add_argument('--build-log', action=argparse.BooleanOptionalAction, default=True)

args = argparser.parse_args()

file_path = args.file
with_build_log = args.build_log


if not os.path.exists(file_path):
    print("[IO Error] file not found.")
    sys.exit(1)

if not os.path.isfile(file_path):
    print("[INPUT ERROR] expected source file, found directory.")
    sys.exit(1)

"""
from pathlib import Path

paths: list[Path] = []
expect: list[bool] = []

os.chdir("test_parsing")

passing = Path("yes")
for file in passing.iterdir():
    assert file.is_file()
    if args.restrict is not None: 
        pref = "y_" + args.restrict 
        if file.name.startswith(pref):
            print(file)
            paths.append(file.absolute())
            expect.append(True)    
    else:
        paths.append(file.absolute())
        expect.append(True)

failing = Path("no")
for file in failing.iterdir(): 
    assert file.is_file()
    if args.restrict is not None:
        pref = "n_" + args.restrict 
        if file.name.startswith(pref):
            print(file)
            paths.append(file.absolute())
            expect.append(False)    
    else:
        paths.append(file.absolute())
        expect.append(False)


os.chdir("..")
build(True)

import time

os.chdir("build")
print(f"running {len(paths)} tests: MODE=", args.restrict if args.restrict else "ALL")
with open(f"0_log_{time.time()}.csv", "w") as output_log:
    for i in range (0, len(expect)):
        proc = subprocess.run([exec, paths[i]])
        result = "Fail"
        if expect[i]==True and proc.returncode == 0:
            result = "Pass"
        elif expect[i] ==False and proc.returncode !=0:
            result = "Pass"
        
        simple_name = paths[i].name
        output_log.write(f"{simple_name}, {result}\n")
        
    
os.chdir("..")