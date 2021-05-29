src = $(wildcard ./sources/*.cpp *.cpp ./includes/*.cpp)
inc = -I ./includes/
lib = -pthread
target = myweb
$(target) : $(src)
	$(CXX) $(src) -o $@ $(inc) $(lib)