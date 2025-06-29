# Multi-stage Docker build for PyFolio C++ Production Environment
# Optimized for size, security, and performance

# Stage 1: Build Environment
FROM ubuntu:24.04 AS builder

LABEL maintainer="PyFolio C++ Team"
LABEL description="Production-ready financial portfolio analytics engine"
LABEL version="1.0.0"

# Avoid interactive prompts during build
ENV DEBIAN_FRONTEND=noninteractive
ENV PYTHONUNBUFFERED=1

# Update package list and install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    wget \
    pkg-config \
    ninja-build \
    ccache \
    clang-15 \
    clang-tidy-15 \
    clang-format-15 \
    libc++-15-dev \
    libc++abi-15-dev \
    python3 \
    python3-pip \
    python3-dev \
    python3-venv \
    libssl-dev \
    zlib1g-dev \
    libbz2-dev \
    libreadline-dev \
    libsqlite3-dev \
    libncursesw5-dev \
    libxml2-dev \
    libxmlsec1-dev \
    libffi-dev \
    liblzma-dev \
    libopenmpi-dev \
    openmpi-bin \
    libhdf5-dev \
    libeigen3-dev \
    liblapack-dev \
    libopenblas-dev \
    libgsl-dev \
    libtbb-dev \
    doxygen \
    graphviz \
    gcovr \
    lcov \
    cppcheck \
    valgrind \
    && rm -rf /var/lib/apt/lists/*

# Set up environment
ENV CC=clang-15
ENV CXX=clang++-15
ENV CMAKE_GENERATOR=Ninja

# Create non-root user for security
RUN groupadd -r pyfolio && useradd -r -g pyfolio -s /bin/bash pyfolio

# Create directories
RUN mkdir -p /app/build /app/install /app/data /app/logs \
    && chown -R pyfolio:pyfolio /app

# Copy source code
COPY --chown=pyfolio:pyfolio . /app/source

# Switch to non-root user
USER pyfolio
WORKDIR /app

# Build with optimizations
RUN cd build && \
    cmake ../source \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/app/install \
        -DCMAKE_CXX_COMPILER=clang++-15 \
        -DCMAKE_C_COMPILER=clang-15 \
        -DCMAKE_CXX_FLAGS="-O3 -DNDEBUG -march=native -mtune=native -flto" \
        -DCMAKE_EXE_LINKER_FLAGS="-flto" \
        -DCMAKE_SHARED_LINKER_FLAGS="-flto" \
        -DUSE_OPENMP=ON \
        -DUSE_MPI=ON \
        -DENABLE_SIMD=ON \
        -DENABLE_CUDA=OFF \
        -DENABLE_TESTING=OFF \
        -DENABLE_BENCHMARKS=OFF \
        -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
        -G Ninja && \
    ninja -j$(nproc) && \
    ninja install

# Stage 2: Runtime Environment
FROM ubuntu:24.04 AS runtime

# Install only runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl3 \
    zlib1g \
    libopenmpi3 \
    libhdf5-103-1 \
    liblapack3 \
    libopenblas0 \
    libgsl27 \
    libtbb12 \
    python3 \
    python3-pip \
    curl \
    wget \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && apt-get clean

# Create non-root user
RUN groupadd -r pyfolio && useradd -r -g pyfolio -s /bin/bash -m pyfolio

# Create application directories
RUN mkdir -p /app/bin /app/lib /app/data /app/logs /app/config \
    && chown -R pyfolio:pyfolio /app

# Copy built artifacts from builder stage
COPY --from=builder --chown=pyfolio:pyfolio /app/install /app
COPY --from=builder --chown=pyfolio:pyfolio /app/source/examples /app/examples
COPY --from=builder --chown=pyfolio:pyfolio /app/source/docker/entrypoint.sh /app/bin/
COPY --from=builder --chown=pyfolio:pyfolio /app/source/docker/healthcheck.sh /app/bin/

# Set up Python environment
USER pyfolio
WORKDIR /app

# Install Python dependencies
RUN python3 -m pip install --user --no-cache-dir \
    numpy \
    pandas \
    scipy \
    matplotlib \
    seaborn \
    plotly \
    dash \
    fastapi \
    uvicorn \
    pydantic \
    aiofiles \
    httpx \
    websockets \
    redis \
    psycopg2-binary \
    sqlalchemy \
    alembic

# Set environment variables
ENV PATH="/app/bin:$PATH"
ENV LD_LIBRARY_PATH="/app/lib:$LD_LIBRARY_PATH"
ENV PYTHONPATH="/app/lib:$PYTHONPATH"
ENV PYFOLIO_CONFIG_DIR="/app/config"
ENV PYFOLIO_DATA_DIR="/app/data"
ENV PYFOLIO_LOG_DIR="/app/logs"

# Create default configuration
RUN echo '{"debug": false, "log_level": "INFO", "max_threads": 4, "cache_size": "1GB"}' > /app/config/pyfolio.json

# Expose ports
EXPOSE 8000 8080 9090

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD /app/bin/healthcheck.sh

# Default command
CMD ["/app/bin/entrypoint.sh"]

# Stage 3: Development Environment (Optional)
FROM builder AS development

USER pyfolio
WORKDIR /app

# Install additional development tools
USER root
RUN apt-get update && apt-get install -y \
    gdb \
    strace \
    ltrace \
    perf-tools-unstable \
    htop \
    iotop \
    vim \
    emacs \
    nano \
    tmux \
    screen \
    && rm -rf /var/lib/apt/lists/*

USER pyfolio

# Install Python development packages
RUN python3 -m pip install --user --no-cache-dir \
    pytest \
    pytest-cov \
    pytest-benchmark \
    black \
    flake8 \
    mypy \
    jupyter \
    ipython \
    notebook \
    jupyterlab

# Set development environment variables
ENV CMAKE_BUILD_TYPE=Debug
ENV PYFOLIO_DEBUG=1
ENV PYFOLIO_ENABLE_PROFILING=1

# Development command
CMD ["/bin/bash"]