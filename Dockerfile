FROM ubuntu:24.04 AS build_seastar
ARG THREADS=2

COPY ./seastar/install-dependencies.sh /install-dependencies.sh

RUN apt-get update && apt-get install -y \
    curl \
    gnupg \
    && echo "deb http://apt.llvm.org/noble/ llvm-toolchain-noble-18 main" \
    >> /etc/apt/sources.list.d/llvm.list \
    && echo "deb http://apt.llvm.org/noble/ llvm-toolchain-noble-19 main" \
    >> /etc/apt/sources.list.d/llvm.list \
    && curl -sSL https://apt.llvm.org/llvm-snapshot.gpg.key -o /etc/apt/trusted.gpg.d/apt.llvm.org.asc \
    && apt-get update && apt-get install -y \
    build-essential \
    clang-18 \
    clang-19 \
    clang-tools-19 \
    gcc-13 \
    g++-13 \
    gcc-14 \
    g++-14 \
    pandoc \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 13 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 13 \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 14 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 14 \
    && update-alternatives --install /usr/bin/clang clang /usr/bin/clang-18 18 \
    && update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-19 19 \
    && update-alternatives --install /usr/bin/clang clang /usr/bin/clang-19 19 \
    && update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-19 19 \
    && bash ./install-dependencies.sh \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

ENV CC=clang-19 CXX=clang++-19

WORKDIR /build_seastar
COPY seastar/ seastar/
WORKDIR /build_seastar/seastar
RUN mkdir -p /seastar_install
RUN ./configure.py --mode=release --prefix=/seastar_install --compiler=clang++ --c-compiler=clang
RUN ninja -C build/release -j${THREADS} install 

FROM build_seastar AS build_kvstore

WORKDIR /build_kv_store

COPY CMakeLists.txt ./
COPY src/ src/
COPY --from=build_seastar /seastar_install /seastar_install

RUN mkdir build && cd build \
    && cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/seastar_install -DCMAKE_MODULE_PATH=/seastar_install/lib/cmake .. \
    && cmake --build .

FROM build_seastar AS runtime

WORKDIR /app

COPY --from=build_kvstore /build_kv_store/bin/kv_store ./kv_store

CMD ["/bin/bash"]