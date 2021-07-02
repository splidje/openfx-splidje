PLUGINOBJECTS = FaceTrackPluginFactory.o FaceTrackPlugin.o FaceTrackInteract.o
PLUGINNAME = FaceTrack
RESOURCES = shape_predictor_68_face_landmarks.dat

SRCDIR = ..
include $(SRCDIR)/Makefile.master

CXXFLAGS += `pkg-config --cflags dlib-1`
LINKFLAGS += `pkg-config --libs dlib-1`
