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

RUN g++ -o experiment \
    main.cpp \
    audiogenerator_alsa.cpp \
    -lasound \
    -lm \
    -O2 \
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

# Группа с GID 63 (как на хосте Fedora)
RUN addgroup -g 63 audio_host && \
    adduser -D -G audio_host experiment

WORKDIR /app

# Копируем entrypoint из корня
COPY docker_entrypoint.sh .
COPY --from=builder /build/experiment .

RUN chmod +x docker_entrypoint.sh && \
    chown -R experiment:audio_host /app

USER experiment

ENTRYPOINT ["./docker_entrypoint.sh"]
