# To build this, you have to use the BuildKit backend.
# For further information visit https://docs.docker.com/build/buildkit/

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
    libmaxminddb-dev \
    git \
    ca-certificates \
    python3-polib

RUN mkdir -p /build-stage/dist /build-stage/build /build-stage/source

RUN --mount=type=cache,target=/build-stage/build \
    --mount=type=bind,source=.,target=/build-stage/source \
    cd /build-stage/build && \
     cmake ../source \
    -Wno-dev \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=../dist \
    -GNinja && \
    cmake --build . --target install

FROM debian:13-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -y \
    --no-install-recommends \
    libicu76 \
    libpng16-16t64 \
    libcurl4t64 \
    libsqlite3-0 \
    zlib1g \
    libmaxminddb0 \
    ca-certificates

COPY --from=build-stage /build-stage/dist /dist

WORKDIR /dist

CMD ["./Infclass-Server"]
