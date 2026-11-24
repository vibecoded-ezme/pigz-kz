# The ksflate engine and its public C API are vendored under engine/ so this
# fork builds standalone. `make sync-engine` refreshes them from ../ksflate.
ENGINE_DIR=engine

CC=gcc
CXX=g++
CFLAGS=-O3 -Wall -Wextra -Wno-unknown-pragmas -Wcast-qual -I$(ENGINE_DIR)
CXXFLAGS=-std=c++20 -O2 -Wall -Wextra -Wpedantic -I$(ENGINE_DIR)
LDFLAGS=
# CFLAGS=-O3 -Wall -Wextra -Wno-unknown-pragmas -Wcast-qual -g -fsanitize=thread
# LDFLAGS=-g -fsanitize=thread
# CFLAGS=-O3 -Wall -Wextra -Wno-unknown-pragmas -Wcast-qual -g -fsanitize=address
# LDFLAGS=-g -fsanitize=address
LIBS=-lm -lpthread -lz
ZOPFLI=zopfli/src/zopfli/
ZOP=deflate.o blocksplitter.o tree.o lz77.o cache.o hash.o util.o squeeze.o katajainen.o symbols.o

ENGINE_OBJS=strategy0_encoder.o strategy12_parser.o strategy0_model.o \
	parser_cost.o huffman.o deflate_writer.o block_splitter.o \
	split_score_model.o
KSFLATE_OBJS=$(ENGINE_OBJS) ksflate_api.o

# use gcc and gmake on Solaris

pigz: pigz.o yarn.o try.o $(ZOP) $(KSFLATE_OBJS)
	$(CXX) $(LDFLAGS) -o pigz pigz.o yarn.o try.o $(ZOP) $(KSFLATE_OBJS) $(LIBS)
	ln -f pigz unpigz

pigz.o: pigz.c yarn.h try.h $(ENGINE_DIR)/ksflate.h $(ZOPFLI)deflate.h $(ZOPFLI)util.h

ksflate_api.o: $(ENGINE_DIR)/ksflate_api.cpp $(ENGINE_DIR)/ksflate.h \
		$(ENGINE_DIR)/strategy0_encoder.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $(ENGINE_DIR)/ksflate_api.cpp

