source "../emsdk/emsdk_env.sh"
rm -rf pub/web
mkdir -p pub/web
emcc src/*.c \
	-o pub/web/index.html \
	--shell-file shell_minimal.html \
	--preload-file res \
	-Os -Wall ../raylib/src/libraylib.a -DPLATFORM_WEB \
	-I. -I../raylib/src \
	-L. -L../raylib/src \
	-Wno-incompatible-function-pointer-types \
	-s USE_GLFW=3 -s ASYNCIFY
