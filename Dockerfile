FROM ubuntu:24.04 AS build
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential cmake git ca-certificates pkg-config \
        libpq-dev libsqlite3-dev libmongoc-dev libbson-dev libssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Klient Memgraph (Bolt) - nie ma go w repozytorium Ubuntu, budujemy ze zrodel
RUN git clone --depth 1 --branch v1.4.5 https://github.com/memgraph/mgclient.git /tmp/mgclient \
    && cmake -S /tmp/mgclient -B /tmp/mgclient/build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DCMAKE_INSTALL_LIBDIR=lib \
    && cmake --build /tmp/mgclient/build -j \
    && cmake --install /tmp/mgclient/build \
    && rm -rf /tmp/mgclient

WORKDIR /src
COPY CMakeLists.txt .
COPY src ./src
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j

FROM ubuntu:24.04
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        libpq5 libsqlite3-0 libmongoc-1.0-0 libbson-1.0-0 libssl3 \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=build /src/build/ztb_bench /app/ztb_bench
COPY sql ./sql
ENV SQL_DIR=/app/sql RESULTS_DIR=/app/results
ENTRYPOINT ["/app/ztb_bench"]