$(ENGINE_OBJS): %.o: $(ENGINE_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Development convenience: refresh the vendored engine and C API from the
# ksflate tree. Not needed to build; the copy under engine/ ships with the fork.
sync-engine:
	@test -d ../ksflate/src || \
		{ echo "sync-engine: ../ksflate/src not found"; exit 1; }
	cp ../ksflate/src/*.cpp ../ksflate/src/*.hpp engine/
	cp ../ksflate/include/ksflate.h engine/

tests/engine_wrapper: tests/engine_wrapper.cpp $(KSFLATE_OBJS) \
		$(ENGINE_DIR)/ksflate.h
	$(CXX) $(CXXFLAGS) -I. -o $@ tests/engine_wrapper.cpp $(KSFLATE_OBJS) -lz

yarn.o: yarn.c yarn.h

try.o: try.c try.h

deflate.o: $(ZOPFLI)deflate.c $(ZOPFLI)deflate.h $(ZOPFLI)blocksplitter.h $(ZOPFLI)lz77.h $(ZOPFLI)squeeze.h $(ZOPFLI)tree.h $(ZOPFLI)zopfli.h $(ZOPFLI)cache.h $(ZOPFLI)hash.h $(ZOPFLI)util.h $(ZOPFLI)symbols.h
	$(CC) $(CFLAGS) -c $(ZOPFLI)deflate.c

blocksplitter.o: $(ZOPFLI)blocksplitter.c $(ZOPFLI)blocksplitter.h $(ZOPFLI)deflate.h $(ZOPFLI)lz77.h $(ZOPFLI)squeeze.h $(ZOPFLI)tree.h $(ZOPFLI)util.h $(ZOPFLI)zopfli.h $(ZOPFLI)cache.h $(ZOPFLI)hash.h
	$(CC) $(CFLAGS) -c $(ZOPFLI)blocksplitter.c

tree.o: $(ZOPFLI)tree.c $(ZOPFLI)tree.h $(ZOPFLI)katajainen.h $(ZOPFLI)util.h
	$(CC) $(CFLAGS) -c $(ZOPFLI)tree.c

lz77.o: $(ZOPFLI)lz77.c $(ZOPFLI)lz77.h $(ZOPFLI)util.h $(ZOPFLI)cache.h $(ZOPFLI)hash.h $(ZOPFLI)zopfli.h $(ZOPFLI)symbols.h
	$(CC) $(CFLAGS) -c $(ZOPFLI)lz77.c

cache.o: $(ZOPFLI)cache.c $(ZOPFLI)cache.h $(ZOPFLI)util.h
	$(CC) $(CFLAGS) -c $(ZOPFLI)cache.c

hash.o: $(ZOPFLI)hash.c $(ZOPFLI)hash.h $(ZOPFLI)util.h
	$(CC) $(CFLAGS) -c $(ZOPFLI)hash.c

util.o: $(ZOPFLI)util.c $(ZOPFLI)util.h
	$(CC) $(CFLAGS) -c $(ZOPFLI)util.c

squeeze.o: $(ZOPFLI)squeeze.c $(ZOPFLI)squeeze.h $(ZOPFLI)blocksplitter.h $(ZOPFLI)deflate.h $(ZOPFLI)tree.h $(ZOPFLI)util.h $(ZOPFLI)zopfli.h $(ZOPFLI)lz77.h $(ZOPFLI)cache.h $(ZOPFLI)hash.h $(ZOPFLI)symbols.h
	$(CC) $(CFLAGS) -c $(ZOPFLI)squeeze.c

katajainen.o: $(ZOPFLI)katajainen.c $(ZOPFLI)katajainen.h
	$(CC) $(CFLAGS) -c $(ZOPFLI)katajainen.c

symbols.o: $(ZOPFLI)symbols.c $(ZOPFLI)symbols.h
	$(CC) $(CFLAGS) -c $(ZOPFLI)symbols.c

dev: pigz pigzj pigzt pigzn

pigzj: pigzj.o yarn.o try.o $(KSFLATE_OBJS)
	$(CXX) $(LDFLAGS) -o pigzj pigzj.o yarn.o try.o $(KSFLATE_OBJS) $(LIBS)

pigzj.o: pigz.c yarn.h try.h $(ENGINE_DIR)/ksflate.h
	$(CC) $(CFLAGS) -DNOZOPFLI -c -o pigzj.o pigz.c

pigzt: pigzt.o yarnt.o try.o $(ZOP) $(KSFLATE_OBJS)
	$(CXX) $(LDFLAGS) -o pigzt pigzt.o yarnt.o try.o $(ZOP) $(KSFLATE_OBJS) $(LIBS)

pigzt.o: pigz.c yarn.h try.h $(ENGINE_DIR)/ksflate.h
	$(CC) $(CFLAGS) -DPIGZ_DEBUG -g -c -o pigzt.o pigz.c

yarnt.o: yarn.c yarn.h
	$(CC) $(CFLAGS) -DPIGZ_DEBUG -g -c -o yarnt.o yarn.c

pigzn: pigzn.o tryn.o $(ZOP)
	$(CC) $(LDFLAGS) -o pigzn pigzn.o tryn.o $(ZOP) $(LIBS)

pigzn.o: pigz.c try.h $(ENGINE_DIR)/ksflate.h
	$(CC) $(CFLAGS) -DPIGZ_DEBUG -DNOTHREAD -g -c -o pigzn.o pigz.c

tryn.o: try.c try.h
	$(CC) $(CFLAGS) -DPIGZ_DEBUG -DNOTHREAD -g -c -o tryn.o try.c

test: pigz tests/engine_wrapper
	./pigz -kf pigz.c ; ./pigz -t pigz.c.gz
	./pigz -kfb 32 pigz.c ; ./pigz -t pigz.c.gz
	./pigz -kfp 1 pigz.c ; ./pigz -t pigz.c.gz
	./pigz -kfz pigz.c ; ./pigz -t pigz.c.zz
	./pigz -kfK pigz.c ; ./pigz -t pigz.c.zip
	./pigz -kf -10 pigz.c ; ./pigz -t pigz.c.gz
	./pigz -kf -10 -p 1 pigz.c ; ./pigz -t pigz.c.gz
	./pigz -kf -10 -R -b 32 pigz.c ; ./pigz -t pigz.c.gz
	./pigz -kf -10 -i pigz.c ; ./pigz -t pigz.c.gz
	./pigz -kf -10 -H pigz.c ; ./pigz -t pigz.c.gz
	./pigz -kf -10 -z pigz.c ; ./pigz -t pigz.c.zz
	./pigz -kf -10 -K pigz.c ; ./pigz -t pigz.c.zip
	./tests/engine_wrapper
	printf "" | ./pigz -cdf | wc -c | test `cat` -eq 0
	printf "x" | ./pigz -cdf | wc -c | test `cat` -eq 1
	printf "xy" | ./pigz -cdf | wc -c | test `cat` -eq 2
	printf "xyz" | ./pigz -cdf | wc -c | test `cat` -eq 3
	(printf "w" | gzip ; printf "x") | ./pigz -cdf | wc -c | test `cat` -eq 2
	(printf "w" | gzip ; printf "xy") | ./pigz -cdf | wc -c | test `cat` -eq 3
	(printf "w" | gzip ; printf "xyz") | ./pigz -cdf | wc -c | test `cat` -eq 4
	-@if test "`which compress | grep /`" != ""; then \
	  echo 'compress -f < pigz.c | ./unpigz | cmp - pigz.c' ;\
	  compress -f < pigz.c | ./unpigz | cmp - pigz.c ;\
	fi
	@rm -f pigz.c.gz pigz.c.zz pigz.c.zip

tests: dev test
	./pigzn -kf pigz.c ; ./pigz -t pigz.c.gz
	@rm -f pigz.c.gz

docs: pigz.pdf

pigz.pdf: pigz.1
	groff -mandoc -f H -T ps pigz.1 | ps2pdf - pigz.pdf

all: pigz pigzj pigzt pigzn docs

clean:
	@rm -f *.o pigz unpigz pigzj pigzn pigzt pigz.c.gz pigz.c.zz pigz.c.zip \
		tests/engine_wrapper
