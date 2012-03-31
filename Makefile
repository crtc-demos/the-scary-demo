# Dirty configurable bits, uncomment as appropriate

CONSOLE := gamecube
#CONSOLE := wii

FILESYSTEM := -lfat
#FILESYSTEM := -lext2fs

#REFRESHRATE := -DPAL60
REFRESHRATE :=

#NETWORKING := -DNETWORKING
NETWORKING :=

ifeq ($(CONSOLE),gamecube)
include $(DEVKITPPC)/gamecube_rules
else
include $(DEVKITPPC)/wii_rules
endif

TOOLROOT :=	/home/jules/stuff/gamecube

GXTEXCONV :=	$(DEVKITPPC)/bin/gxtexconv
TEVSL :=	$(TOOLROOT)/tevsl/tevsl
OBJCONVERT :=	$(TOOLROOT)/objconvert/objconvert
BUMPTOOL :=	$(TOOLROOT)/bumpmap-tool/bumpmap
TARGET :=	demo.dol
CFLAGS =	-g -O3 -Wno-unused-value -Werror -Wall -std=gnu99 $(REFRESHRATE) $(MACHDEP) $(NETWORKING) $(INCLUDE)
ifeq ($(CONSOLE),wii)
CFLAGS +=	-DWII
endif
ifeq ($(FILESYSTEM),-lext2fs)
CFLAGS +=	-DUSE_EXT2FS
endif
LDFLAGS =	-g $(MACHDEP) -Wl,-Map,$(notdir $@).map
ifeq ($(CONSOLE),gamecube)
LIBS :=		-ldb -lbba $(FILESYSTEM) -lmodplay -laesnd -logc -lm
else
LIBS :=		-ldb $(FILESYSTEM) -lmodplay -laesnd -logc -lm
endif
INCLUDE :=	-I$(LIBOGC_INC)
LIBPATHS :=	-L$(LIBOGC_LIB)
LD :=		$(CC)

COMPASS_OBJ :=	libcompass/restrip.o libcompass/perlin.o \
		libcompass/perlin-3d.o libcompass/loader.o \
		libcompass/geosphere.o libcompass/skybox.o \
		libcompass/torus.o libcompass/tube.o libcompass/cube.o

OBJS :=		sintab.o backbuffer.o list.o server.o matrixutil.o \
		rendertarget.o object.o skybox.o reduce-cubemap.o \
		tracking-obj.o cam-path.o shader.o scene.o screenspace.o \
		light.o lighting-texture.o utility-texture.o spline.o \
		shadow.o world.o pumpkin.o soft-crtc.o tubes.o ghost-obj.o \
		reflection.o spooky-ghost.o bloom.o glass.o \
		parallax-mapping.o tentacles.o greets.o loading.o adpcm.o \
		timing.o

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
		fancy-envmap.inc embm.inc skybox-mixcol.inc \
		column-texture.inc fog-texture.inc ghastly-lighting.inc \
		tile-grid.inc

TEXTURES :=	images/more_stones.tpl.o images/stones_bump.tpl.o \
		images/snakeskin.tpl.o images/pumpkin_skin.tpl.o \
		images/mighty_zebu.tpl.o images/gradient.tpl.o \
		images/sky.tpl.o images/skull_tangentmap_gx.tpl.o \
		images/snakytextures.tpl.o images/font.tpl.o \
		images/loading.tpl.o

MODS :=		back_to_my_roots.mod.o

GENERATED_IMAGES :=	images/stones_bump.png images/skull_tangentmap_gx.png

OBJECTS_INC :=	objects/spooky-ghost.inc objects/beam-left.inc \
		objects/beam-right.inc objects/beam-mouth.inc \
		objects/pumpkin.inc objects/carved-pumpkin.inc \
		objects/softcube.inc objects/plane.inc \
		objects/textured-cube.inc objects/tentacles.inc \
		objects/cross-cube.inc objects/scary-skull-2.inc \
		objects/cobra.inc objects/column.inc objects/blobby-thing.inc

TEXTURE_HEADERS := $(TEXTURES:.tpl.o=.h)

TEXTURE_TPL_HEADERS := $(foreach texture,$(TEXTURES:.tpl.o=_tpl.h),$(subst images/,,$(texture)))

FILEMGR_OBJS :=	filemgr.o
FILEMGR_LIBS := -ldb -lbba -lfat -logc -lm

$(TARGET):	demo.elf
demo.elf:	$(OBJS) $(TEXTURES) $(MODS)

.PHONY:	.depend

.PRECIOUS: %.inc

all:	$(TARGET)

