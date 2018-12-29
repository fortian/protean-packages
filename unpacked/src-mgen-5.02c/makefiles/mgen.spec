%define _topdir @TOPDIR@
%define name mgen
%define release 1%{?dist}
%define version @VERSION@
%define buildroot %{_topdir}/%{name}-%{version}-root

BuildRoot: %{buildroot}
Summary: A packet generator for IP network performance testing
License: BSD
URL: https://www.nrl.navy.mil/itd/ncs/products/mgen
Name: %{name}
Version: %{version}
BuildRequires: gcc-c++
BuildRequires: make
BuildRequires: libpcap-devel
Release: %{release}
Source0: src-%{name}-%{version}.tgz
Patch0: mgen-protolib-rhel.patch
Prefix: /usr

%description
MGEN provides the ability to perform IP network performance tests and measurements
using TCP and UDP/IP traffic. Test messages can be generated, received and logged.
MGEN offers control over all network parameters and timing of these messages. This
can be done via the command line or via an input file (for greater reproducibility).

This package doesn't include documentation or the Python bindings for MGEN.

%prep
%setup -q -n src-%{name}-%{version}

%build
make -C makefiles -f Makefile.linux

%install
mkdir -p %{buildroot}/%{_bindir}
install -m 0755 makefiles/mgen %{buildroot}/%{_bindir}/mgen
install -m 0755 makefiles/mpmgr %{buildroot}/%{_bindir}/mpmgr

%files
%defattr(0755,root,root)
/usr/bin/mgen
/usr/bin/mpmgr

%changelog
* Thu Dec 20 2018 Ben Stern <bstern@fortian.com> - 5.02c-1
- Initial release
