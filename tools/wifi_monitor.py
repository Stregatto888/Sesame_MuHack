#!/usr/bin/env python3
"""
Sesame MuHack — WiFi AP monitor/diagnostic tool
Continuously scans for the "Sesame MuHack" network.
If not found within TIMEOUT_SECONDS, reports scan results to help diagnose the issue.
"""

import subprocess
import time
import sys

TARGET_SSID   = "Sesame MuHack"
SCAN_INTERVAL = 5       # seconds between scans
TIMEOUT_SEC   = 90      # give up after this many seconds
FOUND_CONFIRM = 2       # how many consecutive scans must see it before declaring success


def scan_wifi() -> list[str]:
    """Return list of visible SSIDs using nmcli."""
    try:
        result = subprocess.run(
            ["nmcli", "-t", "-f", "SSID", "device", "wifi", "list", "--rescan", "yes"],
            capture_output=True, text=True, timeout=20
        )
        ssids = [line.strip() for line in result.stdout.splitlines() if line.strip()]
        return ssids
    except FileNotFoundError:
        # fallback: use iwlist if nmcli not available
        try:
            iface = get_wifi_interface()
            result = subprocess.run(
                ["sudo", "iwlist", iface, "scan"],
                capture_output=True, text=True, timeout=20
            )
            ssids = []
            for line in result.stdout.splitlines():
                line = line.strip()
                if line.startswith('ESSID:"'):
                    ssid = line[7:-1]
                    if ssid:
                        ssids.append(ssid)
            return ssids
        except Exception as e:
            print(f"  [WARN] Scan failed: {e}")
            return []
    except subprocess.TimeoutExpired:
        print("  [WARN] Scan timed out.")
        return []


def get_wifi_interface() -> str:
    try:
        result = subprocess.run(["iw", "dev"], capture_output=True, text=True)
        for line in result.stdout.splitlines():
            if "Interface" in line:
                return line.strip().split()[-1]
    except Exception:
        pass
    return "wlan0"


def main():
    print(f"{'='*55}")
    print(f"  Sesame MuHack — WiFi AP Monitor")
    print(f"  Looking for SSID: '{TARGET_SSID}'")
    print(f"  Scan interval: {SCAN_INTERVAL}s  |  Timeout: {TIMEOUT_SEC}s")
    print(f"{'='*55}\n")

    start_time   = time.time()
    scan_count   = 0
    confirm_streak = 0

    while True:
        elapsed = time.time() - start_time
        scan_count += 1

        print(f"[{elapsed:5.0f}s] Scan #{scan_count}...", end=" ", flush=True)
        ssids = scan_wifi()

        if TARGET_SSID in ssids:
            confirm_streak += 1
            print(f"FOUND! (streak {confirm_streak}/{FOUND_CONFIRM})")
            if confirm_streak >= FOUND_CONFIRM:
                print(f"\n{'='*55}")
                print(f"  SUCCESS: '{TARGET_SSID}' is visible and stable!")
                print(f"  Connect and open http://192.168.4.1")
                print(f"{'='*55}")
                sys.exit(0)
        else:
            confirm_streak = 0
            visible = [s for s in ssids if s]
            print(f"NOT found. Visible ({len(visible)}): {', '.join(visible[:8]) or 'none'}")

        if elapsed >= TIMEOUT_SEC:
            print(f"\n{'='*55}")
            print(f"  TIMEOUT after {TIMEOUT_SEC}s — '{TARGET_SSID}' never appeared.")
            print(f"\n  Possible causes for ESP32-S2 AP failure:")
            print(f"   1. softAP() returned true but netif never came up")
            print(f"      → Check serial monitor for 'AP Created' message")
            print(f"   2. Power issue: servos drawing too much current at boot")
            print(f"      → Disconnect servos and retry (WiFi init is before servo init)")
            print(f"   3. USB CDC event loop contention")
            print(f"      → Add longer delay before WiFi init in setup()")
            print(f"   4. Flash corruption")
            print(f"      → Erase flash fully, re-upload firmware")
            print(f"\n  Run these commands to re-flash:")
            print(f"    cd /home/stregatto/workspace/hardware/Sesame_MuHack")
            print(f"    pio run --target erase")
            print(f"    pio run --target upload")
            print(f"{'='*55}")
            sys.exit(1)

        time.sleep(SCAN_INTERVAL)


if __name__ == "__main__":
    main()
