%define _unpackaged_files_terminate_build 1

Name: sf100linux
Version: 4.0.0
Release: alt1

Summary: Control software DediProg SF100 programmer
License: %gpl2plus
Group: Other
Url: https://github.com/DediProgSW/SF100Linux

BuildRequires(pre): rpm-build-licenses

BuildRequires: libusb-devel

Requires: libusb

Source0: %name-%version.tar

%description
Linux software for Dediprog SF100 and SF600 SPI flash programmers

%prep
%setup -q

%build
%make

%install
mkdir -vp %buildroot%_bindir
mkdir -vp %buildroot%_datadir/DediProg
mkdir -vp %buildroot%_sysconfdir/udev/rules.d/
install -v -m 0755 dpcmd %buildroot%_bindir/dpcmd
install -v -m 0644 ChipInfoDb.dedicfg %buildroot%_datadir/DediProg/ChipInfoDb.dedicfg
install -v -m 0644 60-dediprog.rules %buildroot%_sysconfdir/udev/rules.d/60-dediprog.rules

%files
%_sysconfdir/udev/rules.d/60-dediprog.rules
%_bindir/dpcmd
%_datadir/DediProg/ChipInfoDb.dedicfg

%changelog
* Fri Aug 27 2021 Igor Chudov <nir@altlinux.org> 4.0.0-alt1
- Initial release

