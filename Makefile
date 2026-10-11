PREFIX = /usr/local

stub:
	@echo "Do not run $(MAKE) directly without any arguments. Please refer to the wiki on how to compile Abyssland."

release:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_INSTALL_PREFIX:STRING=${PREFIX} -S . -B ./build
	cmake --build ./build --config Release --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`

debug:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Debug -DTESTS=true -DCMAKE_INSTALL_PREFIX:STRING=${PREFIX} -S . -B ./build
	cmake --build ./build --config Debug --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`

nopch:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_INSTALL_PREFIX:STRING=${PREFIX} -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON -S . -B ./build
	cmake --build ./build --config Release --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`

clear:
	rm -rf build
	rm -f ./protocols/*.h ./protocols/*.c ./protocols/*.cpp ./protocols/*.hpp
	rm -f ./abyssctl/hw-protocols/*.cpp ./abyssctl/hw-protocols/*.hpp

all:
	$(MAKE) clear
	$(MAKE) release

install:
	cmake --install ./build

uninstall:
	xargs rm < ./build/install_manifest.txt

pluginenv:
	@echo -en "$(MAKE) pluginenv has been deprecated.\nPlease run $(MAKE) all && sudo $(MAKE) installheaders\n"
	@exit 1

installheaders:
	@if [ ! -f ./src/version.h ]; then echo -en "You need to run $(MAKE) all first.\n" && exit 1; fi

	# remove previous headers from abysspm's dir
	rm -fr ${PREFIX}/include/abyssland
	mkdir -p ${PREFIX}/include/abyssland
	mkdir -p ${PREFIX}/include/abyssland/protocols
	mkdir -p ${PREFIX}/share/pkgconfig

	cmake --build ./build --config Release --target generate-protocol-headers

	find src -type f \( -name '*.hpp' -o -name '*.h' -o -name '*.inc' \) -print0 | cpio --quiet -0dump ${PREFIX}/include/abyssland
	cp ./protocols/*.h* ${PREFIX}/include/abyssland/protocols
	cp ./build/abyssland.pc ${PREFIX}/share/pkgconfig
	if [ -d /usr/share/pkgconfig ]; then cp ./build/abyssland.pc /usr/share/pkgconfig 2>/dev/null || true; fi

	chmod -R 755 ${PREFIX}/include/abyssland
	chmod 755 ${PREFIX}/share/pkgconfig

man:
	pandoc ./docs/Abyssland.1.rst \
		--standalone \
		--variable=header:"Abyssland User Manual" \
		--variable=date:"${DATE}" \
		--variable=section:1 \
		--from rst \
		--to man > ./docs/Abyssland.1

	pandoc ./docs/abyssctl.1.rst \
		--standalone \
		--variable=header:"abyssctl User Manual" \
		--variable=date:"${DATE}" \
		--variable=section:1 \
		--from rst \
		--to man > ./docs/abyssctl.1

asan:
	@echo -en "!!WARNING!!\nOnly run this in the TTY.\n"
	@pidof Abyssland > /dev/null && echo -ne "Refusing to run with Abyssland running.\n" || echo ""
	@pidof Abyssland > /dev/null && exit 1 || echo ""

	rm -rf ./wayland
	#git reset --hard

	@echo -en "If you want to apply a patch, input its path (leave empty for none):\n"
	@read patchvar; \
	 if [ -n "$$patchvar" ]; then patch -p1 < "$$patchvar" || echo ""; else echo "No patch specified"; fi

	git clone --recursive https://gitlab.freedesktop.org/wayland/wayland
	cd wayland && patch -p1 < ../scripts/waylandStatic.diff && meson setup build --buildtype=debug -Db_sanitize=address -Ddocumentation=false && ninja -C build && cd ..
	cp ./wayland/build/src/libwayland-server.a .
	@echo "Wayland done"

	patch -p1 < ./scripts/abysslandStaticAsan.diff
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Debug -DWITH_ASAN:STRING=True -DUSE_TRACY:STRING=False -DUSE_TRACY_GPU:STRING=False -S . -B ./build
	cmake --build ./build --config Debug --target all
	@echo "Abyssland done"

	ASAN_OPTIONS="detect_odr_violation=0,log_path=asan.log" ABYSSLAND_NO_CRASHREPORTER=1 ./build/Abyssland -c ~/.config/abyss/abyssland.lua

format-check:
	@find src abyssctl abysspm start tests abysstester -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \) \
		! -path "src/render/shaders/Shaders.hpp" \
		! -path "abyssctl/hw-protocols/*" | \
		xargs clang-format --dry-run --Werror

format-fix:
	@find src abyssctl abysspm start tests abysstester -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \) \
		! -path "src/render/shaders/Shaders.hpp" \
		! -path "abyssctl/hw-protocols/*" | \
		xargs clang-format -i

test:
	$(MAKE) debug
	./build/abysstester/abysstester -c abysstester/test.lua -b ./build/Abyssland -p abysstester/plugin/abysstestplugin.so
