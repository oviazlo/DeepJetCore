
# location of the Boost Python include files and library
PYTHON_INCLUDE = `python-config --includes`
PYTHON_LIB=`python-config --libs`


LINUXADD= #-Wl --export-dynamic
ROOTSTUFF=`root-config --libs --glibs --ldflags`
ROOTCFLAGS=`root-config  --cflags`
CPP_FILES := $(wildcard src/*.cpp)
OBJ_FILES := $(addprefix obj/,$(notdir $(CPP_FILES:.cpp=.o)))

BINS := $(patsubst bin/%.cpp, %, $(wildcard bin/*.cpp))

MODULES := $(wildcard src/*.C)
MODULES_OBJ_FILES := $(addprefix ./,$(notdir $(MODULES:.C=.o)))
MODULES_SHARED_LIBS := $(addprefix ./,$(notdir $(MODULES:.C=.so)))



all: $(MODULES_SHARED_LIBS) $(patsubst to_bin/%.cpp, %, $(wildcard to_bin/*.cpp)) classdict.so

classdict.cxx: src/LinkDef.h
	rootcling -v4 -f $@  -rmf classdict.rootmap -rml classdict.so  $^

classdict.so: classdict.cxx
	g++ $(ROOTCFLAGS) $(ROOTSTUFF) -std=c++11 -fPIC -shared -o classdict.so classdict.cxx 	


%: to_bin/%.cpp libdeepjetcorehelpers.so classdict.cxx
	g++ $(CFLAGS) -I./interface $(LINUXADD) $(PYTHON_INCLUDE) $< -L. -ldeepjetcorehelpers -lquicklz  $(PYTHON_LIB) -lboost_python -lboost_numpy $(ROOTCFLAGS) $(ROOTSTUFF)   -o  $@  
	mv $@ ../bin/

#helpers
libquicklz.so:
	gcc -shared -O2 -fPIC src/quicklz.c -o libquicklz.so

obj/%.o: src/%.cpp
	g++ $(CFLAGS) $(PYTHON_INCLUDE) $(ROOTCFLAGS) -I./interface -O2 -fPIC -c -o $@ $< 

#pack helpers in lib
libdeepjetcorehelpers.so: $(OBJ_FILES) 
	g++ -o $@ -shared -fPIC  $(LINUXADD) -fPIC  obj/*.o $(ROOTSTUFF) $(PYTHON_LIB) -lboost_python -lboost_numpy 

%.so: %.o libdeepjetcorehelpers.so libquicklz.so
	g++  -o $(@) -shared -fPIC  $(LINUXADD) $<   $(ROOTSTUFF) -L./ -lquicklz  $(PYTHON_LIB) -lboost_python -lboost_numpy -L./ -ldeepjetcorehelpers 

%.o: src/%.C 
	g++   $(ROOTCFLAGS) -O2 -I./interface $(PYTHON_INCLUDE) -fPIC -c -o $(@) $<



clean: 
	rm -f $(OBJ_FILES) $(SHARED_LIBS) $(MODULES_SHARED_LIBS) $(MODULES_OBJ_FILES) libdeepjetcorehelpers.so libquicklz.so classdict.so classdict.cxx classdict.rootmap classdict_rdict.pcm
