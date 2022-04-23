# the compiler: gcc for C program, define as g++ for C++
CC = g++

all: serverM.out serverA.out serverB.out serverC.out clientA.out clientB.out

clean:
	@ $(RM) serverC serverT serverS serverP clientA clientB

serverM.out: serverM.cpp
	@ $(CC) -o serverM serverM.cpp

serverA.out: serverA.cpp
	@ $(CC) -o serverA serverA.cpp

serverB.out: serverB.cpp
	@ $(CC) -o serverB serverB.cpp

serverC.out: serverC.cpp
	@ $(CC) -o serverC serverC.cpp

clientA.out: clientA.cpp
	@ $(CC) -o clientA clientA.cpp

clientB.out: clientB.cpp
	@ $(CC) -o clientB clientB.cpp
