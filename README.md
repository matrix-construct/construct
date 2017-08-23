# charybdis/5

Charybdis is a collaboration server designed to be scalable and community-developed.

It implements communication protocols for Matrix and IRC.

## Building from git

	* `git clone https://github.com/charybdis-ircd/charybdis`
	* `cd charybdis`
	* `./configure`
	* `make`
	* `make install`

### Notable configuration options when building

	* `--enable-debug`
	* `--with-included-boost[=shared]`
	* `--with-included-rocksdb[=shared]`
	* `--with-included-js[=shared]`

## Platforms

[![Charybdis](http://img.shields.io/SemVer/v5.0.0-dev.png)](https://github.com/charybdis-ircd/charybdis/tree/master)
*This branch is not meant for production. Use at your own risk.*

| <sub> Continuously Integrated Host </sub>   | <sub> Compiler </sub>    | <sub> Third party </sub> | <sub> Status </sub> |
|:------------------------------------------- |:------------------------ |:------------------------ |:------------------- |
| <sub> Linux Ubuntu 12.04 Precise </sub>     | <sub> GCC 5       </sub> | <sub> Boost 1.61 </sub>  | [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=master)](https://travis-ci.org/charybdis-ircd/charybdis) |
| <sub> Linux Ubuntu 14.04 Trusty </sub>      | <sub> GCC 6       </sub> | <sub> Boost 1.61 </sub>  | [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=master)](https://travis-ci.org/charybdis-ircd/charybdis) |
| <sub> Linux Ubuntu 14.04 Trusty </sub>      | <sub> Clang 3.8   </sub> | <sub> Boost 1.61 </sub>  | [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=master)](https://travis-ci.org/charybdis-ircd/charybdis) |
| <sub> Apple Darwin 15.5 </sub>              | <sub> LLVM 7.3.0  </sub> | <sub> Boost 1.61 </sub>  | [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=master)](https://travis-ci.org/charybdis-ircd/charybdis) |
| <sub> Windows </sub>                        | <sub> mingw 3.5   </sub> | <sub> Boost 1.61 </sub>  | [![Windows Build Status](https://ci.appveyor.com/api/projects/status/is0obsml8xyq2qk7/branch/master?svg=true)](https://ci.appveyor.com/project/kaniini/charybdis/branch/master) |

## Tips

 * Please read doc/index.txt to get an overview of the current documentation.

 * Read the NEWS file for what's new in this release.

## Git access

 * The Charybdis GIT repository can be checked out using the following command:
	`git clone https://github.com/charybdis-ircd/charybdis`

 * Charybdis's GIT repository depot can be browsed over the Internet at the following address:
	https://github.com/charybdis-ircd/charybdis

## Developers

### Style

#### Misc

	* When using a `switch` over an `enum` type, put what would be the `default` case after/outside
	of the `switch` unless the situation specifically calls for one. We use -Wswitch so changes to
	the enum will provide a good warning to update any `switch`.
