TARGET=trafficlight.out
APP_DIR = $(CURDIR)
APP_BIN = $(APP_DIR)/trafficlight.out

MK2_IP_ADDR :=192.168.1.1
MK2_APP_DIR :=/home/root

OBJ = main.o \
	  tl_tcpserver.o \
	  tl_study.o	\

all:$(TARGET)

$(TARGET):$(OBJ)
	$(CC) -o $(TARGET) $(OBJ) -lpthread

%.o:%.c
	$(CC) -c $^ -o $@

install:$(APP_BIN)
	sudo rm ~/.ssh -rf
	sshpass -p root scp -o "StrictHostKeyChecking no" $(APP_BIN) $(MK2_IP_ADDR):$(MK2_APP_DIR)
	#sshpass -p root scp -o "StrictHostKeyChecking no" $(APP_CONFIG) $(MK2_IP_ADDR):$(MK2_CONFIG_DIR)

clean:
	rm -rf $(TARGET) $(OBJ)
