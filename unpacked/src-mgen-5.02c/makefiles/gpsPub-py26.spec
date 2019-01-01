%define _topdir @TOPDIR@
%define name gpsPub-py26
%define release 1%{?dist}
%define version @VERSION@
%define buildroot %{_topdir}/%{name}-%{version}-root

BuildRoot: %{buildroot}
Summary: A GPS location publisher for use with MGEN and CORE
License: BSD
URL: https://www.nrl.navy.mil/itd/ncs/products/mgen
Name: %{name}
Version: %{version}
BuildRequires: gcc-c++
BuildRequires: python-devel
Requires: mgen
Release: %{release}
Source0: src-mgen-%{version}.tgz
Patch0: mgen-protolib-rhel.patch
Prefix: /usr

%description
gpsPub publishes GPS locations for MGEN to receive inside CORE-created
containers.

This package provides Python 2.6 bindings.

%prep
%setup -q -n src-mgen-%{version}

%build
python setup.py build

%install
python setup.py install --root %{buildroot} gpsPub

%files
%defattr(0644, root, root)
%{_libdir}/python*/site-packages/gpsPub-1.0-py*.egg-info
%attr(0755, root, root) %{_libdir}/python*/site-packages/_gpsPub.so
/usr/lib/debug/%{_libdir}/python*/site-packages/_gpsPub.so.debug
/usr/src/debug/src-mgen-5.02c/include/gpsPub.h
/usr/src/debug/src-mgen-5.02c/src/common/gpsPub.cpp
/usr/src/debug/src-mgen-5.02c/src/python/gpsPub_wrap.c

%changelog
* Mon Dec 31 2018 Ben Stern <bstern@fortian.com> - 5.02c-1
- Initial release
