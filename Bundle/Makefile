PLUGINNAME = Splidje

PLUGINOBJECTS = \
TriangleMaths.o\
QuadrangleDistort.o\
CornerPinPluginFactory.o CornerPinPlugin.o CornerPinPluginInteract.o\
PatchMatchPluginFactory.o PatchMatchPlugin.o PatchMatcher.o\
OffsetMapPluginFactory.o OffsetMapPlugin.o\
TranslateMapPluginFactory.o TranslateMapPlugin.o\
FaceTrackPluginBase.o\
FaceTrackPluginFactory.o FaceTrackPlugin.o FaceTrackPluginInteract.o\
FaceTranslationMapPluginFactory.o FaceTranslationMapPlugin.o FaceTranslationMapPluginInteract.o\
EstimateGradePluginFactory.o EstimateGradePlugin.o\
PlotAlgebraicPluginFactory.o PlotAlgebraicPlugin.o

SRCDIR = ..

RESOURCES = \
$(SRCDIR)/README.md\
$(SRCDIR)/FaceTrack/shape_predictor_68_face_landmarks.dat

include $(SRCDIR)/Makefile.master

VPATH += \
$(SRCDIR)/TriangleMaths\
$(SRCDIR)/QuadrangleDistort\
$(SRCDIR)/CornerPin\
$(SRCDIR)/PatchMatch\
$(SRCDIR)/OffsetMap\
$(SRCDIR)/TranslateMap\
$(SRCDIR)/FaceTrackPluginBase\
$(SRCDIR)/FaceTrack\
$(SRCDIR)/FaceTranslationMap\
$(SRCDIR)/EstimateGrade\
$(SRCDIR)/PlotAlgebraic

CXXFLAGS += `pkg-config --cflags dlib-1 gsl`
LINKFLAGS += `pkg-config --libs dlib-1 gsl`
