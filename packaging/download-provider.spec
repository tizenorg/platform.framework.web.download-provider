
Name:       download-provider
Summary:    Download the contents in background
Version:    1.0.5
Release:    10
Group:      Development/Libraries
License:    Apache License, Version 2.0
Source0:    %{name}-%{version}.tar.gz
Source1:    download-provider.service
Source101:  org.download-provider.conf
Source1001: 	download-provider.manifest
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
BuildRequires:  pkgconfig(capi-base-common)
BuildRequires:  pkgconfig(capi-appfw-app-manager)
BuildRequires:  pkgconfig(capi-network-connection)
BuildRequires:  pkgconfig(notification)
BuildRequires:  pkgconfig(appsvc)
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(wifi-direct)
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
%description
Description: download the contents in background

%package devel
Summary:    Download-provider
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
Description: download the contents in background (developement files)

%prep
%setup -q
cp %{SOURCE101} .
cp %{SOURCE1001} .

%define _imagedir /usr/share/download-provider
%define _databasedir /opt/usr/dbspace
%define _databasefile %{_databasedir}/.download-provider.db
%define _dbusservicedir /usr/share/dbus-1/system-services
%define _dbuspolicydir /etc/dbus-1/system.d
%define _licensedir /usr/share/license

%define cmake \
	CFLAGS="${CFLAGS:-%optflags} -fPIC -D_REENTRANT -fvisibility=hidden"; export CFLAGS \
	FFLAGS="${FFLAGS:-%optflags} -fPIC -fvisibility=hidden"; export FFLAGS \
	LDFLAGS+=" -Wl,--as-needed -Wl,--hash-style=both"; export LDFLAGS \
	%__cmake \\\
		-DCMAKE_INSTALL_PREFIX:PATH=%{_prefix} \\\
		-DBIN_INSTALL_DIR:PATH=%{_bindir} \\\
		-DLIB_INSTALL_DIR:PATH=%{_libdir} \\\
		-DINCLUDE_INSTALL_DIR:PATH=%{_includedir} \\\
		-DPKG_NAME=%{name} \\\
		-DPKG_VERSION=%{version} \\\
		-DPKG_RELEASE=%{release} \\\
		-DIMAGE_DIR:PATH=%{_imagedir} \\\
		-DDATABASE_FILE:PATH=%{_databasefile} \\\
		-DDBUS_SERVICE_DIR:PATH=%{_dbusservicedir} \\\
		-DLICENSE_DIR:PATH=%{_licensedir} \\\
		-DSUPPORT_DBUS_SYSTEM:BOOL=ON \\\
		-DSUPPORT_WIFI_DIRECT:BOOL=OFF \\\
		-DSUPPORT_LOG_MESSAGE:BOOL=ON \\\
		-DSUPPORT_CHECK_IPC:BOOL=ON \\\
		%if "%{?_lib}" == "lib64" \
		%{?_cmake_lib_suffix64} \\\
		%endif \
		%{?_cmake_skip_rpath} \\\
		-DBUILD_SHARED_LIBS:BOOL=ON

%build
%cmake .
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

install -d -m 755 %{buildroot}%{_dbuspolicydir}
install -m 644 %{SOURCE101} %{buildroot}%{_dbuspolicydir}

mkdir -p %{buildroot}%{_licensedir}
mkdir -p  %{buildroot}%{_sysconfdir}/rc.d/rc3.d
ln -s %{_sysconfdir}/rc.d/init.d/download-provider-service  %{buildroot}%{_sysconfdir}/rc.d/rc3.d/S70download-provider-service
mkdir -p  %{buildroot}%{_sysconfdir}/rc.d/rc5.d
ln -s %{_sysconfdir}/rc.d/init.d/download-provider-service  %{buildroot}%{_sysconfdir}/rc.d/rc5.d/S70download-provider-service

mkdir -p %{buildroot}/%{_unitdir}/graphical.target.wants
install %{SOURCE1} %{buildroot}/%{_unitdir}/
ln -s ../download-provider.service %{buildroot}/%{_unitdir}/graphical.target.wants/

