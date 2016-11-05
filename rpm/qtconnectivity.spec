Name:       qt5-qtconnectivity
Summary:    Qt Connectivity module
Version:    5.0.0
Release:    1%{?dist}
Group:      Qt/Qt
License:    LGPLv2.1 with exception or GPLv3
URL:        http://qt.nokia.com
Source0:    %{name}-%{version}.tar.xz
BuildRequires:  qt5-qtcore-devel
BuildRequires:  qt5-qtgui-devel
BuildRequires:  qt5-qtwidgets-devel
BuildRequires:  qt5-qtopengl-devel
BuildRequires:  qt5-qtnetwork-devel
BuildRequires:  qt5-qtdeclarative-devel
BuildRequires:  qt5-qtdeclarative-qtquick-devel
BuildRequires:  qt5-qmake
BuildRequires:  qt5-qtv8-devel
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Concurrent)
BuildRequires:	pkgconfig(bluez)
BuildRequires:  fdupes

%description
Qt is a cross-platform application and UI framework. Using Qt, you can
write web-enabled applications once and deploy them across desktop,
mobile and embedded systems without rewriting the source code.
.
This package contains the Qt connectivity module


%package qtbluetooth
Summary:    QtBluetooth
Group:      Qt/Qt

%description qtbluetooth
Qt is a cross-platform application and UI framework. Using Qt, you can
write web-enabled applications once and deploy them across desktop,
mobile and embedded systems without rewriting the source code.
.
This package contains the QtBluetooth module

%package qtbluetooth-devel
Summary:    QtBluetooth - development files
Group:      Qt/Qt
Requires:   %{name}-qtbluetooth = %{version}-%{release}

%description qtbluetooth-devel
Qt is a cross-platform application and UI framework. Using Qt, you can
write web-enabled applications once and deploy them across desktop,
mobile and embedded systems without rewriting the source code.
.
This package contains the QtBluetooth development files

%package qtnfc
Summary:    QtNfc
Group:      Qt/Qt

%description qtnfc
Qt is a cross-platform application and UI framework. Using Qt, you can
write web-enabled applications once and deploy them across desktop,
mobile and embedded systems without rewriting the source code.
.
This package contains the QtNfc module

%package qtnfc-devel
Summary:    QtNfc - development files
Group:      Qt/Qt
Requires:   %{name}-qtnfc = %{version}-%{release}

%description qtnfc-devel
Qt is a cross-platform application and UI framework. Using Qt, you can
write web-enabled applications once and deploy them across desktop,
mobile and embedded systems without rewriting the source code.
.
This package contains the QtNfc development files


%prep
%setup -q -n %{name}-%{version}

%build
touch .git

qmake -qt=5 "CONFIG += nfc"

make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%qmake5_install
# Remove unneeded .la files
rm -f %{buildroot}/%{_libdir}/*.la
# Fix wrong path in pkgconfig files
find %{buildroot}%{_libdir}/pkgconfig -type f -name '*.pc' \
-exec perl -pi -e "s, -L%{_builddir}/?\S+,,g" {} \;
# Fix wrong path in prl files
find %{buildroot}%{_libdir} -type f -name '*.prl' \
-exec sed -i -e "/^QMAKE_PRL_BUILD_DIR/d;s/\(QMAKE_PRL_LIBS =\).*/\1/" {} \;
#

# We don't need qt5/Qt/
rm -rf %{buildroot}/%{_includedir}/qt5/Qt


%fdupes %{buildroot}/%{_includedir}


%post qtbluetooth
/sbin/ldconfig
%postun qtbluetooth
/sbin/ldconfig

%post qtnfc
/sbin/ldconfig
%postun qtnfc
/sbin/ldconfig




%files qtbluetooth
%defattr(-,root,root,-)
%{_libdir}/libQt5Bluetooth.so.5
%{_libdir}/libQt5Bluetooth.so.5.*
%{_libdir}/qt5/qml/QtBluetooth

%files qtbluetooth-devel
%defattr(-,root,root,-)
%{_libdir}/libQt5Bluetooth.so
%{_libdir}/libQt5Bluetooth.prl
%{_libdir}/pkgconfig/*
%{_includedir}/qt5/*
%{_datadir}/qt5/mkspecs/modules/qt_lib_bluetooth.pri
%{_datadir}/qt5/mkspecs/modules/qt_lib_bluetooth_private.pri
%{_libdir}/cmake/Qt5Bluetooth/

%files qtnfc
%defattr(-,root,root,-)
%{_libdir}/libQt5Nfc.so.5
%{_libdir}/libQt5Nfc.so.5.*
%{_libdir}/qt5/qml/QtNfc

%files qtnfc-devel
%defattr(-,root,root,-)
%{_libdir}/libQt5Nfc.so
%{_libdir}/libQt5Nfc.prl
%{_libdir}/pkgconfig/*
%{_includedir}/qt5/*
%{_datadir}/qt5/mkspecs/modules/qt_lib_nfc.pri
%{_datadir}/qt5/mkspecs/modules/qt_lib_nfc_private.pri
%{_libdir}/cmake/Qt5Nfc/


