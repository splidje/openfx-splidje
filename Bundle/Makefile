PLUGINNAME = AJPTechnical

PLUGINOBJECTS = \
MeshWarp.o\
PatchMatchPluginFactory.o PatchMatchPlugin.o PatchMatcher.o\
OffsetMapPluginFactory.o OffsetMapPlugin.o\
FaceTrackPluginFactory.o FaceTrackPlugin.o FaceTrackInteract.o\
QuadrangleDistort.o\
TranslateMapPluginFactory.o TranslateMapPlugin.o\
CornerPinPluginFactory.o CornerPinPlugin.o CornerPinPluginInteract.o

SRCDIR = ..

RESOURCES = \
$(SRCDIR)/README.md\
$(SRCDIR)/FaceTrack/shape_predictor_68_face_landmarks.dat

include $(SRCDIR)/Makefile.master

VPATH += \
$(SRCDIR)/MeshWarp\
$(SRCDIR)/PatchMatch\
$(SRCDIR)/OffsetMap\
$(SRCDIR)/FaceTrack\
$(SRCDIR)/QuadrangleDistort\
$(SRCDIR)/TranslateMap\
$(SRCDIR)/CornerPin

CXXFLAGS += `pkg-config --cflags dlib-1`
LINKFLAGS += `pkg-config --libs dlib-1`
