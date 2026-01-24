FROM debian:bookworm-slim

# Install CMake, build tools, QEMU, and cross-compilers
# Note:
# - gcc-mipsel-linux-gnu is for MIPS 32-bit Little Endian
# - gcc-mips64el-linux-gnuabi64 is for MIPS64 Little Endian
# - gcc-powerpc64le-linux-gnu is for PowerPC 64 Little Endian
# - gcc-s390x-linux-gnu is for IBM Z (S390x)
# - gcc-arm-linux-gnueabihf is for ARM 32-bit (Cortex-A/M)
# - gcc-aarch64-linux-gnu is for ARM 64-bit (Cortex-A)
# - gcc-riscv64-linux-gnu is for RISC-V 64-bit
# - gcc-sparc64-linux-gnu is for SPARC 64-bit
# - gcc-powerpc-linux-gnu is for PowerPC 32-bit
RUN apt-get update && apt-get install -y \
    cmake \
    build-essential \
    qemu-user \
    qemu-user-static \
    gcc-mipsel-linux-gnu \
    gcc-mips64el-linux-gnuabi64 \
    gcc-powerpc64le-linux-gnu \
    gcc-s390x-linux-gnu \
    gcc-arm-linux-gnueabihf \
    gcc-aarch64-linux-gnu \
    gcc-riscv64-linux-gnu \
    gcc-sparc64-linux-gnu \
    gcc-powerpc-linux-gnu \
    gcc-multilib \
    wget \
    xz-utils \
    && rm -rf /var/lib/apt/lists/*

# Install Espressif Xtensa Toolchain (ESP32)
RUN wget -q https://github.com/espressif/crosstool-NG/releases/download/esp-13.2.0_20240530/xtensa-esp32-elf-13.2.0_20240530-x86_64-linux-gnu.tar.xz \
    && tar -xJf xtensa-esp32-elf-13.2.0_20240530-x86_64-linux-gnu.tar.xz -C /opt \
    && rm xtensa-esp32-elf-13.2.0_20240530-x86_64-linux-gnu.tar.xz

ENV PATH="/opt/xtensa-esp32-elf/bin:${PATH}"

WORKDIR /src