#!/bin/python

import os
import subprocess
import argparse
import shutil

argparser = argparse.ArgumentParser()

argparser.add_argument('--build-only', action=argparse.BooleanOptionalAction, default=False)
argparser.add_argument('--cleanup', action=argparse.BooleanOptionalAction, default=False)

args = argparser.parse_args()

# change this, if you are on windows:
VISUAL_STUDIO_AT = R"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"

exec = "include_as_head"
if os.name == "nt":
    exec = exec + ".exe"
else:
    exec = exec + ".out"

headers = ["common.h", "parser.h", "interner.h"] 

if not os.path.exists("build"):
    os.mkdir("build")

src = "include_as_head.c"
source_file_abs = os.path.abspath("include_as_head.c")

if os.name == "nt":
    for header_file in headers:
        subprocess.run(["xcopy", "/f", "/y", ("..\\" + header_file), "."], shell=True)

    os.chdir("build")
    subprocess.run([VISUAL_STUDIO_AT, "x64", "&&", "clang", source_file_abs, "-o", exec], shell=True)
    
    if not args.build_only:    
        subprocess.run([exec])
    
    if args.cleanup:
        subprocess.run(["del", exec], shell=True)
    
    os.chdir("..")

    if args.cleanup:
        for header_file in headers:
            subprocess.run(["del", header_file], shell=True)
else: 
    for header_file in headers:
        shutil.copyfile("../" + header_file, header_file)
    
    os.chdir("build")
    subprocess.run(["clang", source_file_abs, "-o", exec]) 
    if not args.build_only:    
        subprocess.run([exec])
        if args.cleanup:
            subprocess.run(["rm", exec], shell=True)
    os.chdir("..")

    if args.cleanup:
        for header_file in headers:
            subprocess.run(["rm", header_file], shell=True)
