REM This will clone the dependencies and compile them and run the initial CMAKE on the EDL project so you are ready to go
REM 
REM

mkdir ext
cd ext
git clone https://github.com/llvm/llvm-project.git llvm-src
cd llvm-src
git checkout -f llvmorg-11.0.0
mkdir build-release
cd build-release
cmake -Thost=x64 ..\llvm -DLLVM_ENABLE_DUMP=ON -DCMAKE_INSTALL_PREFIX=..\..\llvm-rel -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel --config Release
cmake --install .
cd ..
cd ..
cd ..

cd ext
git clone https://github.com/glfw/glfw.git glfw-src
cd glfw-src
mkdir build
cd build
cmake -Thost=x64 .. -DCMAKE_INSTALL_PREFIX=..\..\glfw-ins -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel --config Release
cmake --install .
cd ..
cd ..
cd ..

cd ext
git clone https://github.com/kcat/openal-soft.git openal-src
cd openal-src
mkdir build
cd build
cmake -Thost=x64 .. -DCMAKE_INSTALL_PREFIX=..\..\openal-ins -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel --config Release
cmake --install .
cd ..
cd ..
cd ..

cd ext
git clone https://github.com/lexxmark/winflexbison.git flexbison-src
cd flexbison-src
mkdir build
cd build
cmake -Thost=x64 .. -DCMAKE_INSTALL_PREFIX=..\..\flexbison-ins -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel --config Release
cmake --install .
cd ..
cd ..
cd ..

mkdir build
cd build
cmake -Thost=x64 .. -DLLVM_DIR=%CD%\..\ext\llvm-rel\lib\cmake\llvm -DFLEX_EXECUTABLE=%CD%\..\ext\flexbison-ins\win_flex.exe -DBISON_EXECUTABLE=%CD%\..\ext\flexbison-ins\win_bison.exe -Dglfw3_DIR=%CD%\..\ext\glfw-ins\lib\cmake\glfw3 -DOPENAL_LIBRARY=%CD%\..\ext\openal-ins\lib\openal32.lib -DOPENAL_INCLUDE_DIR=%CD%\..\ext\openal-ins\include
cd ..


