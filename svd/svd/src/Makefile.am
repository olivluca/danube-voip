bin_PROGRAMS = svd svd_if

svd_SOURCES = \
svd_cfg.c \
svd_ua.c \
svd_atab.c \
svd_engine_if.c \
svd_server_if.c \
svd.c 
INCLUDES = -Wunused ${SOFIA_SIP_UA_CFLAGS} 
#-I../../libab/ -I../../libconfig/ 
svd_LDADD = -lpthread -L. ${SOFIA_SIP_UA_LIBS} -luci -lucimap -lab 


svd_if_SOURCES = \
svd_engine_if.c \
svd_if.c 
svd_if_INCLUDES = -Wunused 

