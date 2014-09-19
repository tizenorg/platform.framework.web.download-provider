%define _data_install_path %{_datadir}/%{name}
%define _imagedir          %{_data_install_path}/images
%define _localedir         %{_datadir}/locale
%define _sqlschemadir      %{_data_install_path}/sql
%define _sqlschemafile     %{_sqlschemadir}/download-provider-schema.sql

Name:       download-provider
Summary:    Download the contents in background
Version:    1.1.6
Release:    0
Group:      System/Libraries
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
BuildRequires:  cmake
BuildRequires:  fdupes
BuildRequires:  libprivilege-control-conf
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gobject-2.0)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(libsoup-2.4)
BuildRequires:  pkgconfig(xdgmime)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(sqlite3)
BuildRequires:  pkgconfig(bundle)
BuildRequires:  pkgconfig(capi-base-common)
BuildRequires:  pkgconfig(capi-appfw-app-manager)
BuildRequires:  pkgconfig(capi-network-connection)
BuildRequires:  pkgconfig(notification)
BuildRequires:  pkgconfig(appsvc)
BuildRequires:  pkgconfig(wifi-direct)
BuildRequires:  pkgconfig(libsmack)
BuildRequires:  gettext-devel
BuildRequires:  pkgconfig(libsystemd-daemon)
BuildRequires:  pkgconfig(libtzplatform-config)
Requires(post): /sbin/ldconfig
Requires(post): vconf
Requires:       sqlite
Requires:       connman
Requires:       net-config


%description
Description: download the contents in background

%package devel
Summary:    Download-provider
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
Description: download the contents in background (development files)

%prep
%setup -q

%build
%if 0%{?tizen_build_binary_release_type_eng}
export CFLAGS="${CFLAGS} -DTIZEN_ENGINEER_MODE"
export CXXFLAGS="${CXXFLAGS} -DTIZEN_ENGINEER_MODE"
export FFLAGS="${FFLAGS} -DTIZEN_ENGINEER_MODE"
%endif

CFLAGS="${CFLAGS:-%optflags} -fPIC -D_REENTRANT -fvisibility=hidden"; export CFLAGS
FFLAGS="${FFLAGS:-%optflags} -fPIC -fvisibility=hidden"; export FFLAGS
LDFLAGS="${LDFLAGS} -Wl,--as-needed -Wl,--hash-style=both"; export LDFLAGS

%cmake . \
         -DBIN_INSTALL_DIR:PATH=%{_bindir} \
         -DPKG_NAME=%{name} \
         -DPKG_VERSION=%{version} \
         -DPKG_RELEASE=%{release} \
         -DIMAGE_DIR:PATH=%{_imagedir} \
         -DLOCALE_DIR:PATH=%{_localedir} \
         -DSYSTEMD_DIR:PATH=%{_unitdir} \
         -DDATABASE_SCHEMA_DIR=%{_sqlschemadir} \
         -DDATABASE_SCHEMA_FILE=%{_sqlschemafile} \
         -DSUPPORT_WIFI_DIRECT:BOOL=OFF \
         -DSUPPORT_LOG_MESSAGE:BOOL=ON \
         -DSUPPORT_CHECK_IPC:BOOL=ON

%__make %{?_smp_mflags}

%install
%make_install
mkdir -p %{buildroot}/%{_data_install_path}
mkdir -p %{buildroot}%{_unitdir}/graphical.target.wants
mkdir -p %{buildroot}%{_unitdir}/sockets.target.wants
ln -s ../download-provider.service %{buildroot}%{_unitdir}/graphical.target.wants/
ln -s ../download-provider.socket %{buildroot}%{_unitdir}/sockets.target.wants/

%find_lang %{name}

%fdupes %{buildroot}%{_localedir}

%post
/sbin/ldconfig
%systemd_post %{name}.service
vconftool set -t int db/setting/default_memory/wap 0

%preun
%systemd_preun %{name}.service

%postun
/sbin/ldconfig
%systemd_postun %{name}.service


%files -f %{name}.lang
%defattr(-,root,root,-)
%manifest %{name}.manifest
%license LICENSE.APLv2
%{_bindir}/%{name}
%{_imagedir}/*.png
%{_imagedir}/*.gif
%{_libdir}/libdownloadagent2.so*
%{_libdir}/libdownload-provider-interface.so*
%{_unitdir}/download-provider.service
%{_unitdir}/graphical.target.wants/download-provider.service
%{_unitdir}/download-provider.socket
%{_unitdir}/sockets.target.wants/download-provider.socket
%{_sqlschemafile}

%files devel
%defattr(-,root,root,-)
%manifest %{name}.manifest
%{_libdir}/libdownloadagent2.so
%{_libdir}/libdownload-provider-interface.so
%{_includedir}/download-provider/download-provider-defs.h
%{_includedir}/download-provider/download-provider-interface.h
%{_libdir}/pkgconfig/download-provider.pc
%{_libdir}/pkgconfig/download-provider-interface.pc
