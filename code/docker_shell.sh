#!/bin/bash
# Get a bash shell in the docker environment.

docker run --privileged -v $(pwd):/code -u $(id -u):$(id -g) -v $(cd ..; pwd):/project_base  -it $(cat .docker_image_name) bash --rcfile /code/.docker_bashrc
