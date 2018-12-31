%define _topdir @TOPDIR@
%define name protolib-python
%define release 1%{?dist}
%define version @VERSION@
%define buildroot %{_topdir}/%{name}-%{version}-root

BuildRoot: %{buildroot}
Summary: A cross-platform C/C++ library supporting the simulation environments of NS2 and Opnet
License: BSD
URL: https://www.nrl.navy.mil/itd/ncs/products/protolib
Name: %{name}
Version: %{version}
BuildRequires: gcc-c++
BuildRequires: make
BuildRequires: libpcap-devel
BuildRequires: libxml2-devel
BuildRequires: libnetfilter_queue-devel
Release: %{release}
Source0: src-protolib-%{version}.tgz
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
%setup -q -n protolib
if grep -q RHEL_VERSION protolib/src/linux/linuxDetour.cpp; then
  :
else
  patch -p0 < %{_topdir}/SOURCES/protolib-rhel.patch
fi

%build
python setup.py build

%install
python setup.py install --root %{buildroot}

%files
%defattr(0755,root,root)
%{_libdir}/libprotokit.a
%{_bindir}/arposer
%{_bindir}/averageExample
%{_bindir}/base64Example
%{_bindir}/detourExample
%{_bindir}/graphExample
%{_bindir}/graphRider
%{_bindir}/graphXMLExample
%{_bindir}/jsonExample
%{_bindir}/lfsrExample
%{_bindir}/msg2MsgExample
%{_bindir}/msgExample
%{_bindir}/netExample
%{_bindir}/pcmd
%{_bindir}/pipe2SockExample
%{_bindir}/pipeExample
%{_bindir}/protoCapExample
%{_bindir}/protoFileExample
%{_bindir}/queueExample
%{_bindir}/riposer
%{_bindir}/serialExample
%{_bindir}/simpleTcpExample
%{_bindir}/sock2PipeExample
%{_bindir}/threadExample
%{_bindir}/timerTest
%{_bindir}/ting
%{_bindir}/vifExample
%{_bindir}/vifLan
%{_bindir}/protoExample

%changelog
* Thu Dec 27 2018 Ben Stern <bstern@fortian.com> - 3.01b-1
- Initial release
