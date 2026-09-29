#!/usr/bin/env python3
"""Read-only configuration gate for daemon-v0.15.0 + this Protocol2 subset.
Does not edit files, restart services, write a bus, or validate physical hardware.
"""
import argparse
import json
from pathlib import Path
import sys
import tomllib


def check(text):
    config = tomllib.loads(text)
    bus = config.get('bus', {})
    if bus.get('fast_sync_read') is not False:
        raise ValueError('Set an uncommented fast_sync_read = false inside the existing [bus] section; omitted defaults to true in daemon-v0.15.0.')
    port = bus.get('port', '/dev/ttyS2')
    if not isinstance(port, str) or not port.startswith('/dev/'):
        raise ValueError('Expected an actual /dev/ serial device path for the Linux main controller.')
    return {'configuration_compatible': True, 'port': port, 'instruction': '0x82',
            'id': 200, 'address': 124, 'length': 12, 'hardware_validated': False,
            'note': 'Check systemctl cat robotd to ensure this is the configuration used by the running service. Restart is a separate operator action.'}


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('config', type=Path)
    args = p.parse_args()
    try:
        print(json.dumps(check(args.config.read_text()), indent=2))
    except (ValueError, OSError) as e:
        print(f'NOT COMPATIBLE: {e}', file=sys.stderr)
        sys.exit(1)
