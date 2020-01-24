
include ../config.make

TOPLEVEL=../..
DOCS=$(TOPLEVEL)/docs
EXAMPLES=$(TOPLEVEL)/examples

ZIP=$(PACKAGE)-$(PACKAGE_VERSION)-win32.zip

all: $(ZIP)

$(ZIP): staging
	cd $< && zip -X -r ../$@ *

staging:
	mkdir -p $@/docs $@/examples
	LC_ALL=C ./cp-with-libs --ldflags="$(LDFLAGS)" "$(TOPLEVEL)/Source/woof.exe" $@

	$(STRIP) $@/*.exe $@/*.dll

	unix2dos --add-bom --newfile "$(TOPLEVEL)/README.md" $@/README.txt
	unix2dos --add-bom --newfile "$(TOPLEVEL)/COPYING" $@/COPYING.txt

	for f in $(DOCS)/*.txt $(DOCS)/*.html; do                            \
		unix2dos --add-bom --newfile $$f $@/docs/$$(basename $$f);   \
	done

	for f in $(EXAMPLES)/*.txt $(EXAMPLES)/*.wad $(EXAMPLES)/*.deh; do   \
		cp $$f $@/examples/$$(basename $$f);                         \
	done

clean:
	$(RM) -r -- $(ZIP)
	$(RM) -r -- staging
