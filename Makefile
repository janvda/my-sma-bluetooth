export COMPOSE_DOCKER_CLI_BUILD=0

current_docker_context:=`docker context ls | grep -e "*" | cut -d ' ' -f1 `
project_name=my-sma-bluetooth
compose_override=docker-compose.$(current_docker_context).yml

default: current_context

current_context:
	@echo  "========================================================================================="
	@echo  "current context:"
	@echo  "    current docker context = $(current_docker_context)"
	@echo  "    project_name = $(project_name)"
	@echo  "    COMPOSE_DOCKER_CLI_BUILD = $(COMPOSE_DOCKER_CLI_BUILD)"
	@echo  "    docker_compose_override = $(compose_override)"
	@echo  "========================================================================================="
	@test -s $(compose_override) || { echo "ERROR: File $(compose_override) doesn't exist.  Exiting..." ; exit 1; }

all: 
	docker-compose -f docker-compose.yml -f $(compose_override)  -p $(project_name) up -d --build

up: 
	docker-compose -f docker-compose.yml -f $(compose_override)  -p $(project_name) up -d

down stop start restart ps logs top images build: 
	docker-compose -f docker-compose.yml -f $(compose_override)  -p $(project_name) $@

copy_config:
	docker cp .my-smatool.conf `docker ps -aqf "name=my-sma-bluetooth"`:/etc/smatool.conf

.PHONY: default up  build down stop start restart ps logs top images 