mkdir -p %{buildroot}/opt/data/%{name}
mkdir -p %{buildroot}%{_databasedir}
if [ ! -f %{buildroot}%{_databasefile} ];
then
sqlite3 %{buildroot}%{_databasefile} 'PRAGMA journal_mode=PERSIST;
PRAGMA foreign_keys=ON;
CREATE TABLE logging
(
id INTEGER UNIQUE PRIMARY KEY,
state INTEGER DEFAULT 0,
errorcode INTEGER DEFAULT 0,
startcount INTEGER DEFAULT 0,
packagename TEXT DEFAULT NULL,
createtime DATE,
accesstime DATE
);

CREATE TABLE requestinfo
(
id INTEGER UNIQUE PRIMARY KEY,
auto_download BOOLEAN DEFAULT 0,
state_event BOOLEAN DEFAULT 0,
progress_event BOOLEAN DEFAULT 0,
noti_enable BOOLEAN DEFAULT 0,
network_type TINYINT DEFAULT 0,
filename TEXT DEFAULT NULL,
destination TEXT DEFAULT NULL,
url TEXT DEFAULT NULL,
FOREIGN KEY(id) REFERENCES logging(id) ON DELETE CASCADE
);

CREATE TABLE downloadinfo
(
id INTEGER UNIQUE PRIMARY KEY,
http_status INTEGER DEFAULT 0,
content_size UNSIGNED BIG INT DEFAULT 0,
mimetype VARCHAR(64) DEFAULT NULL,
content_name TEXT DEFAULT NULL,
saved_path TEXT DEFAULT NULL,
tmp_saved_path TEXT DEFAULT NULL,
etag TEXT DEFAULT NULL,
FOREIGN KEY(id) REFERENCES logging(id) ON DELETE CASCADE
);

CREATE TABLE httpheaders
(
id INTEGER NOT NULL,
header_field TEXT DEFAULT NULL,
header_data TEXT DEFAULT NULL,
FOREIGN KEY(id) REFERENCES logging(id) ON DELETE CASCADE
);

CREATE TABLE notification
(
id INTEGER NOT NULL,
extra_key TEXT DEFAULT NULL,
extra_data TEXT DEFAULT NULL,
FOREIGN KEY(id) REFERENCES logging(id) ON DELETE CASCADE
);

CREATE UNIQUE INDEX requests_index ON logging (id, state, errorcode, packagename, createtime, accesstime);
'
fi


%post
/sbin/ldconfig

%postun
/sbin/ldconfig


%files
%defattr(-,root,root,-)
%manifest %{name}.manifest
%dir %attr(0775,root,app) /opt/data/%{name}
%{_imagedir}/*.png
%{_libdir}/libdownloadagent2.so.0.0.1
%{_libdir}/libdownloadagent2.so
%{_unitdir}/download-provider.service
%{_unitdir}/graphical.target.wants/download-provider.service
%{_libdir}/libdownload-provider-interface.so.%{version}
%{_libdir}/libdownload-provider-interface.so.0
%{_bindir}/%{name}
%{_sysconfdir}/rc.d/init.d/download-provider-service
%{_sysconfdir}/rc.d/rc3.d/S70download-provider-service
%{_sysconfdir}/rc.d/rc5.d/S70download-provider-service
%{_licensedir}/%{name}
%{_dbusservicedir}/org.download-provider.service
%{_dbuspolicydir}/org.download-provider.conf
%attr(660,root,app) /opt/usr/dbspace/.download-provider.db
%attr(660,root,app) /opt/usr/dbspace/.download-provider.db-journal

%files devel
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_libdir}/libdownloadagent2.so
%{_libdir}/libdownload-provider-interface.so
%{_includedir}/download-provider/download-provider-defs.h
%{_includedir}/download-provider/download-provider-interface.h
%{_bindir}/%{name}
%{_libdir}/pkgconfig/download-provider.pc
%{_libdir}/pkgconfig/download-provider-interface.pc

