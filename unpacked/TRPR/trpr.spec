%define _topdir @TOPDIR@
%define name trpr
%define release 1%{?dist}
%define version @VERSION@
%define buildroot %{_topdir}/%{name}-%{version}-root
%define binaries trpr hcat

BuildRoot: %{buildroot}
Summary: Analyzes output from the tcpdump packet sniffing program and creates output suitable for plotting
License: BSD
URL: https://downloads.pf.itd.nrl.navy.mil/docs/proteantools/trpr.html
Name: %{name}
Version: %{version}
BuildRequires: gcc-c++
BuildRequires: make
Release: %{release}
Source0: src-%{name}-%{version}.tgz
Prefix: /usr

%description

The TRace Plot Real-time (TRPR), aka Tcpdump Real Time Plot, is open source
software by the Naval Research Laboratory (NRL) PROTocol Engineering
Advanced Networking (PROTEAN) group that analyzes output from the tcpdump
packet sniffing program and creates output suitable for plotting. It also
specifically supports a range of functionality for specific use of the
gnuplot graphing program.  TRPR can operate in a "real-time" plotting mode
where tcpdump stdout can be piped into TRPR and TRPR's stdout in turn can be
piped directly into gnuplot for a sort of real-time network oscilloscope.
TRPR can also parse tcpdump text trace files and produce files which can be
plotted by gnuplot or imported into other plotting or spreadsheet programs.
IPv4 and IPv6 traces from tcpdump are supported.  TRPR can also perform the
same functions with MGEN log files and ns-2 trace files.

%prep
%setup -q -n TRPR

%build
make -f Makefile.linux

%install
mkdir -p %{buildroot}/%{_bindir}
for i in %{binaries}; do
  install -m 0755 $i %{buildroot}/%{_bindir}/$i
  strip %{buildroot}/%{_bindir}/$i
done

%files
%defattr(0755,root,root)
/usr/bin/trpr
/usr/bin/hcat

%changelog
* Sat Dec 29 2018 Ben Stern <bstern@fortian.com> - 2.1b2
- Initial release
