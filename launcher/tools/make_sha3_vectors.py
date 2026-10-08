#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Independent hashlib vectors at SHA3 rate boundaries and package block sizes."""
import base64
import hashlib
import json
import random
import sys

rng = random.Random(99027)
inputs = [b'', b'abc', b'a' * 1000000]
for size in [1, 7, 8, 31, 32, 63, 64, 134, 135, 136, 137, 138,
             270, 271, 272, 273, 4095, 4096, 4097, 65535, 65536, 65537]:
    inputs.append(rng.randbytes(size))
json.dump([{'data': base64.b64encode(data).decode('ascii'),
            'sha3_256': hashlib.sha3_256(data).hexdigest()} for data in inputs], sys.stdout)
sys.stdout.write('\n')
