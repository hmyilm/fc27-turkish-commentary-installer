#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Make a build-only copy with SHA3 fallback; never edit the pinned source."""
import argparse
import hashlib
from pathlib import Path
import shutil

FILES = {
    'PKG/ProsperoImageDigests.cs': '5fb2b3c2a9d11b4e675a4e0d1a3c6b2b96ed69ba43887327fad8128f69b7e2c7',
    'PFS/ProsperoOuterPfsSignature.cs': '9cff4ac9c9b55620d66fd2a1782e1c9b1585a4ace07140198a5841ae830d3d4f',
    'PFS/Compression/ProsperoPfsDigest.cs': '5fc361226ff995bf661bf6c354fb034c590a7f65e2e6cd401b75db884cd7a4f3',
    'Util/Crypto.cs': '28e67fa00626b4efa7283ba531e9d66a2c7a1c72b17a8cc0becf49f30837ec7c',
    'PFS/ProsperoPs5InnerImageReader.cs': '832d3cc0d861ff141ad164522a080f0bfd2418a4a49583629378d54178cd492e',
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    source = args.source.resolve()
    output = args.output.resolve()
    if output == source or source in output.parents or output in source.parents:
        parser.error('Source and output must be separate directories.')
    library = source / 'src/LibProsperoPkg'
    for name, digest in FILES.items():
        if hashlib.sha256((library / name).read_bytes()).hexdigest() != digest:
            parser.error('Pinned source hash mismatch: ' + name)
    if output.exists():
        if not (output / '.fc27-build-copy').is_file():
            parser.error('Existing output was not created by this script.')
        shutil.rmtree(output)
    output.mkdir(parents=True)
    (output / '.fc27-build-copy').write_text('748eabf1b7d17819528cabf367d8e27109d8fce3\n')
    shutil.copy2(source / 'README.md', output / 'README.md')
    target = output / 'src/LibProsperoPkg'
    shutil.copytree(library, target, ignore=shutil.ignore_patterns('bin', 'obj'))
    for name in FILES:
        p = target / name
        text = p.read_text()
        text = text.replace('using System.Security.Cryptography;',
                            'using System.Security.Cryptography;\n'
                            'using SHA3_256 = Fc27.Packaging.PortableSha3;')
        text = text.replace('using IncrementalHash hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA3_256);',
                            'using var hash = new Fc27.Packaging.PortableSha3Incremental();')
        p.write_text(text)
    # The pinned producer writes its entire inter-file/meta gap as ONE padding
    # record. Its reader otherwise consumes only 256 KiB and misplaces metadata.
    # Change only the reader; package generation remains the upstream algorithm.
    reader = target / 'PFS/ProsperoPs5InnerImageReader.cs'
    text = reader.read_text()
    anchor = '            bool kraken = e.KdePredictor == 2;\n'
    if text.count(anchor) != 1:
        parser.error('Unexpected pinned inner-reader layout.')
    text = text.replace(anchor, anchor + '''
            // FC27 build-only fix for ProsperoNwonlyNapsGenerator's single
            // padding STD. This marker fills the whole gap up to metaBase.
            if (uncompOff < metaBase && fileEnd == metaBase
                && (metaBase & (Ublock256K - 1)) == 0
                && next.IsRunBase && e.KdePredictor == 4
                && e.Even == 0 && e.Odd == 1 && e.ShuffleIdx == 0
                && totalComp == 16 && evenComp == 8)
            {
                uncompOff = metaBase;
                continue;
            }
''')
    reader.write_text(text)
    helper = Path(__file__).resolve().parent.parent / 'portable-sha3/PortableSha3.cs'
    shutil.copy2(helper, target / 'Util/PortableSha3.cs')
    print('Prepared pinned package library with portable SHA3:', output)


if __name__ == '__main__':
    main()
