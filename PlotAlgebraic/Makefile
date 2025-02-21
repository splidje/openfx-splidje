PLUGINOBJECTS = PlotAlgebraicPluginFactory.o PlotAlgebraicPlugin.o
PLUGINNAME = PlotAlgebraic
RESOURCES =

SRCDIR = ..
include $(SRCDIR)/Makefile.master

CXXFLAGS += `pkg-config --cflags gmp`
LINKFLAGS += `pkg-config --libs gmp`
LINKFLAGS += "-lsymengine"
