#!/bin/bash

eval "make"
eval "gcc -g -Wall -std=gnu11 -o test test.c buddy.c -lm"


