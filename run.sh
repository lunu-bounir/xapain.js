#! /bin/bash

# install m4
apt-get update
apt-get install m4

# prepare
git clone https://github.com/xapian/xapian
./xapian/bootstrap

pwd
ls ./xapian/xapian-core/include/xapian/

# make libxapian.a
pushd xapian/xapian-core
emconfigure ./configure CPPFLAGS='-DFLINTLOCK_USE_FLOCK' CXXFLAGS='-Oz -s USE_ZLIB=1' --disable-backend-remote
emmake make
popd

pwd
ls ./xapian/xapian-core/.libs/

# xapian.js (compatible with WebExtension; no wasm code)
em++ -Oz \
  -s NO_DYNAMIC_EXECUTION=1 \
  -s USE_ZLIB=1 \
  -s WASM=0 \
  -s EXTRA_EXPORTED_RUNTIME_METHODS='["cwrap"]' \
  -s EXPORTED_FUNCTIONS='["_key", "_add", "_query", "_percent", "_languages", "_snippet"]' \
  -std=c++11 \
  -I./xapian/xapian-core \
  -I./xapian/xapian-core/include \
  -I./xapian/xapian-core/common \
  xapian.cc ./xapian/xapian-core/.libs/libxapian.a -o xapian.js
