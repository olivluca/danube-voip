bin_PROGRAMS = svd svd_if

svd_SOURCES = \
svd_cfg.c \
svd_ua.c \
svd_atab.c \
svd_engine_if.c \
svd_server_if.c \
svd.c 
INCLUDES = -Wunused -I../../libab/ -I../../libconfig/ 
svd_LDADD = -lpthread -L. -L../../libab/ -L../../libconfig/ -lsofia-sip-ua -lconfig -lab
svd_LDFLAGS = -Wl,-S -static


svd_if_SOURCES = \
svd_engine_if.c \
svd_if.c 
#svd_if_INCLUDES = -Wunused 
svd_if_LDFLAGS = -Wl,-S -static

