PLUGINOBJECTS = PlotAlgebraicPluginFactory.o PlotAlgebraicPlugin.o
PLUGINNAME = PlotAlgebraic
RESOURCES =

SRCDIR = ..
include $(SRCDIR)/Makefile.master

CXXFLAGS += `pkg-config --cflags eigen3`
LINKFLAGS += `pkg-config --libs eigen3`
