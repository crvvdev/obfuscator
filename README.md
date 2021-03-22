# Obfuscator LLVM

###### Fixed PRNG so it's portable to both Linux and Windows. 
###### Fixed Flattening (the original version had crashes). 
###### Fixed String Obfuscation now works but it's limited to ASCII strings only (please fix).

Currently using clang version [10.0.1](https://github.com/llvm/llvm-project/tree/release/10.x "10.0.1")

Please have a look at the [wiki](https://github.com/obfuscator-llvm/obfuscator/wiki)!

Obfuscator current version: [LLVM-4.0](https://github.com/obfuscator-llvm/obfuscator/tree/llvm-4.0)

You can cite Obfuscator-LLVM using the following Bibtex entry:

```
@INPROCEEDINGS{ieeespro2015-JunodRWM,
  author={Pascal Junod and Julien Rinaldini and Johan Wehrli and Julie Michielin},
  booktitle={Proceedings of the {IEEE/ACM} 1st International Workshop on Software Protection, {SPRO'15}, Firenze, Italy, May 19th, 2015},
  editor = {Brecht Wyseur},
  publisher = {IEEE},
  title={Obfuscator-{LLVM} -- Software Protection for the Masses},
  year={2015},
  pages={3--9},
  doi={10.1109/SPRO.2015.10},
}
```

# Build Windows
Personally i use Ninja because it's way faster than Visual Studio and it's better than dealing with GCC on Windows, all you gotta do is use VS Command Prompt x64 and run the following commands:

```
git clone https://github.com/crvvdev/obfuscator.git
cd obfuscator
mkdir build
cd build
cmake ../llvm -G Ninja -DLLVM_ENABLE_PROJECTS="clang;libcxx" -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD=X86 -DLLVM_INCLUDE_EXAMPLES=OFF -DLLVM_BUILD_TESTS=OFF -DLLVM_INCLUDE_TESTS=OFF -DCMAKE_CXX_FLAGS=/std:c++14 -DBUILD_SHARED_LIBS=OFF -DLLVM_PARALLEL_COMPILE_JOBS=4 -DLLVM_PARALLEL_LINK_JOBS=4
ninja
```

You can modify the flags to fit what you want.
