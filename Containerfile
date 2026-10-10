# QRPickle development container: the same tools and versions as CI (Ubuntu 24.04 runner).
# Use it through scripts/dev.sh (podman or docker). See docs/DEVELOPMENT.md.
ARG UBUNTU=24.04
FROM docker.io/library/ubuntu:${UBUNTU}

# Pinned like CI (.github/workflows/*.yml); bump both together.
ARG PLATFORMIO_VERSION=6.2.0
ARG CLANG_FORMAT_VERSION=19.1.7
ARG RUFF_VERSION=0.16.10
ARG TYPST_VERSION=0.15.1

ENV DEBIAN_FRONTEND=noninteractive PIP_DISABLE_PIP_VERSION_CHECK=1
RUN apt-get update \
 && apt-get install -y --no-install-recommends python3 python3-venv git curl ca-certificates xz-utils g++ \
      pandoc imagemagick poppler-utils \
 && rm -rf /var/lib/apt/lists/*
RUN python3 -m venv /opt/venv \
 && /opt/venv/bin/pip install --no-cache-dir "platformio==${PLATFORMIO_VERSION}" \
      "clang-format==${CLANG_FORMAT_VERSION}" "ruff==${RUFF_VERSION}" \
      # esptool's Python helpers: PlatformIO would install them at runtime into this (ephemeral)
      # venv, while its package cache volume remembers them as installed -> broken on the next run.
      intelhex pyserial
RUN curl -sSfL "https://github.com/typst/typst/releases/download/v${TYPST_VERSION}/typst-x86_64-unknown-linux-musl.tar.xz" \
      | tar -xJ -C /tmp && mv /tmp/typst-*/typst /usr/local/bin/ && rm -rf /tmp/typst-*

ENV PATH=/opt/venv/bin:$PATH \
    CLANG_FORMAT=/opt/venv/bin/clang-format \
    # Build output and PlatformIO packages live in volumes, never in the mounted source tree.
    PLATFORMIO_WORKSPACE_DIR=/pio-workspace \
    PLATFORMIO_CORE_DIR=/pio-core
RUN git config --global --add safe.directory /src
WORKDIR /src
