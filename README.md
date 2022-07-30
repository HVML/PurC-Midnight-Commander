# PurC Midnight Commander

A generic HVML renderer which runs in text-mode. This program is derived from
the famous GNU [Midnight Commander].

__Table of Contents__

- [Introduction](#introduction)
- [Source Tree of PurcMC](#source-tree-of-purcmc)
- [Current Status](#current-status)
- [Building](#building)
   + [Commands](#commands)
- [Usage](#usage)
   + [Start HVML Renderer](#start-hvml-renderer)
   + [Run Sample Client](#run-sample-client)
- [Authors and Contributors](#authors-and-contributors)
- [Copying](#copying)


## Introduction

PurC Midnight Commander (abbreviated as `PurcMC`) is an HVML renderer for
development and debugging. It runs in a terminal or console like its ancestor:
GNU [Midnight Commander].

It keeps almost all functions of GNU MC with the following enhancements:

1. We use `cmake` as the building system instead of autoconf/automake.
1. We cleaned up the code and removed some features, such as support for
   various charsets (codepages). PurcMC assumes that the encoding is
   always UTF-8 by default.
1. It can load an HTML file on file systems and show the DOM tree. The user can
   travel the DOM tree and check the attributes and text content of elements.
1. It runs as an HVML renderer, accepts the connections from PurC-based HVML
   interpreter instances via Unix Domain Socket or Web Socket, and shows
   the uDOMs as trees in a screen. Driven by the remove HVML programs,
   it can update the uDOMs dynamically.

The following features are still under development:

1. The user can edit a uDOM tree, e.g., changing the attributes and contents
   of an element, or creating a new element.

## Source Tree of PurcMC

The source tree of PurcMC is illustrated as follow:

- `source/cmake/`: The cmake modules.
- `source/wtf/`: The simplified WTF (Web Template Framework) from WebKit.
- `source/lib/`: The common utilities library.
- `source/bin/`: The source files of the executable `purcmc` and others.
- `source/tests/`: The unit test programs.
- `source/misc/`: The miscellaneous and the default configuration files.
- `source/po/`: The translation files.
- `docs/`: Documents.
- `tools/`: Some tools for easy development.

## Current Status

This project was launched in Nov. 2021.

We welcome anybody to take part in the development and contribute your effort!

For the community conduct, please refer to [Code of Conduct](CODE_OF_CONDUCT.md).

## Building

Building and install the latest PurC to your system first:

- [PurC](https://github.com/HVML/PurC): the HVML interpreter for C language.

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

## Usage

### Start HVML Renderer

If you did not install PurCMC, please change to the `build/` directory:

```
$ cd build/
```

Run `purcmc` with `--help` or `--help-renderer` option to show the command
line options:

```
$ source/bin/purcmc --help
```

Run `purcmc` with `-R` option to start the PurCMC renderer server:

```
$ source/bin/purcmc -R
```

If you want to disable the support on WebSocket, you can run:

```
$ source/bin/purcmc -R --nowebsocket
```

Sencond, open another terminal to run an HVML interpreter or the sample client
`purcsmg` in this repo.

Here are some tips:

1. In the default screen, you can choose a HTML file, press `F3` or click `View`
   on the button bar to load the file and show the DOM tree in the DOM viewer.
1. If there is some clients connected to the renderer server and load
   a HTML content successfully, you can also choose `DOM viewer` in
   the `Commands` pull-down menu to show the DOM viewer.
1. In the DOM viewer, you can press `F3` or click `Switch` on the button bar
   to switch to another opened HTML file or loaded uDOM.
1. In the DOM viewer, you can use `Tab` key to travel among widgets.
   In the DOM Tree widget, you can use arrow keys to travel the elements,
   fold or unfold a sub tree in DOM Tree widget.

### Run Sample Client

First, change to the `build/` directory:

```
$ cd build/
```

Run `purcsmg` with `--help` option to show the command line options:

```
$ source/bin/purcsmg --help
```

Use `--file` (`-f`) to load a HTML file, and use `--testmethod` (`-m`) to
specify a test method (a value in 0 ~ 13):

```
$ source/bin/purcsmg -f clock.html -m 0
```

Switch to the terminal running `purcmc`. In the DOM viewer screen, you can
show the uDOM and see the dynamically changes of the uDOM.

## Authors and Contributors

- Vincent Wei
- Authors of GNU MC.

## Copying

Copyright (C) 2022 Beijing FMSoft Technologies Co., Ltd.  
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
