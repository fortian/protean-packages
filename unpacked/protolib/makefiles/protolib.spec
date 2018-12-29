%define _topdir @TOPDIR@
%define name mgen
%define release 1%{?dist}
%define version @VERSION@
%define buildroot %{_topdir}/%{name}-%{version}-root

BuildRoot: %{buildroot}
Summary: A cross-platform C/C++ library supporting the simulation environments of NS2 and Opnet
License: BSD
URL: https://www.nrl.navy.mil/itd/ncs/products/mgen
Name: %{name}
Version: %{version}
BuildRequires: gcc-c++
BuildRequires: make
BuildRequires: libpcap-devel
BuildRequires: libxml2-devel
Release: %{release}
Source0: src-mgen-5.02c.tgz
Patch0: protolib-rhel.patch
Prefix: /usr

%description

Protolib provides an overall framework for developing working protocol
implementations, applications, and simulation modules.  The individual
classes are designed for use as stand-alone components when possible.
Although Protolib is principally for research purposes, the code has been
constructed to provide robust, efficient performance and adaptability to
real applications. In some cases, the code consists of data structures, etc
useful in protocol implementations and, in other cases, provides common,
cross-platform interfaces to system services and functions (e.g., sockets,
timers, routing tables, etc).

This package doesn't include documentation or the Python bindings for
Protolib.

%prep
%setup -q -n src-%{name}-%{version}

%build
make -C makefiles -f Makefile.linux

%install
mkdir -p %{buildroot}/%{_libdir}
install -m 0755 makefiles/protolib.a %{buildroot}/%{_libdir}/protolib.a

%files
%defattr(0755,root,root)
/usr/lib/protolib.a

%changelog
* Thu Dec 29 2018 Ben Stern <bstern@fortian.com> - 3.01b-1
- Initial release
