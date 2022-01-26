# SPLiT

Software Power Limiting Tools

# Anotation

This repository is an open source work-in-progress repository collecting a set of software tools for power monitoring and controlling in HPC systems.

# Build and usage
It is recommended to create a build directory.
```
mkdir build
cd build
cmake ..
make
```
The tools available at the moment:
- Static Energy Profiler (StEP) `./build/StEP`
- Dynamic Energy-Performance Optimizer `./build/DEPO`