Name: libdbusaccess
Version: 1.0.10
Release: 0
Summary: Access control over D-Bus
Group: Development/Libraries
License: BSD
URL: https://git.sailfishos.org/mer-core/libdbusaccess
Source: %{name}-%{version}.tar.bz2
BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(gio-2.0)
BuildRequires: pkgconfig(libglibutil)
BuildRequires: bison
BuildRequires: flex
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%description
Provides library for controlling access over D-Bus

%package -n dbusaccess-tools
Summary: D-Bus access utilities

%description -n dbusaccess-tools
Provides dbus-creds tool

%files -n dbusaccess-tools
%defattr(-,root,root,-)
%{_bindir}/dbus-creds

%package devel
Summary: Development library for %{name}
Requires: %{name} = %{version}
Requires: pkgconfig

%description devel
This package contains the development library for %{name}.

%prep
%setup -q

%build
make LIBDIR=%{_libdir} KEEP_SYMBOLS=1 release pkgconfig
make LIBDIR=%{_libdir} KEEP_SYMBOLS=1 -C tools/dbus-creds release

%install
rm -rf %{buildroot}
make DESTDIR=%{buildroot} LIBDIR=%{_libdir} install-dev
make DESTDIR=%{buildroot} -C tools/dbus-creds install

%check
make -C test test

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_libdir}/%{name}.so.*

%files devel
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/*.pc
%{_libdir}/%{name}.so
%{_includedir}/dbusaccess/*.h
