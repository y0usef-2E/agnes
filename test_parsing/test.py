#!/bin/python

import os
import subprocess
import argparse
from pathlib import Path

argparser = argparse.ArgumentParser()

argparser.add_argument('--build-only', action=argparse.BooleanOptionalAction, default=False)
argparser.add_argument('--cleanup', action=argparse.BooleanOptionalAction, default=False)
argparser.add_argument('--top', type=str, default=None)
argparser.add_argument("--restrict", type=str, default=None)

args = argparser.parse_args()

# change this, if you are on Windows:
VISUAL_STUDIO_AT = R"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"

exec = "test"
if os.name == "nt":
    exec = exec + ".exe"
else:
    exec = exec + ".out"

headers = ["common.h", "parser.h", "interner.h"] 

if not os.path.exists("build"):
    os.mkdir("build")

source_file = os.path.abspath("test.c")

paths: list[Path] = []
sizes: list[int] = []
expect: list[bool] = []


def run_tests(): 
    if args.top:
        input = Path(args.top).absolute()
        assert input.exists()
        assert input.is_file()
        input_size = input.stat().st_size
        os.chdir("build")
        proc = subprocess.run([exec, input, str(input_size)])
        print("Got back: ", "ACCEPTED" if not proc.returncode else "REJECTED" )

    else:
        passing = Path("yes")
        for file in passing.iterdir():
            assert file.is_file()
            if args.restrict is not None: 
                pref = "y_" + args.restrict 
                if file.name.startswith(pref):
                    print(file)
                    paths.append(file.absolute())
                    sizes.append(file.stat().st_size)
                    expect.append(True)    
            else:
                paths.append(file.absolute())
                sizes.append(file.stat().st_size)
                expect.append(True)

        failing = Path("no")
        for file in failing.iterdir(): 
            assert file.is_file()
            if args.restrict is not None:
                pref = "n_" + args.restrict 
                if file.name.startswith(pref):
                    print(file)
                    paths.append(file.absolute())
                    sizes.append(file.stat().st_size)
                    expect.append(False)    
            else:
                paths.append(file.absolute())
                sizes.append(file.stat().st_size)
                expect.append(False)


        import time

        os.chdir("build")
        print(f"running {len(paths)} tests: MODE=", args.restrict if args.restrict else "ALL")
        with open(f"0_log_{time.time()}.csv", "w") as output_log:
            for i in range (0, len(expect)):
                proc = subprocess.run([exec, paths[i], str(sizes[i])])
                result = "Fail"
                if expect[i]==True and proc.returncode == 0:
                    result = "Pass"
                elif expect[i] ==False and proc.returncode !=0:
                    result = "Pass"
                
                simple_name = paths[i].name
                output_log.write(f"{simple_name}, {result}\n")
        
        os.chdir("..")

if os.name == "nt":
    for header_file in headers:
        subprocess.run(["xcopy", "/f", "/y", ("..\\" + header_file), "."], shell=True)

    os.chdir("build")
    subprocess.run([VISUAL_STUDIO_AT, "x64", "&&", "clang", source_file, "-g", "-o", exec], shell=True)
    os.chdir("..")

    if not args.build_only:    
        run_tests()
    
    if args.cleanup:
        os.chdir("build")
        subprocess.run(["del", exec], shell=True)
        os.chdir("..")
    
    if args.cleanup:
        for header_file in headers:
            subprocess.run(["del", header_file], shell=True)
else: 
    pass