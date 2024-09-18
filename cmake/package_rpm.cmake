# RPM specific CPack configuration parameters

set(CPACK_GENERATOR "${CPACK_GENERATOR};RPM")
set(CPACK_RPM_PACKAGE_VENDOR "MariaDB plc")
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "/etc /etc/ld.so.conf.d /etc/init.d /etc/rc.d/init.d /usr/share/man /usr/share/man1")
set(CPACK_RPM_SPEC_MORE_DEFINE "%define ignore \#")
set(CPACK_RPM_PACKAGE_NAME "${CPACK_PACKAGE_NAME}")
set(CPACK_RPM_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}")
set(CPACK_RPM_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")
set(CPACK_RPM_SPEC_INSTALL_POST "/bin/true")

if(DISTRIB_SUFFIX)
  set(CPACK_RPM_PACKAGE_RELEASE "${MAXSCALE_BUILD_NUMBER}.${DISTRIB_SUFFIX}")
else()
  set(CPACK_RPM_PACKAGE_RELEASE ${MAXSCALE_BUILD_NUMBER})
endif()

# This prevents the default %post from running which causes binaries to be
# striped. Without this, MaxCtrl will not work on all systems as the
# binaries will be stripped.
set(CPACK_RPM_SPEC_INSTALL_POST "/bin/true")

# If the package defines an explicit license, use that. Otherwise, use BSL 1.1
if (${TARGET_COMPONENT}_LICENSE)
  set(CPACK_RPM_PACKAGE_LICENSE ${TARGET_COMPONENT}_LICENSE)
else()
  set(CPACK_RPM_PACKAGE_LICENSE "MariaDB BSL 1.1")
endif()

set(IGNORED_DIRS
  "%ignore /etc"
  "%ignore /etc/init.d"
  "%ignore /etc/ld.so.conf.d"
  "%ignore ${CMAKE_INSTALL_PREFIX}"
  "%ignore ${CMAKE_INSTALL_PREFIX}/bin"
  "%ignore ${CMAKE_INSTALL_PREFIX}/include"
  "%ignore ${CMAKE_INSTALL_PREFIX}/lib"
  "%ignore ${CMAKE_INSTALL_PREFIX}/lib64"
  "%ignore ${CMAKE_INSTALL_PREFIX}/share"
  "%ignore ${CMAKE_INSTALL_PREFIX}/share/man"
  "%ignore ${CMAKE_INSTALL_PREFIX}/share/man/man1")

set(CPACK_RPM_USER_FILELIST "${IGNORED_DIRS}")

if(TARGET_COMPONENT STREQUAL "core" OR TARGET_COMPONENT STREQUAL "all")
  set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE ${CMAKE_BINARY_DIR}/postinst)
  set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE ${CMAKE_BINARY_DIR}/prerm)
endif()

if(EXTRA_PACKAGE_DEPENDENCIES)
  set(CPACK_RPM_PACKAGE_REQUIRES "${EXTRA_PACKAGE_DEPENDENCIES}")
endif()

if (WITH_SYSTEM_NODEJS)
  set(CPACK_RPM_PACKAGE_REQUIRES "nodejs >= ${MINIMUM_NODEJS_VERSION}")
endif()

if (WITH_VALGRIND)
  if (CPACK_RPM_PACKAGE_REQUIRES)
    set(CPACK_RPM_PACKAGE_REQUIRES "${CPACK_RPM_PACKAGE_REQUIRES}, valgrind")
  else()
    set(CPACK_RPM_PACKAGE_REQUIRES "valgrind")
  endif()
endif()

message(STATUS "Generating RPM packages")
