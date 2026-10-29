# Third-party notices

mkdcdisc's own source code is under the MIT license; see [LICENSE](LICENSE).

A built mkdcdisc binary also contains the third-party components listed
below, several of them under licenses other than MIT. Anyone redistributing
a binary, or the source as a whole, must satisfy those licenses' terms in
addition to MIT's. This file is a factual inventory of what's in there and
where each component's license text lives. It is not legal advice.

Each vendored component keeps its own license. Nothing has been relicensed,
except for one file noted under `libedc`.

## Compiled into the binary

### libisofs: GPL-2.0-or-later

The ISO 9660 filesystem writer. It's copyleft and present in every build.

- Upstream: <https://dev.lovelyhq.com/libburnia/libisofs.git>. The revision the
  bundled copy was taken from is in
  [`git_info.json`](src/third_party/libisofs/git_info.json)
- License text: [`src/third_party/libisofs/COPYING`](src/third_party/libisofs/COPYING),
  with authors and the notice in
  [`COPYRIGHT`](src/third_party/libisofs/COPYRIGHT)
- Copyright © 2007–2018 Vreixo Formoso, Mario Danic, Vladimir Serbinenko,
  Thomas Schmitt

The linking differs by platform, which changes what redistributing a build
requires:

| Build | Type | How libisofs is used |
|---|---|---|
| MinGW-w64 | Native Windows `.exe` without dependencies | The lightly patched copy in this repository, compiled in and statically linked |
| MSYS2 | Windows `.exe` with MSYS2 dependencies | The package release `libisofs`, dynamically linked |
| Linux, macOS | Native binary | The system `libisofs`, dynamically linked |

[`src/third_party/libisofs/README.md`](src/third_party/libisofs/README.md)
describes the Windows patches and why they exist.

### libedc: CDDL-1.0

The Reed-Solomon EDC/ECC encoder. It computes the error detection and
correction fields for every raw sector written.

- Upstream: <https://codeberg.org/schilytools/schilytools> (schilytools, the
  successor to cdrtools). See
  [`git_info.json`](src/third_party/libedc/git_info.json) for the exact revision
- License text: [`src/third_party/libedc/CDDL.Schily.txt`](src/third_party/libedc/CDDL.Schily.txt),
  Jörg Schilling's own copy. It closes with a notice, under section 9 of the
  license, that places it under German law and the courts of Berlin
- Copyright © 1998–2002, 2008 Heiko Eissfeldt; © 2002–2013 Jörg Schilling

The CDDL is a per-file, weak copyleft license. Seven files in that directory
fall under it. The rest of the directory is mkdcdisc's own work, including
`libedc.h`, which came from `img4dc` under the GPLv3 and was relicensed to MIT
here by its author. [`src/third_party/libedc/COPYRIGHT`](src/third_party/libedc/COPYRIGHT)
lists which file is which, and why.

### elf_parser: MIT

Reads a SuperH ELF executable and flattens it into the flat binary format a
Dreamcast loads.

- Upstream: <https://github.com/finixbit/elf-parser>
- License text: [`src/third_party/elf_parser/LICENSE`](src/third_party/elf_parser/LICENSE)
- Copyright © 2018 finixbit; © 2022–2023 Colton Pawielski

A fork, not a vendored copy; see
[`src/third_party/elf_parser/README.md`](src/third_party/elf_parser/README.md).

### scramble

The Dreamcast bootstrap scrambler, rewritten in C++ from Marcus Comstedt's
`scramble.c`.

- Upstream: <https://mc.pp.se/dc/files/scramble.c>, also copied in KallistiOS
  under `utils/scramble/`
- The original contains no copyright notice and no license text, and none
  accompanies it on its author's site. mkdcdisc treats it as public domain.
  See [`src/third_party/scramble/COPYRIGHT`](src/third_party/scramble/COPYRIGHT)
  for the full reasoning.

### makeip: BSD-2-Clause

Two pieces of the boot header (`IP.BIN`) come from here: the checksum written into its
Device Info field, and the PNG-to-MR conversion behind
`-i`/`--bootstrap-mr-image`, which converts an image into the format the
license screen displays.

