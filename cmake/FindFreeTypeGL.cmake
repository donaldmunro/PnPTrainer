#
# FindFreeTypeGL - Try to find FreeTypeGL library and include path.
# Once done this will define
#
# FREETYPEGL_FOUND
# FREETYPEGL_INCLUDE_PATH
# FREETYPEGL_LIBRARY
#

IF (WIN32)
	FIND_PATH( FREETYPEGL_INCLUDE_PATH freetype-gl/freetype-gl.h
		$ENV{PROGRAMFILES}/freetype-gl/include
   	${WINDOWS_INC}/SOIL2/SOIL2.h ${WINDOWS_INC}/SOIL2.h
		${FREETYPEGL_ROOT_DIR}/include
		DOC "The directory where freetype-gl/freetype-gl.h resides")

    FIND_LIBRARY( FREETYPEGL_LIBRARY
        NAMES freetype-gl freetype-gl32 freetype-gl32s
        PATHS
        $ENV{PROGRAMFILES}/freetype-gl/lib
        ${FREETYPEGL_ROOT_DIR}/lib
        DOC "The FreeType-GL library")
ELSE (WIN32)
	FIND_PATH( FREETYPEGL_INCLUDE_PATH freetype-gl/freetype-gl.h
		/usr/include
		/usr/local/include
		/sw/include
		/opt/local/include
		${FREETYPEGL_ROOT_DIR}/include
		DOC "The directory where freetype-gl/freetype-gl.h resides")

	# Prefer the static library.
	FIND_LIBRARY( FREETYPEGL_LIBRARY
		NAMES libfreetype-gl.a
		PATHS
		/usr/lib64
		/usr/lib
		/usr/local/lib64
		/usr/local/lib
		/sw/lib
		/opt/local/lib
		${FREETYPEGL_ROOT_DIR}/lib
		DOC "The FreeType-GL library")
ENDIF (WIN32)

SET(FREETYPEGL_FOUND "NO")
IF (FREETYPEGL_INCLUDE_PATH AND FREETYPEGL_LIBRARY)
	SET(FREETYPEGL_LIBRARIES ${FREETYPEGL_LIBRARY})
	SET(FREETYPEGL_FOUND "YES")
ENDIF (FREETYPEGL_INCLUDE_PATH AND FREETYPEGL_LIBRARY)

INCLUDE(${CMAKE_ROOT}/Modules/FindPackageHandleStandardArgs.cmake)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(FREETYPEGL DEFAULT_MSG FREETYPEGL_LIBRARY)

