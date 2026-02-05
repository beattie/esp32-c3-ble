ARG DOCKER_TAG=v5.5.2
FROM espressif/idf:${DOCKER_TAG}

RUN apt-get update \
    && apt-get install -y --no-install-recommends udev usbutils neovim \
    && rm -rf /var/lib/apt/lists/*

RUN echo ". /opt/esp/idf/export.sh" >> /etc/bash.bashrc
