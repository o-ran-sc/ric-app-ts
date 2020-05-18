#!/bin/bash


docker build --tag localhost:5000/ts-write-sdl:0.0.1 .

helm install helm --name dbprepop  --namespace ricplt
