# pricer/Dockerfile
FROM ubuntu:22.04 AS build

ENV DEBIAN_FRONTEND=noninteractive

# Install build deps
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git ca-certificates libboost-system-dev libboost-chrono-dev libboost-test-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY pricer_server.cpp /src/

# Build single binary with g++
RUN g++ -std=c++17 -O3 -march=native pricer_server.cpp -o /usr/local/bin/pricer_server -lboost_system -lpthread

# Final runtime image (slim)
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates libstdc++6 && rm -rf /var/lib/apt/lists/*
COPY --from=build /usr/local/bin/pricer_server /usr/local/bin/pricer_server
EXPOSE 9000
ENTRYPOINT ["/usr/local/bin/pricer_server"]
