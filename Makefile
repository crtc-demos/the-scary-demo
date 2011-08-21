include $(DEVKITPPC)/gamecube_rules

TOOLROOT :=	/home/jules/stuff/gamecube

GXTEXCONV :=	$(DEVKITPPC)/bin/gxtexconv
TEVSL :=	$(TOOLROOT)/tevsl/tevsl
OBJCONVERT :=	$(TOOLROOT)/objconvert/objconvert
BUMPTOOL :=	$(TOOLROOT)/bumpmap-tool/bumpmap
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

OBJS :=		server.o rendertarget.o object.o scene.o light.o \
		utility-texture.o pumpkin.o soft-crtc.o tubes.o \
		spooky-ghost.o timing.o

SHADERS_INC :=  plain-lighting.inc specular-lighting.inc \
		shadow-mapped-lighting.inc shadow-depth.inc \
		specular-shadowed-lighting.inc tube-lighting.inc \
		cube-lighting.inc tunnel-lighting.inc bump-mapping.inc \
		just-texture.inc alpha-texture.inc water-texture.inc \
		pumpkin-lighting.inc beam-front-or-back.inc beam-render.inc \
		remap-texchans.inc

TEXTURES :=	images/snakeskin.tpl.o images/more_stones.tpl.o \
		images/stones_bump.tpl.o images/pumpkin_skin.tpl.o \
		images/gradient.tpl.o

GENERATED_IMAGES :=	images/stones_bump.png

OBJECTS_INC :=	objects/spooky-ghost.inc objects/beam-left.inc \
		objects/beam-right.inc objects/beam-mouth.inc \
		objects/pumpkin.inc objects/softcube.inc

FILEMGR_OBJS :=	filemgr.o
FILEMGR_LIBS := -ldb -lbba -lfat -logc -lm

$(TARGET):	demo.elf
demo.elf:	$(OBJS) $(TEXTURES)

.PHONY:	.depend

all:	$(TARGET)

clean:
	rm -f *.o libcompass/*.o libcompass/*.a $(TARGET)

cleaner: clean
	rm -f libcompass/*.d *.d $(SHADERS_INC) $(TEXTURES) $(OBJECTS_INC) \
	      $(GENERATED_IMAGES)

.PHONY:	images_clean
images_clean:
	rm -f $(TEXTURES) $(GENERATED_IMAGES)

filemgr.elf: $(FILEMGR_OBJS)
	$(LD)  $^ $(LDFLAGS) $(LIBPATHS) $(FILEMGR_LIBS) -o $@

%.o:	%.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

%.d:	%.c $(SHADERS_INC) $(OBJECTS_INC)
	$(CC) $(CFLAGS) $(INCLUDE) -MM $< | ./dirify.sh "$@" > $@

%.d:	%.scf
	$(GXTEXCONV) -d $@ -s $<

%.inc:	%.tev
	$(TEVSL) $< -o $@

images/stones_bump.scf:	images/stones_bump.png

images/stones_bump.png:	images/fake_stone_depth.png
	$(BUMPTOOL) $< -o $@

#%.tpl:	%.scf
#	$(GXTEXCONV) -s $< -o $@

%.tpl.o:	%.tpl
	@echo $(notdir $<)
	@$(bin2o)

objects/spooky-ghost.inc:	objects/spooky-ghost.dae
	$(OBJCONVERT) -c -n spooky_ghost $< -o $@

objects/pumpkin.inc:	objects/carved-pumpkin.dae
	$(OBJCONVERT) -c -yz -i -n pumpkin $< -o $@

objects/beam-left.inc:	objects/beam-left.dae
	$(OBJCONVERT) -c -yz -i -n beam_left $< -o $@

objects/beam-right.inc:	objects/beam-right.dae
	$(OBJCONVERT) -c -yz -i -n beam_right $< -o $@

objects/beam-mouth.inc:	objects/beam-mouth.dae
	$(OBJCONVERT) -c -yz -i -n beam_mouth $< -o $@

objects/tunnel-section.inc:	objects/tunnel-section.dae
	$(OBJCONVERT) -c -t -n tunnel_section $< -o $@

objects/softcube.inc:	objects/softcube.dae
	$(OBJCONVERT) -c -n softcube $< -o $@

#demo.elf:	$(OBJS)
#	$(LD)  $^ $(LDFLAGS) $(LIBPATHS) $(LIBS) -o $@	

exec: $(TARGET)
	psolore -i 192.168.2.251 $(TARGET)

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),cleaner)
include $(OBJS:.o=.d) $(TEXTURES:.tpl.o=.d)
endif
endif

