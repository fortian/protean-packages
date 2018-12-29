%define _topdir @TOPDIR@
%define name protolib
%define release 1%{?dist}
%define version @VERSION@
%define buildroot %{_topdir}/%{name}-%{version}-root
%define binaries arposer averageExample base64Example detourExample \
  graphExample graphRider graphXMLExample jsonExample lfsrExample \
  msg2MsgExample msgExample netExample pcmd pipe2SockExample pipeExample \
  protoCapExample protoFileExample queueExample riposer serialExample \
  simpleTcpExample sock2PipeExample threadExample timerTest ting vifExample \
  vifLan protoExample

BuildRoot: %{buildroot}
Summary: A cross-platform C/C++ library supporting the simulation environments of NS2 and Opnet
License: BSD
URL: https://www.nrl.navy.mil/itd/ncs/products/%{name}
Name: %{name}
Version: %{version}
BuildRequires: gcc-c++
BuildRequires: make
BuildRequires: libpcap-devel
BuildRequires: libxml2-devel
Release: %{release}
Source0: src-%{name}-%{version}.tgz
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
%setup -q -n %{name}

%build
make -C makefiles -f Makefile.linux

%install
mkdir -p %{buildroot}/%{_libdir}
install -m 0755 makefiles/protolib.a %{buildroot}/%{_libdir}/protolib.a
mkdir -p %{buildroot}/%{_bindir}
for i in %{binaries}; do
  install -m 0755 makefiles/$i %{buildroot}/%{_bindir}/$i
  strip %{buildroot}/%{_bindir}/$i
done

%files
%defattr(0755,root,root)
/usr/lib/protolib.a
/usr/bin/arposer
/usr/bin/averageExample
/usr/bin/base64Example
/usr/bin/detourExample
/usr/bin/graphExample
/usr/bin/graphRider
/usr/bin/graphXMLExample
/usr/bin/jsonExample
/usr/bin/lfsrExample
/usr/bin/msg2MsgExample
/usr/bin/msgExample
/usr/bin/netExample
/usr/bin/pcmd
/usr/bin/pipe2SockExample
/usr/bin/pipeExample
/usr/bin/protoCapExample
/usr/bin/protoFileExample
/usr/bin/queueExample
/usr/bin/riposer
/usr/bin/serialExample
/usr/bin/simpleTcpExample
/usr/bin/sock2PipeExample
/usr/bin/threadExample
/usr/bin/timerTest
/usr/bin/ting
/usr/bin/vifExample
/usr/bin/vifLan
/usr/bin/protoExample

%changelog
* Thu Dec 27 2018 Ben Stern <bstern@fortian.com> - 3.01b-1
- Initial release
