#!/bin/bash
docker exec --user "$(id -u)" -it `docker ps | grep px4io | cut -f1 -d' '` bash
