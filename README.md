# ODB: On Demand Buffer

Ce projet a été réalisé dans le cadre du module **Projet Réseau 3A SEOC**. Il a pour objectif de concevoir et d’implémenter un protocole réseau ainsi qu’une bibliothèque système permettant l’intégration du concept d’On-Demand Buffer au sein de serveurs web.

## Structure du projet


```
On_Demand_Buffer
├── ODB_v1
│   ├── backend.c
│   ├── file
│   │   └── index.html
│   ├── loadBalencer.c
│   ├── Makefile
│   ├── odb_backend.c
│   ├── odb_frontend.c
│   ├── odb_is.c
│   ├── webServer.c
│   ├── websocket.c
│   └── websocket.h
└── ODB_v2
    ├── backend.c
    ├── file
    │   └── Index.html
    ├── file_descriptor.c
    ├── file_descriptor.h
    ├── loadBalencer.c
    ├── Makefile
    ├── odb_backend.c
    ├── odb_frontend.c
    ├── odb_is.c
    ├── webServer.c
    ├── websocket.c
    └── websocket.h
```

Le projet est structuré en deux sous-répertoires correspondant à deux versions distinctes. La version 2 intègre des optimisations supplémentaires, notamment dans les cas où l’un des IS tente de modifier le fichier en cours de transmission. 

Chaque version contient les composants suivants :

- un serveur web,
- un load balancer,
- un backend,
- ainsi que les bibliothèques ODB associées à chaque composant.


## Installation et configuration

Cloner le dépôt puis se placer dans le répertoire du projet :

```bash
git clone https://github.com/Yassine-elhammoumi/Projet_reseau_seoc_2026.git
cd Projet_reseau_seoc_2026
```

Compiler la version souhaitée du projet :

```bash
cd ODB_v2 # ou ODB_v1
make all
```

Un **Makefile** est fourni et intègre plusieurs cibles facilitant la compilation, l’exécution et la gestion des différents composants du projet :

```bash
all                           # Compile l’ensemble des binaires
clean                         # Nettoie l’arborescence de compilation
kill_all_ports                # Termine les applications utilisant les ports spécifiques au projet

run_all                       # Exécute tous les binaires en mode normal
run_all_preload               # Exécute tous les binaires avec le mécanisme ODB activé

run_backend                   # Lance le backend
run_backend_preload           # Lance le backend avec ODB

run_loadBalancer              # Lance le load balancer
run_loadBalancer_preload      # Lance le load balancer avec ODB

run_webServer                 # Lance le serveur web
run_webServer_preload         # Lance le serveur web avec ODB
```

### Configuration manuelle

#### Compilation

```bash
gcc -Wall -fPIC -o webServer webServer.c websocket.c
gcc -Wall -fPIC -o loadBalencer loadBalencer.c websocket.c
gcc -Wall -fPIC -o backend backend.c websocket.c
gcc -Wall -fPIC -shared -o odb_frontend.so odb_frontend.c -ldl
gcc -Wall -fPIC -shared -o odb_backend.so odb_backend.c -ldl
gcc -Wall -fPIC -shared -o odb_is.so odb_is.c -ldl
```

#### Exécution

L’activation du mécanisme ODB repose sur l’utilisation de la variable d’environnement LD_PRELOAD, permettant l’injection dynamique de la bibliothèque correspondante au composant exécuté.

```bash
LD_PRELOAD=./odb_is.so ./webServer
```


```bash
LD_PRELOAD=./odb_frontend.so ./loadBalencer
```


```bash
LD_PRELOAD=./odb_backend.so ./backend
```
