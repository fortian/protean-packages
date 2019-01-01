%define _topdir @TOPDIR@
%define name mgen-py34
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
BuildRequires: python34-devel
Requires: mgen
Release: %{release}
Source0: src-mgen-%{version}.tgz
Patch0: mgen-protolib-rhel.patch
Prefix: /usr

%description
MGEN provides the ability to perform IP network performance tests and measurements
using TCP and UDP/IP traffic. Test messages can be generated, received and logged.
MGEN offers control over all network parameters and timing of these messages. This
can be done via the command line or via an input file (for greater reproducibility).

gpsPub publishes GPS coordinates for use by MGEN in CORE-created containers.

This package provides Python 3.4 bindings for MGEN and gpsPub.

%prep
%setup -q -n src-mgen-%{version}

%build
python3 setup.py build

%install
python3 setup.py install --root %{buildroot}

%files
%defattr(0644, root, root)
%attr(0755, root, root) %{_libdir}/python3.4/site-packages/_gpsPub.cpython-34m.so
/usr/lib/python3.4/site-packages/mgen.py
/usr/lib/python3.4/site-packages/mgen-1.0-py3.4.egg-info
/usr/lib/debug%{_libdir}/python3.4/site-packages/_gpsPub.cpython-34m.so.debug
%{_libdir}/python3.4/site-packages/gpsPub-1.0-py3.4.egg-info
%{_libdir}/python3.4/site-packages/_gpsPub.cpython-34m.so
/usr/src/debug/src-mgen-5.02c/include/gpsPub.h
/usr/src/debug/src-mgen-5.02c/src/common/gpsPub.cpp
/usr/src/debug/src-mgen-5.02c/src/python/gpsPub_wrap.c

%changelog
* Mon Dec 31 2018 Ben Stern <bstern@fortian.com> - 5.02c-1
- Initial release
