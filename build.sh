#!/bin/bash

FILES="src/main.c src/utils.c md4c/md4c.c"
gcc -Wall -Werror -o main $FILES -I./raylib-5.5/include -L./raylib-5.5/lib/ -l:libraylib.a -lm -lcurl
