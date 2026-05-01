#!/usr/bin/env bash
#
# Capture hardware/software fingerprint as JSON, one line. Both the PHP
# and Python runners can prepend this to their output so any downstream
# consumer knows what machine produced the numbers.

set -euo pipefail

cd "$(dirname "$0")/.."
ROOT="$(pwd)"
SO="$ROOT/modules/numphp.so"

cpu_model=$(grep -m1 "model name" /proc/cpuinfo 2>/dev/null | sed 's/^.*: //' || echo "unknown")
cpu_mhz=$(grep -m1 "cpu MHz" /proc/cpuinfo 2>/dev/null | sed 's/^.*: //' | xargs printf "%.0f" || echo "unknown")
cpu_count=$(nproc 2>/dev/null || echo "unknown")
mem_total=$(grep -m1 MemTotal /proc/meminfo | awk '{print $2 " " $3}' || echo "unknown")
kernel=$(uname -srm)
distro=$(grep -m1 PRETTY_NAME /etc/os-release 2>/dev/null | sed -e 's/.*="//' -e 's/"$//' || echo "unknown")

php_version=$(php -r 'echo PHP_VERSION;' 2>/dev/null || echo "unknown")
numphp_version=$(php -d extension="$SO" -r 'echo phpversion("numphp");' 2>/dev/null || echo "unknown")

# Which OpenBLAS does numphp link against?
numphp_blas=$(ldd "$SO" 2>/dev/null | grep -i 'openblas\|blas\|lapack' | awk '{print $1, "->", $3}' | tr '\n' '; ' || echo "unknown")

# Which BLAS does NumPy use? Let numpy itself tell us — be tolerant if numpy
# isn't available; the fingerprint stage runs before run.py.
numpy_info=$(./bench/.venv/bin/python -c "
import numpy as np, json
info = {'version': np.__version__}
try:
    cfg = np.show_config(mode='dicts')
    info['blas'] = cfg.get('Build Dependencies', {}).get('blas', {}).get('name', 'unknown')
    info['lapack'] = cfg.get('Build Dependencies', {}).get('lapack', {}).get('name', 'unknown')
except Exception as e:
    info['error'] = str(e)
print(json.dumps(info))
" 2>/dev/null || echo '{"version":"not-installed"}')

cat <<EOF
{
  "captured_at": "$(date -u +%FT%TZ)",
  "host": {
    "kernel": "$kernel",
    "distro": "$distro",
    "cpu_model": "$cpu_model",
    "cpu_mhz_at_capture": "$cpu_mhz",
    "cpu_count": "$cpu_count",
    "mem_total": "$mem_total"
  },
  "numphp": {
    "version": "$numphp_version",
    "linked_blas_lapack": "$numphp_blas",
    "php_version": "$php_version"
  },
  "numpy": $numpy_info
}
EOF
