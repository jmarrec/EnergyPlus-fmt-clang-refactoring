Using **conan >= 2.0**:

Install conan dependencies and create toolchain file.

```
cat ~/.conan2/profiles/clang

[settings]
arch=x86_64
build_type=Release
compiler=clang
compiler.cppstd=20
compiler.libcxx=libc++
compiler.version=20
os=Linux
```


```shell
export CC=/usr/bin/clang-20
export CXX=/usr/bin/clang++-20
conan install . --output-folder=./build --build=missing -c tools.cmake.cmaketoolchain:generator=Ninja -s compiler.cppstd=20 -s build_type=Release --profile:all clang -c tools.build:cxxflags="['-Wno-deprecated-literal-operator', '-DFMT_CONSTEVAL=']"
```

Build using conan-presets

```shell
cmake --preset conan-release
cmake --build --preset conan-release
```
