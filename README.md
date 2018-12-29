# Overview
A collection of `.spec` files to make RPMs of software packages commonly
used by the Network and Communications Systems Branch and others interested
in network protocol research.

# Using the Repository

This requires GNU Make, which usually isn't a problem under Linux.

RPM package building has been tested under:
- CentOS 6, i686
- CentOS 6, x86_64
- CentOS 7, x86_64

Generally, RPMs can be made with
```shell
make -f Makefile.linux rpm
```
in the appropriate directory.  If you get an error trying to `mkdir`
`/BUILD`, this is a bug; please open an issue, but the workaround is to
create a directory named `rpmbuild` at the same level as `unpacked`.

Source, both packed and unpacked, and pre-generated packages, including RPMs
and SRPMs, are included in this repository for convenience.  Only `.spec`
and `.patch` files are strictly required for RPMs, *or* the unpacked source
tree with the modified `Makefile`s, to make RPMs from scratch.

Versions are current as of their check-in time, but are unlikely to be kept
up-to-date.  Upstream has expressed interest in including the changes made
to the `Makefile`s, so this may not be a problem moving forward.

# TODO
- Package more software
  - SDT
    - Needs Java and a lot of work
  - SMF
    - Has an out-of-date Protolib, which no longer builds under Linux
      distributions without `linux/netfilter_ipv4/ip_queue.h`
  - gpsLogger
    - Low priority package
- More packages
  - Debian package creation
    - Ubuntu 18.04 LTS
    - Ubuntu 16.04 LTS
    - Ubuntu 18.10 (i.e., current)
  - `-devel` packages
    - Python bindings
      - Protolib is not building a shared library properly
      - MGEN's Python bindings say they need the Protolib Python bindings and
        the Protolib shared library, which, as above, doesn't build
    - Java bindings
      - Lower priority
    - C/C++ bindings
      - Protolib is really the only package that seems be able to be shipped
        as a `-devel` package
  - Docs packages
    - Lower priority
    - Can be included in main RPM, under `%files` section, e.g.:
      ```spec
      %files
      %{_bindir}/foo
      %doc %{_docdir}/README
      ```
    - If there are a lot of docs, making a standalone package is preferred
