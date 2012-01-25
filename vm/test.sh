set -e
make DEBUG=1 LLVM_LIB=~/src/llvm-3.0/Debug+Asserts/lib -j 4
clang -emit-llvm -ffreestanding -O2 -c test.c
echo ------------------------
./svmc -print-isel-input -O=0 test.o
echo ------------------------
cat test.o.s
echo ------------------------
