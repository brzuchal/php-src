#!/bin/sh
# Hybrid persistent vec spike — full reproduction.
# Run from the worktree root (php-src checkout at 5b98890223c + spike patch).
set -e
export PATH="/opt/homebrew/opt/bison/bin:$PATH"
J=$(sysctl -n hw.ncpu)

echo "== 1. debug build + correctness selftest (leak-checked) =="
./buildconf --force
./configure --disable-all --enable-zend-test --enable-debug -q
make -j"$J" >/dev/null
./sapi/cli/php -n -d zend.enable_gc=0 -r 'var_dump(zend_test_vec_spike_selftest());'
# stderr must stay empty: the debug allocator reports any leak at shutdown

echo "== 2. release build =="
make clean >/dev/null
./configure --disable-all --enable-zend-test -q
make -j"$J" >/dev/null
./sapi/cli/php -n -r 'var_dump(zend_test_vec_spike_selftest()["ok"]);'

echo "== 3. benchmark matrix (fresh process per case, randomized order) =="
./sapi/cli/php -n spike-hybrid-vec/driver.php

echo "== 4. layout report =="
./sapi/cli/php -n -r 'print_r(zend_test_vec_spike_layout());' \
  > spike-hybrid-vec/results/layout.txt
