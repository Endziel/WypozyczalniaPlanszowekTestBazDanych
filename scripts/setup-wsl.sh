#!/usr/bin/env bash
# Jednorazowa instalacja narzedzi i bibliotek w WSL (Ubuntu), potrzebnych,
# zeby Visual Studio moglo kompilowac i debugowac projekt.
# Uruchomienie (w terminalu WSL):  bash scripts/setup-wsl.sh
set -euo pipefail

SUDO=""
[ "$(id -u)" -ne 0 ] && SUDO="sudo"

$SUDO apt-get update
$SUDO env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    g++ gdb cmake ninja-build make rsync zip pkg-config git ca-certificates \
    libpq-dev libsqlite3-dev libmongoc-dev libbson-dev libssl-dev

# Klient Memgraph (mgclient) - nie ma go w repozytorium Ubuntu
if [ ! -f /usr/local/lib/libmgclient.a ]; then
    tmp=$(mktemp -d)
    git clone --depth 1 --branch v1.4.5 https://github.com/memgraph/mgclient.git "$tmp/mgclient"
    cmake -S "$tmp/mgclient" -B "$tmp/mgclient/build" -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_TESTING=OFF -DCMAKE_INSTALL_LIBDIR=lib
    cmake --build "$tmp/mgclient/build" -j"$(nproc)"
    $SUDO cmake --install "$tmp/mgclient/build"
    rm -rf "$tmp"
fi

echo
echo "Gotowe. Teraz otworz folder projektu w Visual Studio i wybierz cel 'WSL: Ubuntu'."
