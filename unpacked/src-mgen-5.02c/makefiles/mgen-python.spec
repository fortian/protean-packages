%define _topdir @TOPDIR@
%define name mgen-python
%define release 1%{?dist}
%define version @VERSION@
%define buildroot %{_topdir}/%{name}-%{version}-root

BuildRoot: %{buildroot}
Summary: A packet generator for IP network performance testing
License: BSD
URL: https://www.nrl.navy.mil/itd/ncs/products/%{name}
Name: %{name}
Version: %{version}
BuildRequires: gcc-c++
BuildRequires: python
BuildRequires: python-devel
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
python setup.py build

%install
python setup.py install --root %{buildroot}

%files
%defattr(0644, root, root)
%{_libdir}/python2.7/site-packages/gpsPub-1.0-py2.7.egg-info
%{_libdir}/python2.7/site-packages/mgen-1.0-py2.7.egg-info
%attr(0755, root, root) %{_libdir}/python2.7/site-packages/_gpsPub.so
%{_libdir}/python2.7/site-packages/mgen.pyc
%{_libdir}/python2.7/site-packages/mgen.py

%changelog
* Sun Dec 30 2018 Ben Stern <bstern@fortian.com> - 5.02c-1
- Initial release
