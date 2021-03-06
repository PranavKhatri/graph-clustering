all: serial parallel

.PHONY: serial parallel clean unit_tests tester
serial:
	@echo "Compiling the serial code..."
	mkdir -p output times
	make -C serial

parallel:
	@echo "Compiling the parallel code..."
	mkdir -p output times
	make -C parallel

tester_serial:
	@echo "Compiling serial tester..."
	make -C serial tester_serial

tester_parallel:
	@echo "Compiling parallel tester..."
	make -C parallel tester_parallel

clean:
	rm -rf *.z *.dSYM *.otf *.thumb tester* output/temp*
	make -C serial clean
	make -C parallel clean
	make -C unit_tests clean

unit_tests:
	@echo "Serial unit testing..."
	make -C unit_tests test
