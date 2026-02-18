#!/bin/bash
echo "Testing memory usage across configurations..."

# Configuration
EXECUTABLE="./result/bin/vertexcover-16"
TEST_FILE="test/brock200_1.clq"
SKELETON="seq"
THREADS="8"

# Check if executable exists
if [ ! -f "$EXECUTABLE" ]; then
  echo "Error: $EXECUTABLE not found. Please run 'nix-build' first."
  exit 1
fi

for mode in "backtrack" "revert" "copy"; do
  echo -e "\n=== Mode: $mode ==="
  
  if [ "$mode" = "backtrack" ]; then
    FLAGS="--enable-backtracking"
  elif [ "$mode" = "revert" ]; then
    FLAGS="--revert-to-copy"
  else
    FLAGS=""
  fi
  
  /usr/bin/time -f "Peak Memory: %M KB | Time: %E | CPU: %P" \
    $EXECUTABLE --input-file $TEST_FILE --skeleton $SKELETON --hpx:threads $THREADS $FLAGS
done
