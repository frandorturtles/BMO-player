test: test_core
	./test_core
test_core: test_core.cpp ../../lib/bmo_core/*.h
	g++ -std=c++17 -Wall -Wextra -O1 -fsanitize=address,undefined -o test_core test_core.cpp
clean:
	rm -f test_core
