# Use the espressif/idf:release-v5.3 as the base image
FROM espressif/idf:release-v5.3

# Install Node.js 20.x
RUN apt-get update && \
    apt-get install -y curl python3-pip && \
    curl -fsSL https://deb.nodesource.com/setup_20.x | bash - && \
    apt-get install -y nodejs && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Create an unprivileged user with UID:GID 1000:1000
RUN groupadd -g 1000 builder && \
    useradd -m -u 1000 -g builder -s /bin/bash builder

# Create /project directory and set permissions
RUN mkdir /project && \
    chown builder:builder /project

RUN pip3 install bitaxetool

# Set working directory
WORKDIR /project

# Switch to the unprivileged user
USER builder

