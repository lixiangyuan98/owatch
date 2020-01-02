SRC=rtp.cpp collector.cpp connection.cpp parse_args.cpp
OBJS=$(SRC:%.cpp=%.o)
TEST_SRC=test/connection_test.cpp test/file_test.cpp test/un_server_test.cpp  test/un_client_test.cpp
TEST=$(TEST_SRC:%.cpp=%)
RELEASE_DIR=$(shell pwd)/release

CC=$(CROSS_COMPILE)g++
LD=$(CROSS_COMPILE)ld
CPPFLAGS=-g -O3 -Wall
INC=
LIB=-ljrtp -lpthread -lrt

.PHONY: default owatch clean test all release

default: owatch

all: owatch test

$(OBJS): %.o: %.cpp
	$(CC) $(CPPFLAGS) $(INC) -c -o $@ $<

owatch: owatch.cpp $(OBJS)
	$(CC) $(CPPFLAGS) $(INC) -o $@ $< $(OBJS) $(LIB)
	@echo "\nDone!"

$(TEST): %: %.cpp $(OBJS)
	$(CC) $(CPPFLAGS) $(INC) -o $@ $< $(OBJS) $(LIB)

test: $(TEST)
	@echo "\nDone! See source file to find how to test."

clean:
	@rm -f *.o owatch
	@rm -f $(TEST)
	@rm -rf $(RELEASE_DIR)

release:
	@mkdir -p $(RELEASE_DIR)
	@mkdir -p $(RELEASE_DIR)/test
	@cp owatch $(RELEASE_DIR)/
	@cp $(TEST) $(RELEASE_DIR)/test/
