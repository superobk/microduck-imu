#!/usr/bin/env python3
"""Summarize captures/workbench logs without guessing a physical root cause."""
import argparse
import json
from pathlib import Path


def summarize(path):
    rows = [json.loads(s) for s in path.read_text().splitlines() if s.strip()]
    diag = [r['diagnostic'] for r in rows if isinstance(r.get('diagnostic'), dict)]
    return {'file': path.name, 'records': len(rows),
            'who_values_hex': sorted({f"0x{x['who_am_i']:02X}" for x in diag}),
            'init_stages': sorted({x.get('init_stage_name', 'legacy-unavailable') for x in diag}),
            'init_errors': sorted({x.get('init_error_name', 'legacy-unavailable') for x in diag}),
            'gyro_changes': sum(a['gyro_seq'] != b['gyro_seq'] for a,b in zip(diag,diag[1:])),
            'quat_changes': sum(a['quat_seq'] != b['quat_seq'] for a,b in zip(diag,diag[1:])),
            'transport_or_read_errors': sum(bool(r.get('error')) for r in rows),
            'is_simulated': any(r.get('is_simulated') is True for r in rows),
            'root_cause_confirmed': False,
            'note': 'WHO00/FF with unchanged counters points to init failure; SPIERR=0 does not exclude electrical faults. Inspect the actual power/SPI signals.'}


if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('logs',type=Path,nargs='+')
    a=p.parse_args();print(json.dumps([summarize(f) for f in a.logs],indent=2,ensure_ascii=False))
