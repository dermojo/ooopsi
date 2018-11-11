# build instructions

## windows

### Google test

```bash
mkdir build
cd build
cmake .. -G "Visual Studio 15 2017 Win64" -Dgtest_force_shared_crt=TRUE
cmake --build . --config Debug
cmake --build . --config Release
cmake --build . --config RelWithDebInfo
cmake --build . --config MinSizeRel
cd ..
cd googletest
cp -r ../build/lib .
cd lib
cp Release/*.lib .
```

TODO: simplify path handling (reduce `cd ...`)

### ooopsi

```bash
mkdir build
cd build
cmake .. -G "Visual Studio 15 2017 Win64"
set GTEST_ROOT=D:\src\googletest\googletest
cmake ..
```

## Linux (Ubuntu)

### Google test

```bash
sudo apt install libgtest-dev
cd ~/src
mkdir build-google-test
cd build-google-test
cmake /usr/src/gtest
make
# install gtest inc&lib to /usr/local
sudo make install
```

### lib unwind

```bash
sudo apt install libunwind-dev
```
