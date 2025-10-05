#!/bin/bash


if [ $# -ne 1 ]; then
  echo "Usage: $0 <file.c>"
  exit 1
fi

filename="$1"
basename="${filename%.c}"
outfile="bin/${basename}.out"

gcc "$filename" -o "$outfile" -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -lasound

if [ $? -eq 0 ]; then
  echo "Compilation succeeded!! Trying to run $outfile..."
  ./"$outfile"
else
  echo "Compilation failed :c"
  exit 1
fi
