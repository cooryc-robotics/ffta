cmake_minimum_required(VERSION 3.13)
project(gba_web C)

# mGBA como librería, con los núcleos de GBA y de Game Boy / Color, sin extras.
set(LIBMGBA_ONLY ON CACHE BOOL "" FORCE)
set(M_CORE_GB ON CACHE BOOL "" FORCE)
set(M_CORE_GBA ON CACHE BOOL "" FORCE)
set(ENABLE_SCRIPTING OFF CACHE BOOL "" FORCE)

# Sin hilos en WebAssembly (ver el parche en src/mgba/CMakeLists.txt).
option(NO_THREADS "Compilar mGBA sin hilos" OFF)
if(EMSCRIPTEN OR NO_THREADS)
	set(MGBA_NO_THREADS ON)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDISABLE_THREADING")
endif()

# mGBA compila en C11 estricto y activa _GNU_SOURCE solo en Linux; sin él,
# Emscripten no declara strdup/strlcpy/PATH_MAX.
if(EMSCRIPTEN)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE")
endif()
add_subdirectory(src/mgba EXCLUDE_FROM_ALL)

# El wrapper tiene que compilarse con EXACTAMENTE las mismas macros que
# mGBA (cambian el tamaño de sus estructuras), y mGBA no las comparte.
get_target_property(MGBA_DEFS mgba COMPILE_DEFINITIONS)
set(CMAKE_C_STANDARD 11)

if(EMSCRIPTEN)
	# WebAssembly: web/gb.js + web/gb.wasm
	add_executable(gb src/wasm_gba.c)
	target_link_libraries(gb mgba)
	target_compile_definitions(gb PRIVATE ${MGBA_DEFS})
	set_target_properties(gb PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/web
		SUFFIX ".js")
	target_link_options(gb PRIVATE
		-O3
		--no-entry
		"SHELL:--post-js ${CMAKE_SOURCE_DIR}/src/post.js"
		-sMODULARIZE=1
		-sEXPORT_NAME=createGBModule
		-sENVIRONMENT=web
		-sALLOW_MEMORY_GROWTH=1
		-sEXPORTED_RUNTIME_METHODS=['HEAPU8'])
else()
	# Prueba nativa (sin navegador): ./native_test
	add_executable(native_test src/wasm_gba.c test/native_test.c)
	target_link_libraries(native_test mgba m)
	target_compile_definitions(native_test PRIVATE ${MGBA_DEFS})
endif()
