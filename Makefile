CPPFLAGS=--std=c++14 -Wall

% : %.cpp
	g++ $< -o $@ $(CPPFLAGS)

task_queue.o: task_queue.cpp task_queue.h
	g++ -c $< -o $@ $(CPPFLAGS)

task_queue_test: task_queue_test.cpp task_queue.o
	g++ $^ -o $@ $(CPPFLAGS)

nonogram: nonogram.cpp task_queue.o
	g++ $^ -o $@ $(CPPFLAGS)
