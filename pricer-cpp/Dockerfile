FROM gcc:12.2.0

WORKDIR /app

COPY . .

RUN apt-get update && apt-get install -y \
    libboost-system-dev \
    libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

RUN g++ -std=c++17 -O3 \
    pricer_server.cpp optiondb.cpp \
    -o /usr/local/bin/pricer_server \
    -lboost_system -lsqlite3 -lpthread

EXPOSE 9000

CMD ["/usr/local/bin/pricer_server"]