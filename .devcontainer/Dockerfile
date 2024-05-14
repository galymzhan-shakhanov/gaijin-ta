FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    software-properties-common ca-certificates gpg wget git ninja-build 2>&1

RUN wget -qO- https://github.com/Kitware/CMake/releases/download/v3.29.3/cmake-3.29.3-linux-x86_64.tar.gz | tar xz --strip-components=1 --one-top-level=/opt/cmake \
    && ln -s /opt/cmake/bin/cmake /usr/local/bin/cmake \
    && ln -s /opt/cmake/bin/ctest /usr/local/bin/ctest

RUN echo "deb http://apt.llvm.org/bookworm/ llvm-toolchain-bookworm main" > /etc/apt/sources.list.d/llvm.list && \
    wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | tee /etc/apt/trusted.gpg.d/llvm.asc && \
    apt-get update && apt-get install -y --no-install-recommends \
    clang-19 clang-tidy-19 clang-format-19 clangd-19 llvm-19 lldb-19 lld-19 \
    libclang-rt-19-dev libc++-19-dev libc++abi-19-dev libunwind-19-dev \
    && ln -s /usr/bin/clang-19 /usr/bin/clang \
    && ln -s /usr/bin/clang++-19 /usr/bin/clang++ \
    && ln -s /usr/bin/clang-tidy-19 /usr/bin/clang-tidy \
    && ln -s /usr/bin/clang-format-19 /usr/bin/clang-format \
    && ln -s /usr/bin/clangd-19 /usr/bin/clangd \
    && ln -s /usr/bin/llvm-ar-19 /usr/bin/llvm-ar \
    && ln -s /usr/bin/llvm-ranlib-19 /usr/bin/llvm-ranlib \
    && ln -s /usr/bin/lldb-19 /usr/bin/lldb \
    && ln -s /usr/bin/lld-19 /usr/bin/lld

RUN apt-get update && apt-get install -y --no-install-recommends \
    libnuma1 2>&1

RUN echo "deb https://packages.microsoft.com/debian/12/prod bookworm main" > /etc/apt/sources.list.d/microsoft.list \
    && wget -qO- https://packages.microsoft.com/keys/microsoft.asc | tee /etc/apt/trusted.gpg.d/microsoft.asc \
    && apt-get update && apt-get install -y --no-install-recommends libmsquic=2.3.5 \
    && ln -s /usr/lib/x86_64-linux-gnu/libmsquic.so.2.3.5 /usr/lib/x86_64-linux-gnu/libmsquic.so

RUN apt-get autoremove -y \
    && apt-get clean -y \
    && rm -rf /var/lib/apt/lists/* \
    && rm -rf /tmp/*

ENV DEBIAN_FRONTEND=dialog
ENV SHELL /bin/bash

ARG USERNAME=vscode
