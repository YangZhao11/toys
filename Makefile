CPPFLAGS=--std=c++14 -Wall

% : %.cpp
	g++ $< -o $@ $(CPPFLAGS)

%.o : %.cpp %.h
	g++ -c $< -o $@ $(CPPFLAGS)

%_test: %_test.cpp %.o
	g++ $^ -o $@ $(CPPFLAGS)

nonogram: nonogram.cpp nonogram_solver.o task_queue.o neuronet.o
	g++ $^ -o $@ $(CPPFLAGS)
