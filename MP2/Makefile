all: vec_router ls_router manager_send

vec_router: main.c monitor_neighbors.c
	gcc -g -pthread -o vec_router main.c monitor_neighbors.c

ls_router: main.c monitor_neighbors.c
	gcc -g -pthread -o ls_router main.c monitor_neighbors.c

manager_send: manager_send.c
	gcc -g -o manager_send manager_send.c

.PHONY: clean
clean:
	rm *.o vec_router ls_router manager_send
