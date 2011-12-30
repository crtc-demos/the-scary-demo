include $(DEVKITPPC)/gamecube_rules

TOOLROOT :=	/home/jules/stuff/gamecube

GXTEXCONV :=	$(DEVKITPPC)/bin/gxtexconv
TEVSL :=	$(TOOLROOT)/tevsl/tevsl
OBJCONVERT :=	$(TOOLROOT)/objconvert/objconvert
BUMPTOOL :=	$(TOOLROOT)/bumpmap-tool/bumpmap
TARGET :=	demo.dol
CFLAGS =	-g -O2 -Wall -std=gnu99 $(MACHDEP) $(INCLUDE)
LDFLAGS =	-g $(MACHDEP) -Wl,-Map,$(notdir $@).map
LIBS :=		-ldb -lbba -lext2fs -lmodplay -laesnd -logc -lm
#LIBS :=		-ldb -lbba -lext2fs -lmad -lasnd -logc -lm
INCLUDE :=	-I$(LIBOGC_INC)
LIBPATHS :=	-L$(LIBOGC_LIB)
LD :=		$(CC)

COMPASS_OBJ :=	libcompass/restrip.o libcompass/perlin.o \
		libcompass/perlin-3d.o libcompass/loader.o \
		libcompass/geosphere.o libcompass/skybox.o \
		libcompass/torus.o libcompass/tube.o libcompass/cube.o

OBJS :=		sintab.o backbuffer.o list.o server.o rendertarget.o object.o \
		skybox.o reduce-cubemap.o tracking-obj.o cam-path.o shader.o \
		scene.o screenspace.o light.o lighting-texture.o \
		utility-texture.o spline.o shadow.o world.o pumpkin.o \
		soft-crtc.o tubes.o ghost-obj.o reflection.o spooky-ghost.o \
		bloom.o glass.o parallax-mapping.o tentacles.o timing.o

SHADERS_INC :=  plain-lighting.inc specular-lighting.inc \
		shadow-mapped-lighting.inc shadow-depth.inc \
		specular-shadowed-lighting.inc tube-lighting.inc \
		cube-lighting.inc tunnel-lighting.inc bump-mapping.inc \
		just-texture.inc alpha-texture.inc water-texture.inc \
		pumpkin-lighting.inc beam-front-or-back.inc beam-render.inc \
		remap-texchans.inc beam-composite.inc beam-z-render.inc \
		bloom-lighting.inc bloom-composite.inc bloom-gaussian.inc \
		bloom-gaussian2.inc null.inc plain-texturing.inc \
		alpha-test.inc refraction.inc do-refraction.inc \
		glass-postpass.inc parallax.inc parallax-lit.inc \
		parallax-lit-phase1.inc parallax-lit-phase2.inc \
		parallax-lit-phase3.inc channelsplit.inc skybox.inc \
		fancy-envmap.inc embm.inc

TEXTURES :=	images/snakeskin.tpl.o images/more_stones.tpl.o \
		images/stones_bump.tpl.o images/pumpkin_skin.tpl.o \
		images/gradient.tpl.o images/spiderweb.tpl.o \
		images/primary.tpl.o images/mighty_zebu.tpl.o \
		images/fake_stone_depth.tpl.o images/grid.tpl.o \
		images/height.tpl.o images/height_bump.tpl.o \
		images/sky.tpl.o images/skull_tangentmap_gx.tpl.o

MODS :=		back_to_my_roots.mod.o

GENERATED_IMAGES :=	images/stones_bump.png images/skull_tangentmap_gx.png

OBJECTS_INC :=	objects/spooky-ghost.inc objects/beam-left.inc \
		objects/beam-right.inc objects/beam-mouth.inc \
		objects/pumpkin.inc objects/softcube.inc \
		objects/plane.inc objects/textured-cube.inc \
		objects/tentacles.inc objects/cross-cube.inc \
		objects/scary-skull-2.inc

FILEMGR_OBJS :=	filemgr.o
FILEMGR_LIBS := -ldb -lbba -lfat -logc -lm

$(TARGET):	demo.elf
demo.elf:	$(OBJS) $(TEXTURES) $(MODS)

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

images/stones_bump.png:	images/fake_stone_depth.png
	$(BUMPTOOL) $< -o $@

images/height_bump.png: images/height.png
	$(BUMPTOOL) -i $< -o $@

images/skull_tangentmap_gx.png: images/skull_tangentmap.png
	$(BUMPTOOL) -b $< -o $@

#%.tpl:	%.scf
#	$(GXTEXCONV) -s $< -o $@

%.tpl.o:	%.tpl
	@echo $(notdir $<)
	@$(bin2o)

%.mod.o:	%.mod
	@echo $(notdir $<)
	@$(bin2o)

#%.xm.o:	%.xm
#	@echo $(notdir $<)
#	@$(bin2o)

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

objects/knot.inc:	objects/knot.dae
	$(OBJCONVERT) -c -yz -i -n knot $< -o $@

objects/plane.inc:	objects/plane.dae
	$(OBJCONVERT) -c -t -n plane $< -o $@

objects/textured-cube.inc:	objects/textured-cube.dae
	$(OBJCONVERT) -c -t -n tex_cube $< -o $@

objects/tentacles.inc:	objects/tentacles.dae
	$(OBJCONVERT) -c -yz -i -n tentacles $< -o $@

objects/cross-cube.inc:	objects/cross-cube.dae
	$(OBJCONVERT) -c -yz -i -n cross_cube $< -o $@

objects/scary-skull-2.inc:	objects/scary-skull-2.dae
	$(OBJCONVERT) -c -t -yz -i -n scary_skull $< -o $@

objects/rib.inc:	objects/rib.dae
	$(OBJCONVERT) -c -yz -i -n rib $< -o $@

objects/rib-lo.inc:	objects/rib-lo.dae
	$(OBJCONVERT) -c -yz -i -n rib_lo $< -o $@

#demo.elf:	$(OBJS)
#	$(LD)  $^ $(LDFLAGS) $(LIBPATHS) $(LIBS) -o $@	

exec: $(TARGET)
	psolore -i 192.168.2.251 $(TARGET)

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),cleaner)
include $(OBJS:.o=.d) $(TEXTURES:.tpl.o=.d)
endif
endif

