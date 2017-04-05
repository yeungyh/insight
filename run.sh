#!/usr/bin/env bash

# Compile the program
cd src
make
cd ..

# I'll execute my programs, with the input directory log_input and output the files in the directory log_output
./src/process_log ./log_input/log.txt ./log_output/hosts.txt ./log_output/resources.txt ./log_output/hours.txt ./log_output/blocked.txt

