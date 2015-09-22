%define _ux_define tizen2.3
Name:	   download-provider
Summary:	Download the contents in background
Version:	2.1.23
Release:	0
Group:	  Development/Libraries
License:	Apache-2.0
Source0:	%{name}-%{version}.tar.gz
Requires(post): sys-assert
Requires(post): libdevice-node
Requires(post): sqlite
Requires(post): connman
BuildRequires:  cmake
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(gobject-2.0)
BuildRequires:  pkgconfig(xdgmime)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(sqlite3)
BuildRequires:  pkgconfig(bundle)
BuildRequires:  pkgconfig(capi-base-common)
BuildRequires:  pkgconfig(capi-appfw-app-manager)
BuildRequires:  pkgconfig(capi-appfw-application)
BuildRequires:  pkgconfig(capi-network-connection)
BuildRequires:  pkgconfig(appsvc)
BuildRequires:  pkgconfig(libcurl)
BuildRequires:  pkgconfig(capi-content-mime-type)
BuildRequires:  pkgconfig(libsmack)
BuildRequires:  gettext-devel
BuildRequires:  pkgconfig(libsystemd-daemon)
BuildRequires:  pkgconfig(capi-network-wifi-direct)
BuildRequires:  pkgconfig(libresourced)
#BuildRequires:  model-build-features T30
BuildRequires:  pkgconfig(storage)
%if "%{?tizen_profile_name}" == "wearable"
BuildRequires:  pkgconfig(security-server)
%else if "%{?tizen_profile_name}" == "mobile"
BuildRequires:  pkgconfig(notification)
%endif

BuildRequires: pkgconfig(cynara-client)
BuildRequires: pkgconfig(cynara-client-async)
BuildRequires: pkgconfig(cynara-creds-socket)
BuildRequires: pkgconfig(cynara-creds-dbus)

%description
Download the specified contents in background

%package devel
Summary:	Libraries for download-provider
Group:	  Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
Download the contents in background (development files)

%package locale
Summary:	The download-provider locale package
Group:	  Development/Languages
Requires:   %{name} = %{version}-%{release}

%description locale
Package providing locale for download-provider

%prep
%setup -q

%define _data_install_path /opt/usr/data/%{name}
%define _resource_install_path /usr/share/%{name}
%define _imagedir %{_resource_install_path}/images 
%define _localedir %{_resource_install_path}/locales
%define _databasedir %{_data_install_path}/database
%define _database_client_dir %{_databasedir}/clients
%define _notifydir %{_data_install_path}/notify
%define _ipc_socket /opt/data/%{name}/%{name}.sock
%define _licensedir /usr/share/license
%define _logdump_script_dir /opt/etc/dump.d/module.d
%define _http_lib libcurl

%define download_booster OFF
%define sys_resource OFF
%define support_oma_drm OFF
%define wifi_direct ON
%define support_security_privilege OFF
%define support_companion_mode OFF
%define support_notification ON
%define support_knox ON
%define _manifest_name %{name}.manifest

%if 0%{?model_build_feature_wlan_p2p_disable }
%define wifi_direct OFF
%endif
%if "%{?tizen_profile_name}" == "wearable"
%define download_booster OFF
%define support_notification OFF
%define _manifest_name %{name}-w.manifest
%endif
%if 0%{?sec_product_feature_container_enable}
%define support_knox ON
%endif

