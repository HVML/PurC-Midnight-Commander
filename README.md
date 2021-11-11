# PurC Midnight Commander

A generic HVML renderer which runs in text-mode. This program is derived from
the famous GNU [Midnight Commander].

__Table of Contents__

- [Introduction](#introduction)
- [Source Tree of PurcMC](#source-tree-of-purcmc)
- [Current Status](#current-status)
- [Building](#building)
   + [Commands](#commands)
- [Authors and Contributors](#authors-and-contributors)
- [Copying](#copying)


## Introduction

PurC Midnight Commander (abbreviated as `PurcMC`) is a HVML renderer for
development and debugging. It runs in a terminal or console like its ancestor:
GNU [Midnight Commander].

It keeps almost all functions of GNU MC with the following enhancements:

1. We use cmake as the building system instead of autoconf/automake.
1. We cleaned up the code and removed some features, such as support for
   various charsets (codepages). PurcMC supports only UTF-8.
1. It runs as a HVML renderer, accepts the connections from PurC-based HVML
   interpreter instances via Unix Domain Socket or Web Socket, and shows
   the uDOMs as trees in dialogs.
1. The user can edit a uDOM tree, e.g., changing attributes and contents
   of an element.

## Source Tree of PurcMC

The source tree of PurcMC is illustrated as follow:

- `src/cmake/`: The cmake modules.
- `src/wtf/`: The simplified WTF (Web Template Framework) from WebKit.
- `src/lib/`: The common utilities library.
- `src/bin/`: The source files of the executable `purcmc`.
- `src/tests/`: The unit test programs.
- `src/misc/`: The miscellaneous and the default configuration files.
- `src/po/`: The translation files.
- `tools/`: Some tools for easy development.

## Current Status

This project was launched in Nov. 2021.

We welcome anybody to take part in the development and contribute your effort!

For the community conduct, please refer to [Code of Conduct](CODE_OF_CONDUCT.md).

## Building

### Commands

To build the source on Linux:

```
$ mkdir build
$ cd build
$ cmake .. -DPORT=Linux
$ make
```

To build the source on macOS:

```
$ mkdir build
$ cd build
$ cmake .. -DPORT=Mac
$ make
```

## Authors and Contributors

- Vincent Wei
- Authors of GNU MC.

## Copying

Copyright (C) 2021 Beijing FMSoft Technologies Co., Ltd.  
Copyright (C) 2005-2021 Free Software Foundation, Inc.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

[Midnight Commander]: https://midnight-commander.org/
