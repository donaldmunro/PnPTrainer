#
# FindSOIL2 - Try to find SOIL2 (https://bitbucket.org/SpartanJ/soil2)
# Once done this will define
#
# SOIL2_FOUND
# SOIL2_INCLUDE_PATH
# SOIL2_LIBRARY

IF (WIN32)
	FIND_PATH( SOIL2_INCLUDE_PATH include/SOIL2.h soil2/SOIL2.h soil2/src/SOIL2/SOIL2.h
		$ENV{PROGRAMFILES}/include/SOIL2.h $ENV{PROGRAMFILES}/soil2/src/SOIL2/SOIL2.h
		${SOIL2_ROOT_DIR}/src/SOIL2/SOIL2.h ${WINDOWS_INC}/SOIL2/SOIL2.h ${WINDOWS_INC}/SOIL2.h
		DOC "The directory where SOIL2.h resides")

    FIND_LIBRARY( SOIL2_LIBRARY
        NAMES soil2 soikl232 soil232s
        PATHS
        lib soil2/lib/windows
        $ENV{PROGRAMFILES}/soil2/lib/windows
        ${WINDOWS_LIB}
        DOC "The SOIL2 library")
ELSE (WIN32)
	FIND_PATH( SOIL2_INCLUDE_PATH SOIL2.h
		/usr/include
		/usr/include/soil2 /usr/include/SOIL2
		/usr/local/include
		/sw/include
		/opt/local/include
      soil2/src/SOIL2
		DOC "The directory where SOIL2.h resides")

	FIND_LIBRARY( SOIL2_LIBRARY
		NAMES libsoil2.a
		PATHS
		/usr/lib64
		/usr/lib
		/usr/local/lib64
		/usr/local/lib
		/sw/lib
		/opt/local/lib
		soil2/lib/linux/
		DOC "The SOIL2 library")
ENDIF (WIN32)

SET(SOIL2_FOUND "NO")
IF (SOIL2_INCLUDE_PATH AND SOIL2_LIBRARY)
	SET(SOIL2_LIBRARIES ${SOIL2_LIBRARY})
	SET(SOIL2_FOUND "YES")
ENDIF (SOIL2_INCLUDE_PATH AND SOIL2_LIBRARY)

INCLUDE(${CMAKE_ROOT}/Modules/FindPackageHandleStandardArgs.cmake)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SOIL2 DEFAULT_MSG SOIL2_LIBRARY)

