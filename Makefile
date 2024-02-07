.PHONY all:
all:
	gcc -Wall -D PART1 csc360_p3.c -o diskinfo
	gcc -Wall -D PART2 csc360_p3.c -o disklist
	gcc -Wall -D PART3 csc360_p3.c -o diskget
	gcc -Wall -D PART4 csc360_p3.c -o diskput

.PHONY clean:
clean:
	-rm diskinfo disklist diskget diskput