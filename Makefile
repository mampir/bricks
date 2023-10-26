CFLAGS = -g -Wall
PACKAGES = sdl2 SDL2_mixer

ifeq ($(OS), Windows_NT)
	LIBS += -lopengl32 -mwindows
else
	PACKAGES += gl
endif

LIBS += $(shell pkg-config --cflags --libs $(PACKAGES))

bricks: src/bricks.cpp src/vectors.cpp
	g++ $(CFLAGS) -o $@ $< $(LIBS)
clean:
	$(RM) bricks
