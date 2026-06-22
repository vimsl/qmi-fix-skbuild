#!/bin/sh
# install-and-test.sh — Load qmi_fix_skb and verify

set -e

echo "=== (1) Check kernel config ==="
if ! grep -q CONFIG_KPROBES=y /boot/config-$(uname -r) 2>/dev/null; then
    echo "WARN: CONFIG_KPROBES may not be enabled, trying anyway..."
fi

echo ""
echo "=== (2) Check qmi_wwan_f is loaded ==="
lsmod | grep qmi_wwan || { echo "ERROR: qmi_wwan_f not loaded"; exit 1; }

echo ""
echo "=== (3) Insert module ==="
insmod qmi_fix_skb.ko
dmesg | tail -3

echo ""
echo "=== (4) Wait 10s and check fix count ==="
sleep 10
cat /sys/module/qmi_fix_skb/parameters/count

echo ""
echo "=== (5) Check for tailroom errors (should be 0 or stopped growing) ==="
LAST_COUNT=$(dmesg | grep -c "skb_tailroom small" || true)
echo "tailroom errors in dmesg: $LAST_COUNT"

echo ""
echo "=== (6) Watch tailroom errors live (Ctrl-C to stop) ==="
echo "If you see fix_count growing and tailroom errors stopping, fix is working."
echo "---"
dmesg -w | grep --line-buffered "skb_tailroom small\|qmi_fix_skb"
