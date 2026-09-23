# Craft++

A community-driven C++20 reimplementation of Minecraft 1.0 gameplay, written from scratch.

Target: a client that looks, feels and behaves like the original 1.0 client
(singleplayer first, then interoperability testing against a vanilla 1.0 Java server),
with a modern multithreaded engine (main tick / render / chunk-generation threads,
OpenGL 3.3+, CMake).

Status: early development.

## Legal notice — please read

**Craft++ contains no Mojang-copyrighted material.**

* **No original code.** Everything under version control in this repository is
  newly written C++ (plus build files and documentation). No decompiled or
  deobfuscated Mojang Java code is included, and none will ever be committed.
  A local `src/` directory with deobfuscated sources may exist on a
  developer's machine purely as a behavioural reference — it is explicitly
  excluded via `.gitignore` and must never be committed or pushed.
* **No Mojang assets.** No textures, sounds, music, language files, artwork or
  any other asset extracted from `minecraft.jar` or `MinecraftResources` is
  included in this repository. A local `assets/` directory may exist on a
  developer's machine for private interoperability testing — it is explicitly
  excluded via `.gitignore` and must never be committed or pushed. Each user
  must supply their own legally obtained copy of Minecraft 1.0 assets.
* **Clean-room port.** Craft++ reimplements gameplay and mechanics from
  observation and from general knowledge. Where the original behaviour is
  referenced, it is re-expressed in original code. Function names inspired by
  the community (e.g. block/entity concepts) do not imply copying of code.
* **No affiliation.** This is an unofficial fan project. It is not affiliated
  with, endorsed by, or connected to Mojang Studios or Microsoft. Minecraft is
  a trademark of Mojang Synergies AB.
* **No redistribution of Mojang IP.** Do not open issues or pull requests that
  attach Mojang code, jars, or assets. Such contributions will be rejected and
  removed.

See also the pinned `LEGAL NOTICE` issue on GitHub, which restates this notice.

## License

Copyright (C) 2026 Craft++ contributors.

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program (see `LICENSE`). If not, see <https://www.gnu.org/licenses/>.

SPDX-License-Identifier: GPL-3.0-or-later

## Layout (planned)

```text
src_cpp/    # new C++20 sources (core, world, render, entity, gui, audio, net, app)
tests/      # parity tests (JavaRandom, NBT, worldgen, protocol)
docs/       # design notes
```

Local-only, never committed:

```text
src/        # original Java reference (ignored)
assets/     # user-supplied 1.0 assets for local testing (ignored)
```

## Building (placeholder)

CMake + C++20 + GLFW + GLM + OpenAL will be documented once `M0` lands.