- Upstream: **IP creator** (`makeip`) 2.0.1, as included in KallistiOS under
  [`utils/makeip/`](https://github.com/KallistiOS/KallistiOS/tree/master/utils/makeip)
- License text: [`src/third_party/makeip/LICENSE`](src/third_party/makeip/LICENSE),
  reproduced in full below
- Copyright © 2000, 2001, 2002, 2019, 2020 The KOS Team and contributors

These are copies, not reimplementations. The code was taken from `makeip` and
adapted to C++, so its original notice travels with it and sits at the top of
each source file in that directory.
[`COPYRIGHT`](src/third_party/makeip/COPYRIGHT) records who wrote which part:
the CRC is from Marcus Comstedt in the original `makeip`; the MR and PNG code
is from Andrew Kieschnick in `logotools`. `makeip` is a collective work, so no
single contributor can relicense it, and none has.

The BSD 2-Clause License is permissive, not copyleft, so it places no
constraints on how mkdcdisc is distributed. Its second clause does require a
binary distribution to reproduce the notice in its accompanying
documentation. Since the license is short, it's reproduced here in full
rather than just linked from the source tree.

> Copyright (C) 2000, 2001, 2002, 2019, 2020 The KOS Team and contributors.
> All rights reserved.
>
> This code was contributed to KallistiOS (KOS) by Andress Antonio Barajas
> (BBHoodsta). It was originally made by Marcus Comstedt (zeldin). Some portions
> of code were made by Andrew Kieschnick (ADK/Napalm). Heavily updated by
> SiZiOUS. Bootstrap replacement (IP.TMPL) was made by Jacob Alberty (LiENUS).
>
> Redistribution and use in source and binary forms, with or without
> modification, are permitted provided that the following conditions are met:
>
> 1. Redistributions of source code must retain the above copyright notice, this
>    list of conditions and the following disclaimer.
> 2. Redistributions in binary form must reproduce the above copyright notice,
>    this list of conditions and the following disclaimer in the documentation
>    and/or other materials provided with the distribution.
>
> THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
> ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
> WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
> DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
> FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
> DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
> SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
> CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
> OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
> OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

### IP.BIN bootstrap templates

The Dreamcast bootstrap (`IP.BIN`) is what the BIOS loads from the start of a
disc before reaching the executable. mkdcdisc ships 3 templates under
[`assets/ip/`](assets/ip/), embeds them in the binary as C arrays
(`src/assets/*.include`, regenerated by `tools/regen-includes.sh`), and writes
one onto every image it produces.

Each template is 32768 bytes, laid out as follows. See
[dreamcast.wiki/IP.BIN](https://dreamcast.wiki/IP.BIN) for the full description.

| Offset | Size | Section |
|---|---|---|
| `0x0000` | 256 | Metadata: title, product number, release date, boot filename, peripherals. mkdcdisc rewrites all of it from its own options. |
| `0x0100` | 512 | Table of contents |
| `0x0300` | 13312 | Sega license screen code **(see below)** |
| `0x3700` | 256 | Area protection symbols |
| `0x3800` | 18432 | Bootstrap |

**The license screen section cannot be replaced.** The BIOS compares it
against a copy stored in its own ROM, so a disc storing anything else won't
boot on retail hardware. It's Sega's code; no license accompanies it, and
none is claimed here. It's byte-identical across all 3 templates (as it
has to be) and every Dreamcast toolchain has stored these same 13312 bytes
for as long as they've existed. The free replacement efforts rewrote
everything around this section and left it untouched, because it cannot be
anything else.

The bootstrap in the MIL-CD template is one of those free replacements, written by
Jacob Alberty (LiENUS). Early Dreamcast releases reused the `IP.BIN` lifted
from a commercial game, which made them straightforwardly infringing.
Alberty rewrote it from scratch so later releases wouldn't need to, leaving
only the one section that couldn't be rewritten. It has been the community's
de facto bootstrap ever since, and the same code appears in every toolchain
that writes an `IP.BIN`, `makeip` included.

The usual reasoning for keeping that remaining section is that reproducing
it is necessary for interoperability, since no substitute exists. That
argument comes from [Sega v. Accolade](https://en.wikipedia.org/wiki/Sega_v._Accolade) (US Court of Appeals, 9th Circuit,
1992). This is the reading the Dreamcast community has long worked from, not
a legal determination this project is making on its own, and none of this is
legal advice.

The metadata is entirely generated by mkdcdisc. It's written from the `-n`, `-s`,
`-r`, `-H` and `--boot-filename` options among others, and
`-p`/`--bootstrap` replaces the whole template with a file of your own.

## Not in the binary

### MDF/MDS format documentation

[`docs/specs/mdf_mds.txt`](docs/specs/mdf_mds.txt) is Henrik Stokseth's
description of the Alcohol 120% image format, redistributed here because the
MDS writer was written from it. Its own header reads `License : Copyright
(c) 2005. All rights reserved. ALL USE AT OWN RISK!`, so it grants no right
to redistribute it. It's documentation only, and isn't part of any binary.
