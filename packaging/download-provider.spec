Name:       download-provider
Summary:    download the contents in background.
Version: 	0.0.3
Release:    2
Group:      TO_BE/FILLED_IN
License:    TO_BE/FILLED_IN
Source0:    %{name}-%{version}.tar.gz
Requires(post): /usr/bin/sqlite3
BuildRequires:  cmake
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gobject-2.0)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(libsoup-2.4)
BuildRequires:  pkgconfig(xdgmime)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(db-util)
BuildRequires:  pkgconfig(sqlite3)
BuildRequires:  pkgconfig(capi-appfw-application)
BuildRequires:  pkgconfig(capi-network-connection)

%description
Description: download the contents in background

%package devel
Summary:    download-provider
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
Description: download the contents in background (developement files)

%prep
%setup -q

cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix}

%build
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

mkdir -p  %{buildroot}%{_sysconfdir}/rc.d/rc3.d
ln -s %{_sysconfdir}/rc.d/init.d/download-provider-service  %{buildroot}%{_sysconfdir}/rc.d/rc3.d/S70download-provider-service
mkdir -p  %{buildroot}%{_sysconfdir}/rc.d/rc5.d
ln -s %{_sysconfdir}/rc.d/init.d/download-provider-service  %{buildroot}%{_sysconfdir}/rc.d/rc5.d/S70download-provider-service

mkdir -p %{buildroot}/opt/data/download-provider

%post
mkdir -p /opt/dbspace/
if [ ! -f /opt/dbspace/.download-provider.db ];
then
	sqlite3 /opt/dbspace/.download-provider.db 'PRAGMA journal_mode=PERSIST;
	CREATE TABLE downloading (id INTEGER PRIMARY KEY AUTOINCREMENT, uniqueid INTEGER UNIQUE, packagename TEXT, notification INTEGER, installpath TEXT, filename TEXT, creationdate TEXT, retrycount INTEGER, state INTEGER, url TEXT, mimetype TEXT, etag TEXT, savedpath TEXT);'
	sqlite3 /opt/dbspace/.download-provider.db 'PRAGMA journal_mode=PERSIST;
	CREATE TABLE history (id INTEGER PRIMARY KEY AUTOINCREMENT, uniqueid INTEGER UNIQUE, packagename TEXT, filename TEXT, creationdate TEXT, state INTEGER, mimetype TEXT, savedpath TEXT);'
fi

%files
%defattr(-,root,root,-)
%dir /opt/data/download-provider
/opt/data/download-provider/*.png
%{_libdir}/libdownloadagent.so.0.0.1
%{_libdir}/libdownloadagent.so
%{_bindir}/download-provider
%{_sysconfdir}/rc.d/init.d/download-provider-service
%{_sysconfdir}/rc.d/rc3.d/S70download-provider-service
%{_sysconfdir}/rc.d/rc5.d/S70download-provider-service

%files devel
%defattr(-,root,root,-)
%{_libdir}/libdownloadagent.so.0.0.1
%{_libdir}/libdownloadagent.so
%{_bindir}/download-provider
%{_includedir}/download-provider/download-provider.h
%{_libdir}/pkgconfig/download-provider.pc

%changelog
* Tue Aug 17 2012 Jungki Kwak <jungki.kwak@samsung.com>
- Enable to use destination path
- Add to handle invalid id

* Tue Aug 16 2012 Jungki Kwak <jungki.kwak@samsung.com>
- Change socket close timing

* Mon Aug 13 2012 Kwangmin Bang <justine.bang@samsung.com>
- Disable default dlog in launching script.

* Tue Aug 09 2012 Jungki Kwak <jungki.kwak@samsung.com>
- The function to init dbus glib is removed

* Tue Aug 08 2012 Jungki Kwak <jungki.kwak@samsung.com>
- The function to init dbus glib is added for connection network CAPI

* Tue Aug 07 2012 Jungki Kwak <jungki.kwak@samsung.com>
- Change the name of temp direcoty.
- When add requestinfo to slot, save it to DB.

* Mon Aug 06 2012 Jungki Kwak <jungki.kwak@samsung.com>
- Initial version is updated.

