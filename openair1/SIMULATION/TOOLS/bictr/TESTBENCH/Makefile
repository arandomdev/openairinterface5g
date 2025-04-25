OPENAIR_DIR = $(shell readlink -f ../../../../../)
BICTR_DIR = $(OPENAIR_DIR)/openair1/SIMULATION/TOOLS/bictr

all: test_bictr

# .SILENT: test_bictr
test_bictr: test_bictr.c
	$(CC) test_bictr.c \
		$(BICTR_DIR)/bictr.c \
		$(BICTR_DIR)/mtwister.c \
		-o test_bictr \
		-g \
		-lm \
		-lgmt \
		-I$(OPENAIR_DIR) \
		-I$(OPENAIR_DIR)/common/utils \
		-I$(OPENAIR_DIR)/openair1 \
		-I$(BICTR_DIR)

clean:
	rm -f test_bictr