%define cmake \
	CFLAGS="${CFLAGS:-%optflags} -fPIC -D_REENTRANT -fvisibility=hidden"; export CFLAGS \
	FFLAGS="${FFLAGS:-%optflags} -fPIC -fvisibility=hidden"; export FFLAGS \
	LDFLAGS+=" -Wl,--as-needed -Wl,--hash-style=both"; export LDFLAGS \
	%__cmake \\\
		-DCMAKE_INSTALL_PREFIX:PATH=%{_prefix} \\\
		-DBIN_INSTALL_DIR:PATH=%{_bindir} \\\
		-DLIB_INSTALL_DIR:PATH=%{_libdir} \\\
		%ifarch armv7l \
		-DLIB_AGENT_PATH="/usr/lib/libdownloadagent2.so" \\\
		%else \
		-DLIB_AGENT_PATH="/usr/lib64/libdownloadagent2.so" \\\
		%endif \
		-DINCLUDE_INSTALL_DIR:PATH=%{_includedir} \\\
		-DPKG_NAME=%{name} \\\
		-DPKG_VERSION=%{version} \\\
		-DPKG_RELEASE=%{release} \\\
		-DIPC_SOCKET:PATH=%{_ipc_socket} \\\
		-DPROVIDER_DIR:PATH=%{_data_install_path} \\\
		-DNOTIFY_DIR:PATH=%{_notifydir} \\\
		-DDATABASE_DIR:PATH=%{_databasedir} \\\
		-DDATABASE_CLIENT_DIR:PATH=%{_database_client_dir} \\\
		-DIMAGE_DIR:PATH=%{_imagedir} \\\
		-DLOCALE_DIR:PATH=%{_localedir} \\\
		-DLICENSE_DIR:PATH=%{_licensedir} \\\
		-DSUPPORT_WIFI_DIRECT:BOOL=OFF \\\
		%if "%{?sys_resource}" == "ON" \
		-DSUPPORT_SYS_RESOURCE:BOOL=ON \\\
		%else \
		-DSUPPORT_SYS_RESOURCE:BOOL=OFF \\\
		%endif \
		%if "%{?download_booster}" == "ON" \
		-DSUPPORT_DOWNLOAD_BOOSTER:BOOL=ON \\\
		%else \
		-DSUPPORT_DOWNLOAD_BOOSTER:BOOL=OFF \\\
		%endif \
		%if "%{?support_notification}" == "ON" \
		-DSUPPORT_NOTIFICATION:BOOL=ON \\\
		%else \
		-DSUPPORT_NOTIFICATION:BOOL=OFF \\\
		%endif \
		-DSUPPORT_LOG_MESSAGE:BOOL=ON \\\
		%if "%{?support_oma_drm}" == "ON" \
		-DSUPPORT_OMA_DRM:BOOL=ON \\\
		%else \
		-DSUPPORT_OMA_DRM:BOOL=OFF \\\
		%endif \
		%if "%{?support_security_privilege}" == "ON" \
		-DSUPPORT_SECURITY_PRIVILEGE:BOOL=ON \\\
		%else \
		-DSUPPORT_SECURITY_PRIVILEGE:BOOL=OFF \\\
		%endif \
		%if "%{?support_companion_mode}" == "ON" \
		-DSUPPORT_COMPANION_MODE:BOOL=ON \\\
		%else \
		-DSUPPORT_COMPANION_MODE:BOOL=OFF \\\
		%endif \
		%if "%{?support_knox}" == "ON" \
		-DSUPPORT_KNOX:BOOL=ON \\\
		%else \
		-DSUPPORT_KNOX:BOOL=OFF \\\
		%endif \
		%if "%{?_ux_define}" == "tizen2.3" \
		-DTIZEN_2_3_UX:BOOL=ON \\\
		%endif \
		-DCMAKE_LOG_DUMP_SCRIPT_DIR=%{_logdump_script_dir} \\\
		-DHTTP_LIB=%{_http_lib} \\\
		%if "%{?_lib}" == "lib64" \
		%{?_cmake_lib_suffix64} \\\
		%endif \
		%{?_cmake_skip_rpath} \\\
		-DBUILD_SHARED_LIBS:BOOL=ON

%build
export CFLAGS="$CFLAGS -DTIZEN_DEBUG_ENABLE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_DEBUG_ENABLE"
export FFLAGS="$FFLAGS -DTIZEN_DEBUG_ENABLE"
%cmake .
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

#%%if 0%{?sec_product_feature_container_enable}
mkdir -p %{buildroot}/etc/vasum/vsmzone.resource/
mv %{buildroot}/usr/share/download-provider/download-provider.res %{buildroot}/etc/vasum/vsmzone.resource/
#%%endif

mkdir -p %{buildroot}%{_licensedir}
mkdir -p %{buildroot}/lib/systemd/system/graphical.target.wants
mkdir -p %{buildroot}/lib/systemd/system/sockets.target.wants
ln -s ../download-provider.service %{buildroot}/lib/systemd/system/graphical.target.wants/
ln -s ../download-provider.socket %{buildroot}/lib/systemd/system/sockets.target.wants/

%post
#make notify dir in post section for smack
mkdir /opt/data/download-provider
mkdir -p %{_notifydir}
chsmack -a 'System::Shared' %{_notifydir}
chsmack -t %{_notifydir}                                        
mkdir -p --mode=0700 %{_databasedir}
#chsmack -a 'download-provider' %%{_databasedir}
mkdir -p --mode=0700 %{_database_client_dir}
#chsmack -a 'download-provider' %%{_database_client_dir}
/sbin/ldconfig

%postun
/sbin/ldconfig

