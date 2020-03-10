include standalone.mk

TPL_LDFLAGS    = -I/usr/include/lua5.1
TPL_CFLAGS     =
TPL_SO         = parser.so
TPL_PO2LMO     = po2lmo
TPL_PO2LMO_OBJ = po2lmo.o
TPL_LMO_OBJ    = template_lmo.o
TPL_COMMON_OBJ = template_parser.o template_utils.o
TPL_LUALIB_OBJ = template_lualib.o

%.o: %.c
	$(COMPILE) $(TPL_CFLAGS) $(LUA_CFLAGS) $(FPIC) -c -o $@ $<

compile: build-clean $(TPL_COMMON_OBJ) $(TPL_LUALIB_OBJ) $(TPL_LMO_OBJ) $(TPL_PO2LMO_OBJ)
	$(LINK) -o $(TPL_PO2LMO) \
		$(TPL_LMO_OBJ) $(TPL_PO2LMO_OBJ)
	mkdir -p dist$(LUCI_LIBRARYDIR)/template

clean: build-clean

build-clean:
	rm -f *.o $(TPL_SO)

host-compile: build-clean host-clean $(TPL_LMO_OBJ) $(TPL_PO2LMO_OBJ)
	$(LINK) -o $(TPL_PO2LMO) $(TPL_LMO_OBJ) $(TPL_PO2LMO_OBJ)

