REM This will clone the dependencies and compile them and run the initial CMAKE on the EDL project so you are ready to go
REM 
REM

goto next
mkdir ext
cd ext
git clone https://github.com/llvm-mirror/llvm.git llvm-src
cd llvm-src
git checkout -f release_70
mkdir build-debug
cd build-debug
cmake -Thost=x64 -G "Visual Studio 15 2017 Win64" .. -DCMAKE_INSTALL_PREFIX=..\..\llvm-dbg -DCMAKE_BUILD_TYPE=Release
msbuild Install.vcxproj
cd ..
mkdir build-release
cd build-release
cmake -Thost=x64 -G "Visual Studio 15 2017 Win64" .. -DLLVM_ENABLE_DUMP=ON -DCMAKE_INSTALL_PREFIX=..\..\llvm-rel -DCMAKE_BUILD_TYPE=Release
msbuild -p:Configuration=Release Install.vcxproj
cd ..
cd ..
cd ..

cd ext
git clone https://github.com/glfw/glfw.git glfw-src
cd glfw-src
mkdir build
cd build
cmake -Thost=x64 -G "Visual Studio 15 2017 Win64" .. -DCMAKE_INSTALL_PREFIX=..\..\glfw-ins -DCMAKE_BUILD_TYPE=Release
msbuild -p:Configuration=Release Install.vcxproj
cd ..
cd ..
cd ..

cd ext
git clone https://github.com/kcat/openal-soft.git openal-src
cd openal-src
mkdir build
cd build
cmake -Thost=x64 -G "Visual Studio 15 2017 Win64" .. -DCMAKE_INSTALL_PREFIX=..\..\openal-ins -DCMAKE_BUILD_TYPE=Release
msbuild -p:Configuration=Release Install.vcxproj
cd ..
cd ..
cd ..

cd ext
git clone https://github.com/lexxmark/winflexbison.git flexbison-src
cd flexbison-src
mkdir build
cd build
cmake -Thost=x64 -G "Visual Studio 15 2017 Win64" .. -DCMAKE_INSTALL_PREFIX=..\..\flexbison-ins -DCMAKE_BUILD_TYPE=Release
msbuild -p:Configuration=Release Install.vcxproj
cd ..
cd ..
cd ..

:next

mkdir build
cd build
cmake -Thost=x64 -G "Visual Studio 15 2017 Win64" .. -DLLVM_DIR=%CD%\..\ext\llvm-rel\lib\cmake\llvm -DFLEX_EXECUTABLE=%CD%\..\ext\flexbison-ins\win_flex.exe -DBISON_EXECUTABLE=%CD%\..\ext\flexbison-ins\win_bison.exe -Dglfw3_DIR=%CD%\..\ext\glfw-ins\lib\cmake\glfw3 -DOPENAL_LIBRARY=%CD%\..\ext\openal-ins\lib\openal32.lib -DOPENAL_INCLUDE_DIR=%CD%\..\ext\openal-ins\include
cd ..


