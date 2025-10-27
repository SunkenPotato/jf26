CXX 	 := g++
CXXFLAGS := -Wall -Wextra -std=c++20 -pthread

BIN       := bin
SRC       := src
LIB       := ../lib
INCLUDE   := ../include
LIBRARIES := -lpigpio -lrt
EXEC      := main.a

all: $(BIN)/$(EXEC)

run: clean all
	clear
	@echo "Executing..."
	./$(BIN)/$(EXEC)

$(BIN)/$(EXEC): $(SRC)/*.cpp
	@echo "Building..."
	if [ ! -d $(BIN) ]; then mkdir $(BIN); fi
	$(CXX) $(CXXFLAGS) -I$(INCLUDE) -L$(LIB) $^ -o $@ $(LIBRARIES)

clean:
	@echo "Cleaning..."
	-rm $(BIN)/*
