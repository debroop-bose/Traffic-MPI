FROM ubuntu:20.04

# install C++ compiler, MPICH and SSH
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential \
    mpich \
    libmpich-dev \
    openssh-server \
    openssh-client \
    iputils-ping && \
    rm -rf /var/lib/apt/lists/*

# create SSH folder
RUN mkdir -p /root/.ssh

# create SSH key for communication between MPI containers
RUN ssh-keygen -t rsa -N "" -f /root/.ssh/id_rsa

# allow passwordless SSH
RUN cat /root/.ssh/id_rsa.pub >> /root/.ssh/authorized_keys

# avoid asking yes/no when connecting to another container for the first time
RUN printf "Host *\n    StrictHostKeyChecking no\n    UserKnownHostsFile /dev/null\n" > /root/.ssh/config

# correct SSH permissions
RUN chmod 700 /root/.ssh && \
    chmod 600 /root/.ssh/id_rsa && \
    chmod 644 /root/.ssh/id_rsa.pub && \
    chmod 600 /root/.ssh/authorized_keys && \
    chmod 600 /root/.ssh/config

# SSH server requires this directory
RUN mkdir -p /var/run/sshd

# project folder
WORKDIR /app

# SSH port
EXPOSE 22

# start SSH server and keep container running
CMD ["/usr/sbin/sshd", "-D"]