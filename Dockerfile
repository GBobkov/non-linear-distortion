# ============================================
# STAGE 1: BUILD
# ============================================
FROM alpine:3.19 AS builder

RUN echo "ipv4" >> /etc/apk/protocols

RUN apk add --no-cache --allow-untrusted \
    g++ \
    alsa-lib-dev

WORKDIR /build

# Копируем исходники из src/
COPY src/audiogenerator_alsa.h .
COPY src/audiogenerator_alsa.cpp .
COPY src/main.cpp .
COPY dataset /build/dataset

RUN g++ -o experiment \
    main.cpp \
    audiogenerator_alsa.cpp \
    -lasound \
    -lm \
    -O2 \
    -std=c++17 \
    -static-libstdc++ \
    -static-libgcc

# ============================================
# STAGE 2: RUNTIME
# ============================================
FROM alpine:3.19

RUN echo "ipv4" >> /etc/apk/protocols

RUN apk add --no-cache --allow-untrusted \
    alsa-lib \
    alsa-utils

ARG AUDIO_GID=63

RUN addgroup -g ${AUDIO_GID} audio_host && \
    adduser -D -G audio_host experiment

WORKDIR /app

# Копируем entrypoint из корня
COPY docker_entrypoint.sh .
COPY --from=builder /build/experiment .
COPY --from=builder /build/dataset ./dataset

RUN chmod +x docker_entrypoint.sh && \
    chown -R experiment:audio_host /app

USER experiment

ENTRYPOINT ["./docker_entrypoint.sh"]
