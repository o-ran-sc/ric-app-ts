
coverage_opts = -ftest-coverage -fprofile-arcs

binaries = unit_test 

# RMR emulation
rmr_em.o::	rmr_em.c
	cc -g rmr_em.c -c

unit_test:: unit_test.cpp rmr_em.o
	# do NOT link the xapp lib; we include all modules in the test programme
	g++ -g $(coverage_opts) -I ../src/messaging unit_test.cpp -o unit_test rmr_em.o  -lpthread

# prune gcov files generated by system include files
clean::
	rm -f *.h.gcov *.c.gcov

# ditch anything that can be rebuilt
nuke::
	rm -f *.a *.o *.gcov *.gcda *.gcno core a.out $(binaries)

