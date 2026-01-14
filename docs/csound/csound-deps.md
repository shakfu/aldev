# csound dependencies

see: <https://github.com/csound/csound/blob/develop/BUILD.md>

## libsndfile

```sh
cd libsndfile
cmake -B build \
	-DENABLE_EXTERNAL_LIBS=0 \
	-DENABLE_MPEG=0 \
	-DBUILD_TESTING=0 \
	-DBUILD_PROGRAMS=0 \
	-DBUILD_EXAMPLES=0 \
	-DENABLE_BOW_DOCS=0 \
	-DENABLE_PACKAGE_CONFIG=0 \
	-DINSTALL_PKGCONFIG_MODULE=0 \
	-DCMAKE_INSTALL_PREFIX=../sndfile_install
cmake --build build
cmake --build build --target install
cd ..
cmake -B build \
	-DCMAKE_PREFIX_PATH="$PWD/sndfile_install" \
	-DCMAKE_INSTALL_PREFIX="$PWD/csound_install" \
	-DCS_FRAMEWORK_DEST="$PWD/csound_install"
cmake --build build --config Release
cmake --build build --target install
```