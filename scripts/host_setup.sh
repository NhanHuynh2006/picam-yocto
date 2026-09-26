#!/usr/bin/env bash
# Chạy MỘT LẦN trên máy build (Ubuntu). Yocto được build bên trong container
# (kas-container) nên máy host chỉ cần Docker + kas, không phụ thuộc phiên bản Ubuntu.
set -euo pipefail

sudo apt-get update
sudo apt-get install -y docker.io git python3-venv pipx bmap-tools curl

if ! groups "$USER" | grep -qw docker; then
    sudo usermod -aG docker "$USER"
    NEED_RELOGIN=1
fi

pipx ensurepath
pipx install --force kas
export PATH="$HOME/.local/bin:$PATH"

if ! command -v kas-container >/dev/null 2>&1; then
    echo ">> kas-container chưa có trong gói pip, tải script chính thức"
    mkdir -p "$HOME/.local/bin"
    curl -fsSL https://raw.githubusercontent.com/siemens/kas/master/kas-container \
        -o "$HOME/.local/bin/kas-container"
    chmod +x "$HOME/.local/bin/kas-container"
fi

kas --version
echo
echo ">> Cần ~100 GB ổ trống cho thư mục build/ và cache/."
df -h "$(dirname "$0")/.." | tail -1
if [ "${NEED_RELOGIN:-0}" = 1 ]; then
    echo ">> ĐÃ thêm $USER vào nhóm docker: đăng xuất rồi đăng nhập lại (hoặc chạy 'newgrp docker') trước khi build."
fi
