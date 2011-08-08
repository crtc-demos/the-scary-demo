include $(DEVKITPPC)/gamecube_rules

GXTEXCONV :=	$(DEVKITPPC)/bin/gxtexconv
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

OBJS :=		server.o rendertarget.o object.o scene.o light.o pumpkin.o \
		soft-crtc.o tubes.o spooky-ghost.o timing.o
SHADERS_INC :=  plain-lighting.inc specular-lighting.inc \
		shadow-mapped-lighting.inc shadow-depth.inc \
		specular-shadowed-lighting.inc tube-lighting.inc \
		cube-lighting.inc tunnel-lighting.inc bump-mapping.inc \
		just-texture.inc alpha-texture.inc water-texture.inc \
		pumpkin-lighting.inc

TEXTURES :=	images/snakeskin.tpl.o images/more_stones.tpl.o \
		images/stones_bump.tpl.o images/pumpkin_skin.tpl.o

FILEMGR_OBJS :=	filemgr.o
FILEMGR_LIBS := -ldb -lbba -lfat -logc -lm

$(TARGET):	demo.elf
demo.elf:	$(OBJS) $(TEXTURES)

.PHONY:	.depend

all:	$(TARGET)

clean:
	rm -f *.o libcompass/*.o libcompass/*.a $(TARGET)

cleaner: clean $(TEXTURES)
	rm -f libcompass/*.d *.d $(SHADERS_INC)

.PHONY: images_clean
images_clean: 
	rm -f $(TEXTURES)

filemgr.elf: $(FILEMGR_OBJS)
	$(LD)  $^ $(LDFLAGS) $(LIBPATHS) $(FILEMGR_LIBS) -o $@

%.o:	%.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

%.d:	%.c $(SHADERS_INC)
	$(CC) $(CFLAGS) $(INCLUDE) -MM $< | ./dirify.sh "$@" > $@

%.inc:	%.tev
	$(TEVSL) $< -o $@

#%.tpl:	%.scf
#	$(GXTEXCONV) -s $< -o $@

%.tpl.o:	%.tpl
	@echo $(notdir $<)
	@$(bin2o)

#demo.elf:	$(OBJS)
#	$(LD)  $^ $(LDFLAGS) $(LIBPATHS) $(LIBS) -o $@	

exec: $(TARGET)
	psolore -i 192.168.2.251 $(TARGET)

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),cleaner)
include $(OBJS:.o=.d)
endif
endif

