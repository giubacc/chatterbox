#!/bin/bash

USER_UID=$(awk 'NR==1' /hnfo)
USER_GID=$(awk 'NR==2' /hnfo)
DKR_GID=$(awk 'NR==3' /hnfo)

echo putinfo.sh called with user: [$USERNAME, $USER_UID, $USER_GID, $DKR_GID]
groupadd --gid $USER_GID $USERNAME
useradd --uid $USER_UID --gid $USER_GID -m -s /bin/bash $USERNAME
chown -R $USERNAME:$USER_GID /home/$USERNAME
echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME
chmod 0440 /etc/sudoers.d/$USERNAME
groupmod --gid $DKR_GID docker
usermod -a -G $DKR_GID $USERNAME
