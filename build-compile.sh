#!/bin/bash
clang++ -std=c++17 -Wall -Wextra -pedantic \
  $(pkg-config --cflags openssl) \
  $2 -o $1 \
  $(pkg-config --libs openssl)