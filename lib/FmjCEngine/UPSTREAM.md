# Upstream provenance

The engine core in `src/fmj_c_engine/engine.c` and `engine.h` comes from:

- Repository: <https://github.com/erduoniba/baye-fmj-app>
- Directory: `Fmj/fmj_c_engine`
- Commit: `60c41ea2d9932b295833ece7004394497610596a`
- Retrieved: 2026-08-14
- License: GNU General Public License version 2 (`GPL-2.0-only`)

`engine.c` was imported from that revision, then changed where required for
portable C compilation and for obvious translation correctness issues exposed
by compiler diagnostics (system-call pointer types, token whitespace, an
accidental empty `if`, assignment in a condition, an array bound, and an
8-bit intermediate that was shifted by 8).
`engine.h` and `keytable.h` are unchanged. `dictsys.h` was converted from
GB18030 to UTF-8 without changing its definitions. The Windows host, generated
font array, binary font, icons, Visual Studio files, and disassembly listings
are not included.

The portable system layer in this library replaces upstream `middle.c` and
`framework.h`. Files changed from the upstream engine will carry a dated
modification notice as required by GPL-2.0 section 2(a).