clean:
	rm -f *.o libcompass/*.o libcompass/*.a $(TARGET)

cleaner: clean
	rm -f libcompass/*.d *.d images/*.d images/*.tpl $(SHADERS_INC) \
	      $(TEXTURES) $(OBJECTS_INC) $(GENERATED_IMAGES) \
	      $(TEXTURE_HEADERS) $(TEXTURE_TPL_HEADERS)

.PHONY:	images_clean
images_clean:
	rm -f $(TEXTURES) $(GENERATED_IMAGES)

filemgr.elf: $(FILEMGR_OBJS)
	$(LD)  $^ $(LDFLAGS) $(LIBPATHS) $(FILEMGR_LIBS) -o $@

%.o:	%.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

%.d:	%.c $(SHADERS_INC) $(OBJECTS_INC) $(TEXTURE_HEADERS) $(TEXTURE_TPL_HEADERS)
	$(CC) $(CFLAGS) $(INCLUDE) -MM $< | ./dirify.sh "$@" > "$@"

%.d:	%.scf
	$(GXTEXCONV) -d $@ -s $<

%.inc:	%.tev
	$(TEVSL) $< -o $@

images/stones_bump.scf: images/stones_bump.png

images/stones_bump.png:	images/fake_stone_depth.png
	$(BUMPTOOL) $< -o $@

images/height_bump.png: images/height.png
	$(BUMPTOOL) -i $< -o $@

images/skull_tangentmap_gx.scf: images/skull_tangentmap_gx.png

images/skull_tangentmap_gx.png: images/skull_tangentmap.png
	$(BUMPTOOL) -b $< -o $@

images/snaketanmap.png: images/snakenorm2.png
	$(BUMPTOOL) -b $< -o $@

images/snakytextures.d: images/snaketanmap.png

define tpl_o_template
$(1) $(subst .tpl.o,_tpl.h,$(notdir $(1))): $(subst .tpl.o,.tpl,$(1))
	@echo $(notdir $$<)
	@$$(bin2o)
endef

define tpl_template
$(1) $(subst .tpl,.h,$(1)): $(subst .tpl,.scf,$(1))
	$(GXTEXCONV) -s $$< -o $$@
endef

# Rules to make images/*.tpl.o and *_tpl.h files.
$(foreach tpl_o,$(TEXTURES),$(eval $(call tpl_o_template,$(tpl_o))))

# Rules to make images/*.tpl and images/*.h files.
$(foreach tpl,$(TEXTURES:.tpl.o=.tpl),$(eval $(call tpl_template,$(tpl))))

%.mod.o:	%.mod
	@echo $(notdir $<)
	@$(bin2o)

#%.xm.o:	%.xm
#	@echo $(notdir $<)
#	@$(bin2o)

objects/spooky-ghost.inc:	objects/spooky-ghost.dae
	$(OBJCONVERT) -c -n spooky_ghost $< -o $@

objects/pumpkin.inc:	objects/pumpkin.dae
	$(OBJCONVERT) -c -yz -i -n pumpkin $< -o $@

objects/carved-pumpkin.inc:	objects/carved-pumpkin.dae
	$(OBJCONVERT) -c -yz -i -n carved_pumpkin $< -o $@

objects/beam-left.inc:	objects/beam-left.dae
	$(OBJCONVERT) -c -yz -i -n beam_left $< -o $@

objects/beam-right.inc:	objects/beam-right.dae
	$(OBJCONVERT) -c -yz -i -n beam_right $< -o $@

objects/beam-mouth.inc:	objects/beam-mouth.dae
	$(OBJCONVERT) -c -yz -i -n beam_mouth $< -o $@

objects/tunnel-section.inc:	objects/tunnel-section.dae
	$(OBJCONVERT) -c -yz -i -t -n tunnel_section $< -o $@

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

objects/cobra.inc:	objects/cobra9.dae
	$(OBJCONVERT) -c -yz -i -t -s Cube_003-mesh -n cobra $< -o $@

objects/column.inc:	objects/column.dae
	$(OBJCONVERT) -c -yz -i -s column-mesh -n column $< -o $@

objects/blobby-thing.inc:	objects/blobby-thing.dae
	$(OBJCONVERT) -c -yz -i -n blobby_thing $< -o $@

#demo.elf:	$(OBJS)
#	$(LD)  $^ $(LDFLAGS) $(LIBPATHS) $(LIBS) -o $@	

exec: $(TARGET)
	psolore -i 192.168.2.251 $(TARGET)

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),cleaner)
include $(OBJS:.o=.d) $(TEXTURES:.tpl.o=.d)
endif
endif

