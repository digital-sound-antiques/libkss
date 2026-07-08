FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Copy the local working tree (submodules are expected to be checked out already).
COPY . .

# Build libkss.a and the kss2wav example out-of-source.
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && cmake --build build --target kss2wav --parallel

CMD ["/bin/bash"]
