
Name:	org.tizen.download-provider
Summary:	Application for support of the content download
Version:	1.0.0
Release:	1
Group:		TO_BE_FILLED_IN
License:    Samsung Proprietary License
URL:		N/A
Source0:	%{name}-%{version}.tar.gz
BuildRequires: pkgconfig(capi-system-runtime-info)
BuildRequires: pkgconfig(capi-appfw-application)
BuildRequires: pkgconfig(appsvc)
BuildRequires: pkgconfig(libdownload-agent)
BuildRequires: pkgconfig(elementary)
BuildRequires: pkgconfig(aul)
BuildRequires: pkgconfig(drm-service)
BuildRequires: pkgconfig(ecore)
BuildRequires: pkgconfig(vconf)
BuildRequires: pkgconfig(bundle)
BuildRequires: pkgconfig(xdgmime)
BuildRequires: pkgconfig(notification)
BuildRequires: pkgconfig(icu-i18n)
BuildRequires: cmake
BuildRequires: gettext-devel
BuildRequires: expat-devel
BuildRequires: edje-tools

%description
Application for support of the content download 

%prep
%setup -q

%build
cmake . -DCMAKE_INSTALL_PREFIX="/opt/apps/org.tizen.download-provider"

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

%post
#### Download History ####
if [ ${USER} == "root" ]
then
        # Change file owner
        chown -R 5000:5000 /opt/apps/org.tizen.download-provider/data
fi

if [ ! -f /opt/apps/org.tizen.download-provider/data/db/.download-history.db ];
then
		sqlite3 /opt/apps/org.tizen.download-provider/data/db/.download-history.db 'PRAGMA journal_mode=PERSIST; 
		create table history(id integer primary key autoincrement, historyid integer, downloadtype integer, contenttype integer, state integer, err integer, name, vendor, path, url, cookie, date datetime);'
fi

if [ ${USER} == "root" ]
then
        chown -R 5000:5000 /opt/apps/org.tizen.download-provider/data
        chown :6002 /opt/apps/org.tizen.download-provider/data/db/.download-history.db
        chown :6002 /opt/apps/org.tizen.download-provider/data/db/.download-history.db-journal
        chmod 660 /opt/apps/org.tizen.download-provider/data/db/.download-history.db
        chmod 660 /opt/apps/org.tizen.download-provider/data/db/.download-history.db-journal
fi

%files 
%defattr(-,root,root,-)
/opt/apps/org.tizen.download-provider/bin/*
/opt/apps/org.tizen.download-provider/data
/opt/apps/org.tizen.download-provider/res/*
/opt/share/applications/*

