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
    
    source_file = os.path.abspath("parser.c")
    
    if os.name == "nt": 
        subprocess.run(["build.bat", VISUAL_STUDIO_AT, source_file], capture_output=(not with_build_log))
    else:
        os.chdir("build")
        subprocess.run(["gcc", source_file, "-o", "{exec}"])
        os.chdir("..")        

argparser = argparse.ArgumentParser(
    prog="agnes", description="JSON parser."
)

argparser.add_argument("file")
argparser.add_argument('--build-log', action=argparse.BooleanOptionalAction)

args = argparser.parse_args()

file_path = args.file
with_build_log = args.build_log

build(with_build_log)

if not os.path.exists(file_path):
    print("[IO Error] file not found.")
    sys.exit(1)

if not os.path.isfile(file_path):
    print("[INPUT ERROR] expected source file, found directory.")
    sys.exit(1)

absolute_path = os.path.abspath(file_path)
file_size = os.path.getsize(absolute_path)

os.chdir("build")
subprocess.run([exec, absolute_path, str(file_size)])
os.chdir("..")