%files locale
%lang(bg) %{_localedir}/bg/LC_MESSAGES/download-provider.mo
%lang(cs) %{_localedir}/cs/LC_MESSAGES/download-provider.mo
%lang(de) %{_localedir}/de/LC_MESSAGES/download-provider.mo
%lang(eu) %{_localedir}/eu/LC_MESSAGES/download-provider.mo
%lang(fi) %{_localedir}/fi/LC_MESSAGES/download-provider.mo
%lang(fr) %{_localedir}/fr/LC_MESSAGES/download-provider.mo
%lang(gl) %{_localedir}/gl/LC_MESSAGES/download-provider.mo
%lang(hr) %{_localedir}/hr/LC_MESSAGES/download-provider.mo
%lang(hu) %{_localedir}/hu/LC_MESSAGES/download-provider.mo
%lang(ka) %{_localedir}/ka/LC_MESSAGES/download-provider.mo
%lang(kk) %{_localedir}/kk/LC_MESSAGES/download-provider.mo
%lang(lv) %{_localedir}/lv/LC_MESSAGES/download-provider.mo
%lang(nl) %{_localedir}/nl/LC_MESSAGES/download-provider.mo
%lang(pl) %{_localedir}/pl/LC_MESSAGES/download-provider.mo
%lang(pt_BR) %{_localedir}/pt_BR/LC_MESSAGES/download-provider.mo
%lang(sk) %{_localedir}/sk/LC_MESSAGES/download-provider.mo
%lang(sl) %{_localedir}/sl/LC_MESSAGES/download-provider.mo
%lang(sr) %{_localedir}/sr/LC_MESSAGES/download-provider.mo
%lang(sv) %{_localedir}/sv/LC_MESSAGES/download-provider.mo
%lang(uk) %{_localedir}/uk/LC_MESSAGES/download-provider.mo
%lang(zh_CN) %{_localedir}/zh_CN/LC_MESSAGES/download-provider.mo
%lang(zh_HK) %{_localedir}/zh_HK/LC_MESSAGES/download-provider.mo
%lang(zh_TW) %{_localedir}/zh_TW/LC_MESSAGES/download-provider.mo
%lang(ar) %{_localedir}/ar/LC_MESSAGES/download-provider.mo
%lang(az) %{_localedir}/az/LC_MESSAGES/download-provider.mo
%lang(ca) %{_localedir}/ca/LC_MESSAGES/download-provider.mo
%lang(da) %{_localedir}/da/LC_MESSAGES/download-provider.mo
%lang(el_GR) %{_localedir}/el_GR/LC_MESSAGES/download-provider.mo
%lang(en) %{_localedir}/en/LC_MESSAGES/download-provider.mo
%lang(en_PH) %{_localedir}/en_PH/LC_MESSAGES/download-provider.mo
%lang(en_US) %{_localedir}/en_US/LC_MESSAGES/download-provider.mo
%lang(es_ES) %{_localedir}/es_ES/LC_MESSAGES/download-provider.mo
%lang(es_US) %{_localedir}/es_US/LC_MESSAGES/download-provider.mo
%lang(et) %{_localedir}/et/LC_MESSAGES/download-provider.mo
%lang(fr_CA) %{_localedir}/fr_CA/LC_MESSAGES/download-provider.mo
%lang(ga) %{_localedir}/ga/LC_MESSAGES/download-provider.mo
%lang(hi) %{_localedir}/hi/LC_MESSAGES/download-provider.mo
%lang(hy) %{_localedir}/hy/LC_MESSAGES/download-provider.mo
%lang(is) %{_localedir}/is/LC_MESSAGES/download-provider.mo
%lang(it_IT) %{_localedir}/it_IT/LC_MESSAGES/download-provider.mo
%lang(ja_JP) %{_localedir}/ja_JP/LC_MESSAGES/download-provider.mo
%lang(ko_KR) %{_localedir}/ko_KR/LC_MESSAGES/download-provider.mo
%lang(lt) %{_localedir}/lt/LC_MESSAGES/download-provider.mo
%lang(mk) %{_localedir}/mk/LC_MESSAGES/download-provider.mo
%lang(nb) %{_localedir}/nb/LC_MESSAGES/download-provider.mo
%lang(pt_PT) %{_localedir}/pt_PT/LC_MESSAGES/download-provider.mo
%lang(ro) %{_localedir}/ro/LC_MESSAGES/download-provider.mo
%lang(ru_RU) %{_localedir}/ru_RU/LC_MESSAGES/download-provider.mo
%lang(tr_TR) %{_localedir}/tr_TR/LC_MESSAGES/download-provider.mo
%lang(uz) %{_localedir}/uz/LC_MESSAGES/download-provider.mo

%files
%manifest %{_manifest_name}
%{_imagedir}/*.png
%{_libdir}/libdownloadagent2.so.0.1.0
%{_libdir}/libdownloadagent2.so
/lib/systemd/system/download-provider.service
/lib/systemd/system/graphical.target.wants/download-provider.service
/lib/systemd/system/download-provider.socket
/lib/systemd/system/sockets.target.wants/download-provider.socket
%{_libdir}/libdownload-provider-interface.so.%{version}
%{_libdir}/libdownload-provider-interface.so.0
%{_bindir}/%{name}
%{_licensedir}/%{name}
%attr(0544,root,root) %{_logdump_script_dir}/dump-%{name}.sh
#%%if 0%{?sec_product_feature_container_enable}
%config(noreplace) /etc/vasum/vsmzone.resource/download-provider.res
#%%endif

%files devel
%{_libdir}/libdownloadagent2.so
%{_libdir}/libdownload-provider-interface.so
%{_includedir}/download-provider/download-provider.h
%{_includedir}/download-provider/download-provider-interface.h
%{_bindir}/%{name}
%{_libdir}/pkgconfig/download-provider.pc
%{_libdir}/pkgconfig/download-provider-interface.pc
