FROM ghcr.io/namo-robotics/sun-dev:ubuntu26.04-llvm20

USER root

RUN apt-get update && apt-get install -y --no-install-recommends \
    # GTK, graphics, and media libraries.
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libx11-dev \
    libxext-dev \
    libgtk-4-dev \
    libsdl3-dev \
    libepoxy-dev \
    libdecor-0-plugin-1-cairo \
    librsvg2-2 \
    fonts-dejavu-core \
    libgirepository1.0-dev \
    gobject-introspection \
    # Media playback and headless GUI tests.
    ffmpeg \
    gstreamer1.0-libav \
    gstreamer1.0-plugins-good \
    xvfb \
    xauth \
    dbus-x11 \
    && rm -rf /var/lib/apt/lists/*

# Install Sun and the workspace dependency manager.
COPY scripts/bootstrap-sun.sh /usr/local/bin/install-sun-dev
RUN bash /usr/local/bin/install-sun-dev --update
RUN python3 -m venv /opt/vcs2l && /opt/vcs2l/bin/pip install --no-cache-dir vcs2l==1.1.7

ENV PATH="/opt/vcs2l/bin:${PATH}"

USER ubuntu
WORKDIR /workspaces/sun_ui
