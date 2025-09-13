#!/bin/bash
SERVICE_NAME="desktop_ambient"
SERVICE_FILE="/etc/systemd/user/${SERVICE_NAME}.service"
BINARY_PATH="/usr/local/bin/${SERVICE_NAME}"

systemctl --user stop ${SERVICE_NAME}
systemctl --user disable ${SERVICE_NAME}
sudo rm -f ${BINARY_PATH} ${SERVICE_FILE}
systemctl --user daemon-reload

echo "Service uninstalled."