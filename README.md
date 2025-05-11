# dao-max-pd

This repository is a collection of Max/MSP and Pd externals developed from the explanations in the book *"Designing Audio Objects for Max/MSP and Pd"* by Eric Lyon.

## FORK NOTES

This friend WIP fork made the following changes to the original project:

- Changed build system to exclusively use `cmake`.

- Changed Max/MSP externals from 32bit dsp/perform methods to 64bit methods.

- Changed folder structure to a Max package structure.

- Changed use of `sprintf(dst,` in `assist` methods of Max/MSP externals to `snprintf_zero(dst, ASSIST_MAX_STRING_LEN,`.

- Changed use of `error()` in Pd externals to `pd_error()`.

- Added `max-sdk-base` as a git submodule dependency.

- Added puredata `m_pd.h` header to `source/include` folder.

- Dropped common files and headers.. This is becoming too unweildy as Max and PD evolve.

- Dropped 32-bit `multy~`, and renamed `multy64~` to `multy~`

- Dropped Max version of `cartopol~` and `poltocar~` as there are already Max builtins.

- Added github action to build all externals.

### Fork Todo

- [ ] change deprecated use of `buffer~` objects in `bed` to more current api.



### Building

Before buildind Max/MSP externals, you will need to do the following unless you have already cloned the repo recursively:

```sh
git submodule init
git submodule update
```

Then to build, on macOS, type `make macos` in the root of the project or

```sh
mkdir -p build
cd build
cmake .. -GXcode
cmake --build . --config Release
```

For Windows, type `make windows` in the root of the project if `make.exe` is available or

```sh
mkdir -p build
cd build
cmake ..
cmake --build . --config Release
```

For Linux, type `make linux` (to build the puredata externals) or

```sh
mkdir -p build
cd build
cmake ..
cmake --build . --config Release
```

Original `README` continues from here...

### Forking and Building Using Github Actions

If you fork this fork (-:, you can run the included github action, `build-package` which will build Max/MSP externals on MacOS and Windows, and puredata exteransl on MacOS, Windows and Linux and then bundle them together into a package and make it available for download.

If you do this, note that the MacOS externals will be unusable (due to Apple's codesign and notarization rules) unless you copy the downloaded externals folder to this project and run `make sign` which will remove these constraints.

## Overview

This repository is a collection of Max/MSP and Pd externals developed from the explanations in the book *"Designing Audio Objects for Max/MSP and Pd"* by Eric Lyon.  

The externals developed are:

- [**bed**](bed) is a max-only external that provides non-real-time buffer/array operations.

- [**cleaner~**](cleaner~) has basic controls to do adaptive noise reduction in the frequency domain.  

- [**dynstoch~**](dynstoch~) implements dynamic stochastic synthesis as formulated by Iannis Xenakis.  

- [**mirror~**](mirror~) simply copies audio from the input directly to the output without any modifications.  

- [**moogvcf~**](moogvcf~) is a port of the Csound unit generator generator "moogvcf".  

- [**multy~**](multy~) multiplies the two input signals and sends the result to the output.  

- [**multy64~**](multy64~) is the same as [multy~](multy~) but implements the 64-bit version of the dsp and perform routines of the Max/MSP external.  

- [**oscil~**](oscil~) is a wavetable oscilator that allows specification of waveform by additive synthesis and by name of common band-limited waveforms.  

- [**oscil_attributes~**](oscil_attributes~) is the same as [oscil~](oscil~) but implements attributes for the Max/MSP version of the external.  

- [**retroseq~**](retroseq~) is a sample-accurate sequencer that sends a signal representing frequency values.  

- [**scrubber~**](scrubber~) is an implementation of the phase vocoder algorithm.  

- [**vdelay~**](vdelay~) provides variable delay with feedback and allows delays shorter than the signal vector size.  

- [**vpdelay~**](vpdelay~) is the same as [vdelay~](vdelay~) but implemented using pointers instead of array indexes.  

Additional utility externals were written to provide functionality available as built-in Max/MSP externals that do not exist "natively" in Pd:  

- [**cartopol~**](cartopol~) converts real and imaginary parts to magnitude and phase.  

- [**poltocar~**](poltocar~) converts magnitude and phase to real and imaginary parts.  

- [**windowvec~**](windowvec~) multiplies the incoming signal with a Hann window (raised cosine wave).  

## Clone

You can run the following commands in the terminal to clone this repository and to checkout the version of the Max and Pd SDKs that were used to build all the projects:

```sh
git clone https://github.com/juandagilc/DAO-MaxMSP-Pd.git
cd DAO-MaxMSP-Pd/
git submodule update --init
```

## Build

The table below shows details about the tools that have been used to build and test the externals of this repository. For more information about the steps followed to setup the Xcode projects, Visual Studio projects, and Makefiles, see the [SETUP.md](SETUP.md) file.  

| Operating Systems                                                    | Built with                   | Tested on                                              |
|:--------------------------------------------------------------------:|:----------------------------:|:------------------------------------------------------:|
| macOS Sierra 10.12.6                                                 | Xcode 9.1                    | Max 7.3.4 (32/64-bi) & Pd-0.48-0 (32-bit) |
| Windows 10                                                           | Visual Studio Community 2015 | Max 7.3.4 (32/64-bit) & Pd-0.48-0 (32-bit) |
| Linux (Ubuntu MATE 16.04 LTS 32-bit)                                 | GNU Make 4.1    | Pd-0.48-0 (32-bit)                                     |
| Linux (Debian GNU/Linux 7.4 (wheezy) on the BeagleBone Black + Bela) | GNU Make 3.81    | libpd 0.8.3 (armv7l) BeagleBone Black + Bela Cape      |

To build all the externals in this repository at once you can run ``ruby build.rb`` from the *Terminal* on macOS or Linux, or from the *Developer Command Prompt for VS2015* on Windows (on macOS Ruby comes preinstalled, on Linux you can install it using ``sudo apt-get install ruby-full``, and on Windows you can install it from <https://rubyinstaller.org>. The ``build.rb`` script creates a folder called ``_EXTERNALS_`` in the root of the repository with subfolders for Max/MSP and Pd files. The optional argument ``BBB`` can be passed to the script to cross-compile the externals for the [*BeagleBone Black + Bela Cape*](http://bela.io) from a host operating system (cross-compilation was tested only on macOS using the [Linaro toolchain](https://github.com/BelaPlatform/Bela/wiki/Compiling-Bela-projects-in-Eclipse)). Max/MSP and Pd patches showing the usage of the externals will be copied to the ``_EXTERNALS_`` folder as well.

## License

Please see the [LICENSE.md](LICENSE.md) file for details.

1. "Max-only" means that the external does not process audio in real-time, but it is implemented for Max/MSP and Pd.

2. However, the only external that implements 64-bit dsp and perform routines is [multy64~](multy64~).

3. The Makefiles can be used to build Pd externals for macOS, Windows, and Linux, using make.
