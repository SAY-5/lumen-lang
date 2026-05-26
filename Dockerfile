FROM ubuntu:22.04 AS build
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential cmake git ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build -j"$(nproc)" --target lumen

FROM ubuntu:22.04 AS runtime
RUN apt-get update && apt-get install -y --no-install-recommends \
      libstdc++6 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /src/build/lumen /usr/local/bin/lumen
COPY --from=build /src/examples /opt/lumen/examples
ENTRYPOINT ["/usr/local/bin/lumen"]
