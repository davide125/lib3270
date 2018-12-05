#
# spec file for package mingw32-lib3279
#
# Copyright (c) 2014 SUSE LINUX Products GmbH, Nuernberg, Germany.
# Copyright (C) <2008> <Banco do Brasil S.A.>
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via http://bugs.opensuse.org/
#

%define MAJOR_VERSION 5
%define MINOR_VERSION 2

%define _libvrs %{MAJOR_VERSION}_%{MINOR_VERSION}

%define __strip %{_mingw32_strip}
%define __objdump %{_mingw32_objdump}
%define _use_internal_dependency_generator 0
%define __find_requires %{_mingw32_findrequires}
%define __find_provides %{_mingw32_findprovides}
%define __os_install_post %{_mingw32_debug_install_post} \
                          %{_mingw32_install_post}

#---[ Main package ]--------------------------------------------------------------------------------------------------

Summary:		TN3270 Access library
Name:           mingw32-lib3270-%{_libvrs}
Version:        5.2
Release:        0
License:        GPL-2.0

Source:			%{name}-%{version}.tar.xz

Url:			https://portal.softwarepublico.gov.br/social/pw3270/

Group:			Development/Libraries/C and C++
BuildRoot:		/var/tmp/%{name}-%{version}

Provides:		mingw32-lib3270_%{MAJOR_VERSION}_%{MINOR_VERSION}
Conflicts:		otherproviders(mingw32-lib3270_%{MAJOR_VERSION}_%{MINOR_VERSION})

Provides:		mingw32(lib:3270) = %{version}
Provides:		mingw32(lib:3270_%{MAJOR_VERSION}_%{MINOR_VERSION}) = %{version}

BuildRequires:	autoconf
BuildRequires:	automake
BuildRequires:	gettext-tools

BuildRequires:	mingw32-cross-binutils
BuildRequires:	mingw32-cross-gcc
BuildRequires:	mingw32-cross-gcc-c++
BuildRequires:	mingw32-cross-pkg-config
BuildRequires:	mingw32-filesystem
BuildRequires:	mingw32-libopenssl-devel
BuildRequires:	mingw32-zlib-devel
BuildRequires:	mingw32(lib:iconv)

%description

TN3270 access library originally designed as part of the pw3270 application.

See more details at https://softwarepublico.gov.br/social/pw3270/

#---[ Development ]---------------------------------------------------------------------------------------------------

%package devel

Summary:	TN3270 Access library development files
Group:		Development/Libraries/C and C++
Requires:	%{name} = %{version}

Provides:	mingw32-lib3270-devel = %{version}
Conflicts:	otherproviders(mingw32-lib3270-devel)

%description devel

TN3270 access library for C development files.

Originally designed as part of the pw3270 application.

See more details at https://softwarepublico.gov.br/social/pw3270/

#---[ Build & Install ]-----------------------------------------------------------------------------------------------

%prep
%setup

NOCONFIGURE=1 ./autogen.sh

%{_mingw32_configure} \
	--with-sdk-version=%{version}

%build
make clean
make all

%{_mingw32_strip} \
	--strip-all \
	.bin/lib3270/Release/*.dll.%{MAJOR_VERSION}.%{MINOR_VERSION}

%install
%{_mingw32_makeinstall}

%clean
rm -rf %{buildroot}

#---[ Files ]---------------------------------------------------------------------------------------------------------

%files
%defattr(-,root,root)
%doc AUTHORS LICENSE README.md

%{_mingw32_libdir}/lib3270.dll
%{_mingw32_libdir}/lib3270.dll.%{MAJOR_VERSION}
%{_mingw32_libdir}/lib3270.dll.%{MAJOR_VERSION}.%{MINOR_VERSION}

%files devel
%defattr(-,root,root)
%{_mingw32_includedir}/lib3270
%{_mingw32_includedir}/lib3270.h
%{_mingw32_libdir}/pkgconfig/lib3270.pc

%{_mingw32_libdir}/lib3270.a

%changelog

