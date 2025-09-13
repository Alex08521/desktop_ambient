#!/bin/bash
SERVICE_NAME="desktop_ambient"
SERVICE_FILE="/etc/systemd/user/${SERVICE_NAME}.service"
BINARY_PATH="/usr/local/bin/${SERVICE_NAME}"

#mkdir -p build
cd build
#cmake ..
#make -j$(nproc)

sudo cp desktop_ambient ${BINARY_PATH}

cat << EOF | sudo tee ${SERVICE_FILE}
[Unit]
Description=Background Sound Service
After=graphical-session.target

[Service]
ExecStart=${BINARY_PATH}
Restart=always
Environment=DISPLAY=:0
Environment=XDG_RUNTIME_DIR=/run/user/$(id -u)
Environment=PULSE_RUNTIME_PATH=/run/user/$(id -u)/pulse

[Install]
WantedBy=default.target
EOF

systemctl --user daemon-reload
systemctl --user enable ${SERVICE_NAME}
systemctl --user start ${SERVICE_NAME}

echo "Service installed and started. Use 'journalctl --user -u desktop_ambient -f' to view logs."