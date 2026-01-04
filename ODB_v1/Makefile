CC = gcc
CFLAGS = -Wall -fPIC
LDFLAGS = -shared -ldl

# ============================
#   CONFIG
# ============================
WEB_PORT_1 = 8081
WEB_PORT_2 = 9000
LB_PORT_1  = 8080
LB_PORT_2  = 8081
BE_PORT    = 9000

ALL_PORTS = $(WEB_PORT_1) $(WEB_PORT_2) $(LB_PORT_1) $(LB_PORT_2) $(BE_PORT)


# Kill any process using a port
killport = \
	@if sudo lsof -i :$(1) >/dev/null 2>&1; then \
		echo "[INFO] Killing process on port $(1)..."; \
		sudo kill -9 $$(sudo lsof -t -i :$(1)); \
	else \
		echo "[INFO] No process on port $(1)."; \
	fi



# ============================
#   BUILD TARGETS
# ============================

all:  webServer loadBalencer backend odb_frontend.so odb_backend.so odb_is.so 

# Compiler binaire avec websocket.c inclus
webServer:
	$(CC) $(CFLAGS) -o webServer webServer.c websocket.c

loadBalencer:
	$(CC) $(CFLAGS) -o loadBalencer loadBalencer.c websocket.c

backend:
	$(CC) $(CFLAGS) -o backend backend.c websocket.c

odb_frontend.so: odb_frontend.c
	$(CC) $(CFLAGS) -shared -o $@ $< -ldl

odb_backend.so: odb_backend.c
	$(CC) $(CFLAGS) -shared -o $@ $< -ldl

odb_is.so: odb_is.c
	$(CC) $(CFLAGS) -shared -o $@ $< -ldl

# ============================
#   NORMAL RUN TARGETS
# ============================

run_webServer: webServer
	$(call killport,$(WEB_PORT_1))
	$(call killport,$(WEB_PORT_2))
	echo "[START] Running webServer"
	./webServer

run_loadBalencer: loadBalencer
	$(call killport,$(LB_PORT_1))
	$(call killport,$(LB_PORT_2))
	echo "[START] Running loadBalencer"
	./loadBalencer

run_backend: backend
	$(call killport,$(BE_PORT))
	echo "[START] Running backend"
	./backend

# ============================
#   LD_PRELOAD RUN TARGETS
# ============================

run_webServer_preload: webServer odb_is.so
	$(call killport,$(WEB_PORT_1))
	$(call killport,$(WEB_PORT_2))
	echo "[START] Running webServer with LD_PRELOAD"
	LD_PRELOAD=./odb_is.so ./webServer 

run_loadBalencer_preload: loadBalencer odb_frontend.so
	$(call killport,$(LB_PORT_1))
	$(call killport,$(LB_PORT_2))
	echo "[START] Running loadBalencer with LD_PRELOAD"
	LD_PRELOAD=./odb_frontend.so ./loadBalencer

run_backend_preload: backend odb_backend.so
	$(call killport,$(BE_PORT))
	echo "[START] Running backend with LD_PRELOAD"
	LD_PRELOAD=./odb_backend.so ./backend

# ============================
#   RUN ALL (BACKGROUND)
# ============================

run_all: kill_all_ports all
	echo "[RUN_ALL] Starting backend..."
	./backend & echo "backend PID: $$!"
	echo "[RUN_ALL] Starting loadBalencer..."
	./loadBalencer & echo "loadBalencer PID: $$!"
	echo "[RUN_ALL] Starting webServer..."
	./webServer & echo "webServer PID: $$!"
	echo "[RUN_ALL] All services started."

run_all_preload: kill_all_ports all
	echo "[RUN_ALL] Starting backend with LD_PRELOAD..."
	LD_PRELOAD=./odb_backend.so ./backend & echo "backend PID: $$!"
	echo "[RUN_ALL] Starting loadBalencer with LD_PRELOAD..."
	LD_PRELOAD=./odb_frontend.so ./loadBalencer & echo "loadBalencer PID: $$!"
	echo "[RUN_ALL] Starting webServer with LD_PRELOAD..."
	LD_PRELOAD=./odb_is.so ./webServer & echo "webServer PID: $$!"
	echo "[RUN_ALL] All services started with preload."

# ============================
#   KILL ALL PORTS
# ============================

kill_all_ports:
	@for p in $(ALL_PORTS); do \
		{ if sudo lsof -i :$$p >/dev/null 2>&1; then \
			echo "[INFO] Killing process on port $$p..."; \
			sudo kill -9 $$(sudo lsof -t -i :$$p); \
		  else \
			echo "[INFO] No process on port $$p."; \
		  fi; \
		}; \
	done


# ============================
#   CLEAN
# ============================

clean:
	rm -f webServer websocket loadBalencer backend *.so

