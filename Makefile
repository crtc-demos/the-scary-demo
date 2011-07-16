include $(DEVKITPPC)/gamecube_rules

TEVSL :=	/home/jules/stuff/gamecube/tevsl/tevsl
TARGET :=	demo.dol
CFLAGS =	-g -O2 -Wall $(MACHDEP) $(INCLUDE)
LDFLAGS =	-g $(MACHDEP) -Wl,-Map,$(notdir $@).map
LIBS :=		-ldb -lbba -logc -lm
INCLUDE :=	-I$(LIBOGC_INC)
LIBPATHS :=	-L$(LIBOGC_LIB)
LD :=		$(CC)

COMPASS_OBJ :=	libcompass/restrip.o libcompass/perlin.o \
		libcompass/perlin-3d.o libcompass/loader.o \
		libcompass/geosphere.o libcompass/skybox.o \
		libcompass/torus.o libcompass/tube.o libcompass/cube.o

OBJS :=		server.o soft-crtc.o timing.o
SHADERS_INC :=  plain-lighting.inc specular-lighting.inc \
		shadow-mapped-lighting.inc shadow-depth.inc \
		specular-shadowed-lighting.inc

FILEMGR_OBJS :=	filemgr.o
FILEMGR_LIBS := -ldb -lbba -lfat -logc -lm

$(TARGET):	demo.elf
demo.elf:	$(OBJS)

.PHONY:	.depend

all:	$(TARGET)

clean:
	rm -f *.o libcompass/*.o libcompass/*.a $(TARGET)

cleaner: clean
	rm -f libcompass/*.d *.d $(SHADERS_INC)

filemgr.elf: $(FILEMGR_OBJS)
	$(LD)  $^ $(LDFLAGS) $(LIBPATHS) $(FILEMGR_LIBS) -o $@

%.o:	%.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

%.d:	%.c $(SHADERS_INC)
	$(CC) $(CFLAGS) $(INCLUDE) -MM $< | ./dirify.sh "$@" > $@

%.inc:	%.tev
	$(TEVSL) $< -o $@

#demo.elf:	$(OBJS)
#	$(LD)  $^ $(LDFLAGS) $(LIBPATHS) $(LIBS) -o $@	

exec: $(TARGET)
	psolore -i 192.168.2.251 $(TARGET)

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),cleaner)
include $(OBJS:.o=.d)
endif
endif

