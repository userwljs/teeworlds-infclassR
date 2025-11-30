FROM debian:13-slim AS build-stage

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -y \
    --no-install-recommends \
    python3 \
    python-is-python3 \
    build-essential \
    cmake \
    libicu-dev \
    libpng-dev \
    libcurl4-openssl-dev \
    libsqlite3-dev \
    zlib1g-dev \
    ninja-build \
    libmaxminddb-dev

WORKDIR /code

COPY . .

RUN mkdir dist

RUN cmake . \
    -Wno-dev \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=./dist \
    -GNinja

RUN cmake --build . --target install


FROM debian:13-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -y \
    --no-install-recommends \
    libicu76 \
    libpng16-16t64 \
    libcurl4t64 \
    libsqlite3-0 \
    zlib1g \
    libmaxminddb0

COPY --from=build-stage /code/dist /dist

WORKDIR /dist

CMD ["./Infclass-Server"]