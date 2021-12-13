PLUGINNAME = AJPTechnical

PLUGINOBJECTS = \
PatchMatchPluginFactory.o PatchMatchPlugin.o PatchMatcher.o\
OffsetMapPluginFactory.o OffsetMapPlugin.o\
FaceTrackPluginBase.o\
FaceTrackPluginFactory.o FaceTrackPlugin.o FaceTrackPluginInteract.o\
TriangleMaths.o\
FaceTranslationMapPluginFactory.o FaceTranslationMapPlugin.o FaceTranslationMapPluginInteract.o\
QuadrangleDistort.o\
CornerPinPluginFactory.o CornerPinPlugin.o CornerPinPluginInteract.o\
EstimateOffsetMapPluginFactory.o EstimateOffsetMapPlugin.o EstimateOffsetMapPluginInteract.o
# TranslateMapPluginFactory.o TranslateMapPlugin.o\
# MeshWarp.o\

SRCDIR = ..

RESOURCES = \
$(SRCDIR)/README.md\
$(SRCDIR)/FaceTrack/shape_predictor_68_face_landmarks.dat

include $(SRCDIR)/Makefile.master

VPATH += \
$(SRCDIR)/PatchMatch\
$(SRCDIR)/OffsetMap\
$(SRCDIR)/FaceTrackPluginBase\
$(SRCDIR)/FaceTrack\
$(SRCDIR)/TriangleMaths\
$(SRCDIR)/MeshWarp\
$(SRCDIR)/FaceTranslationMap\
$(SRCDIR)/QuadrangleDistort\
$(SRCDIR)/TranslateMap\
$(SRCDIR)/CornerPin\
$(SRCDIR)/EstimateOffsetMap

CXXFLAGS += `pkg-config --cflags dlib-1`
LINKFLAGS += `pkg-config --libs dlib-1`
