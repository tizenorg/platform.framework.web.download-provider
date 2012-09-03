
Name:       download-provider
Summary:    download the contents in background.
Version: 	0.0.5
Release:    7
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
BuildRequires:  pkgconfig(bundle)
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
* Mon Sep 03 2012 Kwangmin Bang <justine.bang@samsung.com>
- fix timeout error

* Mon Sep 03 2012 Kwangmin Bang <justine.bang@samsung.com>
- free slot after getting event from url-download
- fix INTEGER OVERFLOW

* Thu Aug 30 2012 Kwangmin Bang <justine.bang@samsung.com>
- initialize mutex for auto-redownloading
- support Pause/Resume with new connection
- fix the memory leak

* Mon Aug 27 2012 Kwangmin Bang <justine.bang@samsung.com>
- Change the ownership of downloaded file
- Add detached option when pthread is created
- fix the failure getting history info from database
- fix first timeout takes a long time
- fix wrong checking of network status
- fix the crash by double free
- divide log level
- Resolve prevent defects for agent module
- Resolve a bug to join domain in case of playready

* Tue Aug 23 2012 Kwangmin Bang <justine.bang@samsung.com>
- event thread does not deal in some state
- fix the lockup by mutex and the crash by invaild socket event

* Tue Aug 22 2012 Jungki Kwak <jungki.kwak@samsung.com>
- Fix the crash when use notification
- One thread model for socket
- Fix the defects found by prevent tool
- Remove mutex lock/unlock in case of invalid id
- Support the status of download in case of getting new connection with requestid
- Clear db and register notification when stopped the download
- Update notification function
- Enable to set the defined file name by user

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

