
include ../config.make

TOPLEVEL=../..

ZIP=$(PACKAGE)-$(PACKAGE_VERSION)-win32.zip

all: $(ZIP)

$(ZIP): staging
	zip -X -j -r $@ $<

staging:
	mkdir $@
	LC_ALL=C ./cp-with-libs --ldflags="$(LDFLAGS)" "$(TOPLEVEL)/Source/woof.exe" $@

	$(STRIP) $@/*.exe $@/*.dll

	unix2dos --add-bom --newfile "$(TOPLEVEL)/README.md" $@/README.txt
	unix2dos --add-bom --newfile "$(TOPLEVEL)/COPYING" $@/COPYING.txt

clean:
	$(RM) -r -- $(ZIP)
	$(RM) -r -- staging
