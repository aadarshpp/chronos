#!/usr/bin/env bash

javac Benchmark.java
java --enable-native-access=ALL-UNNAMED \
     -Djava.library.path=. \
     Benchmark "$@"