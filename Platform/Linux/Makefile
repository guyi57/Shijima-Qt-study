include ../../common.mk

SOURCES = Platform.cc ActiveWindowObserver.cc KWin.cc PrivateActiveWindowObserver.cc \
	DBus.cc GNOME.cc GNOMEWindowObserverBackend.cc KDEWindowObserverBackend.cc \
	ExtensionFile.cc
QT_LIBS = Core DBus Widgets

define bin2c
python3 -c 'import sys, os; f=open(sys.argv[1], "rb"); d=f.read(); var=os.path.splitext(os.path.basename(sys.argv[2]))[0]; hex_lines=["    \"" + "".join(fr"\x{b:02X}" for b in d[i:i+16]) + "\"" for i in range(0, len(d), 16)]; out=open(sys.argv[2], "w"); out.write("#include <stddef.h>\nstatic const char " + var + "[] = \n" + "\n".join(hex_lines) + "\n;\nstatic const size_t " + var + "_len = " + str(len(d)) + ";\n")' $(1) $(2)
endef


all:: Linux.a

Linux.a: $(OBJECTS)
	rm -f $@
	$(AR) rcs $@ $(OBJECTS)

.work/node_modules/.bin/minify:
	rm -rf .work/node_modules
	mkdir -p .work/node_modules
	if type yarn 2>/dev/null >&2; then \
		cd .work; \
		yarn add --modules-folder node_modules minify; \
		[ -f node_modules/.bin/minify ]; \
	fi

gnome_script.zip: .work/gnome_script
	zip -jr $@ $</*

GNOMEWindowObserverBackend.cc: gnome_script.c
	touch $@

KDEWindowObserverBackend.cc: kwin_script.c
	touch $@

.work/gnome_script: gnome_script/metadata.json gnome_script/extension_minified.js
	rm -rf "$@"
	mkdir -p "$@"
	cp $^ "$@"
	mv "$@/extension_minified.js" "$@/extension.js"

%_minified.js: %.js .work/node_modules/.bin/minify
	(if [ -f .work/node_modules/.bin/minify ]; then \
		.work/node_modules/.bin/minify --js; else cat; fi) < $< > $@

%.c: %_minified.js
	$(call bin2c,$<,$@)

%.c: %.zip
	$(call bin2c,$<,$@)

clean::
	rm -rf .work/gnome_script
	rm -f $(OBJECTS) Linux.a gnome_script.zip kwin_script.c \
		kwin_script_minified.js gnome_script/extension_minified.js \
		gnome_script.c
