use_relative_paths = True

# Dependencies on outside packages.
#
deps = {
  "common": "https://skia.googlesource.com/common.git@c282fe0b6e392b14f88d647cbd86e1a3ef5498e0",

  # There is some duplication here that might be worth cleaning up:
  #   - can use use our existing t_p/e/libjpeg instead of pulling it for Android?

  "third_party/externals/angle2"  : "https://chromium.googlesource.com/angle/angle.git@8182049470aa3ba0c9f7c54ef965bf1877df08cb",
  "third_party/externals/cmake"   : "https://cmake.googlesource.com/cmake.git@v3.3.1",
  "third_party/externals/freetype": "https://skia.googlesource.com/third_party/freetype2.git@6a19a7d332c5446542196e5aeda0ede109ef097b",
  "third_party/externals/gyp"     : "https://chromium.googlesource.com/external/gyp.git@87ac4d0e63fc7dd8152a350327fea8dcf031bf56",
  "third_party/externals/harfbuzz": "https://skia.googlesource.com/third_party/harfbuzz.git@0.9.35",
  "third_party/externals/jsoncpp" : "https://chromium.googlesource.com/external/github.com/open-source-parsers/jsoncpp.git@1.0.0",
  "third_party/externals/libwebp" : "https://chromium.googlesource.com/webm/libwebp.git@v0.5.0",
  "third_party/externals/nanomsg" : "https://skia.googlesource.com/third_party/nanomsg.git@0.4-beta",
  "third_party/externals/zlib"    : "https://chromium.googlesource.com/chromium/src/third_party/zlib@c4e33043fb071b6ea0a153845da625d7ed633d3d",
  # NOTE: If we update libpng, we may need to update the generated file at third_party/libpng/pnglibconf.h
  "third_party/externals/libpng"  : "https://skia.googlesource.com/third_party/libpng.git@5765a2224946b953ec6702bd01205af475f8c80a",
  "third_party/externals/giflib"  : "https://android.googlesource.com/platform/external/giflib.git@ab10e256df4f684260ca239905b1cec727181f6c",

  "third_party/externals/dng_sdk" : "https://android.googlesource.com/platform/external/dng_sdk.git@96443b262250c390b0caefbf3eed8463ba35ecae",
  "third_party/externals/piex"    : "https://android.googlesource.com/platform/external/piex.git@be908191d0a6883a95333bdc0bca749c9b830969",

  "third_party/externals/libjpeg-turbo"             : "https://skia.googlesource.com/third_party/libjpeg-turbo.git@b38b934d246fca482eb6801f4ce79422954a635e",
  # libjpeg-turbo depends on yasm to compile .asm files
  "third_party/externals/yasm/source/patched-yasm/" : "https://chromium.googlesource.com/chromium/deps/yasm/patched-yasm.git@4671120cd8558ce62ee8672ebf3eb6f5216f909b",
  "third_party/externals/yasm/binaries"             : "https://chromium.googlesource.com/chromium/deps/yasm/binaries.git@52f9b3f4b0aa06da24ef8b123058bb61ee468881",

  "third_party/externals/expat" : "https://android.googlesource.com/platform/external/expat.git@android-5.1.0_r3",

  "platform_tools/chromeos/toolchain/src/third_party/chromite": "https://chromium.googlesource.com/chromiumos/chromite.git@d6a4c7e0ee4d53ddc5238dbddfc0417796a70e54",
  "platform_tools/chromeos/toolchain/src/third_party/pyelftools": "https://chromium.googlesource.com/chromiumos/third_party/pyelftools.git@bdc1d380acd88d4bfaf47265008091483b0d614e",

  # The line below is needed for compiling SkV8Example. Do not delete.
  #"third_party/externals/v8": "https://chromium.googlesource.com/v8/v8.git@5f1ae66d5634e43563b2d25ea652dfb94c31a3b4",

  # sfntly is used by the PDF backend for font subsetting
  "third_party/externals/sfntly" : "https://chromium.googlesource.com/external/sfntly/cpp/src.git@1bdaae8fc788a5ac8936d68bf24f37d977a13dac",
  # ICU is needed for sfntly.
  "third_party/externals/icu" : "https://chromium.googlesource.com/chromium/deps/icu.git@ce41627e388fb46ab49671bd16a5db81dcd75a71",

  # sdl will be needed for native windows
  "third_party/externals/sdl" : "https://skia.googlesource.com/third_party/sdl@9b526d28cb2d7f0ccff0613c94bb52abc8f53b6f",

  # microhttpd for skiaserve
  "third_party/externals/microhttpd" : "https://android.googlesource.com/platform/external/libmicrohttpd@748945ec6f1c67b7efc934ab0808e1d32f2fb98d",

  # shaderc for the vulkan backend
  "third_party/externals/shaderc2" : "https://github.com/google/shaderc.git@7308a2cacffce47cc944a59b411ab065c01c3f03",
  "third_party/externals/shaderc2/third_party/googletest" : "https://github.com/google/googletest.git@d225acc90bc3a8c420a9bcd1f033033c1ccd7fe0",
  "third_party/externals/shaderc2/third_party/glslang" : "https://github.com/google/glslang.git@e1cd410d9c03a24c00c570c91a99cad88bb475d1",
  "third_party/externals/shaderc2/third_party/spirv-tools" : "https://github.com/KhronosGroup/SPIRV-Tools.git@009c4358b5a1c93203166b3ed60a548f63522e81",
  "third_party/externals/shaderc2/third_party/android-cmake" : "https://github.com/taka-no-me/android-cmake.git@556cc14296c226f753a3778d99d8b60778b7df4f",
}

deps_os = {
  'llvm': {
    # xSAN bots build Clang from scratch.
    "third_party/externals/llvm": "https://llvm.googlesource.com/llvm@release_38",
    "third_party/externals/llvm/tools/clang": "https://llvm.googlesource.com/clang@release_38",
    "third_party/externals/llvm/projects/compiler-rt": "https://llvm.googlesource.com/compiler-rt@release_38",
    "third_party/externals/llvm/projects/libcxx": "https://llvm.googlesource.com/libcxx@release_38",
    "third_party/externals/llvm/projects/libcxxabi": "https://llvm.googlesource.com/libcxxabi@release_38",
  }
}

recursedeps = [ "common" ]
