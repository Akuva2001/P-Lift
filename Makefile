all: lift input
lift:
	c++ -fsanitize=address -g Lift-pthread.cpp -pthread -o a.out
input:
	c++ random_input.cpp -o rand.out
	