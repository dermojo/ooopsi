# build instructions

## windows

### ooopsi

```bash
mkdir build
cd build
cmake .. -G "Visual Studio 15 2017 Win64"
cmake --build . --config Release
cd ..
```

## Linux (Ubuntu)

### lib unwind

```bash
sudo apt install libunwind-dev
```

### ooopsi

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
cd ..
```
