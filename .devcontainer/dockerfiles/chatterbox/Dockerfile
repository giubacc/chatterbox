FROM opensuse/leap:15.4
LABEL Name=chatterbox-dev

ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN zypper -n install sudo dnf bash make cmake git ccache cmake ninja gdb\
 vi jq binutils bash-completion curl docker docker-compose gcc-c++\
 astyle automake autogen libtool\
 libcurl-devel libopenssl-devel

RUN zypper clean --all

# Create the user
ADD puthnfo.sh /usr/bin/
ADD hnfo /
RUN puthnfo.sh

ADD bashrc.patch /
RUN /bin/bash -c "cat /bashrc.patch >> /home/$USERNAME/.bashrc"

USER $USERNAME
