#! /bin/bash

version="1.4.10"
name="xapian-core-$version"

wget https://oligarchy.co.uk/xapian/$version/$name.tar.xz
tar -xf $name.tar.xz

# make libxapian.a
pushd $name
emconfigure ./configure CPPFLAGS='-DFLINTLOCK_USE_FLOCK' CXXFLAGS='-Oz -s USE_ZLIB=1' --disable-backend-remote
emmake make
popd

# xapian.js (compatible with WebExtension; no wasm code)
em++ -Oz \
  -s NO_DYNAMIC_EXECUTION=1 \
  -s USE_ZLIB=1 \
  -s WASM=0 \
  -s EXTRA_EXPORTED_RUNTIME_METHODS='["cwrap"]' \
  -s EXPORTED_FUNCTIONS='["_prepare", "_commit", "_key", "_add", "_clean", "_query", "_percent", "_languages", "_snippet", "_compact"]' \
  -std=c++11 \
  -I./$name \
  -I./$name/include \
  -I./$name/common \
  xapian.cc ./$name/.libs/libxapian.so -o xapian.js

ls
